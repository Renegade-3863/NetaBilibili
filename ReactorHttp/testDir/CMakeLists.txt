cmake_minimum_required(VERSION 3.10)
project(ReactorHttp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

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