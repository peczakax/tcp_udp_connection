# Platform-specific implementations
if(WIN32)
    set(PLATFORM_SOURCES
        windows/windows_sockets.cpp
    )
else()
    set(PLATFORM_SOURCES
        unix/unix_sockets.cpp
    )
endif()

# Create the library with all sources
add_library(network
    platform_factory.cpp
    ${PLATFORM_SOURCES}
)

# Include directories
target_include_directories(network 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific dependencies
if(WIN32)
    target_link_libraries(network PRIVATE ws2_32)
endif()

# Install targets
install(TARGETS network
        EXPORT network-config
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)