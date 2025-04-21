#ifndef MACOS_SOCKETS_H
#define MACOS_SOCKETS_H

#ifdef __APPLE__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstddef>
#include <sys/event.h> // For kqueue

// Forward declare the UnixSocketHelpers to avoid including unix_sockets.h
// which would create a circular dependency
namespace UnixSocketHelpers {
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs);
}

// macOS-specific helper functions namespace
namespace MacOSSocketHelpers {
    // macOS-specific implementation of WaitForDataWithTimeout using kqueue for better performance
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs);
}

#endif // __APPLE__
#endif // MACOS_SOCKETS_H