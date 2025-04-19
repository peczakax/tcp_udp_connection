# Check if GTest was fetched or found via find_package
if(TARGET GTest::GTest)
  # GTest was found via find_package
  message(STATUS "Using system-installed GTest in tests")
else()
  # GTest was fetched with FetchContent
  message(STATUS "Using fetched GTest in tests")
  # We don't need find_package here as main CMakeLists already fetched GTest
endif()

# Add test executable
add_executable(
  network_tests
  tcp_socket_test.cpp
  tcp_timeout_test.cpp 
  udp_socket_test.cpp
  udp_timeout_test.cpp 
  client_server_connection_test.cpp
)

# Link against GoogleTest/GoogleMock and our library
target_link_libraries(
  network_tests
  PRIVATE
  GTest::gtest_main
  GTest::gmock_main
  network
)

# Include directories
target_include_directories(network_tests 
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(network_tests)