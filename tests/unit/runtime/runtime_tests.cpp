#include "quantedge/runtime/task_scheduler.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge::runtime;

    {
        TaskScheduler scheduler(2);
        auto first = scheduler.submit(TaskPriority::High, [] {
            return 21;
        });
        auto second = scheduler.submit(TaskPriority::Normal, [] {
            return 2;
        });

        if (first.get() * second.get() != 42) {
            fail("Scheduler futures returned the wrong values");
        }

        const RuntimeMetrics metrics = scheduler.metrics();
        if (metrics.p50LatencyMs < 0.0 || metrics.p95LatencyMs < 0.0 || metrics.p99LatencyMs < 0.0) {
            fail("Scheduler latency metrics were invalid");
        }
    }

    {
        TaskScheduler scheduler(1);
        std::mutex mutex;
        std::condition_variable condition;
        bool first_started = false;
        bool release_first = false;
        std::vector<std::string> order;

        auto first = scheduler.submit(TaskPriority::Critical, [&] {
            {
                std::lock_guard<std::mutex> lock(mutex);
                first_started = true;
                order.push_back("critical");
            }
            condition.notify_all();

            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&] {
                return release_first;
            });
        });

        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&] {
                return first_started;
            });
        }

        auto low = scheduler.submit(TaskPriority::Low, [&] {
            std::lock_guard<std::mutex> lock(mutex);
            order.push_back("low");
        });
        auto high = scheduler.submit(TaskPriority::High, [&] {
            std::lock_guard<std::mutex> lock(mutex);
            order.push_back("high");
        });

        {
            std::lock_guard<std::mutex> lock(mutex);
            release_first = true;
        }
        condition.notify_all();

        first.get();
        high.get();
        low.get();

        if (order.size() != 3 || order[0] != "critical" || order[1] != "high" || order[2] != "low") {
            fail("Scheduler did not run queued tasks by priority");
        }
    }

    std::cout << "All QuantEdge runtime tests passed.\n";
    return 0;
}
