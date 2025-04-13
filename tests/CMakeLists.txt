# Add test executable
add_executable(
  network_tests
  tcp_socket_test.cpp
  udp_socket_test.cpp
)

# Link against GoogleTest and our library
target_link_libraries(
  network_tests
  PRIVATE
  gtest
  gtest_main
  gmock
  gmock_main
  network_lib
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