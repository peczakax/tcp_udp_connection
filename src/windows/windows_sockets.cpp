#ifdef _WIN32

#include "windows_sockets.h"
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

// WindowsTcpSocket Implementation
WindowsTcpSocket::WindowsTcpSocket() 
    : m_socket(INVALID_SOCKET), m_isConnected(false) {
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
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_isConnected = false;
    }
}

bool WindowsTcpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
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

    sockaddr_in addr = CreateSockAddr(remoteAddress);
    int result = connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    m_isConnected = (result == 0);
    return m_isConnected;
}

int WindowsTcpSocket::Send(const std::vector<char>& data) {
    if (m_socket == INVALID_SOCKET || !m_isConnected)
        return -1;

    return send(m_socket, data.data(), static_cast<int>(data.size()), 0);
}

int WindowsTcpSocket::Receive(std::vector<char>& buffer, int maxSize) {
    if (m_socket == INVALID_SOCKET || !m_isConnected)
        return -1;

    const int bufferSize = (maxSize > 0) ? maxSize : 4096;
    std::vector<char> tempBuffer(bufferSize);
    
    int bytesRead = recv(m_socket, tempBuffer.data(), bufferSize, 0);
    if (bytesRead > 0) {
        buffer.insert(buffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
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

bool WindowsTcpSocket::SetNoDelay(bool enable) {
    if (m_socket == INVALID_SOCKET)
        return false;

    DWORD value = enable ? 1 : 0;
    return (setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, 
                      reinterpret_cast<char*>(&value), sizeof(value)) == 0);
}

bool WindowsTcpSocket::WaitForDataWithTimeout(int timeoutMs) {
    if (m_socket == INVALID_SOCKET || !m_isConnected)
        return false;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_socket, &readSet);

    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    // Use select to wait for data with timeout
    int result = select(0, &readSet, NULL, NULL, &timeout);  // First param is ignored on Windows

    // Return true if socket has data available
    return (result > 0 && FD_ISSET(m_socket, &readSet));
}

// WindowsTcpListener Implementation
WindowsTcpListener::WindowsTcpListener() 
    : m_socket(INVALID_SOCKET) {
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

WindowsTcpListener::~WindowsTcpListener() {
    Close();
}

void WindowsTcpListener::Close() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool WindowsTcpListener::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
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

// WindowsUdpSocket Implementation
WindowsUdpSocket::WindowsUdpSocket() 
    : m_socket(INVALID_SOCKET) {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

WindowsUdpSocket::~WindowsUdpSocket() {
    Close();
}

void WindowsUdpSocket::Close() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool WindowsUdpSocket::Bind(const NetworkAddress& localAddress) {
    if (m_socket == INVALID_SOCKET)
        return false;

    sockaddr_in addr = CreateSockAddr(localAddress);
    return (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
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

int WindowsUdpSocket::SendTo(const std::vector<char>& data, const NetworkAddress& remoteAddress) {
    if (m_socket == INVALID_SOCKET)
        return -1;

    sockaddr_in addr = CreateSockAddr(remoteAddress);
    return sendto(m_socket, data.data(), static_cast<int>(data.size()), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

int WindowsUdpSocket::ReceiveFrom(std::vector<char>& buffer, NetworkAddress& remoteAddress, int maxSize) {
    if (m_socket == INVALID_SOCKET)
        return -1;

    const int bufferSize = (maxSize > 0) ? maxSize : 4096;
    std::vector<char> tempBuffer(bufferSize);
    
    sockaddr_in fromAddr = {};
    int fromLen = sizeof(fromAddr);
    
    int bytesRead = recvfrom(m_socket, tempBuffer.data(), bufferSize, 0,
                           reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
    
    if (bytesRead > 0) {
        buffer.insert(buffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
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