#ifndef SOCKET_UTILS_INL
#define SOCKET_UTILS_INL

#include "network.h"
#include <cstring> // For strlen

namespace SocketUtils {

    template<typename T>
    bool SetSocketOption(ISocketBase* socket, int level, int optionName, const T& value) {
        return socket->SetSocketOption(level, optionName, &value, sizeof(T));
    }
    
    template<typename T>
    bool GetSocketOption(ISocketBase* socket, int level, int optionName, T& value) {
        socklen_t len = sizeof(T);
        return socket->GetSocketOption(level, optionName, &value, &len);
    }

    // Specialization for const char* to properly handle C-style strings
    template<>
    inline bool SetSocketOption<const char*>(ISocketBase* socket, int level, int optionName, const char* const& value) {
        // Use strlen + 1 to include the null terminator
        return socket->SetSocketOption(level, optionName, value, static_cast<socklen_t>(strlen(value) + 1));
    }
    
    // Specialization for char* to properly handle writable C-style strings
    template<>
    inline bool SetSocketOption<char*>(ISocketBase* socket, int level, int optionName, char* const& value) {
        // Use strlen + 1 to include the null terminator
        return socket->SetSocketOption(level, optionName, value, static_cast<socklen_t>(strlen(value) + 1));
    }
    
    // Improved version for getting socket options into char* buffers with explicit buffer size
    inline bool GetSocketOptionBuffer(ISocketBase* socket, int level, int optionName, char* buffer, socklen_t& bufferSize) {
        if (buffer == nullptr || bufferSize == 0) return false;
        return socket->GetSocketOption(level, optionName, buffer, &bufferSize);
    }
    
    // Convenience method for setting socket options with char buffer and explicit size
    inline bool SetSocketOptionBuffer(ISocketBase* socket, int level, int optionName, const char* buffer, socklen_t bufferSize) {
        if (buffer == nullptr) return false;
        return socket->SetSocketOption(level, optionName, buffer, bufferSize);
    }

} // namespace SocketUtils

#endif // SOCKET_UTILS_INL