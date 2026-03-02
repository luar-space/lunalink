cmake_minimum_required(VERSION 3.15)
project(lsis_afs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_example cpp/example.cpp)
install(TARGETS _example DESTINATION lsis_afs)
