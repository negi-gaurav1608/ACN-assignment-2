#include "CLI.hpp"
#include <stdexcept>
#include <string>

ServerOptions parseServerArgs(int argc, char* argv[]) {
    ServerOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() {
            if (i + 1 >= argc) throw std::runtime_error("error: missing value for " + arg);
            return argv[++i];
        };

        if (arg == "--sched") opts.sched = next();
        else if (arg == "--quantum") {
            try {
                opts.quantum = std::stoull(next());
                opts.quantum_given = true;
            } catch (...) {
                throw std::runtime_error("error: invalid integer for --quantum");
            }
        }
        else if (arg == "--file") opts.file_dir = next();
        else if (arg == "--p") {
            try {
                opts.packetization = std::stoi(next());
            } catch (...) {
                throw std::runtime_error("error: invalid integer for --p");
            }
        }
        else if (arg == "--config") opts.config_path = next();
        else if (arg == "--metrics-out") opts.metrics_out = next();
        else throw std::runtime_error("error: unknown argument '" + arg + "'");
    }
    return opts;
}

void validateServerOptions(const ServerOptions& opts) {
    if (opts.sched.empty()) {
        throw std::runtime_error("error: missing required option --sched");
    }
    if (opts.sched != "fcfs" && opts.sched != "sjf" && opts.sched != "rr" && opts.sched != "drr") {
        throw std::runtime_error("error: invalid policy '" + opts.sched + "'");
    }
    if (opts.file_dir.empty()) {
        throw std::runtime_error("error: missing required option --file");
    }

    if (opts.sched == "rr" || opts.sched == "drr") {
        if (!opts.quantum_given || opts.quantum == 0) {
            throw std::runtime_error("error: --quantum <Q> required for '" + opts.sched + "'");
        }
    } else {
        if (opts.quantum_given) {
            throw std::runtime_error("error: --quantum rejected for '" + opts.sched + "'");
        }
    }

    if (opts.packetization <= 0) {
        throw std::runtime_error("error: --p must be >= 1");
    }
}

ClientArgs parseClientArgs(int argc, char* argv[]) {
    if (argc < 3) {
        throw std::runtime_error("error: invalid client arguments");
    }
    ClientArgs args;
    args.command = argv[1];
    args.target = argv[2];
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() {
            if (i + 1 >= argc) throw std::runtime_error("error: missing value");
            return argv[++i];
        };
        if (arg == "--config") {
            args.config_path = next();
        } else if (arg == "--requests") {
            try {
                args.requests = std::stoi(next());
                args.requests_given = true;
            } catch (...) {
                throw std::runtime_error("error: invalid integer for --requests");
            }
        } else {
            throw std::runtime_error("error: unknown argument '" + arg + "'");
        }
    }
    return args;
}
