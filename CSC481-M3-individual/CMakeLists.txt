cmake_minimum_required(VERSION 3.10)
project(CSC481M3Individual)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

add_executable(
    game
    src/Game.cpp
    src/Entity.cpp
    src/Physics.cpp
    src/Input.cpp
    src/Collision.cpp
    src/Scaling.cpp
    src/Timeline.cpp
)

target_link_libraries(game PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

# Headless tests of the game rules (no window needed): ./logic_test and ./bot
foreach(t logic_test bot)
    add_executable(${t} tests/${t}.cpp src/Physics.cpp src/Collision.cpp src/Entity.cpp src/Scaling.cpp)
    target_include_directories(${t} PRIVATE src)
    target_link_libraries(${t} PRIVATE SDL3::SDL3)
endforeach()
