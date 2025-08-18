cmake_minimum_required(VERSION 3.10)
project(ReactorHttp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD 99)

# Enable debug symbols by default and keep warnings on.
# You can still override the build type by passing -DCMAKE_BUILD_TYPE=Release to cmake.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Basic flags: warnings and debug symbols. Use -O0 to avoid optimizations that hinder debugging.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRC_FILES
    Channel.c
    SelectDispatcher.c
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
    PollDispatcher.c
    SelectDispatcher.c        
)

add_executable(ReactorServer ${SRC_FILES})
target_link_libraries(ReactorServer ${OPENSSL_LIBRARIES})
# macOS不需要额外链接socket库，默认支持