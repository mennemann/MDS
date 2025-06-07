cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(MyProject main.cpp)
