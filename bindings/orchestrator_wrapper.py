"""
ctypes wrapper around the C++ orchestrator core.

Loads the shared library built from core/orchestrator.cpp and exposes
a small Python class so the scheduling logic doesn't have to be
reimplemented on the Python side.
"""

import ctypes
import os
import platform

BACKEND_CPU = 0
BACKEND_GPU = 1
BACKEND_NPU = 2
BACKEND_AUTO = 3

_BACKEND_NAMES = {
    BACKEND_CPU: "CPU",
    BACKEND_GPU: "GPU",
    BACKEND_NPU: "NPU",
}


def _find_library():
    here = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(here, "..", "build")
    system = platform.system()
    if system == "Windows":
        name = "orchestrator.dll"
    elif system == "Darwin":
        name = "liborchestrator.dylib"
    else:
        name = "liborchestrator.so"

    path = os.path.join(build_dir, name)
    if not os.path.exists(path):
        raise FileNotFoundError(
            f"Couldn't find {name} in {build_dir}. "
            "Build the core library first (see README, `make` on Linux "
            "or the Visual Studio project on Windows)."
        )
    return path


class Orchestrator:
    """Thin Python wrapper around the C++ orchestrator."""

    def __init__(self, num_workers=0, lib_path=None):
        self._lib = ctypes.CDLL(lib_path or _find_library())
        self._configure_signatures()
        self._handle = self._lib.orchestrator_create(ctypes.c_int(num_workers))
        if not self._handle:
            raise RuntimeError("orchestrator_create returned a null handle")

    def _configure_signatures(self):
        lib = self._lib
        lib.orchestrator_create.restype = ctypes.c_void_p
        lib.orchestrator_create.argtypes = [ctypes.c_int]

        lib.orchestrator_submit.restype = ctypes.c_int64
        lib.orchestrator_submit.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int
        ]

        lib.orchestrator_wait.restype = ctypes.c_int
        lib.orchestrator_wait.argtypes = [ctypes.c_void_p, ctypes.c_int64]

        lib.orchestrator_pending_count.restype = ctypes.c_int
        lib.orchestrator_pending_count.argtypes = [ctypes.c_void_p]

        lib.orchestrator_completed_count.restype = ctypes.c_int64
        lib.orchestrator_completed_count.argtypes = [ctypes.c_void_p]

        lib.orchestrator_destroy.argtypes = [ctypes.c_void_p]

    def submit(self, job_name, simulated_cost_ms=50, preferred_backend=BACKEND_AUTO):
        job_name_bytes = job_name.encode("utf-8")
        return self._lib.orchestrator_submit(
            self._handle, job_name_bytes, simulated_cost_ms, preferred_backend
        )

    def wait(self, job_id):
        backend = self._lib.orchestrator_wait(self._handle, job_id)
        return _BACKEND_NAMES.get(backend, "UNKNOWN")

    def pending_count(self):
        return self._lib.orchestrator_pending_count(self._handle)

    def completed_count(self):
        return self._lib.orchestrator_completed_count(self._handle)

    def close(self):
        if getattr(self, "_handle", None):
            self._lib.orchestrator_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
