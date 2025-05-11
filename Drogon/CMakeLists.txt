cmake_minimum_required(VERSION 3.10)
project(Drogon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Drogon REQUIRED)
add_executable(main main.cpp)
target_link_libraries(main PRIVATE Drogon::Drogon)