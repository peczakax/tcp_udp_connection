#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)

#include <chrono>

#include <sys/socket.h>

#include "socket_helpers.h"

// Define a function that can be called from macos_socket_helpers.cpp
bool UnixSocketHelpers_WaitForDataWithTimeout(int socketFd, int timeoutMs) {
    // Standard Unix implementation for Linux and other Unix platforms
    if (socketFd == -1)
        return false;
        
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socketFd, &readSet);

    struct timespec timeout;
    timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(timeoutMs)).count();
    timeout.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(timeoutMs) - std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(timeoutMs))).count();

    // Use pselect to wait for data with timeout
    // Pass NULL for sigmask to maintain current signal mask
    int result = pselect(socketFd + 1, &readSet, NULL, NULL, &timeout, NULL);

    // Return true if socket has data available
    return (result > 0 && FD_ISSET(socketFd, &readSet));
}

namespace SocketHelpers {

    // Implementation of the helper function for WaitForDataWithTimeout
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs) {
        return UnixSocketHelpers_WaitForDataWithTimeout(socketFd, timeoutMs);
    }

} // namespace SocketHelpers

#endif // __unix__ || __APPLE__ || __linux__