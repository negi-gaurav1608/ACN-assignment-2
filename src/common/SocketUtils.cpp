#include "common/SocketUtils.hpp"

#include <cerrno>
#include <sys/socket.h>

bool sendAll(int socket_fd, const void* data, std::size_t len) {
    const char* buffer = static_cast<const char*>(data);
    std::size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t sent = send(
            socket_fd,
            buffer + total_sent,
            len - total_sent,
            0
        );

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (sent == 0) {
            return false;
        }

        total_sent += static_cast<std::size_t>(sent);
    }

    return true;
}

bool recvAll(
    int socket_fd,
    void* data,
    std::size_t len) {

    char* buffer =
        static_cast<char*>(data);

    std::size_t total_received = 0;

    while (total_received < len) {

        ssize_t received = recv(
            socket_fd,
            buffer + total_received,
            len - total_received,
            0);

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (received == 0) {
            return false;
        }

        total_received +=
            static_cast<std::size_t>(received);
    }

    return true;
}

bool recvLine(int socket_fd, std::string& line) {
    line.clear();

    char ch;

    while (true) {
        ssize_t received = recv(
            socket_fd,
            &ch,
            1,
            0
        );

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (received == 0) {
            return false;
        }

        if (ch == '\n') {
            break;
        }

        if (ch != '\r') {
            line.push_back(ch);
        }

        // Prevent an accidentally huge protocol header.
        if (line.size() > 4096) {
            return false;
        }
    }

    return true;
}
