#include <chrono>
#include <cstddef>
#include <iostream>

#include <sys/event.h> // For kqueue
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "common_socket_helpers.h"
#include "socket_helpers.h"

namespace SocketHelpers {

    // Implementation will only be used on macOS due to the CMake configuration
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs) {
        if (socketFd == -1)
            return false;
            
        // Use kqueue for efficient I/O multiplexing on macOS
        int kq = kqueue();
        if (kq == -1) {
            // Fall back to select-based implementation
            std::cerr << "kqueue creation failed, falling back to select-based implementation" << std::endl;
            
            // Call the common implementation from unix_socket_helpers.cpp
            return SelectWaitForDataWithTimeout(socketFd, timeoutMs);
        }
        
        struct kevent event;
        // Set up the event for read availability on socketFd
        EV_SET(&event, socketFd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, NULL);
        
        // Register the event with kqueue
        if (kevent(kq, &event, 1, NULL, 0, NULL) == -1) {
            close(kq);
            std::cerr << "kevent registration failed, falling back to select-based implementation" << std::endl;
            
            // Call the common implementation from unix_socket_helpers.cpp
            return SelectWaitForDataWithTimeout(socketFd, timeoutMs);
        }
        
        // Set up the timeout for kevent using std::chrono
        struct timespec timeout;
        auto milliseconds = std::chrono::milliseconds(timeoutMs);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(milliseconds);
        auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(milliseconds - seconds);
        
        timeout.tv_sec = seconds.count();
        timeout.tv_nsec = nanoseconds.count();
        
        // Wait for the event with timeout
        int result = kevent(kq, NULL, 0, &event, 1, &timeout);
        
        // Clean up kqueue
        close(kq);
        
        // Return whether data is available
        return (result > 0);
    }
} // namespace SocketHelpers