#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)

#include "socket_helpers.h"
#include "common_socket_helpers.h"

namespace SocketHelpers {

    // Implementation of the helper function for WaitForDataWithTimeout
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs) {
        return SelectWaitForDataWithTimeout(socketFd, timeoutMs);
    }

} // namespace SocketHelpers

#endif // __unix__ || __APPLE__ || __linux__