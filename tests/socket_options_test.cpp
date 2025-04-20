#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "network/socket_options.h"
#include "network/network.h" // For ISocketBase and platform defines
#include <chrono>
#include <cstring> // For memcmp

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h> // For timeval
#include <netinet/in.h> // For SOL_SOCKET etc.
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

    // Helper to handle templated SetSocketOption calls from the wrappers if needed
    template<typename T>
    bool SetSocketOption(int level, int optionName, const T& value) {
        return SetSocketOption(level, optionName, &value, sizeof(T));
    }

    // Helper to handle templated GetSocketOption calls from the wrappers if needed
    template<typename T>
    bool GetSocketOption(int level, int optionName, T& value) const {
        socklen_t len = sizeof(T);
        return GetSocketOption(level, optionName, &value, &len);
    }
};


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
        expected_tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
        expected_tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
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
    int linger_seconds = 5;
    TestLingerOption(true, linger_seconds);
    TestLingerOption(false, 0); // Seconds don't matter if off

    EXPECT_FALSE(SocketOptions::SetLinger(nullptr, true, 5));
}

TEST_F(SocketOptionsTest, SetReceiveBufferSize) {
    int size = 8192;
    TestIntOption(SocketOptions::SetReceiveBufferSize, SOL_SOCKET, SO_RCVBUF, size);
}

TEST_F(SocketOptionsTest, SetSendBufferSize) {
    int size = 8192;
    TestIntOption(SocketOptions::SetSendBufferSize, SOL_SOCKET, SO_SNDBUF, size);
}

TEST_F(SocketOptionsTest, SetReceiveTimeout) {
    auto timeout = std::chrono::milliseconds(1500); // 1.5 seconds
    TestTimeoutOption(SocketOptions::SetReceiveTimeout, SOL_SOCKET, SO_RCVTIMEO, timeout);
}

TEST_F(SocketOptionsTest, SetSendTimeout) {
    auto timeout = std::chrono::milliseconds(2345); // 2.345 seconds
    TestTimeoutOption(SocketOptions::SetSendTimeout, SOL_SOCKET, SO_SNDTIMEO, timeout);
}

TEST_F(SocketOptionsTest, SetDontRoute) {
    TestBooleanOption(SocketOptions::SetDontRoute, SOL_SOCKET, SO_DONTROUTE);
}

TEST_F(SocketOptionsTest, SetOobInline) {
    TestBooleanOption(SocketOptions::SetOobInline, SOL_SOCKET, SO_OOBINLINE);
}

TEST_F(SocketOptionsTest, SetReceiveLowWatermark) {
    int bytes = 1024;
    TestIntOption(SocketOptions::SetReceiveLowWatermark, SOL_SOCKET, SO_RCVLOWAT, bytes);
}

TEST_F(SocketOptionsTest, SetSendLowWatermark) {
    int bytes = 2048;
    TestIntOption(SocketOptions::SetSendLowWatermark, SOL_SOCKET, SO_SNDLOWAT, bytes);
}

TEST_F(SocketOptionsTest, GetError) {
    int expectedError = 5; // Example error code
    TestGetOption(SocketOptions::GetError, SOL_SOCKET, SO_ERROR, expectedError);
}

TEST_F(SocketOptionsTest, GetType) {
    int expectedType = SOCK_STREAM; // Example socket type
    TestGetOption(SocketOptions::GetType, SOL_SOCKET, SO_TYPE, expectedType);
}

TEST_F(SocketOptionsTest, GetAcceptConn) {
    // Test when socket is in listening state
    int getsockopt_value_true = 1;
    socklen_t expectedLen = sizeof(int);

    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), _))
        .WillOnce(DoAll(
            // Use a lambda to safely copy the value into the void* buffer
            WithArg<2>([getsockopt_value_true](void* val) { 
                *static_cast<int*>(val) = getsockopt_value_true; 
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
    int getsockopt_value_false = 0;
    
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), _))
        .WillOnce(DoAll(
            WithArg<2>([getsockopt_value_false](void* val) { 
                *static_cast<int*>(val) = getsockopt_value_false; 
            }),
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
    int priority = 7; // Example priority
    #ifdef _WIN32
    // On Windows, this function should just return true without calling SetSocketOption
    EXPECT_CALL(mockSocket, SetSocketOption(_, _, _, _)).Times(0);
    EXPECT_TRUE(SocketOptions::SetPriority(&mockSocket, priority));
    #else
    // On Unix, expect a call to SetSocketOption with SO_PRIORITY
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_PRIORITY, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([priority](const void* val) {
                EXPECT_EQ(*static_cast<const int*>(val), priority);
            }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetPriority(&mockSocket, priority));
    #endif

    EXPECT_FALSE(SocketOptions::SetPriority(nullptr, priority));
}
