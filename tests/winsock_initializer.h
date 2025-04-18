#ifndef WINSOCK_INITIALIZER_H
#define WINSOCK_INITIALIZER_H

#ifdef _WIN32
#include <stdexcept>
#include <string> // Include for std::to_string
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") // Link against ws2_32.lib

class WinsockInitializer {
public:
    WinsockInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed with error: " + std::to_string(result));
        }
    }

    ~WinsockInitializer() {
        WSACleanup();
    }

    // Delete copy constructor and assignment operator
    WinsockInitializer(const WinsockInitializer&) = delete;
    WinsockInitializer& operator=(const WinsockInitializer&) = delete;
};

// Global instance to ensure initialization before tests run
// and cleanup after tests finish.
// Note: This relies on static initialization order, which is generally
// reliable for a single global instance like this within the test executable.
inline WinsockInitializer g_winsockInitializer;

#else
// Provide a dummy class for non-Windows platforms
class WinsockInitializer {
public:
    WinsockInitializer() {} // No-op
    ~WinsockInitializer() {} // No-op
    WinsockInitializer(const WinsockInitializer&) = delete;
    WinsockInitializer& operator=(const WinsockInitializer&) = delete;
};
// No global instance needed for non-Windows
#endif // _WIN32

#endif // WINSOCK_INITIALIZER_H
