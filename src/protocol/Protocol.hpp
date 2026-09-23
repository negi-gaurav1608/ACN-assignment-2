#pragma once

#include <string>

enum class RequestType {
    GET,
    PUT,
    HEALTH,
    UNKNOWN
};

struct ParsedRequest {
    RequestType type = RequestType::UNKNOWN;
    std::string filename;
    size_t bytes = 0;
    std::string raw_error;
};

// Validates filename against path traversal and forbidden characters
bool validateFilename(const std::string& name);

// Parses a raw header line (e.g., "GET file.txt\n" or "HEALTH\n")
ParsedRequest parseProtocolRequest(const std::string& header_line);
