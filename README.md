# ML Inference Orchestrator

A small C++ core that manages a worker thread pool and routes
inference jobs to a backend (CPU / GPU / NPU) based on current load.
Wrapped with a Python CLI so it can be called from scripts and
optionally driven by a real ONNX Runtime session.

## Why I built this

I wanted to actually understand what an "orchestration layer" does
between a model file and the hardware it runs on, instead of just
calling `session.run()` in Python and trusting a framework to sort it
out. This is a stripped-down version of that decision layer: given a
job, which backend should handle it right now, and how do you track
that without blocking the caller.

## What it does

- C++ orchestrator with a real thread pool (`std::thread`,
  `std::condition_variable`) - jobs get queued and picked up by
  whichever worker is free
- Load-aware backend routing: `BACKEND_AUTO` looks at how many jobs
  are currently assigned to CPU/GPU/NPU and picks the least loaded one
- Exposed as a C API (`extern "C"`) so it compiles to a shared library
  (`.so` on Linux, `.dll` on Windows) and can be called from any
  language with FFI support
- Python wrapper (`ctypes`, no extra bindings library needed) plus a
  CLI that submits a batch of jobs and reports where they landed
- If you pass `--model some.onnx`, it runs one real inference with
  ONNX Runtime first to get an actual latency number instead of a
  made-up one

## Project structure

```
ml-inference-orchestrator/
├── core/
│   ├── orchestrator.h        C API definitions
│   └── orchestrator.cpp      thread pool + scheduling logic
├── bindings/
│   └── orchestrator_wrapper.py   ctypes wrapper
├── tests/
│   ├── test_scheduler.cpp    C++ smoke tests
│   └── test_python.py        Python smoke tests
├── cli.py                    entry point
├── Makefile
├── requirements.txt
└── README.md
```

## Build (Linux / WSL)

Needs g++ with C++17 and pthread support (standard on most distros).

```bash
make            # builds build/liborchestrator.so
make test       # builds and runs the C++ tests
```

## Build (Windows)

No Visual Studio project file is checked in yet - easiest path right
now is building from a Developer Command Prompt with cl.exe, or using
MSYS2/MinGW with the same g++ command as above but outputting a
`.dll`:

```
g++ -std=c++17 -Wall -O2 -shared -o build/orchestrator.dll core/orchestrator.cpp
```

The Python wrapper already checks `platform.system()` and looks for
the right file name, so no code changes are needed on the Python side.

## Run it

```bash
pip install -r requirements.txt
python3 cli.py --jobs 10 --workers 4
```

With a real model:

```bash
python3 cli.py --model path/to/model.onnx --jobs 10
```

Example output:

```
Job cost: 20 ms (simulated)

Submitted 10 jobs, completed in 0.060s
  CPU: 4 job(s)
  GPU: 3 job(s)
  NPU: 3 job(s)
```

## Running the tests

```bash
make test                    # C++ side
python3 tests/test_python.py # Python side
```

Both are plain assert-based checks, no test framework dependency -
kept it that way so it's easy to run anywhere without setting up
pytest or gtest.

## Design notes / limitations

- NPU routing is tracked separately in the load counters but
  currently executes the same code path as CPU under the hood - I
  don't have Snapdragon hardware to test real NPU dispatch against,
  so this is a stub with the API shape already in place.
- The scheduler is a simple FIFO queue with load-based backend
  selection. There's no priority field yet, so a long job submitted
  first will hold up shorter ones behind it on the same backend.
- Job "cost" is either passed in manually or measured from a single
  ONNX Runtime run - it isn't an average over multiple runs, so
  treat the printed latency as a rough number, not a benchmark result.
- No retry logic if a job fails inside the worker thread - right now
  a crash in one job would take down the worker. Worth adding a
  try/catch boundary if this got extended.

## What I'd do next

- Add priority levels so short jobs aren't stuck behind long ones
- Wire in real QNN/SNPE execution providers once I have access to
  Snapdragon hardware, instead of the CPU fallback
- Add a CMakeLists.txt for a proper cross-platform build instead of
  the current Makefile + manual Windows command
