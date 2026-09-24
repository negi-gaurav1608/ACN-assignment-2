#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include "server/Request.hpp"

enum class SchedulingPolicy {
    FCFS,
    SJF
};

class RequestQueue {
private:
    std::queue<RequestPtr> queue_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    bool shutdown_ = false;

    SchedulingPolicy policy_;

public:
    explicit RequestQueue(SchedulingPolicy policy)
        : policy_(policy) {}

    void push(const RequestPtr& request) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (shutdown_) {
                return;
            }

            queue_.push(request);
        }

        cv_.notify_one();
    }

    RequestPtr pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [this]() {
            return !queue_.empty() || shutdown_;
        });

        if (queue_.empty()) {
            return nullptr;
        }

        RequestPtr selected;

        if (policy_ == SchedulingPolicy::FCFS) {
            // FCFS = first request that entered the queue
            selected = queue_.front();
            queue_.pop();
        }
        else if (policy_ == SchedulingPolicy::SJF) {
            // Find the request with the smallest byte count.
            //
            // std::queue does not allow arbitrary removal, so copy
            // everything into a temporary vector.
            std::vector<RequestPtr> requests;

            while (!queue_.empty()) {
                requests.push_back(queue_.front());
                queue_.pop();
            }

            std::size_t best_index = 0;

            for (std::size_t i = 1; i < requests.size(); ++i) {
                if (requests[i]->bytes < requests[best_index]->bytes) {
                    best_index = i;
                }
            }

            selected = requests[best_index];

            // Put all other requests back in their original order.
            for (std::size_t i = 0; i < requests.size(); ++i) {
                if (i != best_index) {
                    queue_.push(requests[i]);
                }
            }
        }

        return selected;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }

        cv_.notify_all();
    }

    bool isShutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }
};
