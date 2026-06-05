cmake_minimum_required(VERSION 3.16)
project(okin LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(USE_VM "Enable VM backend" OFF)

find_package(benchmark CONFIG REQUIRED HINTS "/usr/local")

set(OKIN_SOURCES
	src/lexer/lexer.c
	src/parser/parser.c
	src/util/arena.c
	src/interpreter/interpreter.cpp
	src/interpreter/interpreter_opcodes.cpp
)

set(OKIN_INCLUDES
	src/opcodes
	src/lexer
	src/parser
	src/util
	src/interpreter
)

if(USE_VM)
	list(APPEND OKIN_SOURCES
		src/compiler/compiler.c
		src/compiler/chunk.c
		src/compiler/scope.c
		src/vm/vm.c
		src/vm/value.c
	)

	list(APPEND OKIN_INCLUDES
		src/compiler
		src/vm
	)
endif()

add_library(okin_lib STATIC ${OKIN_SOURCES})
target_include_directories(okin_lib PUBLIC ${OKIN_INCLUDES})

if(USE_VM)
	target_compile_definitions(okin_lib PUBLIC USE_VM)
endif()

add_executable(okin core/core.cpp)
target_compile_options(okin PRIVATE -O3 -march=native -funroll-loops -fomit-frame-pointer -flto)
target_link_options(okin PRIVATE -flto)

target_link_libraries(okin PRIVATE okin_lib)

add_executable(okin_bench core/core.cpp)
target_compile_options(okin_bench PRIVATE -O3 -march=native -funroll-loops -fomit-frame-pointer -flto)
target_link_options(okin_bench PRIVATE -flto -static-libgcc -static-libstdc++)

target_compile_definitions(okin_bench PRIVATE USE_BENCHMARK BENCHMARK_STATIC_DEFINE)
target_link_libraries(okin_bench PRIVATE okin_lib benchmark::benchmark pthread)
target_link_options(okin_bench PRIVATE -static-libgcc -static-libstdc++)
