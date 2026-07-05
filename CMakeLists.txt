cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-O3 -march=native -flto -Wall -Wextra)

add_executable(lob src/main.cpp)

target_include_directories(lob PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lob ws2_32)

target_link_options(lob PRIVATE -flto)
