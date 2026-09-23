#include "common/config.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ServerConfig loadServerConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("error: unable to open config file '" + config_path + "'");
    }

    json j;
    try {
        file >> j;
    } catch (...) {
        throw std::runtime_error("error: malformed JSON in config file '" + config_path + "'");
    }

    if (!j.contains("server") || !j["server"].is_object()) {
        throw std::runtime_error("error: missing required object 'server'");
    }

    const auto& s = j["server"];
    ServerConfig cfg;

    auto check_field = [&](const std::string& field, auto type_check, const std::string& type_name) {
        if (!s.contains(field)) {
            throw std::runtime_error("error: missing required field 'server." + field + "'");
        }
        if (!type_check(s[field])) {
            throw std::runtime_error("error: field 'server." + field + "' must be " + type_name);
        }
    };

    check_field("ip", [](const json& v) { return v.is_string(); }, "a string");
    check_field("port", [](const json& v) { return v.is_number_integer(); }, "an integer");
    check_field("server_threads", [](const json& v) { return v.is_number_integer(); }, "an integer");
    check_field("client_threads", [](const json& v) { return v.is_number_integer(); }, "an integer");

    cfg.ip = s["ip"].get<std::string>();
    cfg.port = s["port"].get<int>();
    cfg.server_threads = s["server_threads"].get<int>();
    cfg.client_threads = s["client_threads"].get<int>();

    if (cfg.port <= 0 || cfg.port > 65535) {
        throw std::runtime_error("error: invalid 'server.port' range");
    }
    if (cfg.server_threads <= 0) {
        throw std::runtime_error("error: 'server.server_threads' must be > 0");
    }
    if (cfg.client_threads <= 0) {
        throw std::runtime_error("error: 'server.client_threads' must be > 0");
    }

    return cfg;
}
