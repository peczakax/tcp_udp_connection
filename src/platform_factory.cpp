#include "network_lib/platform_factory.h"

#ifdef _WIN32
#include "windows/windows_sockets.h"
#else
#include "unix/unix_sockets.h"
#endif

std::unique_ptr<INetworkSocketFactory> INetworkSocketFactory::CreatePlatformFactory() {
#ifdef _WIN32
    return std::make_unique<WindowsNetworkSocketFactory>();
#else
    return std::make_unique<UnixNetworkSocketFactory>();
#endif
}