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
#include <cstddef>
#include "network/tcp_socket.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"
#include "socket_helpers.h"

// Forward declarations
class UnixTcpSocket;
class UnixTcpListener;
class UnixUdpSocket;

// Unix implementation of TCP socket
class UnixTcpSocket final : public ITcpSocket {
public:
    UnixTcpSocket();
    explicit UnixTcpSocket(int socketFd);
    ~UnixTcpSocket() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;
    bool WaitForDataWithTimeout(int timeoutMs) override;

    // Generic socket option interface overrides
    bool SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) override;
    bool GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const override;

    // IConnectionOrientedSocket implementation
    bool Connect(const NetworkAddress& remoteAddress) override;
    int Send(const std::vector<std::byte>& data) override;
    int Receive(std::vector<std::byte>& buffer) override;
    NetworkAddress GetRemoteAddress() const override;
    bool SetConnectTimeout(int timeoutMs) override;

    // ITcpSocket implementation
    bool SetNoDelay(bool enable) override;

private:
    int m_socketFd;
    bool m_isConnected;
    int m_connectTimeoutMs = -1;
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

    // Generic socket option interface overrides
    bool SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) override;
    bool GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const override;

    bool Listen(int backlog) override;
    std::unique_ptr<IConnectionOrientedSocket> Accept() override;

private:
    int m_socketFd;
};

// Unix implementation of UDP socket
class UnixUdpSocket final : public IUdpSocket {
public:
    UnixUdpSocket();
    ~UnixUdpSocket() override;

    // ISocketBase implementation
    void Close() override;
    bool Bind(const NetworkAddress& localAddress) override;
    NetworkAddress GetLocalAddress() const override;
    bool IsValid() const override;
    bool WaitForDataWithTimeout(int timeoutMs) override;

    // Generic socket option interface overrides
    bool SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) override;
    bool GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const override;

    // IConnectionlessSocket implementation
    int SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) override;
    int ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) override;

    // IUdpSocket implementation
    bool SetBroadcast(bool enable) override;
    bool JoinMulticastGroup(const NetworkAddress& groupAddress) override;
    bool LeaveMulticastGroup(const NetworkAddress& groupAddress) override;

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