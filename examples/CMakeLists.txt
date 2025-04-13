cmake_minimum_required(VERSION 3.10)

# Add individual examples as separate executables
add_executable(example_tcp_client tcp_client.cpp)
target_link_libraries(example_tcp_client PRIVATE network)

add_executable(example_tcp_server tcp_server.cpp)
target_link_libraries(example_tcp_server PRIVATE network)

add_executable(example_udp_sender udp_sender.cpp)
target_link_libraries(example_udp_sender PRIVATE network)

add_executable(example_udp_receiver udp_receiver.cpp)
target_link_libraries(example_udp_receiver PRIVATE network)

add_executable(example_udp_multicast udp_multicast.cpp)
target_link_libraries(example_udp_multicast PRIVATE network)

add_executable(example_udp_broadcast udp_broadcast.cpp)
target_link_libraries(example_udp_broadcast PRIVATE network)