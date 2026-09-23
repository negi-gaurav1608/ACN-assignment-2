#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "protocol/Protocol.hpp"

struct Request {
    uint64_t id = 0;

    RequestType type = RequestType::UNKNOWN;

    std::string filename;

    // Declared/requested byte count.
    // For GET, this will eventually be determined from the file.
    // For PUT, this comes from the PUT header.
    size_t bytes = 0;

    // Client socket associated with this request.
    int client_fd = -1;

    // Time at which the request is admitted into the shared queue.
    // CLOCK_MONOTONIC, nanoseconds.
    uint64_t arrival_ns = 0;
};

using RequestPtr = std::shared_ptr<Request>;
