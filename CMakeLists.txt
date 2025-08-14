cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "/Users/jiechengheren/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(NetaBilibili)

set(CMAKE_CXX_STANDARD 17)

# 启用测试模块
enable_testing()

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

set(SRC_FILES 
    src/main.cpp
    src/middleware/JwtFilter.h)

add_executable(netaBilibili ${SRC_FILES})

target_link_libraries(netaBilibili PRIVATE
    Drogon::Drogon
    OpenSSL::SSL
)

