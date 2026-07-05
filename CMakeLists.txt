cmake_minimum_required(VERSION 3.20)
project(UltraLowLatencyLOB CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC-specific ultra-low latency optimization flags
add_compile_options(
    -O3 
    -march=native     # Optimizes for your specific CPU architecture (AVX2/AVX-512)
    -flto             # Link-Time Optimization for cross-file inlining
    -fno-rtti         # Disable Run-Time Type Information (saves overhead)
    -Wall -Wextra -Wpedantic -Werror # Strict warnings
)

#source files
set(SOURCES 
    src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} ws2_32)
