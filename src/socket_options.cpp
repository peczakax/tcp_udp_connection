#include "network/socket/socket_options.h"
#include "network/network.h"

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
    return socket->SetSocketOption(SOL_SOCKET, SO_REUSEADDR, value);
}

bool SetReusePort(ISocketBase* socket, bool enable) {
    if (!socket) return false;

    int value = enable ? 1 : 0;
    
    #ifdef _WIN32
    // Windows doesn't have SO_REUSEPORT, it's combined with SO_REUSEADDR
    return SetReuseAddr(socket, enable);
    #else
    return socket->SetSocketOption(SOL_SOCKET, SO_REUSEPORT, value);
    #endif
}

bool SetBroadcast(ISocketBase* socket, bool enable) {
    if (!socket) return false;

    int value = enable ? 1 : 0;
    return socket->SetSocketOption(SOL_SOCKET, SO_BROADCAST, value);
}

bool SetKeepAlive(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return socket->SetSocketOption(SOL_SOCKET, SO_KEEPALIVE, value);
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
    
    return socket->SetSocketOption(SOL_SOCKET, SO_LINGER, lingerOpt);
}

bool SetReceiveBufferSize(ISocketBase* socket, int size) {
    if (!socket) return false;
    
    return socket->SetSocketOption(SOL_SOCKET, SO_RCVBUF, size);
}

bool SetSendBufferSize(ISocketBase* socket, int size) {
    if (!socket) return false;
    
    return socket->SetSocketOption(SOL_SOCKET, SO_SNDBUF, size);
}

bool SetReceiveTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout) {
    if (!socket) return false;

    #ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return socket->SetSocketOption(SOL_SOCKET, SO_RCVTIMEO, timeoutMs);
    #else
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    return socket->SetSocketOption(SOL_SOCKET, SO_RCVTIMEO, tv);
    #endif
}

bool SetSendTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout) {
    if (!socket) return false;

    #ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return socket->SetSocketOption(SOL_SOCKET, SO_SNDTIMEO, timeoutMs);
    #else
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    return socket->SetSocketOption(SOL_SOCKET, SO_SNDTIMEO, tv);
    #endif
}

bool SetDontRoute(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return socket->SetSocketOption(SOL_SOCKET, SO_DONTROUTE, value);
}

bool SetOobInline(ISocketBase* socket, bool enable) {
    if (!socket) return false;
    
    int value = enable ? 1 : 0;
    return socket->SetSocketOption(SOL_SOCKET, SO_OOBINLINE, value);
}

bool SetReceiveLowWatermark(ISocketBase* socket, int bytes) {
    if (!socket) return false;
    
    return socket->SetSocketOption(SOL_SOCKET, SO_RCVLOWAT, bytes);
}

bool SetSendLowWatermark(ISocketBase* socket, int bytes) {
    if (!socket) return false;
    
    return socket->SetSocketOption(SOL_SOCKET, SO_SNDLOWAT, bytes);
}

bool GetError(ISocketBase* socket, int& errorCode) {
    if (!socket) return false;
    
    socklen_t len = sizeof(errorCode);
    return socket->GetSocketOption(SOL_SOCKET, SO_ERROR, &errorCode, &len);
}

bool GetType(ISocketBase* socket, int& type) {
    if (!socket) return false;
    
    socklen_t len = sizeof(type);
    return socket->GetSocketOption(SOL_SOCKET, SO_TYPE, &type, &len);
}

bool GetAcceptConn(ISocketBase* socket, bool& isListening) {
    if (!socket) return false;
    
    int value = 0;
    socklen_t len = sizeof(value);
    bool success = socket->GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, &value, &len);
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
    return socket->SetSocketOption(SOL_SOCKET, SO_BINDTODEVICE, 
                                 interfaceName.c_str(), interfaceName.length() + 1);
    #endif
}

bool SetPriority(ISocketBase* socket, int priority) {
    if (!socket) return false;
    
    #ifdef _WIN32
    // Windows doesn't have SO_PRIORITY
    return true; // Not supported in Windows
    #else
    return socket->SetSocketOption(SOL_SOCKET, SO_PRIORITY, priority);
    #endif
}

} // namespace SocketOptions