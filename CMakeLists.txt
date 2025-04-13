cmake_minimum_required(VERSION 3.14)
project(network VERSION 0.1.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_APPS "Build application files" ON)

# Add the library
add_subdirectory(src)

# Add examples if requested
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Add applications if requested 
if(BUILD_APPS)
    add_subdirectory(app)
endif()

# Set up testing with Google Test
if(BUILD_TESTS)
    # First try to find an existing GTest installation
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, fetching from source")
        # Fetch GoogleTest
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0  # Use the latest stable release
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        
        # Make GMock available when fetching GTest
        add_library(GTest::GMock ALIAS gmock)
        add_library(GTest::GMockMain ALIAS gmock_main)
    else()
        message(STATUS "Using system-installed GTest")
    endif()
    
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation settings
install(DIRECTORY include/network
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")