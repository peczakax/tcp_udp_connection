set(UNIX_SOURCES
    unix_sockets.cpp
)

# Add macOS-specific sources if on macOS
if(APPLE)
    list(APPEND UNIX_SOURCES
        macos_socket_helpers.cpp
        common_socket_helpers.cpp
    )
elseif(UNIX AND NOT APPLE)    
    list(APPEND UNIX_SOURCES
        common_socket_helpers.cpp
    )
endif()