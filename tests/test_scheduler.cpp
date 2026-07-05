// Basic smoke tests for the orchestrator core. Not using a test
// framework here on purpose - keeps the build simple with just g++.
// Run via `make test`.

#include <cassert>
#include <cstdio>
#include <vector>

#include "../core/orchestrator.h"

static void test_single_job() {
    OrchestratorHandle h = orchestrator_create(2);
    int64_t id = orchestrator_submit(h, "unit_test_job", 10, BACKEND_AUTO);
    assert(id > 0);
    int backend = orchestrator_wait(h, id);
    assert(backend == BACKEND_CPU || backend == BACKEND_GPU || backend == BACKEND_NPU);
    assert(orchestrator_completed_count(h) == 1);
    orchestrator_destroy(h);
    printf("test_single_job passed\n");
}

static void test_multiple_jobs_load_balance() {
    OrchestratorHandle h = orchestrator_create(4);
    std::vector<int64_t> ids;
    for (int i = 0; i < 12; ++i) {
        ids.push_back(orchestrator_submit(h, "job", 20, BACKEND_AUTO));
    }
    for (auto id : ids) {
        orchestrator_wait(h, id);
    }
    assert(orchestrator_completed_count(h) == 12);
    assert(orchestrator_pending_count(h) == 0);
    orchestrator_destroy(h);
    printf("test_multiple_jobs_load_balance passed\n");
}

static void test_unknown_job_id() {
    OrchestratorHandle h = orchestrator_create(1);
    int backend = orchestrator_wait(h, 9999);
    assert(backend == -1);
    orchestrator_destroy(h);
    printf("test_unknown_job_id passed\n");
}

static void test_explicit_backend_preference() {
    OrchestratorHandle h = orchestrator_create(2);
    int64_t id = orchestrator_submit(h, "gpu_job", 10, BACKEND_GPU);
    int backend = orchestrator_wait(h, id);
    assert(backend == BACKEND_GPU);
    orchestrator_destroy(h);
    printf("test_explicit_backend_preference passed\n");
}

int main() {
    test_single_job();
    test_multiple_jobs_load_balance();
    test_unknown_job_id();
    test_explicit_backend_preference();
    printf("All tests passed.\n");
    return 0;
}
