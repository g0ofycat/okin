cmake_minimum_required(VERSION 3.16)
project(okin LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(okin

)

target_include_directories(okin PRIVATE
	src/opcodes
	src/lexer
	src/parser
	src/interpreter
)
