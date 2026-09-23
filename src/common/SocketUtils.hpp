#pragma once

#include <cstddef>
#include <string>

// Send exactly len bytes unless a socket error occurs.
// Returns true if all bytes were sent.
bool sendAll(int socket_fd, const void* data, std::size_t len);

// Receive one newline-terminated protocol header.
//
// Returns:
//   true  -> a complete line was received
//   false -> connection closed/error before '\n'
//
// The returned line does not contain '\n' or '\r'.
bool recvLine(int socket_fd, std::string& line);
