#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "network/socket_options.h"
#include "network/network.h"
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#endif

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::WithArg;

// Mock ISocketBase
class MockSocket : public ISocketBase {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetSocketOption, (int level, int optionName, const void* optionValue, socklen_t optionLen), (override));
    MOCK_METHOD(bool, GetSocketOption, (int level, int optionName, void* optionValue, socklen_t* optionLen), (const, override));
};

// Constants for socket options testing
constexpr int BUFFER_SIZE = 8192;
constexpr int RECEIVE_TIMEOUT_MS = 1500;
constexpr int SEND_TIMEOUT_MS = 2345;
constexpr int PRIORITY = 7;
constexpr int LINGER_SECONDS = 5;
constexpr int RECEIVE_LOW_WATERMARK = 1024;
constexpr int SEND_LOW_WATERMARK = 2048;
constexpr int ERROR_CODE = 5;

// Test Fixture
class SocketOptionsTest : public ::testing::Test {
protected:
    MockSocket mockSocket;

    // Helper method for testing boolean socket options
    void TestBooleanOption(
        const std::function<bool(ISocketBase*, bool)>& optionSetter,
        int socketLevel,
        int optionName) 
    {
        // Test enabling
        EXPECT_CALL(mockSocket, SetSocketOption(socketLevel, optionName, NotNull(), sizeof(int)))
            .WillOnce(DoAll(
                WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
                Return(true)
            ));
        EXPECT_TRUE(optionSetter(&mockSocket, true));

        // Test disabling
        EXPECT_CALL(mockSocket, SetSocketOption(socketLevel, optionName, NotNull(), sizeof(int)))
            .WillOnce(DoAll(
                WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
                Return(true)
            ));
        EXPECT_TRUE(optionSetter(&mockSocket, false));

        // Test null socket
        EXPECT_FALSE(optionSetter(nullptr, true));
    }

    // Helper for testing integer-value socket options
    void TestIntOption(
        const std::function<bool(ISocketBase*, int)>& optionSetter,
        int socketLevel,
        int optionName,
        int value) 
    {
        EXPECT_CALL(mockSocket, SetSocketOption(socketLevel, optionName, NotNull(), sizeof(int)))
            .WillOnce(DoAll(
                WithArg<2>([value](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), value); }),
                Return(true)
            ));
        EXPECT_TRUE(optionSetter(&mockSocket, value));

        EXPECT_FALSE(optionSetter(nullptr, value));
    }
    
    // Helper for testing get socket option methods
    template<typename T>
    void TestGetOption(
        bool (*optionGetter)(ISocketBase*, T&),
        int socketLevel, 
        int optionName,
        T expectedValue)
    {
        socklen_t expectedLen = sizeof(T);
        // Special handling for booleans - on the socket level, bools are represented as ints
        if constexpr (std::is_same_v<T, bool>) {
            expectedLen = sizeof(int);
            
            EXPECT_CALL(mockSocket, GetSocketOption(socketLevel, optionName, NotNull(), Pointee(expectedLen)))
                .WillOnce(DoAll(
                    WithArg<2>([expectedValue](void* val) { 
                        // For boolean options, the value is stored as an int (0 or 1)
                        *static_cast<int*>(val) = expectedValue ? 1 : 0;
                    }),
                    WithArg<3>([expectedLen](socklen_t* len) {
                        *len = expectedLen;
                    }),
                    Return(true)
                ));
        } else {
            EXPECT_CALL(mockSocket, GetSocketOption(socketLevel, optionName, NotNull(), Pointee(expectedLen)))
                .WillOnce(DoAll(
                    WithArg<2>([expectedValue](void* val) { 
                        *static_cast<T*>(val) = expectedValue; 
                    }),
                    WithArg<3>([expectedLen](socklen_t* len) {
                        *len = expectedLen;
                    }),
                    Return(true)
                ));
        }

        T actualValue{};
        EXPECT_TRUE(optionGetter(&mockSocket, actualValue));
        EXPECT_EQ(actualValue, expectedValue);

        EXPECT_FALSE(optionGetter(nullptr, actualValue));
    }
    
    // Helper for testing timeout options
    void TestTimeoutOption(
        const std::function<bool(ISocketBase*, const std::chrono::milliseconds&)>& optionSetter,
        int socketLevel,
        int optionName,
        std::chrono::milliseconds timeout)
    {
        #ifdef _WIN32
        DWORD expected_timeout_ms = static_cast<DWORD>(timeout.count());
        EXPECT_CALL(mockSocket, SetSocketOption(socketLevel, optionName, NotNull(), sizeof(DWORD)))
            .WillOnce(DoAll(
                WithArg<2>([expected_timeout_ms](const void* val) {
                    EXPECT_EQ(*static_cast<const DWORD*>(val), expected_timeout_ms);
                }),
                Return(true)
            ));
        #else
        struct timeval expected_tv;
        expected_tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
        expected_tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(timeout - std::chrono::duration_cast<std::chrono::seconds>(timeout)).count();
        EXPECT_CALL(mockSocket, SetSocketOption(socketLevel, optionName, NotNull(), sizeof(struct timeval)))
            .WillOnce(DoAll(
                WithArg<2>([expected_tv](const void* val) {
                    const struct timeval* tv = static_cast<const struct timeval*>(val);
                    EXPECT_EQ(tv->tv_sec, expected_tv.tv_sec);
                    EXPECT_EQ(tv->tv_usec, expected_tv.tv_usec);
                }),
                Return(true)
            ));
        #endif
        EXPECT_TRUE(optionSetter(&mockSocket, timeout));

        EXPECT_FALSE(optionSetter(nullptr, timeout));
    }

    // Helper method for testing linger option
    void TestLingerOption(bool onoff, int seconds) {
        #ifdef _WIN32
        EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(LINGER)))
            .WillOnce(DoAll(
                WithArg<2>([onoff, seconds](const void* val) {
                    const LINGER* lingerOpt = static_cast<const LINGER*>(val);
                    EXPECT_EQ(lingerOpt->l_onoff, onoff ? 1 : 0);
                    if (onoff) {
                        EXPECT_EQ(lingerOpt->l_linger, seconds);
                    } else {
                        EXPECT_EQ(lingerOpt->l_linger, 0);
                    }
                }),
                Return(true)
            ));
        #else
        EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(struct linger)))
            .WillOnce(DoAll(
                WithArg<2>([onoff, seconds](const void* val) {
                    const struct linger* lingerOpt = static_cast<const struct linger*>(val);
                    EXPECT_EQ(lingerOpt->l_onoff, onoff ? 1 : 0);
                    if (onoff) {
                        EXPECT_EQ(lingerOpt->l_linger, seconds);
                    } else {
                        EXPECT_EQ(lingerOpt->l_linger, 0);
                    }
                }),
                Return(true)
            ));
        #endif
        EXPECT_TRUE(SocketOptions::SetLinger(&mockSocket, onoff, onoff ? seconds : 0));
    }
};

// --- Test Cases ---

TEST_F(SocketOptionsTest, SetReuseAddr) {
    TestBooleanOption(SocketOptions::SetReuseAddr, SOL_SOCKET, SO_REUSEADDR);
}

TEST_F(SocketOptionsTest, SetReusePort) {
    #ifdef _WIN32 // On Windows, SO_REUSEPORT maps to SO_REUSEADDR
    TestBooleanOption(SocketOptions::SetReusePort, SOL_SOCKET, SO_REUSEADDR);
    #else
    TestBooleanOption(SocketOptions::SetReusePort, SOL_SOCKET, SO_REUSEPORT);
    #endif
}

TEST_F(SocketOptionsTest, SetBroadcast) {
    TestBooleanOption(SocketOptions::SetBroadcast, SOL_SOCKET, SO_BROADCAST);
}

TEST_F(SocketOptionsTest, SetKeepAlive) {
    TestBooleanOption(SocketOptions::SetKeepAlive, SOL_SOCKET, SO_KEEPALIVE);
}

TEST_F(SocketOptionsTest, SetLinger) {
    TestLingerOption(true, LINGER_SECONDS);
    TestLingerOption(false, 0); // Seconds don't matter if off

    EXPECT_FALSE(SocketOptions::SetLinger(nullptr, true, LINGER_SECONDS));
}

TEST_F(SocketOptionsTest, SetReceiveBufferSize) {
    TestIntOption(SocketOptions::SetReceiveBufferSize, SOL_SOCKET, SO_RCVBUF, BUFFER_SIZE);
}

TEST_F(SocketOptionsTest, SetSendBufferSize) {
    TestIntOption(SocketOptions::SetSendBufferSize, SOL_SOCKET, SO_SNDBUF, BUFFER_SIZE);
}

TEST_F(SocketOptionsTest, SetReceiveTimeout) {
    auto timeout = std::chrono::milliseconds(RECEIVE_TIMEOUT_MS);
    TestTimeoutOption(SocketOptions::SetReceiveTimeout, SOL_SOCKET, SO_RCVTIMEO, timeout);
}

TEST_F(SocketOptionsTest, SetSendTimeout) {
    auto timeout = std::chrono::milliseconds(SEND_TIMEOUT_MS);
    TestTimeoutOption(SocketOptions::SetSendTimeout, SOL_SOCKET, SO_SNDTIMEO, timeout);
}

TEST_F(SocketOptionsTest, SetDontRoute) {
    TestBooleanOption(SocketOptions::SetDontRoute, SOL_SOCKET, SO_DONTROUTE);
}

TEST_F(SocketOptionsTest, SetOobInline) {
    TestBooleanOption(SocketOptions::SetOobInline, SOL_SOCKET, SO_OOBINLINE);
}

TEST_F(SocketOptionsTest, SetReceiveLowWatermark) {
    TestIntOption(SocketOptions::SetReceiveLowWatermark, SOL_SOCKET, SO_RCVLOWAT, RECEIVE_LOW_WATERMARK);
}

TEST_F(SocketOptionsTest, SetSendLowWatermark) {
    TestIntOption(SocketOptions::SetSendLowWatermark, SOL_SOCKET, SO_SNDLOWAT, SEND_LOW_WATERMARK);
}

TEST_F(SocketOptionsTest, GetError) {
    TestGetOption(SocketOptions::GetError, SOL_SOCKET, SO_ERROR, ERROR_CODE);
}

TEST_F(SocketOptionsTest, GetType) {
    int expectedType = SOCK_STREAM; // Example socket type
    TestGetOption(SocketOptions::GetType, SOL_SOCKET, SO_TYPE, expectedType);
}

TEST_F(SocketOptionsTest, GetAcceptConn) {
    // Constants for GetAcceptConn test
    constexpr int GETSOCKOPT_VALUE_TRUE = 1;
    constexpr int GETSOCKOPT_VALUE_FALSE = 0;
    socklen_t expectedLen = sizeof(int);

    // Test when socket is in listening state
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), _))
        .WillOnce(DoAll(
            // Use a lambda to safely copy the value into the void* buffer
            WithArg<2>([GETSOCKOPT_VALUE_TRUE](void* val) { 
                *static_cast<int*>(val) = GETSOCKOPT_VALUE_TRUE; 
            }),
            // Use a lambda to safely set the length
            WithArg<3>([expectedLen](socklen_t* len) {
                *len = expectedLen;
            }),
            Return(true)
        ));

    bool isListening = false;
    EXPECT_TRUE(SocketOptions::GetAcceptConn(&mockSocket, isListening));
    EXPECT_TRUE(isListening);

    // Test when socket is not in listening state
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), _))
        .WillOnce(DoAll(
            WithArg<2>([GETSOCKOPT_VALUE_FALSE](void* val) { 
                *static_cast<int*>(val) = GETSOCKOPT_VALUE_FALSE; 
            }),
            // Use a lambda to safely set the length
            WithArg<3>([expectedLen](socklen_t* len) {
                *len = expectedLen;
            }),
            Return(true)
        ));

    isListening = true; // Reset to true to ensure it's changed
    EXPECT_TRUE(SocketOptions::GetAcceptConn(&mockSocket, isListening));
    EXPECT_FALSE(isListening);

    EXPECT_FALSE(SocketOptions::GetAcceptConn(nullptr, isListening));
}

TEST_F(SocketOptionsTest, BindToDevice) {
    std::string interfaceName = "eth0";
    #ifdef _WIN32
    // On Windows, this function should just return true without calling SetSocketOption
    EXPECT_CALL(mockSocket, SetSocketOption(_, _, _, _)).Times(0);
    EXPECT_TRUE(SocketOptions::BindToDevice(&mockSocket, interfaceName));
    #else
    // On Unix, expect a call to SetSocketOption with SO_BINDTODEVICE
    socklen_t expectedLen = interfaceName.length() + 1;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_BINDTODEVICE, NotNull(), expectedLen))
        .WillOnce(DoAll(
            WithArg<2>([&interfaceName](const void* val) {
                EXPECT_STREQ(static_cast<const char*>(val), interfaceName.c_str());
            }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::BindToDevice(&mockSocket, interfaceName));
    #endif

    EXPECT_FALSE(SocketOptions::BindToDevice(nullptr, interfaceName));
}

TEST_F(SocketOptionsTest, SetPriority) {
    #ifdef _WIN32
    // On Windows, this function should just return true without calling SetSocketOption
    EXPECT_CALL(mockSocket, SetSocketOption(_, _, _, _)).Times(0);
    EXPECT_TRUE(SocketOptions::SetPriority(&mockSocket, PRIORITY));
    #else
    // On Unix, expect a call to SetSocketOption with SO_PRIORITY
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_PRIORITY, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([PRIORITY = PRIORITY](const void* val) {
                EXPECT_EQ(*static_cast<const int*>(val), PRIORITY);
            }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetPriority(&mockSocket, PRIORITY));
    #endif

    EXPECT_FALSE(SocketOptions::SetPriority(nullptr, PRIORITY));
}

TEST_F(SocketOptionsTest, SetRawOption) {
    const char* testData = "test-data";
    size_t dataSize = strlen(testData) + 1;  // Include null terminator
    
    // Use a common socket option that exists on both platforms for the test
    int testOption = SO_REUSEADDR;
    
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, testOption, NotNull(), static_cast<socklen_t>(dataSize)))
        .WillOnce(DoAll(
            WithArg<2>([testData](const void* val) {
                EXPECT_STREQ(static_cast<const char*>(val), testData);
            }),
            Return(true)
        ));
    
    EXPECT_TRUE(SocketOptions::SetRawOption(&mockSocket, SOL_SOCKET, testOption, testData, dataSize));
    
    // Test with null socket
    EXPECT_FALSE(SocketOptions::SetRawOption(nullptr, SOL_SOCKET, testOption, testData, dataSize));
    
    // Test with null buffer
    EXPECT_FALSE(SocketOptions::SetRawOption(&mockSocket, SOL_SOCKET, testOption, nullptr, dataSize));
}

TEST_F(SocketOptionsTest, GetRawOption) {
    const char* expectedData = "eth0";
    size_t bufferSize = 16;  // Large enough for the expected data
    char buffer[16] = {0};   // Initialize buffer with zeros
    
    // Use a common socket option that exists on both platforms for the test
    int testOption = SO_REUSEADDR;
    
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, testOption, NotNull(), NotNull()))
        .WillOnce(DoAll(
            WithArg<2>([expectedData](void* val) {
                // Copy the expected data to the buffer
                strcpy(static_cast<char*>(val), expectedData);
            }),
            WithArg<3>([expectedData](socklen_t* len) {
                // Set the length to include the null terminator
                *len = static_cast<socklen_t>(strlen(expectedData) + 1);
            }),
            Return(true)
        ));
    
    size_t actualSize = bufferSize;
    EXPECT_TRUE(SocketOptions::GetRawOption(&mockSocket, SOL_SOCKET, testOption, buffer, actualSize));
    EXPECT_STREQ(buffer, expectedData);
    EXPECT_EQ(actualSize, strlen(expectedData) + 1);
    
    // Test with null socket
    actualSize = bufferSize;
    EXPECT_FALSE(SocketOptions::GetRawOption(nullptr, SOL_SOCKET, testOption, buffer, actualSize));
    
    // Test with null buffer
    EXPECT_FALSE(SocketOptions::GetRawOption(&mockSocket, SOL_SOCKET, testOption, nullptr, actualSize));
    
    // Test with zero buffer size
    actualSize = 0;
    EXPECT_FALSE(SocketOptions::GetRawOption(&mockSocket, SOL_SOCKET, testOption, buffer, actualSize));
}

TEST_F(SocketOptionsTest, BindToDeviceRaw) {
    const char* interfaceName = "eth0";
    size_t nameLen = strlen(interfaceName) + 1;  // Include null terminator
    
#ifdef _WIN32
    // On Windows, this function should just return true without calling SetSocketOption
    EXPECT_CALL(mockSocket, SetSocketOption(_, _, _, _)).Times(0);
    EXPECT_TRUE(SocketOptions::BindToDeviceRaw(&mockSocket, interfaceName));
#else
    // On Unix, expect a call to SetSocketOption with SO_BINDTODEVICE
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_BINDTODEVICE, NotNull(), static_cast<socklen_t>(nameLen)))
        .WillOnce(DoAll(
            WithArg<2>([interfaceName](const void* val) {
                EXPECT_STREQ(static_cast<const char*>(val), interfaceName);
            }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::BindToDeviceRaw(&mockSocket, interfaceName));
    
    // Test with explicit size
    size_t explicitSize = 20;  // Larger than actual size
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_BINDTODEVICE, NotNull(), static_cast<socklen_t>(explicitSize)))
        .WillOnce(DoAll(
            WithArg<2>([interfaceName](const void* val) {
                EXPECT_STREQ(static_cast<const char*>(val), interfaceName);
            }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::BindToDeviceRaw(&mockSocket, interfaceName, explicitSize));
#endif
    
    // Test with null socket
    EXPECT_FALSE(SocketOptions::BindToDeviceRaw(nullptr, interfaceName));
    
    // Test with null interface name
    EXPECT_FALSE(SocketOptions::BindToDeviceRaw(&mockSocket, nullptr));
}

TEST_F(SocketOptionsTest, GetBoundDevice) {
    const char* expectedInterface = "eth0";
    size_t bufferSize = 16;  // Large enough for the expected data
    char buffer[16] = {0};   // Initialize buffer with zeros
    
#ifdef _WIN32
    // On Windows, this function should set empty string and return true
    size_t actualSize = bufferSize;
    EXPECT_TRUE(SocketOptions::GetBoundDevice(&mockSocket, buffer, actualSize));
    EXPECT_STREQ(buffer, "");
    EXPECT_EQ(actualSize, 1);  // Just the null terminator
#else
    // On Unix, expect a call to GetSocketOption with SO_BINDTODEVICE
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_BINDTODEVICE, NotNull(), NotNull()))
        .WillOnce(DoAll(
            WithArg<2>([expectedInterface](void* val) {
                // Copy the expected interface name to the buffer
                strcpy(static_cast<char*>(val), expectedInterface);
            }),
            WithArg<3>([expectedInterface](socklen_t* len) {
                // Set the length to include the null terminator
                *len = static_cast<socklen_t>(strlen(expectedInterface) + 1);
            }),
            Return(true)
        ));
    
    size_t actualSize = bufferSize;
    EXPECT_TRUE(SocketOptions::GetBoundDevice(&mockSocket, buffer, actualSize));
    EXPECT_STREQ(buffer, expectedInterface);
    EXPECT_EQ(actualSize, strlen(expectedInterface) + 1);
#endif
    
    // Test with null socket
    actualSize = bufferSize;
    EXPECT_FALSE(SocketOptions::GetBoundDevice(nullptr, buffer, actualSize));
    
    // Test with null buffer
    actualSize = bufferSize;
    EXPECT_FALSE(SocketOptions::GetBoundDevice(&mockSocket, nullptr, actualSize));
    
    // Test with zero buffer size
    actualSize = 0;
    EXPECT_FALSE(SocketOptions::GetBoundDevice(&mockSocket, buffer, actualSize));
}
