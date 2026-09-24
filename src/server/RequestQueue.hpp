#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

#include "server/Request.hpp"


enum class SchedulingPolicy {
    FCFS,
    SJF,
    RR,
    DRR
};


class RequestQueue {

private:

    std::deque<RequestPtr> queue_;

    mutable std::mutex mutex_;

    std::condition_variable cv_;

    bool shutdown_ = false;

    SchedulingPolicy policy_;


public:

    explicit RequestQueue(
        SchedulingPolicy policy
    )
        : policy_(policy) {}


    /*
     * ------------------------------------------------------------
     * Push request
     * ------------------------------------------------------------
     */
    void push(
        const RequestPtr& request
    ) {

        if (!request) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(
                mutex_
            );

            if (shutdown_) {
                return;
            }

            queue_.push_back(request);
        }

        cv_.notify_one();
    }


    /*
     * ------------------------------------------------------------
     * Pop next request
     * ------------------------------------------------------------
     *
     * FCFS:
     *     front of queue.
     *
     * RR:
     *     front of queue.
     *
     * DRR:
     *     front of queue.
     *
     * SJF:
     *     smallest declared byte count.
     */
    RequestPtr pop() {

        std::unique_lock<std::mutex> lock(
            mutex_
        );


        cv_.wait(
            lock,
            [this]() {
                return
                    !queue_.empty() ||
                    shutdown_;
            }
        );


        /*
         * If shutdown has happened and all requests
         * have been drained, workers exit.
         */
        if (queue_.empty()) {
            return nullptr;
        }


        /*
         * --------------------------------------------------------
         * FCFS / RR / DRR
         * --------------------------------------------------------
         */
        if (
            policy_ == SchedulingPolicy::FCFS ||
            policy_ == SchedulingPolicy::RR ||
            policy_ == SchedulingPolicy::DRR
        ) {

            RequestPtr request =
                queue_.front();

            queue_.pop_front();

            return request;
        }


        /*
         * --------------------------------------------------------
         * SJF
         * --------------------------------------------------------
         */
        std::size_t best_index = 0;

        for (
            std::size_t i = 1;
            i < queue_.size();
            ++i
        ) {

            if (
                queue_[i]->bytes <
                queue_[best_index]->bytes
            ) {

                best_index = i;
            }
        }


        RequestPtr selected =
            queue_[best_index];


        queue_.erase(
            queue_.begin() +
            static_cast<std::ptrdiff_t>(
                best_index
            )
        );


        return selected;
    }


    /*
     * ------------------------------------------------------------
     * Queue size
     * ------------------------------------------------------------
     */
    std::size_t size() const {

        std::lock_guard<std::mutex> lock(
            mutex_
        );

        return queue_.size();
    }


    /*
     * ------------------------------------------------------------
     * Empty
     * ------------------------------------------------------------
     */
    bool empty() const {

        std::lock_guard<std::mutex> lock(
            mutex_
        );

        return queue_.empty();
    }


    /*
     * ------------------------------------------------------------
     * Shutdown
     * ------------------------------------------------------------
     */
    void shutdown() {

        {
            std::lock_guard<std::mutex> lock(
                mutex_
            );

            shutdown_ = true;
        }

        cv_.notify_all();
    }


    /*
     * ------------------------------------------------------------
     * Is shutdown?
     * ------------------------------------------------------------
     */
    bool isShutdown() const {

        std::lock_guard<std::mutex> lock(
            mutex_
        );

        return shutdown_;
    }
};
