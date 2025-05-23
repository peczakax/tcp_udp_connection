set(SOURCE_FILES
    platform_factory.cpp
    socket_options.cpp    
)

# Add platform-specific sources
if(WIN32)
    add_subdirectory(windows)
    list(APPEND SOURCE_FILES
        windows/windows_sockets.cpp
    )
else()
    add_subdirectory(unix)
    list(APPEND SOURCE_FILES
        unix/unix_sockets.cpp
    )
    # Add macOS-specific sources when on macOS
    if(APPLE)
        list(APPEND SOURCE_FILES
            unix/common_socket_helpers.cpp
            unix/macos_socket_helpers.cpp
        )
    elseif(UNIX AND NOT APPLE)
        list(APPEND SOURCE_FILES
            unix/common_socket_helpers.cpp
            unix/unix_socket_helpers.cpp
        )
    endif()
endif()

add_library(network STATIC ${SOURCE_FILES})

target_include_directories(network PUBLIC ${CMAKE_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(network PRIVATE ws2_32)
endif()

# Install targets
install(TARGETS network
        EXPORT network-config
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)