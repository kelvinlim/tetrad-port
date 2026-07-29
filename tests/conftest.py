"""Shared pytest configuration.

Registers the ``slow`` marker used by ``test_simulation.py`` for the larger
graph sizes, and skips those tests unless ``--runslow`` is given, so a normal
test run stays fast.
"""

import pytest


def pytest_addoption(parser):
    parser.addoption("--runslow", action="store_true", default=False,
                     help="also run tests marked slow (large simulated graphs)")


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: large simulations; needs --runslow")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--runslow"):
        return
    skip = pytest.mark.skip(reason="needs --runslow")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip)
