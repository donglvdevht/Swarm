cmake_minimum_required(VERSION 3.20)

project(SwarmServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

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

target_link_libraries(server PRIVATE
    glm::glm
)
