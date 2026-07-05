#include "orchestrator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct Job {
    int64_t id;
    std::string name;
    int cost_ms;
    int preferred_backend;
    int actual_backend = -1;
    bool done = false;
};

// Lower queue depth per backend = "cheaper" to route here right now.
// This is a simple load-based router: BACKEND_AUTO looks at how many
// jobs are currently assigned to each backend and picks the least
// loaded one. It's intentionally simple - a real orchestrator would
// factor in thermal state, power budget, and per-backend throughput,
// but this is enough to demonstrate the routing decision point.
struct BackendLoad {
    std::atomic<int> cpu_jobs{0};
    std::atomic<int> gpu_jobs{0};
    std::atomic<int> npu_jobs{0};
};

}  // namespace

class Orchestrator {
public:
    explicit Orchestrator(int num_workers) : stop_(false), next_id_(1), completed_(0) {
        if (num_workers <= 0) {
            num_workers = static_cast<int>(std::thread::hardware_concurrency());
            if (num_workers <= 0) num_workers = 4;
        }
        for (int i = 0; i < num_workers; ++i) {
            workers_.emplace_back(&Orchestrator::WorkerLoop, this);
        }
    }

    ~Orchestrator() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    int64_t Submit(const std::string& name, int cost_ms, int preferred_backend) {
        auto job = std::make_shared<Job>();
        job->id = next_id_.fetch_add(1);
        job->name = name;
        job->cost_ms = cost_ms;
        job->preferred_backend = preferred_backend;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_[job->id] = job;
            queue_.push(job);
        }
        cv_.notify_one();
        return job->id;
    }

    int Wait(int64_t job_id) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) return -1;
        auto job = it->second;
        done_cv_.wait(lock, [&job] { return job->done; });
        return job->actual_backend;
    }

    int PendingCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        int pending = 0;
        for (auto& kv : jobs_) {
            if (!kv.second->done) pending++;
        }
        return pending;
    }

    int64_t CompletedCount() { return completed_.load(); }

private:
    int PickBackend(int preferred) {
        if (preferred != BACKEND_AUTO) return preferred;

        int cpu = load_.cpu_jobs.load();
        int gpu = load_.gpu_jobs.load();
        int npu = load_.npu_jobs.load();

        // Route to whichever backend has the fewest jobs assigned.
        // NPU currently just mirrors CPU behavior under the hood
        // (see note in orchestrator.h) but is tracked separately so
        // the load numbers are meaningful once real NPU dispatch
        // is wired in.
        if (cpu <= gpu && cpu <= npu) return BACKEND_CPU;
        if (gpu <= cpu && gpu <= npu) return BACKEND_GPU;
        return BACKEND_NPU;
    }

    void BumpLoad(int backend, int delta) {
        switch (backend) {
            case BACKEND_CPU: load_.cpu_jobs.fetch_add(delta); break;
            case BACKEND_GPU: load_.gpu_jobs.fetch_add(delta); break;
            case BACKEND_NPU: load_.npu_jobs.fetch_add(delta); break;
            default: break;
        }
    }

    void WorkerLoop() {
        while (true) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                job = queue_.front();
                queue_.pop();
            }

            int backend = PickBackend(job->preferred_backend);
            BumpLoad(backend, 1);

            // Simulated inference cost. In a real build this is where
            // the ONNX Runtime / QNN session Run() call would go.
            std::this_thread::sleep_for(std::chrono::milliseconds(job->cost_ms));

            BumpLoad(backend, -1);

            {
                std::lock_guard<std::mutex> lock(mutex_);
                job->actual_backend = backend;
                job->done = true;
                completed_.fetch_add(1);
            }
            done_cv_.notify_all();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::shared_ptr<Job>> queue_;
    std::unordered_map<int64_t, std::shared_ptr<Job>> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    bool stop_;
    std::atomic<int64_t> next_id_;
    std::atomic<int64_t> completed_;
    BackendLoad load_;
};

extern "C" {

OrchestratorHandle orchestrator_create(int num_workers) {
    return new Orchestrator(num_workers);
}

int64_t orchestrator_submit(OrchestratorHandle handle, const char* job_name,
                             int simulated_cost_ms, int preferred_backend) {
    auto* orch = static_cast<Orchestrator*>(handle);
    return orch->Submit(job_name ? job_name : "unnamed_job", simulated_cost_ms, preferred_backend);
}

int orchestrator_wait(OrchestratorHandle handle, int64_t job_id) {
    auto* orch = static_cast<Orchestrator*>(handle);
    return orch->Wait(job_id);
}

int orchestrator_pending_count(OrchestratorHandle handle) {
    auto* orch = static_cast<Orchestrator*>(handle);
    return orch->PendingCount();
}

int64_t orchestrator_completed_count(OrchestratorHandle handle) {
    auto* orch = static_cast<Orchestrator*>(handle);
    return orch->CompletedCount();
}

void orchestrator_destroy(OrchestratorHandle handle) {
    delete static_cast<Orchestrator*>(handle);
}

}  // extern "C"
