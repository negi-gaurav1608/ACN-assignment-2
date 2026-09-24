#pragma once

#include <cstddef>
#include <string>

bool sendAll(
    int socket_fd,
    const void* data,
    std::size_t len);

bool recvAll(
    int socket_fd,
    void* data,
    std::size_t len);

bool recvLine(
    int socket_fd,
    std::string& line);
//PUT needs a function that keeps receiving until the exact declared byte count has arrived.
