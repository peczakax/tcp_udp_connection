# TCP/UDP Connection Library

A cross-platform C++ networking library that provides abstractions for TCP and UDP socket communications.

## Features

- Abstract interfaces for TCP and UDP sockets
- Platform-specific implementations for Windows and Unix
- Factory pattern to create appropriate socket implementations
- Simple API for network communications
- Comprehensive test suite using Google Test framework
- Specialized timeout functionality for non-blocking operations

## Requirements

- C++20 compatible compiler
- CMake 3.14 or higher
- For Windows builds: Windows SDK
- For Unix builds: POSIX socket libraries

## Building the Project

This project uses CMake as its build system. Here's how to build it:

```bash
mkdir -p build
cd build
cmake ..
make
```

To build in release mode:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Running Tests

The library includes a comprehensive test suite built with Google Test:

```bash
cd build
ctest
# or run the test executable directly
./tests/network_tests
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
```

These examples show how to:
- Set up TCP servers and clients
- Send and receive UDP datagrams
- Work with UDP broadcast and multicast
- Handle different networking scenarios
- Implement non-blocking operations with timeouts

## Project Structure

```
tcp_udp_connection/
├── CMakeLists.txt                 # Main CMake file
├── README.md                      # Project documentation
├── include/                       # Public API headers
│   └── network/                   # Library namespace
│       ├── network.h              # Core networking abstractions
│       ├── tcp_socket.h           # TCP-specific interfaces
│       ├── udp_socket.h           # UDP-specific interfaces
│       └── platform_factory.h     # Factory interface
├── src/                           # Implementation files
│   ├── CMakeLists.txt             # Library build configuration
│   ├── platform_factory.cpp       # Factory implementation
│   ├── unix/                      # Unix-specific implementations
│   │   ├── CMakeLists.txt         # Unix build configuration
│   │   ├── unix_sockets.h         # Unix internal header
│   │   └── unix_sockets.cpp       # Unix implementation
│   └── windows/                   # Windows-specific implementations
│       ├── CMakeLists.txt         # Windows build configuration
│       ├── windows_sockets.h      # Windows internal header
│       └── windows_sockets.cpp    # Windows implementation
├── tests/                         # Test suite
│   ├── CMakeLists.txt             # Test build configuration
│   ├── tcp_socket_test.cpp        # TCP socket tests
│   ├── tcp_timeout_test.cpp       # TCP timeout functionality tests
│   ├── udp_socket_test.cpp        # UDP socket tests
│   └── udp_timeout_test.cpp       # UDP timeout functionality tests
└── examples/                      # Example applications
    ├── CMakeLists.txt             # Examples build configuration
    ├── examples.cpp               # Common examples code
    ├── tcp_client.cpp             # TCP client example
    ├── tcp_server.cpp             # TCP server example
    ├── udp_broadcast.cpp          # UDP broadcast example
    ├── udp_multicast.cpp          # UDP multicast example
    ├── udp_receiver.cpp           # UDP receiver example
    └── udp_sender.cpp             # UDP sender example
```

## Cross-Platform Support

The library is designed to work seamlessly across different platforms:

- **Windows**: Uses Winsock2 API
- **Unix/Linux/macOS**: Uses POSIX sockets API

The platform-specific implementations are automatically selected at compile time.

## Advanced Features

### Non-blocking Operations with Timeout

Both TCP and UDP implementations support non-blocking operations with configurable timeouts:

```cpp
// Example of using timeout functionality
if (socket->WaitForDataWithTimeout(500)) { // Wait for 500ms
    // Data is available, receive it
    std::vector<char> buffer(1024);
    int bytesReceived = socket->Receive(buffer, buffer.size());
} else {
    // Timeout occurred, no data available
}
```

## License

This project is available under the MIT License.

## Last Updated

April 13, 2025
