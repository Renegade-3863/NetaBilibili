cmake_minimum_required(VERSION 3.10)
project(ReactorHttp LANGUAGES C CXX)

# Use a single C standard across the project
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++ standard for any C++ sources
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Centralized compile flags
add_compile_options(-Wall -g -O0)

# Make GNU extensions available project-wide (helps avoid implicit declaration warnings on Linux)
add_compile_definitions(_GNU_SOURCE)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(${PROJECT_SOURCE_DIR}/base 
    ${PROJECT_SOURCE_DIR}/tcp 
    ${PROJECT_SOURCE_DIR}/http 
    ${PROJECT_SOURCE_DIR}/webSocket
)

add_subdirectory(base)
add_subdirectory(tcp)
add_subdirectory(http)
add_subdirectory(webSocket)

set(SRC_FILES main.c)

add_executable(ReactorServer ${SRC_FILES})
if (TARGET OpenSSL::SSL)
    # Link in dependency order: libraries that depend on others should come first.
    target_link_libraries(ReactorServer PRIVATE ReactorHttpTcp ReactorHttpBase ReactorHttpHttp ReactorHttpWebSocket OpenSSL::SSL OpenSSL::Crypto)
else()
    target_link_libraries(ReactorServer PRIVATE ReactorHttpTcp ReactorHttpBase ReactorHttpHttp ReactorHttpWebSocket ${OPENSSL_LIBRARIES})
endif()
target_compile_features(ReactorServer PRIVATE cxx_std_17)