# TCP/UDP Connection Library

A cross-platform C++ networking library that provides abstractions for TCP and UDP socket communications.

## Features

- Abstract interfaces for TCP and UDP sockets
- Platform-specific implementations for Windows and Unix
- Factory pattern to create appropriate socket implementations
- Simple API for network communications
- Specialized timeout functionality for non-blocking operations
- Extensive socket options configuration
- Utility functions for byte conversions
- Comprehensive test suite using Google Test framework
- Integration tests for UDP broadcast functionality
- Examples using TCP and UDP sockets

## Requirements

- C++20 compatible compiler
- CMake 3.14 or higher
- For Windows builds: Windows SDK
- For Unix builds: POSIX socket libraries
- Google Test framework for running tests

## Building the Project

This project uses CMake as its build system. Here's how to build it:

```bash
mkdir -p build
cd build
cmake ..
# On Unix-like systems (Linux, macOS)
make
# On Windows with Visual Studio, or more generally:
cmake --build .
```

To build in release mode:

```bash
# Ensure you are in the build directory
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

## Running Tests

The library includes a comprehensive test suite built with Google Test:

```bash
cd build
# Run tests using CTest
ctest --output-on-failure
# Or, run the specific test target if using CMake build command
cmake --build . --target RUN_TESTS
# Or, run the test executable directly (name might vary based on build config)
./tests/network_tests # (or tests\Debug\network_tests.exe on Windows)
```

## Examples

The library comes with example applications that demonstrate basic usage:

```bash
cd build
# Run TCP examples
./examples/example_tcp_server
./examples/example_tcp_client

# Run UDP examples
./examples/example_udp_sender
./examples/example_udp_receiver
./examples/example_udp_broadcast
./examples/example_udp_multicast

# Run live chat applications
./app/tcp_live_chat_server
./app/tcp_live_chat_client [server_ip] [port]
./app/udp_live_chat_server
./app/udp_live_chat_client [server_ip] [port]
```

These examples show how to:
- Set up TCP servers and clients
- Send and receive UDP datagrams
- Work with UDP broadcast and multicast
- Handle different networking scenarios
- Implement non-blocking operations with timeouts
- Create real-time chat applications with both TCP and UDP protocols

## Project Structure

```
tcp_udp_connection/
├── CMakeLists.txt                 
│   └── Main CMake file
├── README.md                      
│   └── Project documentation
├── app/                           
│   └── Live chat applications
│   ├── CMakeLists.txt             
│   │   └── App build configuration
│   ├── tcp_live_chat_client.cpp   
│   │   └── TCP chat client implementation
│   ├── tcp_live_chat_server.cpp   
│   │   └── TCP chat server implementation
│   ├── udp_live_chat_client.cpp   
│   │   └── UDP chat client implementation
│   └── udp_live_chat_server.cpp   
│       └── UDP chat server implementation
├── include/                       
│   └── Public API headers
│   └── network/                   
│       └── Library namespace
│       ├── byte_utils.h           
│       │   └── Utility functions for byte conversions
│       ├── network.h              
│       │   └── Core networking abstractions
│       ├── tcp_socket.h           
│       │   └── TCP-specific interfaces
│       ├── udp_socket.h           
│       │   └── UDP-specific interfaces
│       ├── socket_options.h       
│       │   └── Socket configuration options
│       └── platform_factory.h     
│           └── Factory interface
├── src/                           
│   └── Implementation files
│   ├── CMakeLists.txt             
│   │   └── Library build configuration
│   ├── platform_factory.cpp       
│   │   └── Factory implementation
│   ├── socket_options.cpp         
│   │   └── Socket options implementation
│   ├── unix/                      
│   │   └── Unix-specific implementations
│   │   ├── CMakeLists.txt         
│   │   │   └── Unix build configuration
│   │   ├── unix_sockets.h         
│   │   │   └── Unix internal header
│   │   └── unix_sockets.cpp       
│   │       └── Unix implementation
│   └── windows/                   
│       └── Windows-specific implementations
│       ├── CMakeLists.txt         
│       │   └── Windows build configuration
│       ├── windows_sockets.h      
│       │   └── Windows internal header
│       └── windows_sockets.cpp    
│           └── Windows implementation
├── tests/                         
│   └── Test suite
│   ├── CMakeLists.txt             
│   │   └── Test build configuration
│   ├── socket_options_test.cpp    
│   │   └── Socket options functionality tests
│   ├── tcp_client_server_connection_test.cpp 
│   │   └── TCP client/server connection tests
│   ├── tcp_socket_test.cpp        
│   │   └── TCP socket functionality tests
│   ├── tcp_timeout_test.cpp       
│   │   └── TCP timeout functionality tests
│   ├── udp_broadcast_test.cpp     
│   │   └── UDP broadcast integration tests
│   ├── udp_client_server_connection_test.cpp 
│   │   └── UDP client/server connection tests
│   ├── udp_socket_test.cpp        
│   │   └── UDP socket functionality tests
│   ├── udp_timeout_test.cpp       
│   │   └── UDP timeout functionality tests
│   └── utils/                     
│       └── Test utilities
│       └── test_utils.h           
│           └── Test utility header
└── examples/                      
    └── Example applications
    ├── CMakeLists.txt             
    │   └── Examples build configuration
    ├── tcp_client.cpp             
    │   └── TCP client example
    ├── tcp_server.cpp             
    │   └── TCP server example
    ├── udp_broadcast.cpp          
    │   └── UDP broadcast example
    ├── udp_multicast.cpp          
    │   └── UDP multicast example
    ├── udp_receiver.cpp           
    │   └── UDP receiver example
    └── udp_sender.cpp             
        └── UDP sender example
```

## Cross-Platform Support

The library is designed to work seamlessly across different platforms:

- **Windows**: Uses Winsock2 API
- **Unix/Linux/macOS**: Uses POSIX sockets API

Platform-specific implementations are automatically selected through the `NetworkFactorySingleton` class, which provides a clean, thread-safe interface to create the appropriate socket implementations.

## Tests

The library includes a comprehensive test suite built with Google Test that covers both unit tests and integration tests:

### Unit Tests

Unit tests verify the correctness of individual components using mock objects:
- TCP socket functionality (`tcp_socket_test.cpp`)
- UDP socket functionality (`udp_socket_test.cpp`)
- TCP timeout functionality (`tcp_timeout_test.cpp`) 
- UDP timeout functionality (`udp_timeout_test.cpp`)
- Socket options functionality (`socket_options_test.cpp`)

### Integration Tests

Integration tests verify the interaction between components using actual network communication:
- TCP client-server communication (`tcp_client_server_connection_test.cpp`)
- UDP client-server communication (`udp_client_server_connection_test.cpp`)
- UDP broadcast functionality (`udp_broadcast_test.cpp`) - tests broadcasting to multiple receivers

### Test Utilities

The test suite includes utility classes and functions to simplify writing tests:
- Mock socket implementations for unit testing
- Test fixtures for common setup and teardown operations
- Helper methods for testing socket options with different data types
- Cross-platform testing helpers for both Windows and Unix environments
- Consistent timeout constants defined in `test_utils.h`
- TestServerBase template class for server testing infrastructure

You can run specific test categories using the gtest filter:

```bash
# Run all tests
./tests/network_tests

# Run only UDP broadcast tests
./tests/network_tests --gtest_filter="UdpBroadcastTest.*"

# Run only TCP client-server tests
./tests/network_tests --gtest_filter="TcpClientServerConnectionTest.*"

# Run socket options tests
./tests/network_tests --gtest_filter="SocketOptionsTest.*"
```

## Socket Options Support

The library provides comprehensive socket configuration through the `SocketOptions` class:

```cpp
// Example of configuring socket options
auto socket = factory->CreateTcpSocket();

// Configure various socket options
SocketOptions::SetReuseAddr(socket.get(), true);
SocketOptions::SetReceiveTimeout(socket.get(), std::chrono::milliseconds(1000));
SocketOptions::SetSendBufferSize(socket.get(), 8192);
SocketOptions::SetKeepAlive(socket.get(), true);
SocketOptions::SetLinger(socket.get(), true, 5); // Enable linger with 5 second timeout
```

Available socket options include:
- Reuse address and port
- Broadcast permission
- Keep-alive settings
- Connection linger control
- Buffer size configuration
- Send/receive timeouts
- Routing options
- Out-of-band data handling
- Interface binding (Unix-specific)
- Socket priority settings (Unix-specific)

The SocketOptions implementation accounts for platform differences, providing:
- Proper timeval/DWORD conversion for timeout values between Windows and Unix
- Platform-specific handling of options like SO_REUSEPORT (combined with SO_REUSEADDR on Windows)
- Graceful fallbacks for Unix-specific options like SO_BINDTODEVICE and SO_PRIORITY when used on Windows

## Interface Design

The library is architected around clean abstractions with a focus on ease of use and platform independence:

### Core Interfaces

#### Network Address (`NetworkAddress`)
A struct to hold IP address and port information.

```cpp
struct NetworkAddress {
    std::string ipAddress;
    unsigned short port;
    NetworkAddress(const std::string& ip = "", unsigned short p = 0);
};
```

#### Base Socket Interface (`ISocketBase`)
Provides common functionality for all socket types.

```cpp
class ISocketBase {
public:
    virtual ~ISocketBase() = default;
    virtual void Close() = 0;
    virtual bool Bind(const NetworkAddress& localAddress) = 0;
    virtual NetworkAddress GetLocalAddress() const = 0;
    virtual bool IsValid() const = 0;
    virtual bool WaitForDataWithTimeout(int timeoutMs) = 0;
    virtual bool SetSocketOption(int level, int optionName, const void* optionValue, socklen_t optionLen) = 0;
    virtual bool GetSocketOption(int level, int optionName, void* optionValue, socklen_t* optionLen) const = 0;

    template<typename T>
    bool SetSocketOption(int level, int optionName, const T& value;

    template<typename T>
    bool GetSocketOption(int level, int optionName, T& value) const;
};
```

#### Connection-Oriented Sockets (`IConnectionOrientedSocket`, `IConnectionListener`)
Interfaces for sockets that establish a connection before data transfer (like TCP).

```cpp
class IConnectionOrientedSocket : public ISocketBase {
public:
    virtual bool Connect(const NetworkAddress& remoteAddress) = 0;
    virtual int Send(const std::vector<std::byte>& data) = 0;
    virtual int Receive(std::vector<std::byte>& buffer) = 0;
    virtual NetworkAddress GetRemoteAddress() const = 0;
    virtual bool SetConnectTimeout(int timeoutMs) = 0;
};

class IConnectionListener : public ISocketBase {
public:
    virtual bool Listen(int backlog) = 0;
    virtual std::unique_ptr<IConnectionOrientedSocket> Accept() = 0;
};
```

#### Connectionless Sockets (`IConnectionlessSocket`)
Interface for sockets that send data without establishing a connection (like UDP).

```cpp
class IConnectionlessSocket : public ISocketBase {
public:
    virtual int SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) = 0;
    virtual int ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) = 0;
};
```

#### TCP Specific Interfaces (`ITcpSocket`, `ITcpListener`)
Concrete interfaces inheriting from the connection-oriented base interfaces, potentially adding TCP-specific methods.

```cpp
class ITcpSocket : public IConnectionOrientedSocket {
public:
    virtual bool SetNoDelay(bool enable) = 0;
};

class ITcpListener : public IConnectionListener {
public:
    virtual std::unique_ptr<IConnectionOrientedSocket> Accept() override = 0;
    virtual std::unique_ptr<ITcpSocket> AcceptTcp();
};
```

#### UDP Specific Interface (`IUdpSocket`)
Concrete interface inheriting from the connectionless base interface, adding UDP-specific methods.

```cpp
class IUdpSocket : public IConnectionlessSocket {
public:
    virtual bool SetBroadcast(bool enable) = 0;
    virtual bool JoinMulticastGroup(const NetworkAddress& groupAddress) = 0;
    virtual bool LeaveMulticastGroup(const NetworkAddress& groupAddress) = 0;
};
```

#### Factory Pattern (`INetworkSocketFactory`)
An abstract factory creates the appropriate socket implementation based on the current platform.

```cpp
class INetworkSocketFactory {
public:
    virtual ~INetworkSocketFactory() = default;
    virtual std::unique_ptr<ITcpSocket> CreateTcpSocket() = 0;
    virtual std::unique_ptr<ITcpListener> CreateTcpListener() = 0;
    virtual std::unique_ptr<IUdpSocket> CreateUdpSocket() = 0;

    static std::unique_ptr<INetworkSocketFactory> CreatePlatformFactory();
};
```

### Byte Utilities

The library includes utility functions for converting between strings and byte vectors:

```cpp
namespace NetworkUtils {
    // Helper function to convert string to vector of bytes
    std::vector<std::byte> StringToBytes(const std::string& str);
    
    // Helper function to convert vector of bytes to string
    std::string BytesToString(const std::vector<std::byte>& bytes);
}
```

### Design Principles

1.  **Abstraction**: Platform-specific details are hidden behind common interfaces (`ISocketBase`, `IConnectionOrientedSocket`, `IConnectionListener`, `IConnectionlessSocket`) and concrete interfaces (`ITcpSocket`, `ITcpListener`, `IUdpSocket`).
2.  **RAII**: Socket resources are managed using `std::unique_ptr`, ensuring automatic cleanup.
3.  **Platform Independence**: The `INetworkSocketFactory` provides the correct platform-specific socket implementations transparently via `CreatePlatformFactory()`.
4.  **Minimal Dependencies**: Relies only on standard C++ libraries and platform socket APIs (Winsock/POSIX).
5.  **Consistent API**: Core operations are accessed through the base interfaces, promoting similar usage patterns.

### Usage Patterns

#### TCP Client/Server Model

```cpp
// Get the platform-specific factory
auto factory = INetworkSocketFactory::CreatePlatformFactory();

// TCP Server example
auto listener = factory->CreateTcpListener();
NetworkAddress serverAddr("0.0.0.0", 8080); // Listen on all interfaces, port 8080
if (listener->Bind(serverAddr) && listener->Listen(5)) {
    std::cout << "Server listening on port 8080..." << std::endl;
    auto clientConnection = listener->AcceptTcp(); // Accept a TCP connection (returns ITcpSocket)
    if (clientConnection && clientConnection->IsValid()) {
        std::cout << "Client connected from " << clientConnection->GetRemoteAddress().ipAddress << std::endl;
        // ... handle client communication ...
        clientConnection->Close();
    }
} else {
    std::cerr << "Failed to start server." << std::endl;
}

// TCP Client example
auto clientSocket = factory->CreateTcpSocket();
NetworkAddress serverTarget("127.0.0.1", 8080);
if (clientSocket->Connect(serverTarget)) {
    std::cout << "Connected to server." << std::endl;
    // Use NetworkUtils for string to byte conversion
    std::string message = "Hello";
    std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
    clientSocket->Send(data);
    // ... receive response ...
    clientSocket->Close();
} else {
    std::cerr << "Failed to connect to server." << std::endl;
}
```

#### UDP Communication Model

```cpp
// Get the platform-specific factory
auto factory = INetworkSocketFactory::CreatePlatformFactory();

// UDP Sender/Receiver example
auto udpSocket = factory->CreateUdpSocket();
NetworkAddress localAddr("0.0.0.0", 9090); // Bind to specific local port
if (udpSocket->Bind(localAddr)) {
    std::cout << "UDP Socket bound to " << udpSocket->GetLocalAddress().port << std::endl;

    // Sending data (use std::byte)
    std::vector<std::byte> data = {std::byte{'P'}, std::byte{'i'}, std::byte{'n'}, std::byte{'g'}};
    NetworkAddress destAddr("192.168.1.100", 9090);
    udpSocket->SendTo(data, destAddr);

    // Receiving data (use std::byte)
    std::vector<std::byte> buffer;
    NetworkAddress senderAddr;
    if (udpSocket->WaitForDataWithTimeout(1000)) { // Wait 1 second
        udpSocket->ReceiveFrom(buffer, senderAddr);
        std::cout << "Received data from " << senderAddr.ipAddress << ":" << senderAddr.port << std::endl;
    }
    udpSocket->Close();
} else {
    std::cerr << "Failed to bind UDP socket." << std::endl;
}

// UDP Broadcast example
auto broadcastSocket = factory->CreateUdpSocket();
if (broadcastSocket->SetBroadcast(true)) {
    // Use NetworkUtils for string to byte conversion
    std::string message = "Broadcast";
    std::vector<std::byte> broadcastData = NetworkUtils::StringToBytes(message);
    NetworkAddress broadcastAddr("255.255.255.255", 9999);
    broadcastSocket->SendTo(broadcastData, broadcastAddr);
    std::cout << "Sent broadcast message." << std::endl;
    broadcastSocket->Close();
}
```

### Error Handling

The library primarily uses return values (`bool` for success/failure, `int` for bytes sent/received where < 0 often indicates an error). Critical failures (e.g., socket creation, binding) might throw `std::runtime_error` or platform-specific exceptions, although the factory pattern aims to handle initial setup errors gracefully by returning `nullptr`. Check return values of methods like `Connect`, `Bind`, `Listen`, `Send`, `Receive`, `SendTo`, `ReceiveFrom`, and `IsValid`.

```cpp
// Get the platform-specific factory
auto factory = INetworkSocketFactory::CreatePlatformFactory();
if (!factory) {
    std::cerr << "Failed to create socket factory." << std::endl;
    return 1;
}

try {
    auto socket = factory->CreateTcpSocket();
    if (!socket) {
        std::cerr << "Failed to create TCP socket." << std::endl;
        return 1;
    }

    NetworkAddress serverAddr("127.0.0.1", 8080);
    if (!socket->Connect(serverAddr)) {
        std::cerr << "Failed to connect to server." << std::endl;
        // Consider using socket->SetConnectTimeout() if needed
    } else {
        // Connection successful
        std::string message = "Test";
        std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
        int bytesSent = socket->Send(data);
        if (bytesSent < 0) {
             std::cerr << "Failed to send data." << std::endl;
        }
        socket->Close();
    }
} catch (const std::exception& e) {
    // Catch potential exceptions from underlying operations if any are thrown
    std::cerr << "Caught exception: " << e.what() << std::endl;
}
```

## UDP Broadcast Support

The library provides robust UDP broadcast support with the following features:

```cpp
// UDP Broadcast example
auto factory = INetworkSocketFactory::CreatePlatformFactory();
auto socket = factory->CreateUdpSocket();

// Enable broadcasting on the socket
if (socket->SetBroadcast(true)) {
    // Send a broadcast message to all devices on the network
    std::string message = "Hello, network!";
    std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
    
    NetworkAddress broadcastAddr("255.255.255.255", 8084);
    int bytesSent = socket->SendTo(data, broadcastAddr);
    
    if (bytesSent > 0) {
        std::cout << "Broadcast message sent successfully" << std::endl;
    }
}
```

The library includes comprehensive integration tests for the broadcast functionality in `udp_broadcast_test.cpp`, which verify:

- Basic broadcast functionality
- Multiple sequential broadcasts
- Large message broadcasting
- Broadcasting from a bound socket
- Rapid broadcast stress testing

## Class Hierarchy

```mermaid
classDiagram
    class ISocketBase {
        <<interface>>
        +Close()
        +Bind(localAddress)
        +GetLocalAddress()
        +IsValid()
        +WaitForDataWithTimeout(timeoutMs)
    }

    class IConnectionOrientedSocket {
        <<interface>>
        +Connect(remoteAddress)
        +Send(data_byte_vector)
        +Receive(buffer_byte_vector)
        +GetRemoteAddress()
        +SetConnectTimeout(timeoutMs)
    }

    class IConnectionListener {
        <<interface>>
        +Listen(backlog)
        +Accept()
    }

    class IConnectionlessSocket {
        <<interface>>
        +SendTo(data_byte_vector, remoteAddress)
        +ReceiveFrom(buffer_byte_vector, remoteAddress)
    }

    class ITcpSocket {
        <<interface>>
        +SetNoDelay(enable)
    }

    class ITcpListener {
        <<interface>>
        +AcceptTcp()
    }

    class IUdpSocket {
        <<interface>>
        +SetBroadcast(enable)
        +JoinMulticastGroup(groupAddress)
        +LeaveMulticastGroup(groupAddress)
    }

    class INetworkSocketFactory {
        <<interface>>
        +CreateTcpSocket()
        +CreateTcpListener()
        +CreateUdpSocket()
        +CreatePlatformFactory() static
    }

    ISocketBase <|-- IConnectionOrientedSocket
    ISocketBase <|-- IConnectionListener
    ISocketBase <|-- IConnectionlessSocket

    IConnectionOrientedSocket <|-- ITcpSocket
    IConnectionListener <|-- ITcpListener
    IConnectionlessSocket <|-- IUdpSocket
```

*Note: Platform-specific implementation classes (e.g., `WindowsTcpSocket`, `UnixUdpSocket`) inherit from these interfaces but are not shown here for clarity. The `INetworkSocketFactory` is responsible for creating instances of the appropriate platform-specific classes.* 

## Advanced Features

### Non-blocking Operations with Timeout

Both TCP and UDP implementations support non-blocking operations with configurable timeouts:

```cpp
// Example of using timeout functionality
if (socket->WaitForDataWithTimeout(500)) { // Wait for 500ms
    // Data is available, receive it
    std::vector<std::byte> buffer(1024);
    int bytesReceived = socket->Receive(buffer);
} else {
    // Timeout occurred, no data available
}
```

The library includes specific timeout-related tests for both TCP and UDP implementations, validating edge cases such as zero timeouts, very long timeouts, and timeout behavior with invalid sockets.

### Byte Conversion Utilities

The library provides utility functions for easy conversion between strings and byte vectors:

```cpp
// Converting a string to bytes for sending
std::string message = "Hello, world!";
std::vector<std::byte> bytes = NetworkUtils::StringToBytes(message);

// Converting received bytes back to a string
std::vector<std::byte> receivedBytes = /* ... */;
std::string receivedMessage = NetworkUtils::BytesToString(receivedBytes);
```

### Configurability through Socket Options

The library provides extensive socket option configuration through the `SocketOptions` class:

```cpp
// Configure socket for better performance
SocketOptions::SetNoDelay(tcpSocket.get(), true);  // Disable Nagle's algorithm
SocketOptions::SetReuseAddr(socket.get(), true);   // Allow port reuse
SocketOptions::SetDontRoute(socket.get(), true);   // Use direct routing

// Configure timeout behavior
SocketOptions::SetReceiveTimeout(socket.get(), std::chrono::milliseconds(500));
SocketOptions::SetSendTimeout(socket.get(), std::chrono::milliseconds(1000));

// Configure buffer sizes
SocketOptions::SetReceiveBufferSize(socket.get(), 16384);  // 16KB receive buffer
SocketOptions::SetSendBufferSize(socket.get(), 8192);      // 8KB send buffer

// Configure connection teardown behavior
SocketOptions::SetLinger(socket.get(), true, 3);  // Wait up to 3 seconds on close
```

The socket options implementation includes thorough tests that verify:
- Boolean options like SetReuseAddr, SetBroadcast
- Integer options like SetReceiveBufferSize, SetSendBufferSize
- Timeout options with proper struct timeval handling on Unix and DWORD on Windows
- Platform-specific options like BindToDevice and SetPriority with appropriate handling
- Options that use complex structs like linger

### Platform-specific Optimizations

The library automatically applies the appropriate system calls on each platform:

```cpp
// This code works the same on both Windows and Unix systems
auto socket = factory->CreateUdpSocket();

// Enable broadcasting - platform-specific details are handled internally
SocketOptions::SetBroadcast(socket.get(), true);

// Bind to a specific network interface on Unix systems (ignored on Windows)
SocketOptions::BindToDevice(socket.get(), "eth0");

// Set socket priority on Unix systems (ignored on Windows)
SocketOptions::SetPriority(socket.get(), 6);
```

### Live Chat Applications

The library includes complete implementations of chat applications using both TCP and UDP protocols:

```bash
# Run the TCP chat server
./app/tcp_live_chat_server [port]

# Connect with the TCP chat client
./app/tcp_live_chat_client [server_ip] [port]

# Run the UDP chat server
./app/udp_live_chat_server [port]

# Connect with the UDP chat client
./app/udp_live_chat_client [server_ip] [port]
```

These applications demonstrate:
- Real-time text-based chat with multiple users
- Private messaging between users
- User presence notifications (join/leave)
- Signal handling for graceful termination
- Non-blocking socket operations with timeouts

## License

This project is available under the MIT License.

## Last Updated

April 20, 2025
