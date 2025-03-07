cmake_minimum_required(VERSION 3.30)
project(assembler)

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

add_executable(assembler main.cpp src/Lexer.cpp)
