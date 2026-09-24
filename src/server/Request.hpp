#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "protocol/Protocol.hpp"


struct Request {

    // Unique request ID
    uint64_t id = 0;

    // GET / PUT / HEALTH / UNKNOWN
    RequestType type = RequestType::UNKNOWN;

    // File name
    std::string filename;

    // Total bytes belonging to this request
    std::size_t bytes = 0;

    // Client socket
    int client_fd = -1;

    // Arrival timestamp
    uint64_t arrival_ns = 0;


    /*
     * ============================================================
     * Scheduling state
     * ============================================================
     */

    // Number of bytes already transferred
    std::size_t bytes_served = 0;

    // Number of scheduling rounds used
    std::size_t rounds = 0;

    // RR bytes left unused / A14 overrun accounting
    std::size_t forfeited_bytes = 0;

    // DRR deficit
    std::size_t deficit = 0;


    /*
     * ============================================================
     * GET state
     * ============================================================
     */

    // Current byte position in the file
    std::size_t file_offset = 0;

    /*
     * Complete GET line waiting to be sent.
     *
     * It includes the '\n' character.
     */
    std::string pending_line;

    /*
     * True after "OK <bytes>\n" has been sent.
     */
    bool response_started = false;


    /*
     * ============================================================
     * PUT state
     * ============================================================
     */

    /*
     * True after the initial "OK 0\n" has been sent.
     *
     * PUT body reception can then continue across multiple
     * scheduling rounds.
     */
    bool put_ready_sent = false;


	// Bytes already received from the socket while reading the request header.
	// These belong to the PUT body and must not be lost.
	std::string pending_body;

	// Whether the destination file has already been created/truncated.
	// First PUT round truncates the file; later rounds append.
	bool put_file_initialized = false;
};


using RequestPtr = std::shared_ptr<Request>;
