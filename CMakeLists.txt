cmake_minimum_required(VERSION 3.29)
project(untitled3 C)

add_executable(untitled3 nbt2dict.c)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
target_include_directories(untitled3 PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(untitled3 PRIVATE ${Python3_LIBRARIES})

set(CMAKE_C_STANDARD 23)

