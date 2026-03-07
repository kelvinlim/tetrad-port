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


@pytest.fixture
def latent_common_cause_data():
    """L -> X, L -> Y, X -> Z (L unobserved)."""
    np.random.seed(99)
    n = 3000
    L = np.random.randn(n)
    X = 0.8 * L + np.random.randn(n)
    Y = 0.7 * L + np.random.randn(n)
    Z = 0.6 * X + np.random.randn(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


# ----------------------------------------------------------------
# PC algorithm tests
# ----------------------------------------------------------------


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


# ----------------------------------------------------------------
# FGES algorithm tests
# ----------------------------------------------------------------


class TestRunFges:
    def test_chain_structure(self, tp, chain_data):
        results, graph_info = tp.run_fges(chain_data)
        assert results["num_edges"] == 2
        assert set(results["nodes"]) == {"X", "Y", "Z"}

    def test_collider_detection(self, tp, collider_data):
        results, graph_info = tp.run_fges(collider_data)
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
        results, graph_info = tp.run_fges(df)
        assert results["num_edges"] == 0

    def test_model_score_returned(self, tp, chain_data):
        results, _ = tp.run_fges(chain_data)
        assert "model_score" in results
        assert np.isfinite(results["model_score"])

    def test_penalty_discount(self, tp, chain_data):
        r1, _ = tp.run_fges(chain_data, penalty_discount=1.0)
        r2, _ = tp.run_fges(chain_data, penalty_discount=4.0)
        # Higher penalty should produce same or fewer edges
        assert r2["num_edges"] <= r1["num_edges"]

    def test_returns_dict_structure(self, tp, chain_data):
        results, graph_info = tp.run_fges(chain_data)
        assert "edges" in results
        assert "nodes" in results
        assert "model_score" in results
        assert "penalty_discount" in results
        assert "adjacency" in graph_info

    def test_non_dataframe_raises(self, tp):
        with pytest.raises(TypeError):
            tp.run_fges(np.array([[1, 2], [3, 4]]))


# ----------------------------------------------------------------
# GFCI algorithm tests
# ----------------------------------------------------------------


class TestRunGfci:
    def test_chain_structure(self, tp, chain_data):
        results, graph_info = tp.run_gfci(chain_data, alpha=0.05)
        assert results["num_edges"] == 2
        assert set(results["nodes"]) == {"X", "Y", "Z"}

    def test_collider_detection(self, tp, collider_data):
        results, graph_info = tp.run_gfci(collider_data, alpha=0.05)
        assert results["num_edges"] == 2

    def test_independent_variables(self, tp):
        np.random.seed(77)
        df = pd.DataFrame({
            "A": np.random.randn(500),
            "B": np.random.randn(500),
            "C": np.random.randn(500),
        })
        results, graph_info = tp.run_gfci(df)
        assert results["num_edges"] == 0

    def test_latent_common_cause(self, tp, latent_common_cause_data):
        results, graph_info = tp.run_gfci(latent_common_cause_data, alpha=0.05)
        # Should find edges between X-Y, X-Z (and possibly Y-Z)
        assert results["num_edges"] >= 2
        # X-Y should NOT have a tail endpoint (no direct causal edge)
        # Check that the graph info contains the bidirected or circle edge types
        assert "bidirected_edges" in graph_info

    def test_pag_edge_types_in_graph_info(self, tp, chain_data):
        _, graph_info = tp.run_gfci(chain_data, alpha=0.05)
        # PAG graph_info should include all PAG-specific keys
        assert "directed_edges" in graph_info
        assert "bidirected_edges" in graph_info
        assert "partially_oriented_edges" in graph_info
        assert "circle_edges" in graph_info

    def test_returns_dict_structure(self, tp, chain_data):
        results, graph_info = tp.run_gfci(chain_data)
        assert "edges" in results
        assert "nodes" in results
        assert "alpha" in results
        assert "penalty_discount" in results
        assert "adjacency" in graph_info

    def test_non_dataframe_raises(self, tp):
        with pytest.raises(TypeError):
            tp.run_gfci(np.array([[1, 2], [3, 4]]))


# ----------------------------------------------------------------
# Shared utility tests
# ----------------------------------------------------------------


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

    def test_bidirected_edges(self):
        edges = ["X <-> Y"]
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
