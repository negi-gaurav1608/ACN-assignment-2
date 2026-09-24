#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "protocol/Protocol.hpp"

struct Request {
    // Basic request information
    uint64_t id = 0;
    RequestType type = RequestType::UNKNOWN;
    std::string filename;

    // Declared/known request size
    std::size_t bytes = 0;

    // Client socket
    int client_fd = -1;

    // Arrival timestamp
    uint64_t arrival_ns = 0;

    // Scheduling state
    std::size_t bytes_served = 0;

    // Number of scheduling rounds
    std::size_t rounds = 0;

    // Bytes of scheduling allowance that were forfeited
    std::size_t forfeited_bytes = 0;

    // Used later by DRR
    std::size_t deficit = 0;

    // Used later by GET preemption
    std::size_t file_offset = 0;

    // Used later for line-based GET scheduling
    std::string pending_line;

    // Whether the response header has already been sent
    bool response_started = false;
};

using RequestPtr = std::shared_ptr<Request>;
