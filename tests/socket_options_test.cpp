\
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
};

// --- Test Cases ---

TEST_F(SocketOptionsTest, SetReuseAddr) {
    // Test enabling
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEADDR, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            // Check that the value passed is 1 (true)
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetReuseAddr(&mockSocket, true));

    // Test disabling
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEADDR, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            // Check that the value passed is 0 (false)
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetReuseAddr(&mockSocket, false));

    // Test null socket
    EXPECT_FALSE(SocketOptions::SetReuseAddr(nullptr, true));
}

TEST_F(SocketOptionsTest, SetReusePort) {
    // Test enabling
    #ifdef _WIN32 // On Windows, SO_REUSEPORT maps to SO_REUSEADDR
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEADDR, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    #else
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEPORT, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetReusePort(&mockSocket, true));

    // Test disabling
    #ifdef _WIN32
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEADDR, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    #else
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_REUSEPORT, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetReusePort(&mockSocket, false));

    // Test null socket
    EXPECT_FALSE(SocketOptions::SetReusePort(nullptr, true));
}

TEST_F(SocketOptionsTest, SetBroadcast) {
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_BROADCAST, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetBroadcast(&mockSocket, true));

    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_BROADCAST, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetBroadcast(&mockSocket, false));

    EXPECT_FALSE(SocketOptions::SetBroadcast(nullptr, true));
}

TEST_F(SocketOptionsTest, SetKeepAlive) {
     EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_KEEPALIVE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetKeepAlive(&mockSocket, true));

    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_KEEPALIVE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetKeepAlive(&mockSocket, false));

    EXPECT_FALSE(SocketOptions::SetKeepAlive(nullptr, true));
}

TEST_F(SocketOptionsTest, SetLinger) {
    int linger_seconds = 5;
    #ifdef _WIN32
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(LINGER)))
        .WillOnce(DoAll(
            WithArg<2>([linger_seconds](const void* val) {
                const LINGER* lingerOpt = static_cast<const LINGER*>(val);
                EXPECT_EQ(lingerOpt->l_onoff, 1);
                EXPECT_EQ(lingerOpt->l_linger, linger_seconds);
            }),
            Return(true)
        ));
    #else
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(struct linger)))
        .WillOnce(DoAll(
            WithArg<2>([linger_seconds](const void* val) {
                const struct linger* lingerOpt = static_cast<const struct linger*>(val);
                EXPECT_EQ(lingerOpt->l_onoff, 1);
                EXPECT_EQ(lingerOpt->l_linger, linger_seconds);
            }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetLinger(&mockSocket, true, linger_seconds));

    #ifdef _WIN32
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(LINGER)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) {
                const LINGER* lingerOpt = static_cast<const LINGER*>(val);
                EXPECT_EQ(lingerOpt->l_onoff, 0);
                // l_linger value doesn't matter if l_onoff is 0, but check it anyway (should be passed as 0)
                EXPECT_EQ(lingerOpt->l_linger, 0);
            }),
            Return(true)
        ));
    #else
     EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_LINGER, NotNull(), sizeof(struct linger)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) {
                const struct linger* lingerOpt = static_cast<const struct linger*>(val);
                EXPECT_EQ(lingerOpt->l_onoff, 0);
                 // l_linger value doesn't matter if l_onoff is 0, but check it anyway (should be passed as 0)
                EXPECT_EQ(lingerOpt->l_linger, 0);
            }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetLinger(&mockSocket, false, 0)); // Seconds don't matter if off

    EXPECT_FALSE(SocketOptions::SetLinger(nullptr, true, 5));
}

TEST_F(SocketOptionsTest, SetReceiveBufferSize) {
    int size = 8192;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_RCVBUF, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([size](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), size); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetReceiveBufferSize(&mockSocket, size));

    EXPECT_FALSE(SocketOptions::SetReceiveBufferSize(nullptr, size));
}

TEST_F(SocketOptionsTest, SetSendBufferSize) {
    int size = 8192;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_SNDBUF, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([size](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), size); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetSendBufferSize(&mockSocket, size));

    EXPECT_FALSE(SocketOptions::SetSendBufferSize(nullptr, size));
}

TEST_F(SocketOptionsTest, SetReceiveTimeout) {
    auto timeout = std::chrono::milliseconds(1500); // 1.5 seconds
    #ifdef _WIN32
    DWORD expected_timeout_ms = 1500;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_RCVTIMEO, NotNull(), sizeof(DWORD)))
        .WillOnce(DoAll(
            WithArg<2>([expected_timeout_ms](const void* val) {
                EXPECT_EQ(*static_cast<const DWORD*>(val), expected_timeout_ms);
            }),
            Return(true)
        ));
    #else
    struct timeval expected_tv;
    expected_tv.tv_sec = 1;
    expected_tv.tv_usec = 500000;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_RCVTIMEO, NotNull(), sizeof(struct timeval)))
        .WillOnce(DoAll(
            WithArg<2>([expected_tv](const void* val) {
                const struct timeval* tv = static_cast<const struct timeval*>(val);
                EXPECT_EQ(tv->tv_sec, expected_tv.tv_sec);
                EXPECT_EQ(tv->tv_usec, expected_tv.tv_usec);
            }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetReceiveTimeout(&mockSocket, timeout));

    EXPECT_FALSE(SocketOptions::SetReceiveTimeout(nullptr, timeout));
}

TEST_F(SocketOptionsTest, SetSendTimeout) {
    auto timeout = std::chrono::milliseconds(2345); // 2.345 seconds
    #ifdef _WIN32
    DWORD expected_timeout_ms = 2345;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_SNDTIMEO, NotNull(), sizeof(DWORD)))
        .WillOnce(DoAll(
            WithArg<2>([expected_timeout_ms](const void* val) {
                EXPECT_EQ(*static_cast<const DWORD*>(val), expected_timeout_ms);
            }),
            Return(true)
        ));
    #else
    struct timeval expected_tv;
    expected_tv.tv_sec = 2;
    expected_tv.tv_usec = 345000;
     EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_SNDTIMEO, NotNull(), sizeof(struct timeval)))
        .WillOnce(DoAll(
            WithArg<2>([expected_tv](const void* val) {
                const struct timeval* tv = static_cast<const struct timeval*>(val);
                EXPECT_EQ(tv->tv_sec, expected_tv.tv_sec);
                EXPECT_EQ(tv->tv_usec, expected_tv.tv_usec);
            }),
            Return(true)
        ));
    #endif
    EXPECT_TRUE(SocketOptions::SetSendTimeout(&mockSocket, timeout));

    EXPECT_FALSE(SocketOptions::SetSendTimeout(nullptr, timeout));
}

TEST_F(SocketOptionsTest, SetDontRoute) {
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_DONTROUTE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetDontRoute(&mockSocket, true));

    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_DONTROUTE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetDontRoute(&mockSocket, false));

    EXPECT_FALSE(SocketOptions::SetDontRoute(nullptr, true));
}

TEST_F(SocketOptionsTest, SetOobInline) {
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_OOBINLINE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 1); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetOobInline(&mockSocket, true));

    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_OOBINLINE, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), 0); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetOobInline(&mockSocket, false));

    EXPECT_FALSE(SocketOptions::SetOobInline(nullptr, true));
}

TEST_F(SocketOptionsTest, SetReceiveLowWatermark) {
    int bytes = 1024;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_RCVLOWAT, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([bytes](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), bytes); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetReceiveLowWatermark(&mockSocket, bytes));

    EXPECT_FALSE(SocketOptions::SetReceiveLowWatermark(nullptr, bytes));
}

TEST_F(SocketOptionsTest, SetSendLowWatermark) {
    int bytes = 2048;
    EXPECT_CALL(mockSocket, SetSocketOption(SOL_SOCKET, SO_SNDLOWAT, NotNull(), sizeof(int)))
        .WillOnce(DoAll(
            WithArg<2>([bytes](const void* val) { EXPECT_EQ(*static_cast<const int*>(val), bytes); }),
            Return(true)
        ));
    EXPECT_TRUE(SocketOptions::SetSendLowWatermark(&mockSocket, bytes));

    EXPECT_FALSE(SocketOptions::SetSendLowWatermark(nullptr, bytes));
}

TEST_F(SocketOptionsTest, GetError) {
    int expectedError = 5; // Example error code
    socklen_t expectedLen = sizeof(int);

    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ERROR, NotNull(), Pointee(expectedLen)))
        .WillOnce(DoAll(
            // Use a lambda to safely copy the value into the void* buffer
            WithArg<2>([expectedError](void* val) { 
                *static_cast<int*>(val) = expectedError; 
            }),
            // Use a lambda to safely set the length
            WithArg<3>([expectedLen](socklen_t* len) {
                *len = expectedLen;
            }),
            Return(true)
        ));

    int actualError = 0;
    EXPECT_TRUE(SocketOptions::GetError(&mockSocket, actualError));
    EXPECT_EQ(actualError, expectedError);

    EXPECT_FALSE(SocketOptions::GetError(nullptr, actualError));
}

TEST_F(SocketOptionsTest, GetType) {
    int expectedType = SOCK_STREAM; // Example socket type
    socklen_t expectedLen = sizeof(int);

    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_TYPE, NotNull(), Pointee(expectedLen)))
        .WillOnce(DoAll(
            // Use a lambda to safely copy the value into the void* buffer
            WithArg<2>([expectedType](void* val) { 
                *static_cast<int*>(val) = expectedType; 
            }),
             // Use a lambda to safely set the length
            WithArg<3>([expectedLen](socklen_t* len) {
                *len = expectedLen;
            }),
            Return(true)
        ));

    int actualType = 0;
    EXPECT_TRUE(SocketOptions::GetType(&mockSocket, actualType));
    EXPECT_EQ(actualType, expectedType);

    EXPECT_FALSE(SocketOptions::GetType(nullptr, actualType));
}

TEST_F(SocketOptionsTest, GetAcceptConn) {
    int getsockopt_value_true = 1;
    int getsockopt_value_false = 0;
    socklen_t expectedLen = sizeof(int);

    // Test when listening (true)
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), Pointee(expectedLen)))
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

    // Test when not listening (false)
    EXPECT_CALL(mockSocket, GetSocketOption(SOL_SOCKET, SO_ACCEPTCONN, NotNull(), Pointee(expectedLen)))
        .WillOnce(DoAll(
             // Use a lambda to safely copy the value into the void* buffer
            WithArg<2>([getsockopt_value_false](void* val) { 
                *static_cast<int*>(val) = getsockopt_value_false; 
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
