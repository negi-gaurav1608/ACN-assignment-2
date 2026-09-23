#pragma once

#include <string>

struct ServerOptions {
    std::string sched;
    size_t quantum = 0;
    bool quantum_given = false;
    std::string file_dir;
    int packetization = 1;
    std::string config_path = "config.json";
    std::string metrics_out = "metrics.csv";
};

struct ClientArgs {
    std::string command;
    std::string target;
    int requests = 0;
    bool requests_given = false;
    std::string config_path = "config.json";
};

ServerOptions parseServerArgs(int argc, char* argv[]);
void validateServerOptions(const ServerOptions& opts);
ClientArgs parseClientArgs(int argc, char* argv[]);
