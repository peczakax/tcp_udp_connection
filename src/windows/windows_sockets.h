#ifndef WINDOWS_SOCKETS_H
#define WINDOWS_SOCKETS_H

#ifdef _WIN32

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <cstddef> // For std::byte
#include "network/tcp_socket.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"

// Forward declarations
class WindowsTcpSocket;
class WindowsTcpListener;
class WindowsUdpSocket;

// Helper functions namespace
namespace WindowsSocketHelpers {
    // Common helper function for WaitForDataWithTimeout implementation
    bool WaitForDataWithTimeout(SOCKET socket, int timeoutMs);
}

// Windows implementation of TCP socket
class WindowsTcpSocket : public ITcpSocket {
public:
    WindowsTcpSocket();
    explicit WindowsTcpSocket(SOCKET socket);
    ~WindowsTcpSocket() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;

    // IConnectionOrientedSocket implementation
    bool Connect(const NetworkAddress& remoteAddress) override;
    int Send(const std::vector<std::byte>& data) override;
    int Receive(std::vector<std::byte>& buffer) override;
    NetworkAddress GetRemoteAddress() const override;
    bool SetConnectTimeout(int timeoutMs) override; 

    // ITcpSocket implementation
    bool SetNoDelay(bool enable) override;
    bool WaitForDataWithTimeout(int timeoutMs) override;

private:
    SOCKET m_socket;
    bool m_isConnected;
    int m_connectTimeoutMs = -1;
};

// Windows implementation of TCP listener
class WindowsTcpListener : public ITcpListener {
public:
    WindowsTcpListener();
    ~WindowsTcpListener() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;
    bool WaitForDataWithTimeout(int timeoutMs) override;
    bool Listen(int backlog) override;
    std::unique_ptr<IConnectionOrientedSocket> Accept() override;

private:
    SOCKET m_socket;
};

// Windows implementation of UDP socket
class WindowsUdpSocket : public IUdpSocket {
public:
    WindowsUdpSocket();
    ~WindowsUdpSocket() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;

    // IConnectionlessSocket implementation
    int SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) override;
    int ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) override;

    // IUdpSocket implementation
    bool SetBroadcast(bool enable) override;
    bool JoinMulticastGroup(const NetworkAddress& groupAddress) override;
    bool LeaveMulticastGroup(const NetworkAddress& groupAddress) override;
    bool WaitForDataWithTimeout(int timeoutMs) override;

private:
    SOCKET m_socket;
};

// Windows implementation of the network socket factory
class WindowsNetworkSocketFactory : public INetworkSocketFactory {
public:
    WindowsNetworkSocketFactory();
    ~WindowsNetworkSocketFactory() override;

    // Factory methods
    std::unique_ptr<ITcpSocket> CreateTcpSocket() override;
    std::unique_ptr<ITcpListener> CreateTcpListener() override;
    std::unique_ptr<IUdpSocket> CreateUdpSocket() override;

private:
    bool InitializeWinsock();
    void CleanupWinsock();
    bool m_initialized;
};

#endif // _WIN32
#endif // WINDOWS_SOCKETS_H