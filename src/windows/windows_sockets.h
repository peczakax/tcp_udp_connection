#ifndef WINDOWS_SOCKETS_H
#define WINDOWS_SOCKETS_H

#ifdef _WIN32

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include "network_lib/tcp_socket.h"
#include "network_lib/udp_socket.h"
#include "network_lib/platform_factory.h"

// Forward declarations
class WindowsTcpSocket;
class WindowsTcpListener;
class WindowsUdpSocket;

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
    int Send(const std::vector<char>& data) override;
    int Receive(std::vector<char>& buffer, int maxSize = -1) override;
    NetworkAddress GetRemoteAddress() const override;

    // ITcpSocket implementation
    bool SetNoDelay(bool enable) override;

private:
    SOCKET m_socket;
    bool m_isConnected;
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

    // IConnectionListener implementation
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
    int SendTo(const std::vector<char>& data, const NetworkAddress& remoteAddress) override;
    int ReceiveFrom(std::vector<char>& buffer, NetworkAddress& remoteAddress, int maxSize = -1) override;

    // IUdpSocket implementation
    bool SetBroadcast(bool enable) override;
    bool JoinMulticastGroup(const NetworkAddress& groupAddress) override;
    bool LeaveMulticastGroup(const NetworkAddress& groupAddress) override;

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