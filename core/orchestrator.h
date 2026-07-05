#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <cstdint>

// C API so this can be loaded from Python via ctypes, or from any
// other language that can call into a shared library.
extern "C" {

// Which backend a job should be routed to. NPU is a stub for now
// since I don't have Snapdragon hardware to test against - it just
// falls back to CPU internally, but the API is here so the routing
// logic can be swapped out later without changing callers.
enum Backend {
    BACKEND_CPU = 0,
    BACKEND_GPU = 1,
    BACKEND_NPU = 2,
    BACKEND_AUTO = 3
};

// Opaque handle to an orchestrator instance.
typedef void* OrchestratorHandle;

// Creates an orchestrator with a fixed-size worker thread pool.
// num_workers <= 0 falls back to std::thread::hardware_concurrency().
OrchestratorHandle orchestrator_create(int num_workers);

// Submits a job (identified by a name + a simulated cost in ms) and
// returns a job id. preferred_backend is a hint, not a guarantee -
// AUTO lets the scheduler pick based on current queue depth.
int64_t orchestrator_submit(OrchestratorHandle handle,
                             const char* job_name,
                             int simulated_cost_ms,
                             int preferred_backend);

// Blocks until the given job id has finished. Returns the backend
// it actually ran on, or -1 if the job id is unknown.
int orchestrator_wait(OrchestratorHandle handle, int64_t job_id);

// Returns how many jobs are currently queued or running.
int orchestrator_pending_count(OrchestratorHandle handle);

// Returns total jobs completed since creation.
int64_t orchestrator_completed_count(OrchestratorHandle handle);

// Shuts down worker threads and frees the orchestrator.
void orchestrator_destroy(OrchestratorHandle handle);

}  // extern "C"

#endif  // ORCHESTRATOR_H
