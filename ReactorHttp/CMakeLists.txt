cmake_minimum_required(VERSION 3.10)
project(ReactorHttp LANGUAGES C CXX)

# C standard
set(CMAKE_C_STANDARD 99)

# Enable C++17 for any C++ sources and require it (no compiler extensions).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols by default and keep warnings on.
# You can still override the build type by passing -DCMAKE_BUILD_TYPE=Release to cmake.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Basic flags: warnings and debug symbols. Use -O0 to avoid optimizations that hinder debugging.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

# Mirror debug/warning flags for C++ compilation as well.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRC_FILES
    Channel.c
    HttpResponse.c
    EventLoop.c
    Buffer.c
    main.c  
    EpollDispatcher.c      
    ChannelMap.c           
    TcpServer.c         
    TcpConnection.c       
    ThreadPool.c      
    WebSocket.c            
    HttpRequest.c          
    WorkerThread.c         
)

add_executable(ReactorServer ${SRC_FILES})
target_link_libraries(ReactorServer ${OPENSSL_LIBRARIES})
# Make sure the target will compile with C++17 for any C++ sources.
target_compile_features(ReactorServer PRIVATE cxx_std_17)
# macOS不需要额外链接socket库，默认支持