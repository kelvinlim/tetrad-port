"""Tests for the Python tetrad_port package."""
import numpy as np
import pandas as pd
import pytest

from tetrad_port import TetradPort


@pytest.fixture
def tp():
    return TetradPort()


@pytest.fixture
def chain_data():
    """X -> Y -> Z chain data."""
    np.random.seed(42)
    n = 5000
    X = np.random.randn(n)
    Y = 0.6 * X + 0.5 * np.random.randn(n)
    Z = 0.6 * Y + 0.5 * np.random.randn(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


@pytest.fixture
def collider_data():
    """X -> Z <- Y collider data."""
    np.random.seed(123)
    n = 2000
    X = np.random.randn(n)
    Y = np.random.randn(n)
    Z = 0.8 * X + 0.8 * Y + 0.3 * np.random.randn(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


class TestRunPc:
    def test_chain_structure(self, tp, chain_data):
        results, graph_info = tp.run_pc(chain_data, alpha=0.05)
        assert results["num_edges"] == 2
        assert set(results["nodes"]) == {"X", "Y", "Z"}
        assert len(results["edges"]) == 2

    def test_collider_detection(self, tp, collider_data):
        results, graph_info = tp.run_pc(collider_data, alpha=0.05)
        assert results["num_edges"] == 2
        directed = graph_info["directed_edges"]
        assert ("X", "Z") in directed
        assert ("Y", "Z") in directed

    def test_independent_variables(self, tp):
        np.random.seed(77)
        df = pd.DataFrame({
            "A": np.random.randn(500),
            "B": np.random.randn(500),
            "C": np.random.randn(500),
        })
        results, graph_info = tp.run_pc(df)
        assert results["num_edges"] == 0

    def test_returns_dict_structure(self, tp, chain_data):
        results, graph_info = tp.run_pc(chain_data)
        assert "edges" in results
        assert "nodes" in results
        assert "num_edges" in results
        assert "alpha" in results
        assert "adjacency" in graph_info
        assert "directed_edges" in graph_info
        assert "undirected_edges" in graph_info

    def test_non_numeric_raises(self, tp):
        df = pd.DataFrame({"A": [1, 2, 3], "B": ["x", "y", "z"]})
        with pytest.raises(ValueError, match="(?i)non-numeric"):
            tp.run_pc(df)

    def test_non_dataframe_raises(self, tp):
        with pytest.raises(TypeError):
            tp.run_pc(np.array([[1, 2], [3, 4]]))


class TestEdgesToLavaan:
    def test_directed_edges(self):
        edges = ["X --> Y", "X --> Z", "Y --> Z"]
        model = TetradPort.edges_to_lavaan(edges)
        assert "Y ~ X" in model
        assert "Z ~ X + Y" in model or "Z ~ Y + X" in model

    def test_undirected_edges(self):
        edges = ["X --- Y"]
        model = TetradPort.edges_to_lavaan(edges)
        assert "~~" in model

    def test_empty_edges(self):
        assert TetradPort.edges_to_lavaan([]) == ""

    def test_reverse_directed(self):
        edges = ["Y <-- X"]
        model = TetradPort.edges_to_lavaan(edges)
        assert "Y ~ X" in model


class TestDataHelpers:
    def test_standardize(self):
        df = pd.DataFrame({"A": [1.0, 2.0, 3.0, 4.0, 5.0], "B": [10.0, 20.0, 30.0, 40.0, 50.0]})
        result = TetradPort.standardize_df_cols(df)
        assert abs(result["A"].mean()) < 1e-10
        assert abs(result["A"].std() - 1.0) < 0.1

    def test_add_lag(self):
        df = pd.DataFrame({"A": range(10), "B": range(10, 20)})
        result = TetradPort.add_lag_columns(df, n_lags=1)
        assert "A_lag" in result.columns
        assert "B_lag" in result.columns
        assert len(result) == 9  # 10 - 1 NaN row dropped

    def test_add_lag_multiple(self):
        df = pd.DataFrame({"A": range(10), "B": range(10, 20)})
        result = TetradPort.add_lag_columns(df, n_lags=2)
        assert "A_lag1" in result.columns
        assert "A_lag2" in result.columns
        assert len(result) == 8  # 10 - 2 NaN rows dropped

    def test_create_lag_knowledge(self):
        knowledge = TetradPort.create_lag_knowledge(["X", "Y", "Z"])
        assert "addtemporal" in knowledge
        assert 0 in knowledge["addtemporal"]
        assert 1 in knowledge["addtemporal"]
        assert "X_lag" in knowledge["addtemporal"][0]
        assert "X" in knowledge["addtemporal"][1]
