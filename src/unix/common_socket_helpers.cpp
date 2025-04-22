#include <chrono>
#include <sys/select.h> // For fd_set and FD_* macros
#include <sys/socket.h>

#include "common_socket_helpers.h"

namespace SocketHelpers {

    // The shared select-based implementation that can be used across platforms
    bool SelectWaitForDataWithTimeout(int socketFd, int timeoutMs) {
        if (socketFd == -1)
            return false;
            
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socketFd, &readSet);

        struct timespec timeout;
        auto ms = std::chrono::milliseconds(timeoutMs);
        timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(ms).count();
        timeout.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
            ms - std::chrono::duration_cast<std::chrono::seconds>(ms)).count();

        // Use pselect to wait for data with timeout
        // Pass NULL for sigmask to maintain current signal mask
        int result = pselect(socketFd + 1, &readSet, NULL, NULL, &timeout, NULL);

        // Return true if socket has data available
        return (result > 0 && FD_ISSET(socketFd, &readSet));
    }

} // namespace SocketHelpers