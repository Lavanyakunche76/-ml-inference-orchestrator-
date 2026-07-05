"""
Simple sanity checks for the Python wrapper. Run with:
    python3 tests/test_python.py

Not using pytest on purpose - kept dependency-free so it runs
anywhere the shared library is built.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bindings"))
from orchestrator_wrapper import Orchestrator, BACKEND_GPU  # noqa: E402


def test_submit_and_wait():
    with Orchestrator(num_workers=2) as orch:
        job_id = orch.submit("smoke_test", simulated_cost_ms=5)
        backend = orch.wait(job_id)
        assert backend in ("CPU", "GPU", "NPU"), f"unexpected backend: {backend}"
    print("test_submit_and_wait passed")


def test_backend_preference():
    with Orchestrator(num_workers=2) as orch:
        job_id = orch.submit("gpu_job", simulated_cost_ms=5, preferred_backend=BACKEND_GPU)
        backend = orch.wait(job_id)
        assert backend == "GPU"
    print("test_backend_preference passed")


def test_completed_count():
    with Orchestrator(num_workers=2) as orch:
        ids = [orch.submit(f"job_{i}", simulated_cost_ms=5) for i in range(5)]
        for jid in ids:
            orch.wait(jid)
        assert orch.completed_count() == 5
    print("test_completed_count passed")


if __name__ == "__main__":
    test_submit_and_wait()
    test_backend_preference()
    test_completed_count()
    print("All Python tests passed.")
