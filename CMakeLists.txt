CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
ENABLE_TESTING()


PROJECT(hardware_interface_core)


# Compiler configuration
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_CXX_FLAGS "-std=c++0x -Wall")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
SET(CMAKE_CXX_FLAGS_PROFILE ${CMAKE_CXX_FLAGS_RELEASE})


# Directories
INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(/usr/local/admxrc_sdk-2.8.1/include)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)


# Sources
AUX_SOURCE_DIRECTORY(src source)
AUX_SOURCE_DIRECTORY(src/filter_core source)
ADD_EXECUTABLE(core src/core.cc ${source})


# Libraries
find_package(OpenCV REQUIRED)
TARGET_LINK_LIBRARIES(core
                      admxrc2 pthread
                      boost_program_options boost_system boost_timer
                      ${OpenCV_LIBS})

