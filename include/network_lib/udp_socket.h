#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include "network.h"

// UDP socket interface
class IUdpSocket : public IConnectionlessSocket {
    // All functionality inherited from IConnectionlessSocket
public:
    // UDP-specific methods could be added here
    virtual bool SetBroadcast(bool enable) = 0;
    virtual bool JoinMulticastGroup(const NetworkAddress& groupAddress) = 0;
    virtual bool LeaveMulticastGroup(const NetworkAddress& groupAddress) = 0;
};

// Factory for creating UDP sockets
class IUdpSocketFactory {
public:
    virtual ~IUdpSocketFactory() = default;
    virtual std::unique_ptr<IUdpSocket> CreateUdpSocket() = 0;
};

#endif // UDP_SOCKET_H