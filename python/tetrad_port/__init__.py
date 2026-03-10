"""
tetrad_port: Python bindings for causal discovery algorithms from CMU's Tetrad.

Provides PC, FGES, GFCI, BOSS, BOSS-FCI, GRaSP, GRaSP-FCI, and composable
FCI algorithms with a simple facade API:

    tp = TetradPort()
    results, graph_info = tp.run_pc(df, alpha=0.05)
    results, graph_info = tp.run_fges(df, penalty_discount=1.0)
    results, graph_info = tp.run_gfci(df, alpha=0.05)
    results, graph_info = tp.run_boss(df, penalty_discount=1.0)
    results, graph_info = tp.run_boss_fci(df, alpha=0.05)
    results, graph_info = tp.run_grasp(df, penalty_discount=1.0)
    results, graph_info = tp.run_grasp_fci(df, alpha=0.05)
    results, graph_info = tp.run_fci(df, initial_algorithm="fges")
"""

from __future__ import annotations

from typing import Any, Optional, Union

import numpy as np
import pandas as pd

from tetrad_port._tetrad_cpp import (
    run_pc_raw, run_fges_raw, run_gfci_raw, run_boss_raw, run_boss_fci_raw,
    run_grasp_raw, run_grasp_fci_raw, run_fci_raw,
    PcResult, SearchResult, Knowledge,
)

__version__ = "0.2.2"
__all__ = ["TetradPort", "Knowledge", "dict_to_knowledge"]

ALGORITHMS = ("pc", "fges", "gfci", "boss", "boss_fci", "grasp", "grasp_fci", "fci")

# Initial algorithms that can be used with run_fci()
FCI_INITIAL_ALGORITHMS = ("pc", "fges", "boss", "grasp")


def dict_to_knowledge(knowledge_dict: Optional[dict]) -> Optional[Knowledge]:
    """
    Convert a knowledge dict to a C++ Knowledge object.

    Parameters
    ----------
    knowledge_dict : dict or None
        Knowledge dict with optional keys:

        - ``'addtemporal'``: ``{tier_int: [var_names]}`` — temporal tiers
        - ``'forbiddirect'``: ``[(from_var, to_var), ...]`` — forbidden edges
        - ``'requiredirect'``: ``[(from_var, to_var), ...]`` — required edges
        - ``'forbidden_within'``: ``{tier_int, ...}`` — forbid edges within tier

    Returns
    -------
    Knowledge or None
    """
    if knowledge_dict is None:
        return None

    k = Knowledge()

    if "addtemporal" in knowledge_dict:
        forbidden_within = knowledge_dict.get("forbidden_within", set())
        for tier, variables in knowledge_dict["addtemporal"].items():
            tier_int = int(tier)
            for var in variables:
                k.add_to_tier(tier_int, var)
            if tier_int in forbidden_within:
                k.set_tier_forbidden_within(tier_int, True)

    if "forbiddirect" in knowledge_dict:
        for from_var, to_var in knowledge_dict["forbiddirect"]:
            k.set_forbidden(from_var, to_var)

    if "requiredirect" in knowledge_dict:
        for from_var, to_var in knowledge_dict["requiredirect"]:
            k.set_required(from_var, to_var)

    return k


class TetradPort:
    """
    Facade class for running causal discovery via the tetrad-port C++ engine.

    Supported algorithms:
    - **PC**: Constraint-based, returns a CPDAG. Good when you have no latent confounders.
    - **FGES**: Score-based (BIC), returns a CPDAG. Faster than PC for large graphs.
    - **GFCI**: Hybrid (score + constraint), returns a PAG. Handles latent confounders.
    - **BOSS**: Permutation-based (BIC), returns a CPDAG. High precision, fast for large sparse graphs.
    - **BOSS-FCI**: BOSS + FCI orientation rules, returns a PAG. Handles latent confounders.
    - **GRaSP**: Permutation-based (tuck DFS), returns a CPDAG. Very high precision.
    - **GRaSP-FCI**: GRaSP + FCI orientation rules, returns a PAG. Handles latent confounders.
    - **FCI**: Composable: any CPDAG algorithm (PC, FGES, BOSS, GRaSP) + FCI rules, returns a PAG.

    Example
    -------
    >>> tp = TetradPort()
    >>> results, graph_info = tp.run_pc(df, alpha=0.05)
    >>> results, graph_info = tp.run_fges(df)
    >>> results, graph_info = tp.run_gfci(df, alpha=0.05)
    >>> results, graph_info = tp.run_boss(df)
    >>> results, graph_info = tp.run_boss_fci(df, alpha=0.05)
    >>> results, graph_info = tp.run_grasp(df)
    >>> results, graph_info = tp.run_grasp_fci(df, alpha=0.05)
    """

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    # ----------------------------------------------------------------
    # Core: run (dispatcher)
    # ----------------------------------------------------------------

    def run(
        self,
        df: pd.DataFrame,
        algorithm: str = "gfci",
        knowledge: Union[Knowledge, dict, None] = None,
        **kwargs,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run a causal discovery algorithm by name.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        algorithm : str
            One of "pc", "fges", "gfci", "boss", "boss_fci", "grasp",
            "grasp_fci".
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        **kwargs
            Algorithm-specific parameters (alpha, penalty_discount, etc.).

        Returns
        -------
        results : dict
            Search results including edges, nodes, counts.
        graph_info : dict
            Parsed edge information (adjacency, directed_edges, etc.)
        """
        dispatch = {
            "pc": self.run_pc,
            "fges": self.run_fges,
            "gfci": self.run_gfci,
            "boss": self.run_boss,
            "boss_fci": self.run_boss_fci,
            "grasp": self.run_grasp,
            "grasp_fci": self.run_grasp_fci,
            "fci": self.run_fci,
        }

        algo = algorithm.lower()
        if algo not in dispatch:
            raise ValueError(
                f"Unknown algorithm: {algorithm!r}. "
                f"Must be one of {ALGORITHMS}."
            )

        return dispatch[algo](df, knowledge=knowledge, **kwargs)

    # ----------------------------------------------------------------
    # Core: run_pc
    # ----------------------------------------------------------------

    def run_pc(
        self,
        df: pd.DataFrame,
        alpha: float = 0.05,
        depth: int = -1,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the PC algorithm on a pandas DataFrame.

        PC is a constraint-based algorithm that uses conditional independence
        tests (Fisher Z) to discover causal structure. It assumes no latent
        confounders (causal sufficiency). Returns a CPDAG.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        alpha : float
            Significance level for conditional independence tests.
        depth : int
            Maximum size of conditioning sets. -1 for no limit.
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges' (list of str), 'nodes' (list of str),
                  'num_edges' (int), 'num_nodes' (int),
                  'alpha' (float), 'depth' (int)
        graph_info : dict
            Keys: 'adjacency' (dict of node -> list of neighbors),
                  'directed_edges' (list of (from, to) tuples),
                  'undirected_edges' (list of (n1, n2) tuples)
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        pc_result: PcResult = run_pc_raw(
            data=data, col_names=col_names,
            alpha=alpha, depth=depth, verbose=v,
            knowledge=k,
        )

        results = {
            "edges": list(pc_result.edges),
            "nodes": list(pc_result.nodes),
            "num_edges": pc_result.num_edges,
            "num_nodes": pc_result.num_nodes,
            "alpha": alpha,
            "depth": depth,
        }

        graph_info = self._parse_edges_to_graph_info(pc_result.edges, pc_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_fges
    # ----------------------------------------------------------------

    def run_fges(
        self,
        df: pd.DataFrame,
        penalty_discount: float = 1.0,
        faithfulness_assumed: bool = True,
        max_degree: int = -1,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the FGES (Fast Greedy Equivalence Search) algorithm.

        FGES is a score-based algorithm that searches over Markov equivalence
        classes using a greedy forward-backward strategy with BIC scoring.
        It assumes causal sufficiency (no latent confounders). Returns a CPDAG.

        Faster than PC for large, sparse graphs.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        penalty_discount : float
            BIC penalty multiplier. 1.0 = standard BIC. Higher values
            produce sparser graphs.
        faithfulness_assumed : bool
            If True, skips the unfaithfulness phase (faster). Default True.
        max_degree : int
            Maximum node degree in the output graph. -1 for unlimited.
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'model_score', 'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        fges_result: SearchResult = run_fges_raw(
            data=data, col_names=col_names,
            penalty_discount=penalty_discount,
            faithfulness_assumed=faithfulness_assumed,
            max_degree=max_degree, verbose=v,
            knowledge=k,
        )

        results = {
            "edges": list(fges_result.edges),
            "nodes": list(fges_result.nodes),
            "num_edges": fges_result.num_edges,
            "num_nodes": fges_result.num_nodes,
            "model_score": fges_result.model_score,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(fges_result.edges, fges_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_gfci
    # ----------------------------------------------------------------

    def run_gfci(
        self,
        df: pd.DataFrame,
        alpha: float = 0.05,
        penalty_discount: float = 1.0,
        depth: int = -1,
        max_degree: int = -1,
        complete_rule_set: bool = True,
        max_disc_path_length: int = -1,
        faithfulness_assumed: bool = True,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the GFCI (Greedy FCI) algorithm.

        GFCI is a hybrid algorithm that combines score-based search (FGES)
        with FCI orientation rules to handle latent (unmeasured) confounders.
        Returns a PAG (Partial Ancestral Graph) which can represent:

        - ``-->`` : definite directed edge (causal)
        - ``---`` : undirected edge
        - ``<->`` : bidirected edge (latent common cause)
        - ``o->`` : partially oriented (circle endpoint)
        - ``o-o`` : fully ambiguous orientation

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        alpha : float
            Significance level for conditional independence tests.
        penalty_discount : float
            BIC penalty multiplier for the FGES phase.
        depth : int
            Maximum conditioning set size. -1 for unlimited.
        max_degree : int
            Maximum node degree. -1 for unlimited.
        complete_rule_set : bool
            If True, use Zhang's complete rules R1-R10 (arrow and tail
            complete). If False, use only Spirtes' R1-R4.
        max_disc_path_length : int
            Maximum discriminating path length for R4. -1 for unlimited.
        faithfulness_assumed : bool
            Faithfulness assumption for the FGES phase.
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'alpha', 'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges',
                  'bidirected_edges', 'partially_oriented_edges',
                  'circle_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        gfci_result: SearchResult = run_gfci_raw(
            data=data, col_names=col_names,
            alpha=alpha, penalty_discount=penalty_discount,
            depth=depth, max_degree=max_degree,
            complete_rule_set=complete_rule_set,
            max_disc_path_length=max_disc_path_length,
            faithfulness_assumed=faithfulness_assumed,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(gfci_result.edges),
            "nodes": list(gfci_result.nodes),
            "num_edges": gfci_result.num_edges,
            "num_nodes": gfci_result.num_nodes,
            "alpha": alpha,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(gfci_result.edges, gfci_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_boss
    # ----------------------------------------------------------------

    def run_boss(
        self,
        df: pd.DataFrame,
        penalty_discount: float = 1.0,
        use_bes: bool = False,
        num_starts: int = 1,
        use_data_order: bool = True,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the BOSS (Best Order Score Search) algorithm.

        BOSS is a permutation-based algorithm that finds optimal variable
        orderings by iteratively moving variables to score-maximizing
        positions using GrowShrink trees for efficient caching.

        Characterized by high adjacency and orientation precision, especially
        for moderate sample sizes. Faster than GES for large sparse graphs.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        penalty_discount : float
            BIC penalty multiplier. 1.0 = standard BIC.
        use_bes : bool
            Run Backward Equivalence Search refinement. Needed for
            correctness under faithfulness, but has little effect on
            large models. Default False.
        num_starts : int
            Number of random restarts. Best-scoring result is returned.
        use_data_order : bool
            Use data column order for the first run. Default True.
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        boss_result: SearchResult = run_boss_raw(
            data=data, col_names=col_names,
            penalty_discount=penalty_discount,
            use_bes=use_bes, num_starts=num_starts,
            use_data_order=use_data_order,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(boss_result.edges),
            "nodes": list(boss_result.nodes),
            "num_edges": boss_result.num_edges,
            "num_nodes": boss_result.num_nodes,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(boss_result.edges, boss_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_boss_fci
    # ----------------------------------------------------------------

    def run_boss_fci(
        self,
        df: pd.DataFrame,
        alpha: float = 0.05,
        penalty_discount: float = 1.0,
        depth: int = -1,
        complete_rule_set: bool = True,
        max_disc_path_length: int = -1,
        use_bes: bool = False,
        num_starts: int = 1,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the BOSS-FCI algorithm.

        BOSS-FCI combines the permutation-based BOSS algorithm with FCI
        orientation rules to handle latent (unmeasured) confounders.
        Returns a PAG (Partial Ancestral Graph).

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        alpha : float
            Significance level for conditional independence tests.
        penalty_discount : float
            BIC penalty multiplier for the BOSS scoring phase.
        depth : int
            Maximum conditioning set size. -1 for unlimited.
        complete_rule_set : bool
            Use Zhang's complete rules R1-R10 (default True).
        max_disc_path_length : int
            Maximum discriminating path length for R4. -1 for unlimited.
        use_bes : bool
            Run BES refinement in BOSS (default False).
        num_starts : int
            Number of random restarts for BOSS (default 1).
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'alpha', 'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges',
                  'bidirected_edges', 'partially_oriented_edges',
                  'circle_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        bfci_result: SearchResult = run_boss_fci_raw(
            data=data, col_names=col_names,
            alpha=alpha, penalty_discount=penalty_discount,
            depth=depth, complete_rule_set=complete_rule_set,
            max_disc_path_length=max_disc_path_length,
            use_bes=use_bes, num_starts=num_starts,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(bfci_result.edges),
            "nodes": list(bfci_result.nodes),
            "num_edges": bfci_result.num_edges,
            "num_nodes": bfci_result.num_nodes,
            "alpha": alpha,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(bfci_result.edges, bfci_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_grasp
    # ----------------------------------------------------------------

    def run_grasp(
        self,
        df: pd.DataFrame,
        penalty_discount: float = 1.0,
        depth: int = 3,
        uncovered_depth: int = 1,
        non_singular_depth: int = 1,
        ordered: bool = False,
        num_starts: int = 1,
        use_data_order: bool = True,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the GRaSP (Greedy Relaxations of SP) algorithm.

        GRaSP searches permutation space using depth-first tuck moves
        with backtracking to find optimal variable orderings. Very high
        adjacency and orientation precision for linear Gaussian data.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        penalty_discount : float
            BIC penalty multiplier. 1.0 = standard BIC.
        depth : int
            Max DFS depth for singular tucks (default 3).
        uncovered_depth : int
            Max depth for uncovered tucks (default 1).
        non_singular_depth : int
            Max depth for non-singular tucks (default 1).
        ordered : bool
            Enforce GRaSP0/1/2 ordering (default False).
        num_starts : int
            Number of random restarts. Best result is returned.
        use_data_order : bool
            Use data column order for the first run (default True).
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        grasp_result: SearchResult = run_grasp_raw(
            data=data, col_names=col_names,
            penalty_discount=penalty_discount,
            depth=depth, uncovered_depth=uncovered_depth,
            non_singular_depth=non_singular_depth,
            ordered=ordered, num_starts=num_starts,
            use_data_order=use_data_order,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(grasp_result.edges),
            "nodes": list(grasp_result.nodes),
            "num_edges": grasp_result.num_edges,
            "num_nodes": grasp_result.num_nodes,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(grasp_result.edges, grasp_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_grasp_fci
    # ----------------------------------------------------------------

    def run_grasp_fci(
        self,
        df: pd.DataFrame,
        alpha: float = 0.05,
        penalty_discount: float = 1.0,
        depth: int = -1,
        grasp_depth: int = 3,
        uncovered_depth: int = 1,
        non_singular_depth: int = 1,
        ordered: bool = False,
        complete_rule_set: bool = True,
        max_disc_path_length: int = -1,
        num_starts: int = 1,
        use_data_order: bool = True,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the GRaSP-FCI algorithm.

        GRaSP-FCI combines the GRaSP algorithm with FCI orientation rules
        to handle latent (unmeasured) confounders. Returns a PAG.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        alpha : float
            Significance level for conditional independence tests.
        penalty_discount : float
            BIC penalty multiplier for the GRaSP scoring phase.
        depth : int
            Maximum conditioning set size for FCI. -1 for unlimited.
        grasp_depth : int
            Max DFS depth for GRaSP tucks (default 3).
        uncovered_depth : int
            Max depth for uncovered tucks (default 1).
        non_singular_depth : int
            Max depth for non-singular tucks (default 1).
        ordered : bool
            Enforce GRaSP0/1/2 ordering (default False).
        complete_rule_set : bool
            Use Zhang's complete rules R1-R10 (default True).
        max_disc_path_length : int
            Maximum discriminating path length for R4. -1 unlimited.
        num_starts : int
            Number of random restarts for GRaSP (default 1).
        use_data_order : bool
            Use data column order for the first run (default True).
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'alpha', 'penalty_discount'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges',
                  'bidirected_edges', 'partially_oriented_edges',
                  'circle_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        gfci_result: SearchResult = run_grasp_fci_raw(
            data=data, col_names=col_names,
            alpha=alpha, penalty_discount=penalty_discount,
            depth=depth, grasp_depth=grasp_depth,
            uncovered_depth=uncovered_depth,
            non_singular_depth=non_singular_depth,
            ordered=ordered,
            complete_rule_set=complete_rule_set,
            max_disc_path_length=max_disc_path_length,
            num_starts=num_starts,
            use_data_order=use_data_order,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(gfci_result.edges),
            "nodes": list(gfci_result.nodes),
            "num_edges": gfci_result.num_edges,
            "num_nodes": gfci_result.num_nodes,
            "alpha": alpha,
            "penalty_discount": penalty_discount,
        }

        graph_info = self._parse_edges_to_graph_info(gfci_result.edges, gfci_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # Core: run_fci (composable FCI pipeline)
    # ----------------------------------------------------------------

    def run_fci(
        self,
        df: pd.DataFrame,
        initial_algorithm: str = "fges",
        alpha: float = 0.05,
        penalty_discount: float = 1.0,
        depth: int = -1,
        complete_rule_set: bool = True,
        max_disc_path_length: int = -1,
        knowledge: Union[Knowledge, dict, None] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run any CPDAG algorithm + FCI orientation rules to produce a PAG.

        This is a composable FCI pipeline: first run an initial algorithm
        to produce a CPDAG (skeleton with orientations), then apply the
        *-FCI pipeline (extra edge removal, collider orientation, and FCI
        rules R1-R10) to handle latent confounders.

        Use ``initial_algorithm="pc"`` for classic constraint-based FCI.
        Use ``initial_algorithm="fges"`` for a GFCI-like hybrid approach.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        initial_algorithm : str
            CPDAG algorithm to run first. One of "pc", "fges", "boss",
            "grasp". Default is "fges".
        alpha : float
            Significance level for conditional independence tests
            (used in both the initial algorithm and FCI rules).
        penalty_discount : float
            BIC penalty multiplier (for FGES, BOSS, GRaSP initial algos).
        depth : int
            Maximum conditioning set size for FCI. -1 for unlimited.
        complete_rule_set : bool
            Use Zhang's complete rules R1-R10 (default True).
        max_disc_path_length : int
            Maximum discriminating path length for R4. -1 for unlimited.
        knowledge : Knowledge, dict, or None
            Background knowledge. Can be a C++ Knowledge object or a dict
            with keys 'addtemporal', 'forbiddirect', 'requiredirect'.
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results : dict
            Keys: 'edges', 'nodes', 'num_edges', 'num_nodes',
                  'alpha', 'penalty_discount', 'initial_algorithm'
        graph_info : dict
            Keys: 'adjacency', 'directed_edges', 'undirected_edges',
                  'bidirected_edges', 'partially_oriented_edges',
                  'circle_edges'
        """
        v = verbose if verbose is not None else self.verbose
        data, col_names = self._validate_and_extract(df)
        k = self._resolve_knowledge(knowledge)

        algo = initial_algorithm.lower()
        if algo not in FCI_INITIAL_ALGORITHMS:
            raise ValueError(
                f"Unknown initial algorithm: {initial_algorithm!r}. "
                f"Must be one of {FCI_INITIAL_ALGORITHMS}."
            )

        fci_result: SearchResult = run_fci_raw(
            data=data, col_names=col_names,
            initial_algorithm=algo,
            alpha=alpha, penalty_discount=penalty_discount,
            depth=depth, complete_rule_set=complete_rule_set,
            max_disc_path_length=max_disc_path_length,
            verbose=v, knowledge=k,
        )

        results = {
            "edges": list(fci_result.edges),
            "nodes": list(fci_result.nodes),
            "num_edges": fci_result.num_edges,
            "num_nodes": fci_result.num_nodes,
            "alpha": alpha,
            "penalty_discount": penalty_discount,
            "initial_algorithm": algo,
        }

        graph_info = self._parse_edges_to_graph_info(fci_result.edges, fci_result.nodes)
        return results, graph_info

    # ----------------------------------------------------------------
    # SEM fitting (matching FastCDA pattern)
    # ----------------------------------------------------------------

    @staticmethod
    def edges_to_lavaan(edges: list[str]) -> str:
        """
        Convert a list of Tetrad edge strings to a lavaan/semopy model string.

        Only directed edges ('-->') are converted to regression paths.
        Undirected ('---') and bidirected ('<->') edges are noted as
        covariance terms.

        Parameters
        ----------
        edges : list of str
            Edge strings like 'X --> Y', 'A --- B', 'C <-> D'

        Returns
        -------
        str
            A lavaan-style model string compatible with semopy.
        """
        regressions: dict[str, list[str]] = {}
        covariances: list[tuple[str, str]] = []

        for edge_str in edges:
            parts = edge_str.strip().split()
            if len(parts) != 3:
                continue

            node1, edge_type, node2 = parts

            if edge_type == "-->":
                regressions.setdefault(node2, []).append(node1)
            elif edge_type == "<--":
                regressions.setdefault(node1, []).append(node2)
            elif edge_type in ("---", "<->"):
                covariances.append((node1, node2))

        lines = []
        for child in sorted(regressions.keys()):
            parents = sorted(regressions[child])
            lines.append(f"{child} ~ {' + '.join(parents)}")

        for n1, n2 in sorted(covariances):
            lines.append(f"{n1} ~~ {n2}")

        return "\n".join(lines)

    @staticmethod
    def run_semopy(
        lavaan_model: str,
        df: pd.DataFrame,
    ) -> dict[str, Any]:
        """
        Fit a SEM model using semopy and return results.

        Parameters
        ----------
        lavaan_model : str
            Model in lavaan syntax (as produced by edges_to_lavaan).
        df : pd.DataFrame
            Data to fit the model on.

        Returns
        -------
        dict with keys:
            'estimates': pd.DataFrame of parameter estimates
            'fit_stats': model fit statistics
            'model': the fitted semopy Model object
        """
        try:
            import semopy
        except ImportError:
            raise ImportError(
                "semopy is required for SEM fitting. "
                "Install it with: pip install tetrad-port[sem]"
            )

        model = semopy.Model(lavaan_model)
        model.fit(df)

        estimates = model.inspect()
        try:
            stats = semopy.calc_stats(model)
            fit_stats = stats.to_dict() if hasattr(stats, "to_dict") else stats
        except Exception:
            fit_stats = None

        return {
            "estimates": estimates,
            "fit_stats": fit_stats,
            "model": model,
        }

    # ----------------------------------------------------------------
    # Data preparation helpers (matching FastCDA pattern)
    # ----------------------------------------------------------------

    @staticmethod
    def add_lag_columns(
        df: pd.DataFrame,
        columns: Optional[list[str]] = None,
        n_lags: int = 1,
        lag_stub: str = "_lag",
    ) -> pd.DataFrame:
        """
        Add lagged columns to a DataFrame for time-series causal analysis.

        Parameters
        ----------
        df : pd.DataFrame
            Time-series data (rows are time points).
        columns : list of str or None
            Columns to lag. If None, lag all numeric columns.
        n_lags : int
            Number of lags to add (default 1).
        lag_stub : str
            Suffix appended to lagged column names.

        Returns
        -------
        pd.DataFrame
            DataFrame with lagged columns appended, NaN rows dropped.
        """
        result = df.copy()
        cols = (
            columns
            if columns is not None
            else list(df.select_dtypes(include=[np.number]).columns)
        )

        for lag in range(1, n_lags + 1):
            suffix = lag_stub if n_lags == 1 else f"{lag_stub}{lag}"
            for col in cols:
                result[f"{col}{suffix}"] = df[col].shift(lag)

        return result.dropna().reset_index(drop=True)

    @staticmethod
    def standardize_df_cols(
        df: pd.DataFrame,
        columns: Optional[list[str]] = None,
    ) -> pd.DataFrame:
        """
        Standardize columns to zero mean and unit variance.

        Parameters
        ----------
        df : pd.DataFrame
            Input data.
        columns : list of str or None
            Columns to standardize. If None, standardize all numeric columns.

        Returns
        -------
        pd.DataFrame
            Copy with specified columns standardized.
        """
        result = df.copy()
        cols = (
            columns
            if columns is not None
            else list(df.select_dtypes(include=[np.number]).columns)
        )

        for col in cols:
            mean = result[col].mean()
            std = result[col].std()
            if std > 0:
                result[col] = (result[col] - mean) / std
            else:
                result[col] = 0.0

        return result

    @staticmethod
    def create_lag_knowledge(
        columns: list[str],
        lag_stub: str = "_lag",
        as_dict: bool = False,
    ) -> Union[Knowledge, dict]:
        """
        Create temporal knowledge for lagged data.

        Lag variables (tier 0) can only be parents of current-day
        variables (tier 1), never the other way around.

        Parameters
        ----------
        columns : list of str
            Original (non-lagged) column names.
        lag_stub : str
            The suffix used for lagged columns.
        as_dict : bool
            If True, return a plain dict instead of a Knowledge object.

        Returns
        -------
        Knowledge or dict
            Knowledge object (default) or dict with 'addtemporal' key.
        """
        lag_cols = [f"{c}{lag_stub}" for c in columns]
        d = {"addtemporal": {0: lag_cols, 1: list(columns)}}
        if as_dict:
            return d
        return dict_to_knowledge(d)

    # ----------------------------------------------------------------
    # Internal helpers
    # ----------------------------------------------------------------

    @staticmethod
    def _resolve_knowledge(
        knowledge: Union[Knowledge, dict, None],
    ) -> Knowledge:
        """Convert knowledge arg to a C++ Knowledge object."""
        if knowledge is None:
            return Knowledge()
        if isinstance(knowledge, dict):
            result = dict_to_knowledge(knowledge)
            return result if result is not None else Knowledge()
        return knowledge

    @staticmethod
    def _validate_and_extract(df: pd.DataFrame) -> tuple[np.ndarray, list[str]]:
        """Validate DataFrame and extract numpy array + column names."""
        if not isinstance(df, pd.DataFrame):
            raise TypeError(f"Expected pd.DataFrame, got {type(df).__name__}")

        numeric_df = df.select_dtypes(include=[np.number])
        if numeric_df.shape[1] != df.shape[1]:
            non_numeric = set(df.columns) - set(numeric_df.columns)
            raise ValueError(
                f"All columns must be numeric. Non-numeric columns: {non_numeric}"
            )

        data = df.values.astype(np.float64, copy=False)
        col_names = list(df.columns.astype(str))
        return data, col_names

    @staticmethod
    def _parse_edges_to_graph_info(
        edges: list[str], nodes: list[str]
    ) -> dict[str, Any]:
        """Parse edge strings into structured graph info."""
        adjacency: dict[str, list[str]] = {n: [] for n in nodes}
        directed_edges: list[tuple[str, str]] = []
        undirected_edges: list[tuple[str, str]] = []
        bidirected_edges: list[tuple[str, str]] = []
        partially_oriented_edges: list[tuple[str, str]] = []
        circle_edges: list[tuple[str, str]] = []

        for edge_str in edges:
            parts = edge_str.strip().split()
            if len(parts) != 3:
                continue

            node1, edge_type, node2 = parts

            if node1 in adjacency:
                adjacency[node1].append(node2)
            if node2 in adjacency:
                adjacency[node2].append(node1)

            if edge_type == "-->":
                directed_edges.append((node1, node2))
            elif edge_type == "<--":
                directed_edges.append((node2, node1))
            elif edge_type == "---":
                undirected_edges.append((node1, node2))
            elif edge_type == "<->":
                bidirected_edges.append((node1, node2))
            elif edge_type == "o->":
                partially_oriented_edges.append((node1, node2))
            elif edge_type == "o-o":
                circle_edges.append((node1, node2))

        return {
            "adjacency": adjacency,
            "directed_edges": directed_edges,
            "undirected_edges": undirected_edges,
            "bidirected_edges": bidirected_edges,
            "partially_oriented_edges": partially_oriented_edges,
            "circle_edges": circle_edges,
        }
