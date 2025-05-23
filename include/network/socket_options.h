#ifndef SOCKET_OPTIONS_H
#define SOCKET_OPTIONS_H

#include <chrono>
#include <string>

// Forward declaration of the base socket interface
class ISocketBase;

namespace SocketOptions {

// SOL_SOCKET level options
// ------------------------

/**
 * Allows reuse of local addresses
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetReuseAddr(ISocketBase* socket, bool enable);

/**
 * Allows multiple instances of a socket to bind to the same port
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetReusePort(ISocketBase* socket, bool enable);

/**
 * Permits sending broadcast messages
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetBroadcast(ISocketBase* socket, bool enable);

/**
 * Enables periodic keep-alive messages
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetKeepAlive(ISocketBase* socket, bool enable);

/**
 * Controls socket closure behavior when data is pending
 * @param socket The socket to set the option on
 * @param onoff Whether to enable lingering
 * @param seconds How long to linger for (in seconds)
 * @return Whether the operation was successful
 */
bool SetLinger(ISocketBase* socket, bool onoff, int seconds);

/**
 * Sets receive buffer size
 * @param socket The socket to set the option on
 * @param size Buffer size in bytes
 * @return Whether the operation was successful
 */
bool SetReceiveBufferSize(ISocketBase* socket, int size);

/**
 * Sets send buffer size
 * @param socket The socket to set the option on
 * @param size Buffer size in bytes
 * @return Whether the operation was successful
 */
bool SetSendBufferSize(ISocketBase* socket, int size);

/**
 * Sets receive timeout
 * @param socket The socket to set the option on
 * @param timeout Timeout duration
 * @return Whether the operation was successful
 */
bool SetReceiveTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout);

/**
 * Sets send timeout
 * @param socket The socket to set the option on
 * @param timeout Timeout duration
 * @return Whether the operation was successful
 */
bool SetSendTimeout(ISocketBase* socket, const std::chrono::milliseconds& timeout);

/**
 * Don't route packets, send directly to interface
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetDontRoute(ISocketBase* socket, bool enable);

/**
 * Places out-of-band data inline with regular data
 * @param socket The socket to set the option on
 * @param enable Whether to enable the option
 * @return Whether the operation was successful
 */
bool SetOobInline(ISocketBase* socket, bool enable);

/**
 * Sets minimum number of bytes for select() to return readable
 * @param socket The socket to set the option on
 * @param bytes Minimum number of bytes
 * @return Whether the operation was successful
 */
bool SetReceiveLowWatermark(ISocketBase* socket, int bytes);

/**
 * Sets minimum number of bytes for select() to return writable
 * @param socket The socket to set the option on
 * @param bytes Minimum number of bytes
 * @return Whether the operation was successful
 */
bool SetSendLowWatermark(ISocketBase* socket, int bytes);

// Socket information retrieval functions
// ------------------------------------

/**
 * Gets error status
 * @param socket The socket to get the option from
 * @param errorCode Output parameter to store the error code
 * @return Whether the operation was successful
 */
bool GetError(ISocketBase* socket, int& errorCode);

/**
 * Gets socket type
 * @param socket The socket to get the option from
 * @param type Output parameter to store the socket type
 * @return Whether the operation was successful
 */
bool GetType(ISocketBase* socket, int& type);

/**
 * Returns whether socket is in listening mode
 * @param socket The socket to get the option from
 * @param isListening Output parameter to store the listening status
 * @return Whether the operation was successful
 */
bool GetAcceptConn(ISocketBase* socket, bool& isListening);

// Platform-specific options
// ------------------------

/**
 * Binds socket to a specific network interface (Unix-specific)
 * @param socket The socket to set the option on
 * @param interfaceName Name of the network interface
 * @return Whether the operation was successful
 */
bool BindToDevice(ISocketBase* socket, const std::string& interfaceName);

/**
 * Sets socket priority (Unix-specific)
 * @param socket The socket to set the option on
 * @param priority Priority value
 * @return Whether the operation was successful
 */
bool SetPriority(ISocketBase* socket, int priority);

// Raw buffer operations (using the char* specializations)
// -----------------------------------------------------

/**
 * Sets a socket option with a raw buffer
 * @param socket The socket to set the option on
 * @param level The socket option level (SOL_SOCKET, IPPROTO_TCP, etc.)
 * @param optionName The specific option to set
 * @param buffer The buffer containing the option value
 * @param bufferSize Size of the buffer in bytes
 * @return Whether the operation was successful
 */
bool SetRawOption(ISocketBase* socket, int level, int optionName, const char* buffer, size_t bufferSize);

/**
 * Gets a socket option into a raw buffer
 * @param socket The socket to get the option from
 * @param level The socket option level (SOL_SOCKET, IPPROTO_TCP, etc.)
 * @param optionName The specific option to get
 * @param buffer The buffer to store the option value
 * @param bufferSize Input: capacity of buffer, Output: actual size of option value
 * @return Whether the operation was successful
 */
bool GetRawOption(ISocketBase* socket, int level, int optionName, char* buffer, size_t& bufferSize);

/**
 * Enhanced version of BindToDevice that uses raw buffer specialization
 * @param socket The socket to set the option on
 * @param interfaceName Name of the network interface
 * @param ifNameMaxLen Maximum length of the interface name (including null terminator)
 * @return Whether the operation was successful
 */
bool BindToDeviceRaw(ISocketBase* socket, const char* interfaceName, size_t ifNameMaxLen = 0);

/**
 * Gets the name of the device the socket is bound to (Unix-specific)
 * @param socket The socket to get the option from
 * @param buffer Buffer to store the interface name
 * @param bufferSize Input: capacity of buffer, Output: actual size of interface name
 * @return Whether the operation was successful
 */
bool GetBoundDevice(ISocketBase* socket, char* buffer, size_t& bufferSize);

} // namespace SocketOptions

#endif // SOCKET_OPTIONS_H