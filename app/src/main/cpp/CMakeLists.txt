cmake_minimum_required(VERSION 3.22.1)
project(aimodeling LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aimodeling SHARED
    native-lib.cpp
    core/mesh.cpp
    core/mesh_ops.cpp
    core/scene.cpp
    core/editor.cpp)

target_include_directories(aimodeling PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)
target_link_libraries(aimodeling GLESv3 ${log-lib})
