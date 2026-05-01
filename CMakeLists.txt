cmake_minimum_required(VERSION 3.16)
project(okin LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(okin
	src/lexer/lexer.c
	src/parser/parser.c
	src/parser/util/arena.c
	src/interpreter/interpreter.cpp

	testing/core.c
)

target_include_directories(okin PRIVATE
	src/opcodes
	src/lexer
	src/parser
	src/parser/util
	src/interpreter
)
