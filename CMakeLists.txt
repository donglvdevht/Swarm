cmake_minimum_required(VERSION 3.20)

project(Swarm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

add_executable(client
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp
    client/src/main.cpp
    client/src/Resources.cpp
    client/src/InputHandler.cpp
    client/src/GameObject.cpp
    client/src/Animation.h
    client/src/AnimationData.h
    client/src/GameObject.h
    client/src/Resources.h
    client/src/SDLState.h
    client/src/AnimationTypes.h
    client/src/InputHandler.h
    common/include/Protocol.h
    client/src/Network.cpp
    client/src/Network.h
    client/src/GameScene.cpp
    client/src/GameScene.h
    client/src/Scene.h
    client/src/GameConstants.h
    client/src/LoginScene.cpp
    client/src/LoginScene.h
)

target_include_directories(client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends"
)

target_compile_definitions(client PRIVATE
    SWARM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/client/src"
)

target_link_libraries(client PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    glm::glm
    nlohmann_json::nlohmann_json
)

add_executable(server
    server/src/main.cpp
    server/src/TcpServer.cpp
    server/src/TcpServer.h
    server/src/Database.h
)

target_include_directories(server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include"
)

target_compile_options(server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(server PRIVATE
    glm::glm
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
)
