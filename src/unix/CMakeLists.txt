set(UNIX_SOURCES
    unix_sockets.cpp
)

# Add macOS-specific sources if on macOS
if(APPLE)
    list(APPEND UNIX_SOURCES
        macos_sockets.cpp
    )
endif()

# Add header files for IDEs
set(UNIX_HEADERS
    unix_sockets.h
)

# Add macOS-specific headers if on macOS
if(APPLE)
    list(APPEND UNIX_HEADERS
        macos_sockets.h
    )
endif()