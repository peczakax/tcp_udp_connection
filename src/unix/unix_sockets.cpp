#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)

#include "unix_sockets.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>

// Helper functions
namespace {
    // Convert NetworkAddress to sockaddr_in
    sockaddr_in CreateSockAddr(const NetworkAddress& address) {
        sockaddr_in result = {};
        result.sin_family = AF_INET;
        result.sin_port = htons(address.port);
        inet_pton(AF_INET, address.ipAddress.c_str(), &(result.sin_addr));
        return result;
    }

    // Convert sockaddr_in to NetworkAddress
    NetworkAddress CreateNetworkAddress(const sockaddr_in& sockAddr) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sockAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
        return NetworkAddress(ipStr, ntohs(sockAddr.sin_port));
    }

    // Get socket address
    bool GetSockAddr(int socketFd, sockaddr_in& address, bool local) {
        sockaddr_in addr = {};
        socklen_t len = sizeof(addr);
        int result = local ? 
            getsockname(socketFd, reinterpret_cast<sockaddr*>(&addr), &len) : 
            getpeername(socketFd, reinterpret_cast<sockaddr*>(&addr), &len);
        if (result == 0) {
            address = addr;
            return true;
        }
        return false;
    }
}

// Implementation of the helper function for WaitForDataWithTimeout
namespace UnixSocketHelpers {

    bool WaitForDataWithTimeout(int socketFd, int timeoutMs) {
        if (socketFd == -1)
            return false;
            
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socketFd, &readSet);

        struct timespec timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_nsec = (timeoutMs % 1000) * 1000000; // convert to nanoseconds

        // Use pselect to wait for data with timeout
        // Pass NULL for sigmask to maintain current signal mask
        int result = pselect(socketFd + 1, &readSet, NULL, NULL, &timeout, NULL);

        // Return true if socket has data available
        return (result > 0 && FD_ISSET(socketFd, &readSet));
    }
}

// UnixTcpSocket Implementation
UnixTcpSocket::UnixTcpSocket() 
    : m_socketFd(-1), m_isConnected(false), m_connectTimeoutMs(-1) {
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
}

UnixTcpSocket::UnixTcpSocket(int socketFd) 
    : m_socketFd(socketFd), m_isConnected(true), m_connectTimeoutMs(-1) {
}

UnixTcpSocket::~UnixTcpSocket() {
    Close();
}

void UnixTcpSocket::Close() {
    if (m_socketFd != -1) {
        close(m_socketFd);
        m_socketFd = -1;
        m_isConnected = false;
    }
}

bool UnixTcpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socketFd == -1)
        return false;

    // Apply SO_REUSEADDR option if enabled
    int value = 1;
    if (setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) != 0) {
        // Continue anyway, but could log error here if needed
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
}

NetworkAddress UnixTcpSocket::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socketFd, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool UnixTcpSocket::IsValid() const {
    return m_socketFd != -1;
}

bool UnixTcpSocket::Connect(const NetworkAddress& remoteAddress) {
    if (m_socketFd == -1)
        return false;

    sockaddr_in addr = CreateSockAddr(remoteAddress);
    
    if (m_connectTimeoutMs <= 0) {
        // Blocking connect with system default timeout
        int result = connect(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        m_isConnected = (result == 0);
        return m_isConnected;
    }
    
    // Non-blocking connect with custom timeout
    // Save original socket flags
    int flags = fcntl(m_socketFd, F_GETFL, 0);
    if (flags == -1) return false;
    
    // Set non-blocking mode
    if (fcntl(m_socketFd, F_SETFL, flags | O_NONBLOCK) == -1) 
        return false;
    
    // Initiate connection
    int result = connect(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    if (result == 0) {
        // Immediate connection success
        fcntl(m_socketFd, F_SETFL, flags); // Restore blocking mode
        m_isConnected = true;
        return true;
    }
    
    if (errno != EINPROGRESS) {
        // Connection failed immediately
        fcntl(m_socketFd, F_SETFL, flags); // Restore blocking mode
        return false;
    }
    
    // Wait for the socket to become writable (connection complete)
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(m_socketFd, &writeSet);
    
    struct timespec timeout;
    timeout.tv_sec = m_connectTimeoutMs / 1000;
    timeout.tv_nsec = (m_connectTimeoutMs % 1000) * 1000000;
    
    result = pselect(m_socketFd + 1, NULL, &writeSet, NULL, &timeout, NULL);
    
    // Restore blocking mode regardless of outcome
    fcntl(m_socketFd, F_SETFL, flags);
    
    if (result <= 0) {
        // Timeout or error
        m_isConnected = false;
        return false;
    }
    
    // Check if connection was successful
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(m_socketFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error) {
        m_isConnected = false;
        return false;
    }
    
    m_isConnected = true;
    return true;
}

int UnixTcpSocket::Send(const std::vector<std::byte>& data) {
    if (m_socketFd == -1 || !m_isConnected)
        return -1;

    return send(m_socketFd, reinterpret_cast<const char*>(data.data()), data.size(), 0);
}

int UnixTcpSocket::Receive(std::vector<std::byte>& buffer) {
    if (m_socketFd == -1 || !m_isConnected)
        return -1;

    const int bufferSize = 4096;
    std::vector<std::byte> tempBuffer(bufferSize);
    
    int bytesRead = recv(m_socketFd, reinterpret_cast<char*>(tempBuffer.data()), bufferSize, 0);
    if (bytesRead > 0) {
        buffer.assign(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
    }
    
    return bytesRead;
}

NetworkAddress UnixTcpSocket::GetRemoteAddress() const {
    sockaddr_in addr = {};
    if (m_isConnected && GetSockAddr(m_socketFd, addr, false)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool UnixTcpSocket::SetConnectTimeout(int timeoutMs) {
    if (timeoutMs < 0) {
        // Use system default
        m_connectTimeoutMs = -1;
    } else {
        m_connectTimeoutMs = timeoutMs;
    }
    return true; // Indicate success (no direct socket option needed here)
}

bool UnixTcpSocket::SetNoDelay(bool enable) {
    if (m_socketFd == -1)
        return false;

    int value = enable ? 1 : 0;
    return (setsockopt(m_socketFd, IPPROTO_TCP, TCP_NODELAY, 
                     &value, sizeof(value)) == 0);
}

bool UnixTcpSocket::WaitForDataWithTimeout(int timeoutMs) {
    if (!m_isConnected)
        return false;
        
    return UnixSocketHelpers::WaitForDataWithTimeout(m_socketFd, timeoutMs);
}

bool UnixTcpSocket::SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) {
    if (m_socketFd == -1)
        return false;

    return (setsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

bool UnixTcpSocket::GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const {
    if (m_socketFd == -1)
        return false;

    return (getsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

// UnixTcpListener Implementation
UnixTcpListener::UnixTcpListener() 
    : m_socketFd(-1) {
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
}

UnixTcpListener::~UnixTcpListener() {
    Close();
}

void UnixTcpListener::Close() {
    if (m_socketFd != -1) {
        close(m_socketFd);
        m_socketFd = -1;
    }
}

bool UnixTcpListener::Bind(const NetworkAddress& localAddress) {
    if (m_socketFd == -1)
        return false;

    // Apply SO_REUSEADDR option if enabled
    int value = 1;
    if (setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) != 0) {
        // Continue anyway, but could log error here if needed
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
}

NetworkAddress UnixTcpListener::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socketFd, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool UnixTcpListener::IsValid() const {
    return m_socketFd != -1;
}

bool UnixTcpListener::Listen(int backlog) {
    if (m_socketFd == -1)
        return false;

    return (listen(m_socketFd, backlog) == 0);
}

std::unique_ptr<IConnectionOrientedSocket> UnixTcpListener::Accept() {
    if (m_socketFd == -1)
        return nullptr;

    sockaddr_in clientAddr = {};
    socklen_t addrLen = sizeof(clientAddr);
    
    int clientSocket = accept(m_socketFd, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSocket == -1)
        return nullptr;

    return std::make_unique<UnixTcpSocket>(clientSocket);
}

bool UnixTcpListener::WaitForDataWithTimeout(int timeoutMs) {
    return UnixSocketHelpers::WaitForDataWithTimeout(m_socketFd, timeoutMs);
}

bool UnixTcpListener::SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) {
    if (m_socketFd == -1)
        return false;

    return (setsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

bool UnixTcpListener::GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const {
    if (m_socketFd == -1)
        return false;

    return (getsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

// UnixUdpSocket Implementation
UnixUdpSocket::UnixUdpSocket() 
    : m_socketFd(-1) {
    m_socketFd = socket(AF_INET, SOCK_DGRAM, 0);
}

UnixUdpSocket::~UnixUdpSocket() {
    Close();
}

void UnixUdpSocket::Close() {
    if (m_socketFd != -1) {
        close(m_socketFd);
        m_socketFd = -1;
    }
}

bool UnixUdpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socketFd == -1)
        return false;

    // Apply SO_REUSEADDR option if enabled
    int value = 1;
    if (setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) != 0) {
        // Continue anyway, but could log error here if needed
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
}

NetworkAddress UnixUdpSocket::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socketFd, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool UnixUdpSocket::IsValid() const {
    return m_socketFd != -1;
}

int UnixUdpSocket::SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) {
    if (m_socketFd == -1)
        return -1;

    sockaddr_in addr = CreateSockAddr(remoteAddress);
    return sendto(m_socketFd, reinterpret_cast<const char*>(data.data()), data.size(), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

int UnixUdpSocket::ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) {
    if (m_socketFd == -1)
        return -1;

    const int bufferSize = 4096;
    std::vector<std::byte> tempBuffer(bufferSize);
    
    sockaddr_in fromAddr = {};
    socklen_t fromLen = sizeof(fromAddr);
    
    int bytesRead = recvfrom(m_socketFd, reinterpret_cast<char*>(tempBuffer.data()), bufferSize, 0,
                           reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
    
    if (bytesRead > 0) {
        buffer.assign(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
        remoteAddress = CreateNetworkAddress(fromAddr);
    }
    
    return bytesRead;
}

bool UnixUdpSocket::SetBroadcast(bool enable) {
    if (m_socketFd == -1)
        return false;

    int value = enable ? 1 : 0;
    return (setsockopt(m_socketFd, SOL_SOCKET, SO_BROADCAST, 
                     &value, sizeof(value)) == 0);
}

bool UnixUdpSocket::JoinMulticastGroup(const NetworkAddress& groupAddress) {
    if (m_socketFd == -1)
        return false;

    ip_mreq mreq = {};
    inet_pton(AF_INET, groupAddress.ipAddress.c_str(), &(mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    return (setsockopt(m_socketFd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     &mreq, sizeof(mreq)) == 0);
}

bool UnixUdpSocket::LeaveMulticastGroup(const NetworkAddress& groupAddress) {
    if (m_socketFd == -1)
        return false;

    ip_mreq mreq = {};
    inet_pton(AF_INET, groupAddress.ipAddress.c_str(), &(mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    return (setsockopt(m_socketFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                     &mreq, sizeof(mreq)) == 0);
}

bool UnixUdpSocket::WaitForDataWithTimeout(int timeoutMs) {
    return UnixSocketHelpers::WaitForDataWithTimeout(m_socketFd, timeoutMs);
}

bool UnixUdpSocket::SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) {
    if (m_socketFd == -1)
        return false;

    return (setsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

bool UnixUdpSocket::GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const {
    if (m_socketFd == -1)
        return false;

    return (getsockopt(m_socketFd, level, optionName, optionValue, optionLen) == 0);
}

// UnixNetworkSocketFactory Implementation
UnixNetworkSocketFactory::UnixNetworkSocketFactory() {
    // Nothing to initialize for Unix sockets
}

UnixNetworkSocketFactory::~UnixNetworkSocketFactory() {
    // Nothing to clean up for Unix sockets
}

std::unique_ptr<ITcpSocket> UnixNetworkSocketFactory::CreateTcpSocket() {
    return std::make_unique<UnixTcpSocket>();
}

std::unique_ptr<ITcpListener> UnixNetworkSocketFactory::CreateTcpListener() {
    return std::make_unique<UnixTcpListener>();
}

std::unique_ptr<IUdpSocket> UnixNetworkSocketFactory::CreateUdpSocket() {
    return std::make_unique<UnixUdpSocket>();
}

#endif // __unix__ || __APPLE__ || __linux__