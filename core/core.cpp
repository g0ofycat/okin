#include "../src/lexer/lexer.h"
#include "../src/parser/parser.h"
#include "../src/interpreter/interpreter.hpp"

#ifdef USE_VM
#include "../src/vm/vm.h"
#include "../src/compiler/compiler.h"
#include "../src/compiler/chunk.h"
#endif

#ifdef USE_BENCHMARK

#include <benchmark/benchmark.h>

static void BM_okin(benchmark::State &state, const char *code) {
	for (auto _ : state) {
		lexer_t* lex = lexer_init(code);

		lexer_run(lex);
		parser_t* parse = parser_init(lex);
		parser_run(parse);
		interpreter interp(parse);
		interp.run();
		parser_free(parse);
		lexer_free(lex);
	}
}

#ifdef USE_VM
static void BM_okin_vm(benchmark::State &state, const char *code) {
	for (auto _ : state) {
		lexer_t* lex = lexer_init(code);
		lexer_run(lex);
		parser_t* parse = parser_init(lex);
		parser_run(parse);
		compiler_t* c = compiler_init(parse);
		compiler_run(c);
		vm_t* vm = vm_init(c->root);
		vm_run(vm);
		vm_free(vm);
		compiler_free(c);
		parser_free(parse);
		lexer_free(lex);
	}
}
#endif

int main(int argc, char** argv) {
	benchmark::MaybeReenterWithoutASLR(argc, argv);
	benchmark::Initialize(&argc, argv);
	if (argc < 2) return 1;

	bool use_vm = false;
	const char *src = nullptr;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-vm") == 0) use_vm = true;
		else src = argv[i];
	}

	if (!src) return 1;


#ifdef USE_VM
	if (use_vm)
		benchmark::RegisterBenchmark("BM_okin_vm", BM_okin_vm, src);
	else
#endif
		benchmark::RegisterBenchmark("BM_okin", BM_okin, src);

	benchmark::RunSpecifiedBenchmarks();
	return 0;
}

#else

int main(int argc, char** argv) {
	if (argc < 2) return 1;

	bool use_vm = false;
	const char *src = nullptr;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-vm") == 0) use_vm = true;
		else src = argv[i];
	}

	if (!src) return 1;

	lexer_t* lex = lexer_init(src);
	lexer_run(lex);
	parser_t* parse = parser_init(lex);
	parser_run(parse);

#ifdef USE_VM
	if (use_vm) {
		compiler_t* c = compiler_init(parse);
		compiler_run(c);
		vm_t* vm = vm_init(c->root);
		vm_run(vm);
		vm_free(vm);
		compiler_free(c);
	} else
#endif
	{
		interpreter interp(parse);
		interp.run();
	}

	parser_free(parse);
	lexer_free(lex);

	return 0;
}

#endif
