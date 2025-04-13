# App CMakeLists.txt

# TCP Live Chat Applications
add_executable(tcp_live_chat_server tcp_live_chat_server.cpp)
target_link_libraries(tcp_live_chat_server network)

add_executable(tcp_live_chat_client tcp_live_chat_client.cpp)
target_link_libraries(tcp_live_chat_client network)

# UDP Live Chat Applications
add_executable(udp_live_chat_server udp_live_chat_server.cpp)
target_link_libraries(udp_live_chat_server network)

add_executable(udp_live_chat_client udp_live_chat_client.cpp)
target_link_libraries(udp_live_chat_client network)
