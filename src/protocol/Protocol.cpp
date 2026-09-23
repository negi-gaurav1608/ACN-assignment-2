#include "protocol/Protocol.hpp"
#include <sstream>
#include <algorithm>

bool validateFilename(const std::string& name) {
    if (name.empty()) return false;
    if (name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\\') != std::string::npos) return false;
    if (name.find('\0') != std::string::npos) return false;
    return true;
}

ParsedRequest parseProtocolRequest(const std::string& header_line) {
    ParsedRequest req;
    std::stringstream ss(header_line);
    std::string cmd;
    if (!(ss >> cmd)) {
        req.type = RequestType::UNKNOWN;
        req.raw_error = "Empty request header";
        return req;
    }

    if (cmd == "HEALTH") {
        std::string extra;
        if (ss >> extra) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Extra arguments for HEALTH";
            return req;
        }
        req.type = RequestType::HEALTH;
        return req;
    } else if (cmd == "GET") {
        if (!(ss >> req.filename)) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Missing filename for GET";
            return req;
        }
        std::string extra;
        if (ss >> extra) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Extra arguments for GET";
            return req;
        }
        if (!validateFilename(req.filename)) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Invalid filename for GET";
            return req;
        }
        req.type = RequestType::GET;
        return req;
    } else if (cmd == "PUT") {
        if (!(ss >> req.filename)) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Missing filename for PUT";
            return req;
        }
        if (!(ss >> req.bytes)) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Missing or invalid byte count for PUT";
            return req;
        }
        std::string extra;
        if (ss >> extra) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Extra arguments for PUT";
            return req;
        }
        if (!validateFilename(req.filename)) {
            req.type = RequestType::UNKNOWN;
            req.raw_error = "Invalid filename for PUT";
            return req;
        }
        req.type = RequestType::PUT;
        return req;
    } else {
        req.type = RequestType::UNKNOWN;
        req.raw_error = "Unknown command '" + cmd + "'";
        return req;
    }
}
