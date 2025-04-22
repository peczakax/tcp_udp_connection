#ifndef COMMON_SOCKET_HELPERS_H
#define COMMON_SOCKET_HELPERS_H

namespace SocketHelpers {
    // Select-based implementation that can be used by multiple platforms
    bool SelectWaitForDataWithTimeout(int socketFd, int timeoutMs);
}

#endif // COMMON_SOCKET_HELPERS_H