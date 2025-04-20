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