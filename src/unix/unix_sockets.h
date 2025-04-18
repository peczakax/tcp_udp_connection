#ifndef UNIX_SOCKETS_H
#define UNIX_SOCKETS_H

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstddef> // For std::byte
#include "network/tcp_socket.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"

// Forward declarations
class UnixTcpSocket;
class UnixTcpListener;
class UnixUdpSocket;

// Helper functions namespace
namespace UnixSocketHelpers {
    // Common helper function for WaitForDataWithTimeout implementation
    bool WaitForDataWithTimeout(int socketFd, int timeoutMs);
}

// Unix implementation of TCP socket
class UnixTcpSocket : public ITcpSocket {
public:
    UnixTcpSocket();
    explicit UnixTcpSocket(int socketFd);
    ~UnixTcpSocket() override;

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

    // ITcpSocket implementation
    bool SetNoDelay(bool enable) override;
    bool WaitForDataWithTimeout(int timeoutMs) override;

private:
    int m_socketFd;
    bool m_isConnected;
};

// Unix implementation of TCP listener
class UnixTcpListener : public ITcpListener {
public:
    UnixTcpListener();
    ~UnixTcpListener() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;
    bool WaitForDataWithTimeout(int timeoutMs) override;
    bool Listen(int backlog) override;
    std::unique_ptr<IConnectionOrientedSocket> Accept() override;

private:
    int m_socketFd;
};

// Unix implementation of UDP socket
class UnixUdpSocket : public IUdpSocket {
public:
    UnixUdpSocket();
    ~UnixUdpSocket() override;

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
    int m_socketFd;
};

// Unix implementation of the network socket factory
class UnixNetworkSocketFactory : public INetworkSocketFactory {
public:
    UnixNetworkSocketFactory();
    ~UnixNetworkSocketFactory() override;

    // Factory methods
    std::unique_ptr<ITcpSocket> CreateTcpSocket() override;
    std::unique_ptr<ITcpListener> CreateTcpListener() override;
    std::unique_ptr<IUdpSocket> CreateUdpSocket() override;
};

#endif // __unix__ || __APPLE__ || __linux__
#endif // UNIX_SOCKETS_H