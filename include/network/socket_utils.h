#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/types.h> // For socklen_t

// Forward declaration of the base socket interface
class ISocketBase;

namespace SocketUtils {

    // Templated helper for type safety when setting socket options
    template<typename T>
    bool SetSocketOption(ISocketBase* socket, int level, int optionName, const T& value);
    
    // Templated helper for type safety when getting socket options
    template<typename T>
    bool GetSocketOption(ISocketBase* socket, int level, int optionName, T& value);
    
    // Specialization declarations for char* socket options
    template<>
    bool SetSocketOption<const char*>(ISocketBase* socket, int level, int optionName, const char* const& value);
    
    template<>
    bool SetSocketOption<char*>(ISocketBase* socket, int level, int optionName, char* const& value);
    
    // Convenience functions for working with character buffers
    bool GetSocketOptionBuffer(ISocketBase* socket, int level, int optionName, char* buffer, socklen_t& bufferSize);
    
    bool SetSocketOptionBuffer(ISocketBase* socket, int level, int optionName, const char* buffer, socklen_t bufferSize);

} // namespace SocketUtils

// Include implementation after declarations
#include "socket_utils.inl"

#endif // SOCKET_UTILS_H