#include "common/config.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include "common/CLI.cpp"
int connectToServer(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

int main(int argc, char* argv[]) {
    try {
        ClientArgs args = parseClientArgs(argc, argv);
        ServerConfig cfg = loadServerConfig(args.config_path);

        if (args.command == "put") {
            std::ifstream ifs(args.target, std::ios::binary | std::ios::ate);
            if (!ifs.is_open()) {
                std::cerr << "error: cannot open local file '" << args.target << "'\n";
                return 1;
            }
            size_t size = ifs.tellg();
            ifs.seekg(0);

            std::filesystem::path p(args.target);
            std::string basename = p.filename().string();

            int sock = connectToServer(cfg.ip, cfg.port);
            if (sock < 0) {
                std::cerr << "error: failed to connect to server\n";
                return 1;
            }

            std::string req = "PUT " + basename + " " + std::to_string(size) + "\n";
            send(sock, req.c_str(), req.size(), 0);

            char buf[256];
            recv(sock, buf, sizeof(buf), 0); // OK 0

            std::vector<char> file_buf(size);
            ifs.read(file_buf.data(), size);
            send(sock, file_buf.data(), size, 0);
            recv(sock, buf, sizeof(buf), 0); // OK 0
            close(sock);

            std::cout << "[PUT Success] Uploaded " << basename << " (" << size << " bytes)\n";
        } else if (args.command == "get") {
            int sock = connectToServer(cfg.ip, cfg.port);
            if (sock < 0) {
                std::cerr << "error: failed to connect to server\n";
                return 1;
            }

            std::string req = "GET " + args.target + "\n";
            send(sock, req.c_str(), req.size(), 0);

            char buf[4096];
            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                std::cerr << "error: empty response from server\n";
                close(sock);
                return 1;
            }

            std::string resp(buf, n);
            if (resp.rfind("ERR", 0) == 0) {
                std::cerr << resp;
                close(sock);
                return 1;
            }

            size_t newline_pos = resp.find('\n');
            size_t file_size = std::stoull(resp.substr(3, newline_pos - 3));

            std::ofstream ofs(args.target, std::ios::binary);
            size_t written = 0;
            size_t initial_body_sz = n - (newline_pos + 1);
            if (initial_body_sz > 0) {
                ofs.write(buf + newline_pos + 1, initial_body_sz);
                written += initial_body_sz;
            }

            while (written < file_size) {
                ssize_t r = recv(sock, buf, sizeof(buf), 0);
                if (r <= 0) break;
                ofs.write(buf, r);
                written += r;
            }
            close(sock);

            std::cout << "[GET Success] Downloaded " << args.target << " (" << file_size << " bytes)\n";
        } else {
            std::cerr << "error: unknown command '" << args.command << "' (use 'put' or 'get')\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
