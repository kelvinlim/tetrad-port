"""
tetrad_port: Python bindings for the tetrad-port C++ PC algorithm.

Provides a TetradPort facade class following the FastCDA pattern:
    tp = TetradPort()
    results, graph_info = tp.run_pc(df, alpha=0.05)
"""

from __future__ import annotations

from typing import Any, Optional

import numpy as np
import pandas as pd

from tetrad_port._tetrad_cpp import run_pc_raw, PcResult

__version__ = "0.1.0"
__all__ = ["TetradPort"]


class TetradPort:
    """
    Facade class for running causal discovery via the tetrad-port C++ engine.

    Follows the FastCDA pattern: a single object that provides
    run_pc(), edges_to_lavaan(), run_semopy(), and data-prep helpers.

    Example
    -------
    >>> tp = TetradPort()
    >>> results, graph_info = tp.run_pc(df, alpha=0.05)
    >>> print(results['edges'])
    ['X --> Y', 'Y --> Z']
    """

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    # ----------------------------------------------------------------
    # Core: run_pc
    # ----------------------------------------------------------------

    def run_pc(
        self,
        df: pd.DataFrame,
        alpha: float = 0.05,
        depth: int = -1,
        knowledge: Optional[dict] = None,
        verbose: Optional[bool] = None,
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        """
        Run the PC algorithm on a pandas DataFrame.

        Parameters
        ----------
        df : pd.DataFrame
            Continuous data. All columns must be numeric.
        alpha : float
            Significance level for conditional independence tests.
        depth : int
            Maximum size of conditioning sets. -1 for no limit.
        knowledge : dict or None
            Background knowledge (reserved for future use).
        verbose : bool or None
            Override instance-level verbose setting.

        Returns
        -------
        results_dict : dict
            Keys: 'edges' (list of str), 'nodes' (list of str),
                  'num_edges' (int), 'num_nodes' (int),
                  'alpha' (float), 'depth' (int)
        graph_info : dict
            Keys: 'adjacency' (dict of node -> list of neighbors),
                  'directed_edges' (list of (from, to) tuples),
                  'undirected_edges' (list of (n1, n2) tuples)
        """
        if knowledge is not None:
            import warnings
            warnings.warn(
                "Background knowledge is not yet supported in C++ engine. "
                "The knowledge argument is ignored.",
                UserWarning,
                stacklevel=2,
            )

        v = verbose if verbose is not None else self.verbose

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

        pc_result: PcResult = run_pc_raw(
            data=data,
            col_names=col_names,
            alpha=alpha,
            depth=depth,
            verbose=v,
        )

        results_dict = {
            "edges": list(pc_result.edges),
            "nodes": list(pc_result.nodes),
            "num_edges": pc_result.num_edges,
            "num_nodes": pc_result.num_nodes,
            "alpha": alpha,
            "depth": depth,
        }

        graph_info = self._parse_edges_to_graph_info(pc_result.edges, pc_result.nodes)

        return results_dict, graph_info

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
    ) -> dict:
        """
        Create a temporal knowledge dict for lagged data.

        Parameters
        ----------
        columns : list of str
            Original (non-lagged) column names.
        lag_stub : str
            The suffix used for lagged columns.

        Returns
        -------
        dict
            Knowledge dict with 'addtemporal' key, compatible with
            the FastCDA knowledge format.
        """
        lag_cols = [f"{c}{lag_stub}" for c in columns]
        return {"addtemporal": {0: lag_cols, 1: list(columns)}}

    # ----------------------------------------------------------------
    # Internal helpers
    # ----------------------------------------------------------------

    @staticmethod
    def _parse_edges_to_graph_info(
        edges: list[str], nodes: list[str]
    ) -> dict[str, Any]:
        """Parse edge strings into structured graph info."""
        adjacency: dict[str, list[str]] = {n: [] for n in nodes}
        directed_edges: list[tuple[str, str]] = []
        undirected_edges: list[tuple[str, str]] = []

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

        return {
            "adjacency": adjacency,
            "directed_edges": directed_edges,
            "undirected_edges": undirected_edges,
        }
