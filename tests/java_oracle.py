"""
Thin jpype wrapper around the Tetrad 7.6.8 JAR for use as a Java oracle
in Java vs C++ comparison tests.

Requires:
  - Java 21+ on PATH
  - jars/tetrad-gui-7.6.8-launch.jar (downloaded from Maven Central)
  - jpype1 installed in the venv

Usage:
    from tests.java_oracle import TetradOracle
    oracle = TetradOracle()
    edges = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0)
    # returns list of strings like ["X1 o-> X2", "X2 --> X3"]
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd

_DEFAULT_JAR = Path(__file__).parent.parent / "jars" / "tetrad-gui-7.6.8-launch.jar"

_EDGE_RE = re.compile(r"^\d+\.\s*")
_SEP_TOKENS = ["<->", "-->", "o->", "o-o", "<-o", "<--", "---"]


def _parse_edges(graph_str: str) -> list[str]:
    """Parse Tetrad graph.toString() output into canonical edge strings."""
    edges = []
    if not graph_str:
        return edges
    for line in str(graph_str).strip().split("\n"):
        line = _EDGE_RE.sub("", line.strip())
        for sep in _SEP_TOKENS:
            if sep in line:
                parts = line.split(sep, 1)
                if len(parts) == 2:
                    edges.append(f"{parts[0].strip()} {sep} {parts[1].strip()}")
                break
    return edges


class TetradOracle:
    """
    Wraps the Tetrad 7.6.8 JAR via jpype. The JVM is started once and
    reused across calls (jpype does not support restart within a process).
    """

    def __init__(self, jar: Optional[Path] = None, jvm_args: str = "-Xmx4g"):
        import jpype
        import jpype.imports

        self._jar = Path(jar) if jar else _DEFAULT_JAR
        if not self._jar.exists():
            raise FileNotFoundError(
                f"Tetrad JAR not found at {self._jar}. "
                "Download with:\n"
                "  curl -o jars/tetrad-gui-7.6.8-launch.jar \\\n"
                "    https://repo1.maven.org/maven2/io/github/cmu-phil/"
                "tetrad-gui/7.6.8/tetrad-gui-7.6.8-launch.jar"
            )

        if not jpype.isJVMStarted():
            jpype.startJVM(jvm_args, classpath=str(self._jar))

        self._util = jpype.JPackage("java.util")
        self._td = jpype.JPackage("edu.cmu.tetrad.data")
        self._ts = jpype.JPackage("edu.cmu.tetrad.search")
        self._score_pkg = jpype.JPackage("edu.cmu.tetrad.search.score")
        self._test_pkg = jpype.JPackage("edu.cmu.tetrad.search.test")

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def run(
        self,
        algo: str,
        df: pd.DataFrame,
        *,
        alpha: float = 0.01,
        penalty_discount: float = 1.0,
        knowledge: Optional[dict] = None,
    ) -> list[str]:
        """
        Run a Tetrad algorithm and return edges as a list of strings.

        algo: one of "pc", "fges", "gfci", "boss", "boss_fci", "grasp", "grasp_fci"
        """
        data = self._df_to_dataset(df)
        kn = self._build_knowledge(knowledge, df.columns.tolist()) if knowledge else self._td.Knowledge()

        algo = algo.lower().replace("-", "_")
        dispatch = {
            "pc": self._run_pc,
            "fges": self._run_fges,
            "gfci": self._run_gfci,
            "boss": self._run_boss,
            "boss_fci": self._run_boss_fci,
            "grasp": self._run_grasp,
            "grasp_fci": self._run_grasp_fci,
        }
        if algo not in dispatch:
            raise ValueError(f"Unknown algorithm '{algo}'. Choose from: {list(dispatch)}")

        import jpype
        try:
            graph_str = dispatch[algo](data, alpha, penalty_discount, kn)
        except jpype.JException as e:
            raise RuntimeError(f"Java exception in {algo}: {e}") from e

        return _parse_edges(graph_str)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _df_to_dataset(self, df: pd.DataFrame):
        n, p = df.shape
        variables = self._util.ArrayList()
        for col in df.columns:
            variables.add(self._td.ContinuousVariable(str(col)))
        databox = self._td.DoubleDataBox(n, p)
        for col_idx, col_vals in enumerate(df.values.T):
            for row_idx, val in enumerate(col_vals):
                databox.set(row_idx, col_idx, float(val))
        return self._td.BoxDataSet(databox, variables)

    def _build_knowledge(self, knowledge: dict, all_cols: list[str]):
        kn = self._td.Knowledge()
        # Support both "tiers" and legacy "addtemporal" key
        tiers = knowledge.get("tiers") or knowledge.get("addtemporal") or {}
        for tier, vars_ in tiers.items():
            for v in vars_:
                kn.addToTier(int(tier), str(v))
        if "forbidden" in knowledge:
            for src, tgt in knowledge["forbidden"]:
                kn.setForbidden(str(src), str(tgt))
        if "required" in knowledge:
            for src, tgt in knowledge["required"]:
                kn.setRequired(str(src), str(tgt))
        return kn

    def _make_test(self, data, alpha: float):
        return self._test_pkg.IndTestFisherZ(data, alpha)

    def _make_score(self, data, penalty_discount: float):
        score = self._score_pkg.SemBicScore(data, True)
        score.setPenaltyDiscount(penalty_discount)
        score.setStructurePrior(0)
        return score

    def _run_pc(self, data, alpha, penalty_discount, kn) -> str:
        test = self._make_test(data, alpha)
        pc = self._ts.Pc(test)
        pc.setDepth(-1)
        pc.setKnowledge(kn)
        pc.setVerbose(False)
        return pc.search().toString()

    def _run_fges(self, data, alpha, penalty_discount, kn) -> str:
        score = self._make_score(data, penalty_discount)
        fges = self._ts.Fges(score)
        fges.setFaithfulnessAssumed(True)
        fges.setKnowledge(kn)
        fges.setVerbose(False)
        return fges.search().toString()

    def _run_gfci(self, data, alpha, penalty_discount, kn) -> str:
        test = self._make_test(data, alpha)
        score = self._make_score(data, penalty_discount)
        gfci = self._ts.Gfci(test, score)
        gfci.setCompleteRuleSetUsed(True)
        gfci.setDepth(-1)
        gfci.setFaithfulnessAssumed(True)
        gfci.setMaxDiscriminatingPathLength(-1)
        gfci.setKnowledge(kn)
        gfci.setVerbose(False)
        return gfci.search().toString()

    def _run_boss(self, data, alpha, penalty_discount, kn) -> str:
        score = self._make_score(data, penalty_discount)
        boss = self._ts.Boss(score)
        ps = self._ts.PermutationSearch(boss)
        ps.setKnowledge(kn)
        return ps.search().toString()

    def _run_boss_fci(self, data, alpha, penalty_discount, kn) -> str:
        test = self._make_test(data, alpha)
        score = self._make_score(data, penalty_discount)
        boss_fci = self._ts.BossFci(test, score)
        boss_fci.setCompleteRuleSetUsed(True)
        boss_fci.setDepth(-1)
        boss_fci.setMaxDiscriminatingPathLength(-1)
        boss_fci.setKnowledge(kn)
        boss_fci.setVerbose(False)
        return boss_fci.search().toString()

    def _run_grasp(self, data, alpha, penalty_discount, kn) -> str:
        score = self._make_score(data, penalty_discount)
        grasp = self._ts.Grasp(score)
        grasp.setKnowledge(kn)
        grasp.setVerbose(False)
        grasp.bestOrder(data.getVariables())
        return grasp.getGraph(True).toString()

    def _run_grasp_fci(self, data, alpha, penalty_discount, kn) -> str:
        test = self._make_test(data, alpha)
        score = self._make_score(data, penalty_discount)
        grasp_fci = self._ts.GraspFci(test, score)
        grasp_fci.setCompleteRuleSetUsed(True)
        grasp_fci.setDepth(-1)
        grasp_fci.setMaxDiscriminatingPathLength(-1)
        grasp_fci.setKnowledge(kn)
        grasp_fci.setVerbose(False)
        return grasp_fci.search().toString()
