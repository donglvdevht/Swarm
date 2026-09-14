cmake_minimum_required(VERSION 3.20)

project(SwarmServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(server
    main.cpp
)

target_include_directories(server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
)

target_compile_options(server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
