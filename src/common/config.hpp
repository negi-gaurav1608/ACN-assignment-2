#pragma once

#include <string>

struct ServerConfig {
    std::string ip;
    int port = 0;
    int server_threads = 0;
    int client_threads = 0;
};

ServerConfig loadServerConfig(const std::string& config_path);
