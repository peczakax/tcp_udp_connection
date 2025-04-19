#ifdef _WIN32

#include "windows_sockets.h"
#include <stdexcept>
#include <iostream> // Add this include for std::cerr and std::endl

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
    bool GetSockAddr(SOCKET socket, sockaddr_in& address, bool local) {
        sockaddr_in addr = {};
        int len = sizeof(addr);
        int result = local ? 
            getsockname(socket, reinterpret_cast<sockaddr*>(&addr), &len) : 
            getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &len);
        if (result == 0) {
            address = addr;
            return true;
        }
        return false;
    }
}

// Implementation of the helper function for WaitForDataWithTimeout
namespace WindowsSocketHelpers {
    bool WaitForDataWithTimeout(SOCKET socket, int timeoutMs) {
        if (socket == INVALID_SOCKET)
            return false;
            
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);

        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        // Use select to wait for data with timeout
        // First parameter is ignored on Windows (it's different from Unix)
        int result = select(0, &readSet, NULL, NULL, &timeout);

        // Return true if socket has data available
        return (result > 0 && FD_ISSET(socket, &readSet));
    }
}

// WindowsTcpSocket Implementation
WindowsTcpSocket::WindowsTcpSocket() 
    : m_socket(INVALID_SOCKET), m_isConnected(false) {
    // Ensure Winsock is initialized before creating socket
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsockInitialized = true;
        }
    }
    
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

WindowsTcpSocket::WindowsTcpSocket(SOCKET socket) 
    : m_socket(socket), m_isConnected(true) {
}

WindowsTcpSocket::~WindowsTcpSocket() {
    Close();
}

void WindowsTcpSocket::Close() {
    if (m_socket != INVALID_SOCKET) {
        std::cerr << "Closing TCP socket: " << m_socket << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_isConnected = false;
    }
}

bool WindowsTcpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Bind failed: Invalid TCP socket." << std::endl;
        return false;
    }

    // Enable SO_REUSEADDR
    BOOL reuseAddr = TRUE;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr)) != 0) {
        int error = WSAGetLastError();
        std::cerr << "setsockopt(SO_REUSEADDR) failed with error: " << error << std::endl;
        // Continue anyway, but log the error
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    std::cerr << "Binding TCP socket " << m_socket << " to " << localAddress.ipAddress << ":" << localAddress.port << std::endl;
    int result = bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result != 0) {
        int error = WSAGetLastError();
        std::cerr << "Bind failed with error: " << error << std::endl;
        return false;
    }
    std::cerr << "Bind successful." << std::endl;
    return true;
}

NetworkAddress WindowsTcpSocket::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socket, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool WindowsTcpSocket::IsValid() const {
    return m_socket != INVALID_SOCKET;
}

bool WindowsTcpSocket::Connect(const NetworkAddress& remoteAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    // Begin connection attempt
    sockaddr_in addr = CreateSockAddr(remoteAddress);
    
    // First, set the socket to non-blocking mode
    u_long iMode = 1; // 1 = non-blocking
    if (ioctlsocket(m_socket, FIONBIO, &iMode) != 0) {
        std::cerr << "Failed to set socket to non-blocking mode" << std::endl;
        return false;
    }
    
    // Start non-blocking connect
    int result = connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    // If connect returns immediately with success (unlikely for TCP)
    if (result == 0) {
        // Reset to blocking mode
        iMode = 0;
        ioctlsocket(m_socket, FIONBIO, &iMode);
        m_isConnected = true;
        return true;
    }
    
    // If connect doesn't return WSAEWOULDBLOCK, it failed immediately
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK) {
        std::cerr << "Connect failed immediately with error: " << err << std::endl;
        return false;
    }
    
    // Use select to wait for connection with exact timeout
    fd_set writefds, exceptfds;
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    FD_SET(m_socket, &writefds);
    FD_SET(m_socket, &exceptfds);
    
    // Use our specified connect timeout
    struct timeval tv;
    tv.tv_sec = m_connectTimeoutMs / 1000;
    tv.tv_usec = (m_connectTimeoutMs % 1000) * 1000;
    
    std::cerr << "Waiting for connection with timeout: " << m_connectTimeoutMs << "ms" << std::endl;
    
    // The first parameter is ignored in Windows
    result = select(0, NULL, &writefds, &exceptfds, &tv);
    
    // Timeout occurred
    if (result == 0) {
        std::cerr << "Connection timed out after " << m_connectTimeoutMs << "ms" << std::endl;
        
        // Cancel the pending connection by closing the socket and creating a new one
        closesocket(m_socket);
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        m_isConnected = false;
        return false;
    }
    
    // Error occurred
    if (result < 0) {
        std::cerr << "Select failed with error: " << WSAGetLastError() << std::endl;
        m_isConnected = false;
        return false;
    }
    
    // Check if the socket is in the exception set
    if (FD_ISSET(m_socket, &exceptfds)) {
        std::cerr << "Connection failed with socket exception" << std::endl;
        m_isConnected = false;
        return false;
    }
    
    // Check for socket errors
    int optval = 0;
    int optlen = sizeof(optval);
    if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == 0) {
        if (optval != 0) {
            std::cerr << "Connection failed with socket error: " << optval << std::endl;
            m_isConnected = false;
            return false;
        }
    }
    
    // Reset to blocking mode
    iMode = 0;
    if (ioctlsocket(m_socket, FIONBIO, &iMode) != 0) {
        std::cerr << "Warning: Failed to set socket back to blocking mode" << std::endl;
    }
    
    m_isConnected = true;
    return true;
}

int WindowsTcpSocket::Send(const std::vector<std::byte>& data) {
    if (m_socket == INVALID_SOCKET || !m_isConnected)
        return -1;

    return send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
}

int WindowsTcpSocket::Receive(std::vector<std::byte>& buffer) {
    if (m_socket == INVALID_SOCKET || !m_isConnected)
        return -1;

    const int bufferSize = buffer.size() > 0 ? static_cast<int>(buffer.size()) : 4096;
    std::vector<std::byte> tempBuffer(bufferSize);
    
    int bytesRead = recv(m_socket, reinterpret_cast<char*>(tempBuffer.data()), bufferSize, 0);
    if (bytesRead > 0) {
        // REPLACE the buffer content instead of appending
        buffer.assign(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
    }
    
    return bytesRead;
}

NetworkAddress WindowsTcpSocket::GetRemoteAddress() const {
    sockaddr_in addr = {};
    if (m_isConnected && GetSockAddr(m_socket, addr, false)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool WindowsTcpSocket::SetConnectTimeout(int timeoutMs) {
    // Store the timeout value to be used in Connect method
    if (timeoutMs > 0) {
        m_connectTimeoutMs = timeoutMs;
    }
    return true;
}

bool WindowsTcpSocket::SetNoDelay(bool enable) {
    if (m_socket == INVALID_SOCKET)
        return false;

    DWORD value = enable ? 1 : 0;
    return (setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, 
                      reinterpret_cast<char*>(&value), sizeof(value)) == 0);
}

bool WindowsTcpSocket::WaitForDataWithTimeout(int timeoutMs) {
    if (!m_isConnected)
        return false;
        
    return WindowsSocketHelpers::WaitForDataWithTimeout(m_socket, timeoutMs);
}

// WindowsTcpListener Implementation
WindowsTcpListener::WindowsTcpListener() 
    : m_socket(INVALID_SOCKET) {
    // Ensure Winsock is initialized before creating socket
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsockInitialized = true;
        }
    }
    
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

WindowsTcpListener::~WindowsTcpListener() {
    Close();
}

void WindowsTcpListener::Close() {
    if (m_socket != INVALID_SOCKET) {
        std::cerr << "Closing TCP listener socket: " << m_socket << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool WindowsTcpListener::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Bind failed: Invalid TCP listener socket." << std::endl;
        return false;
    }

    // Enable SO_REUSEADDR
    BOOL reuseAddr = TRUE;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr)) != 0) {
        int error = WSAGetLastError();
        std::cerr << "setsockopt(SO_REUSEADDR) for listener failed with error: " << error << std::endl;
        // Continue anyway, but log the error
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    std::cerr << "Binding TCP listener " << m_socket << " to " << localAddress.ipAddress << ":" << localAddress.port << std::endl;
    int result = bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result != 0) {
        int error = WSAGetLastError();
        std::cerr << "Listener bind failed with error: " << error << std::endl;
        return false;
    }
    std::cerr << "Listener bind successful." << std::endl;
    return true;
}

NetworkAddress WindowsTcpListener::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socket, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool WindowsTcpListener::IsValid() const {
    return m_socket != INVALID_SOCKET;
}

bool WindowsTcpListener::Listen(int backlog) {
    if (m_socket == INVALID_SOCKET)
        return false;

    return (listen(m_socket, backlog) == 0);
}

std::unique_ptr<IConnectionOrientedSocket> WindowsTcpListener::Accept() {
    if (m_socket == INVALID_SOCKET)
        return nullptr;

    sockaddr_in clientAddr = {};
    int addrLen = sizeof(clientAddr);
    
    SOCKET clientSocket = accept(m_socket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSocket == INVALID_SOCKET)
        return nullptr;

    return std::make_unique<WindowsTcpSocket>(clientSocket);
}

bool WindowsTcpListener::WaitForDataWithTimeout(int timeoutMs) {
    return WindowsSocketHelpers::WaitForDataWithTimeout(m_socket, timeoutMs);
}

// WindowsUdpSocket Implementation
WindowsUdpSocket::WindowsUdpSocket() 
    : m_socket(INVALID_SOCKET) {
    // Ensure Winsock is initialized before creating socket
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsockInitialized = true;
        }
    }
    
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

WindowsUdpSocket::~WindowsUdpSocket() {
    Close();
}

void WindowsUdpSocket::Close() {
    if (m_socket != INVALID_SOCKET) {
        std::cerr << "Closing UDP socket: " << m_socket << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool WindowsUdpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Bind failed: Invalid UDP socket." << std::endl;
        return false;
    }

    // Enable SO_REUSEADDR for UDP sockets as well
    BOOL reuseAddr = TRUE;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr)) != 0) {
        int error = WSAGetLastError();
        std::cerr << "setsockopt(SO_REUSEADDR) for UDP failed with error: " << error << std::endl;
        // Continue anyway, but log the error
    }

    sockaddr_in addr = CreateSockAddr(localAddress);
    std::cerr << "Binding UDP socket " << m_socket << " to " << localAddress.ipAddress << ":" << localAddress.port << std::endl;
    int result = bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result != 0) {
        int error = WSAGetLastError();
        std::cerr << "UDP bind failed with error: " << error << std::endl;
        return false;
    }
    std::cerr << "UDP bind successful." << std::endl;
    return true;
}

NetworkAddress WindowsUdpSocket::GetLocalAddress() const {
    sockaddr_in addr = {};
    if (GetSockAddr(m_socket, addr, true)) {
        return CreateNetworkAddress(addr);
    }
    return NetworkAddress();
}

bool WindowsUdpSocket::IsValid() const {
    return m_socket != INVALID_SOCKET;
}

int WindowsUdpSocket::SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) {
    if (m_socket == INVALID_SOCKET)
        return -1;

    sockaddr_in addr = CreateSockAddr(remoteAddress);
    return sendto(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

int WindowsUdpSocket::ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) {
    if (m_socket == INVALID_SOCKET)
        return -1;

    const int bufferSize = buffer.size() > 0 ? static_cast<int>(buffer.size()) : 4096;
    std::vector<std::byte> tempBuffer(bufferSize);
    
    sockaddr_in fromAddr = {};
    int fromLen = sizeof(fromAddr);
    
    int bytesRead = recvfrom(m_socket, reinterpret_cast<char*>(tempBuffer.data()), bufferSize, 0,
                           reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
    
    if (bytesRead > 0) {
        // REPLACE the buffer content instead of appending
        buffer.assign(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
        remoteAddress = CreateNetworkAddress(fromAddr);
    }
    
    return bytesRead;
}

bool WindowsUdpSocket::SetBroadcast(bool enable) {
    if (m_socket == INVALID_SOCKET)
        return false;

    BOOL value = enable ? TRUE : FALSE;
    return (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, 
                     reinterpret_cast<char*>(&value), sizeof(value)) == 0);
}

bool WindowsUdpSocket::JoinMulticastGroup(const NetworkAddress& groupAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    ip_mreq mreq = {};
    inet_pton(AF_INET, groupAddress.ipAddress.c_str(), &(mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    return (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     reinterpret_cast<char*>(&mreq), sizeof(mreq)) == 0);
}

bool WindowsUdpSocket::LeaveMulticastGroup(const NetworkAddress& groupAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    ip_mreq mreq = {};
    inet_pton(AF_INET, groupAddress.ipAddress.c_str(), &(mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    return (setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                     reinterpret_cast<char*>(&mreq), sizeof(mreq)) == 0);
}

bool WindowsUdpSocket::WaitForDataWithTimeout(int timeoutMs) {
    return WindowsSocketHelpers::WaitForDataWithTimeout(m_socket, timeoutMs);
}

// WindowsNetworkSocketFactory Implementation
WindowsNetworkSocketFactory::WindowsNetworkSocketFactory() 
    : m_initialized(false) {
    m_initialized = InitializeWinsock();
}

WindowsNetworkSocketFactory::~WindowsNetworkSocketFactory() {
    if (m_initialized) {
        CleanupWinsock();
    }
}

std::unique_ptr<ITcpSocket> WindowsNetworkSocketFactory::CreateTcpSocket() {
    if (!m_initialized)
        return nullptr;
    
    return std::make_unique<WindowsTcpSocket>();
}

std::unique_ptr<ITcpListener> WindowsNetworkSocketFactory::CreateTcpListener() {
    if (!m_initialized)
        return nullptr;
    
    return std::make_unique<WindowsTcpListener>();
}

std::unique_ptr<IUdpSocket> WindowsNetworkSocketFactory::CreateUdpSocket() {
    if (!m_initialized)
        return nullptr;
    
    return std::make_unique<WindowsUdpSocket>();
}

bool WindowsNetworkSocketFactory::InitializeWinsock() {
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
}

void WindowsNetworkSocketFactory::CleanupWinsock() {
    WSACleanup();
}

#endif // _WIN32