"""
Command line entry point.

Submits a batch of inference jobs to the C++ orchestrator and reports
which backend each one landed on. If an ONNX model path is given and
onnxruntime is installed, it also runs a real inference pass so the
"cost" numbers aren't just made up - otherwise it falls back to a
fixed simulated cost.
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "bindings"))
from orchestrator_wrapper import Orchestrator, BACKEND_AUTO  # noqa: E402


def measure_onnx_cost_ms(model_path):
    """Runs the model once with onnxruntime and returns latency in ms.

    Falls back to None if onnxruntime isn't installed or the model
    can't be loaded, so the CLI still works without a real model.
    """
    try:
        import numpy as np
        import onnxruntime as ort
    except ImportError:
        return None

    if not model_path or not os.path.exists(model_path):
        return None

    try:
        session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        input_meta = session.get_inputs()[0]
        shape = [d if isinstance(d, int) else 1 for d in input_meta.shape]
        dummy_input = np.random.rand(*shape).astype(np.float32)

        start = time.perf_counter()
        session.run(None, {input_meta.name: dummy_input})
        elapsed_ms = (time.perf_counter() - start) * 1000
        return elapsed_ms
    except Exception as exc:
        print(f"  (couldn't run real inference on {model_path}: {exc})")
        return None


def main():
    parser = argparse.ArgumentParser(description="Submit jobs to the ML orchestrator")
    parser.add_argument("--model", help="Path to an .onnx model (optional)")
    parser.add_argument("--jobs", type=int, default=8, help="Number of jobs to submit")
    parser.add_argument("--workers", type=int, default=0,
                         help="Worker threads (0 = auto based on CPU count)")
    parser.add_argument("--cost-ms", type=int, default=50,
                         help="Simulated cost per job if no model is given")
    args = parser.parse_args()

    real_cost = measure_onnx_cost_ms(args.model)
    cost_ms = int(real_cost) if real_cost is not None else args.cost_ms
    source = "measured from real ONNX Runtime run" if real_cost is not None else "simulated"
    print(f"Job cost: {cost_ms} ms ({source})")

    orch = Orchestrator(num_workers=args.workers)
    job_ids = []
    start = time.perf_counter()

    for i in range(args.jobs):
        job_ids.append(orch.submit(f"job_{i}", simulated_cost_ms=cost_ms,
                                    preferred_backend=BACKEND_AUTO))

    results = {}
    for jid in job_ids:
        backend = orch.wait(jid)
        results[backend] = results.get(backend, 0) + 1

    total_time = time.perf_counter() - start
    print(f"\nSubmitted {args.jobs} jobs, completed in {total_time:.3f}s")
    for backend, count in sorted(results.items()):
        print(f"  {backend}: {count} job(s)")

    orch.close()


if __name__ == "__main__":
    main()
