#ifndef SOCKET_HELPERS_H
#define SOCKET_HELPERS_H

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)

namespace SocketHelpers {
    // Common helper function for WaitForDataWithTimeout implementation
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs);
}

#endif // __unix__ || __APPLE__ || __linux__
#endif // SOCKET_HELPERS_H