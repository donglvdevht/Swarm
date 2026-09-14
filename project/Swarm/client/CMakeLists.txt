cmake_minimum_required(VERSION 3.20)
project(MyClient LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(nlohmann_json CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

add_executable(client "src/main.cpp" "src/Resources.cpp" "src/InputHandler.cpp" "src/GameObject.cpp" "src/Animation.h" "src/AnimationData.h" "src/GameObject.h"
                "src/Resources.h" "src/SDLState.h" "src/AnimationTypes.h" "src/InputHandler.h"
                "../common/include/Protocol.h")

target_include_directories(client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
)

target_compile_definitions(client PRIVATE
    SWARM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(client PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    glm::glm
    nlohmann_json::nlohmann_json
)
