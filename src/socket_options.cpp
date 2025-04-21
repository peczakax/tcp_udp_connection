#include "network/socket_options.h"
#include "network/network.h"
#include "network/socket_utils.h"

#ifdef _WIN32
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #include <Windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <cstring> // For memset
#endif

namespace SocketOptions {

// Helper function to get platform-specific option names for better portability
namespace {
    #ifdef _WIN32
    // Windows-specific type conversions
    inline char* asBytePtr(void* ptr) {
        return static_cast<char*>(ptr);
    }
    inline const char* asBytePtr(const void* ptr) {
        return static_cast<const char*>(ptr);
    }
    inline int asIntLen(socklen_t len) {
        return static_cast<int>(len);
    }
    #else
    // Unix-specific type conversions
    inline const void* asBytePtr(const void* ptr) {
        return ptr;
    }
    inline void* asBytePtr(void* ptr) {
        return ptr;
    }
    inline socklen_t asIntLen(socklen_t len) {
        return len;
    }
    #endif
}

// SOL_SOCKET level options implementation
bool SetReuseAddr(ISocketBase* socket, bool enable) {
    if (!socket) return false;

    int value = enable ? 1 : 0;
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_REUSEADDR, value);
}

bool SetReusePort(ISocketBase* socket, bool enable) {
    if (!socket) return false;

    int value = enable ? 1 : 0;
    
    #ifdef _WIN32
    // Windows doesn't have SO_REUSEPORT, it's combined with SO_REUSEADDR
    return SetReuseAddr(socket, enable);
    #else
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_REUSEPORT, value);
    #endif
}

bool SetBroadcast(ISocketBase* socket, bool enable) {
    if (!socket) return false;

    int value = enable ? 1 : 0;
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_BROADCAST, value);
}

bool SetKeepAlive(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_KEEPALIVE, value);
}

bool SetLinger(ISocketBase* socket, bool onoff, int seconds) {
    if (!socket) return false;

    #ifdef _WIN32
    LINGER lingerOpt;
    lingerOpt.l_onoff = onoff ? 1 : 0;
    lingerOpt.l_linger = seconds;
    #else
    struct linger lingerOpt;
    lingerOpt.l_onoff = onoff ? 1 : 0;
    lingerOpt.l_linger = seconds;
    #endif
    
    return SocketUtils::SetSocketOption<struct linger>(socket, SOL_SOCKET, SO_LINGER, lingerOpt);
}

bool SetReceiveBufferSize(ISocketBase* socket, int size) {
    if (!socket) return false;
    
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_RCVBUF, size);
}

bool SetSendBufferSize(ISocketBase* socket, int size) {
    if (!socket) return false;
    
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_SNDBUF, size);
}

bool SetReceiveTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout) {
    if (!socket) return false;

    #ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return SocketUtils::SetSocketOption<DWORD>(socket, SOL_SOCKET, SO_RCVTIMEO, timeoutMs);
    #else
    struct timeval tv;
    tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
    tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(timeout - std::chrono::duration_cast<std::chrono::seconds>(timeout)).count();
    return SocketUtils::SetSocketOption<struct timeval>(socket, SOL_SOCKET, SO_RCVTIMEO, tv);
    #endif
}

bool SetSendTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout) {
    if (!socket) return false;

    #ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return SocketUtils::SetSocketOption<DWORD>(socket, SOL_SOCKET, SO_SNDTIMEO, timeoutMs);
    #else
    struct timeval tv;
    tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
    tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(timeout - std::chrono::duration_cast<std::chrono::seconds>(timeout)).count();
    return SocketUtils::SetSocketOption<struct timeval>(socket, SOL_SOCKET, SO_SNDTIMEO, tv);
    #endif
}

bool SetDontRoute(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_DONTROUTE, value);
}

bool SetOobInline(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_OOBINLINE, value);
}

bool SetReceiveLowWatermark(ISocketBase* socket, int bytes) {
    if (!socket) return false;
    
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_RCVLOWAT, bytes);
}

bool SetSendLowWatermark(ISocketBase* socket, int bytes) {
    if (!socket) return false;
    
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_SNDLOWAT, bytes);
}

bool GetError(ISocketBase* socket, int& errorCode) {
    if (!socket) return false;

    return SocketUtils::GetSocketOption<int>(socket, SOL_SOCKET, SO_ERROR, errorCode);
}

bool GetType(ISocketBase* socket, int& type) {
    if (!socket) return false;

    return SocketUtils::GetSocketOption<int>(socket, SOL_SOCKET, SO_TYPE, type);
}

bool GetAcceptConn(ISocketBase* socket, bool& isListening) {
    if (!socket) return false;

    int value = 0;
    bool success = SocketUtils::GetSocketOption<int>(socket, SOL_SOCKET, SO_ACCEPTCONN, value);
    isListening = (value != 0);
    return success;
}

bool BindToDevice(ISocketBase* socket, const std::string& interfaceName) {
    if (!socket) return false;

    #ifdef _WIN32
    // Windows doesn't have SO_BINDTODEVICE
    // SIO_BINDTODEVICE is closest but works differently
    return true; // Not directly supported in Windows
    #else
    // Use our new raw buffer implementation
    return BindToDeviceRaw(socket, interfaceName.c_str());
    #endif
}

bool SetPriority(ISocketBase* socket, int priority) {
    if (!socket) return false;
    
    #ifdef _WIN32
    // Windows doesn't have SO_PRIORITY
    return true; // Not supported in Windows
    #else
    return SocketUtils::SetSocketOption<int>(socket, SOL_SOCKET, SO_PRIORITY, priority);
    #endif
}

// Raw buffer operations implementation
bool SetRawOption(ISocketBase* socket, int level, int optionName, const char* buffer, size_t bufferSize) {
    if (!socket || !buffer) return false;
    
    return SocketUtils::SetSocketOptionBuffer(socket, level, optionName, buffer, static_cast<socklen_t>(bufferSize));
}

bool GetRawOption(ISocketBase* socket, int level, int optionName, char* buffer, size_t& bufferSize) {
    if (!socket || !buffer || bufferSize == 0) return false;
    
    socklen_t len = static_cast<socklen_t>(bufferSize);
    bool result = SocketUtils::GetSocketOptionBuffer(socket, level, optionName, buffer, len);
    bufferSize = static_cast<size_t>(len);
    return result;
}

bool BindToDeviceRaw(ISocketBase* socket, const char* interfaceName, size_t ifNameMaxLen) {
    if (!socket || !interfaceName) return false;

    #ifdef _WIN32
    // Windows doesn't have SO_BINDTODEVICE
    (void)ifNameMaxLen; // Avoid unused parameter warning
    return true; // Not directly supported in Windows
    #else
    // If ifNameMaxLen is not provided, use strlen + 1 for null terminator
    size_t len = (ifNameMaxLen == 0) ? (strlen(interfaceName) + 1) : ifNameMaxLen;
    return SetRawOption(socket, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, len);
    #endif
}

bool GetBoundDevice(ISocketBase* socket, char* buffer, size_t& bufferSize) {
    if (!socket || !buffer || bufferSize == 0) return false;

    #ifdef _WIN32
    // Windows doesn't have SO_BINDTODEVICE
    if (bufferSize > 0) {
        buffer[0] = '\0';  // Return empty string
        bufferSize = 1;
    }
    return true; // Not directly supported in Windows
    #else
    return GetRawOption(socket, SOL_SOCKET, SO_BINDTODEVICE, buffer, bufferSize);
    #endif
}

} // namespace SocketOptions