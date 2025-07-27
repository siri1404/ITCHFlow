cmake_minimum_required(VERSION 3.12)
project(TickShaper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -fsanitize=address")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Optional: Find Google Test for unit tests
find_package(GTest QUIET)

# Include directories
include_directories(include)
include_directories(${ZMQ_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/TickShaper.cpp
    src/ITCHParser.cpp
    src/MessageProcessor.cpp
    src/ZMQPublisher.cpp
    src/SharedMemoryManager.cpp
    src/MicroburstDetector.cpp
    src/ThrottleController.cpp
)

# Create main executable
add_executable(tickshaper ${SOURCES} src/main.cpp)

# Create sample data generator
add_executable(create_sample data/create_sample.cpp)

# Link libraries
target_link_libraries(tickshaper 
    ${ZMQ_LIBRARIES}
    pthread
    rt
)

# Try to build test client if jsoncpp is available
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSONCPP jsoncpp)
    if(JSONCPP_FOUND)
        add_executable(test_client test_client.cpp)
        target_link_libraries(test_client ${ZMQ_LIBRARIES} ${JSONCPP_LIBRARIES} pthread)
        target_include_directories(test_client PRIVATE ${JSONCPP_INCLUDE_DIRS})
    endif()
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(tickshaper PRIVATE 
        -Wall -Wextra -Wpedantic
        -ffast-math
        -funroll-loops
        -finline-functions
        -fno-omit-frame-pointer
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(tickshaper PRIVATE 
        -Wall -Wextra -Wpedantic
        -ffast-math
        -funroll-loops
        -finline-functions
    )
endif()

# Enable Link Time Optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET tickshaper PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Unit tests (if Google Test is available)
if(GTest_FOUND)
    enable_testing()
    
    add_executable(tickshaper_tests ${SOURCES} test/test_tickshaper.cpp)
    target_link_libraries(tickshaper_tests 
        ${ZMQ_LIBRARIES}
        pthread
        rt
        GTest::gtest
        GTest::gtest_main
    )
    
    add_test(NAME TickShaperUnitTests COMMAND tickshaper_tests)
    
    # Performance test executable
    add_executable(performance_test test/performance_test.cpp ${SOURCES})
    target_link_libraries(performance_test 
        ${ZMQ_LIBRARIES}
        pthread
        rt
    )
endif()

# Installation
install(TARGETS tickshaper DESTINATION bin)
install(TARGETS create_sample DESTINATION bin)
install(FILES config/tickshaper.conf DESTINATION etc/tickshaper)
install(DIRECTORY DESTINATION var/log/tickshaper)

# Package configuration
set(CPACK_PACKAGE_NAME "TickShaper")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Real-Time Market Data Throttler")
set(CPACK_PACKAGE_VENDOR "TickShaper Development Team")
set(CPACK_GENERATOR "TGZ;DEB;RPM")

# Debian package specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libzmq5")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "tickshaper@example.com")

# RPM package specific  
set(CPACK_RPM_PACKAGE_REQUIRES "zeromq")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")

include(CPack)

# Custom targets
add_custom_target(format
    COMMAND find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting source code"
)

add_custom_target(docs
    COMMAND doxygen Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating documentation"
)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "ZeroMQ version: ${ZMQ_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")