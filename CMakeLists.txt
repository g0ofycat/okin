cmake_minimum_required(VERSION 3.16)
project(okin LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(benchmark CONFIG REQUIRED)

add_library(okin_lib STATIC
	src/lexer/lexer.c
	src/parser/parser.c
	src/parser/util/arena.c
	src/interpreter/interpreter.cpp
	src/interpreter/interpreter_opcodes.cpp
)

target_include_directories(okin_lib PUBLIC
	src/opcodes
	src/lexer
	src/parser
	src/parser/util
	src/interpreter
)

add_executable(okin core/core.cpp)
target_link_libraries(okin PRIVATE okin_lib shlwapi)

add_executable(okin_bench core/core.cpp)
target_compile_definitions(okin_bench PRIVATE USE_BENCHMARK BENCHMARK_STATIC_DEFINE)
target_link_libraries(okin_bench PRIVATE okin_lib benchmark::benchmark shlwapi)
target_link_options(okin_bench PRIVATE -static-libgcc -static-libstdc++)
