#include "../src/preprocessor/okin_config.h"
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
		if (!c->errors) {
			vm_t* vm = vm_init(c->root);
			vm_run(vm);
			vm_free(vm);
		}
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
	const char *config_arg = nullptr;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-vm") == 0) use_vm = true;
		else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) config_arg = argv[++i];
		else src = argv[i];
	}

	if (!src) return 1;

	okin_config_t cfg;
	bool has_config = config_arg ? okin_config_load(&cfg, config_arg) : okin_config_load_cache(&cfg);
	char *expanded = has_config ? okin_config_expand(&cfg, src) : nullptr;
	const char *final_src = expanded ? expanded : src;

#ifdef USE_VM
	if (use_vm)
		benchmark::RegisterBenchmark("BM_okin_vm", BM_okin_vm, final_src);
	else
#endif
		benchmark::RegisterBenchmark("BM_okin", BM_okin, final_src);

	benchmark::RunSpecifiedBenchmarks();
	free(expanded);
	return 0;
}

#else

int main(int argc, char** argv) {
	if (argv[1] && strcmp(argv[1], "-help") == 0) {
		printf("Usage: okin <source_code> [flags...]\n\nFlag list:\n\n-d : Enable print debugging based on the source code.\n-vm: Enable the Compiler and OVM (Okin Virtual Machine) pipeline to convert to true bytecode instead of interpreting, allowing for much faster code execution depending on the task.\n--config <file|toml>: Load opcode mappings for compact <> syntax; cached in .okin_config_cache for future runs.\n");
		return 0;
	}
	if (argc < 2) {
		printf("Okin: An LLM based programming language. (main - June 10th, 2026; v.1.0.0-ALPHA)\nType \"-help\" for more information.\n");
		return 0;
	}

	bool use_vm = false;
	bool debug = false;

	const char *src = nullptr;
	const char *config_arg = nullptr;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-vm") == 0)          use_vm = true;
		else if (strcmp(argv[i], "-d") == 0)      debug  = true;
		else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) config_arg = argv[++i];
		else src = argv[i];
	}

	if (!src) return 1;

	okin_config_t cfg;
	bool has_config = config_arg ? okin_config_load(&cfg, config_arg) : okin_config_load_cache(&cfg);
	char *expanded = has_config ? okin_config_expand(&cfg, src) : nullptr;
	const char *final_src = expanded ? expanded : src;

	lexer_t* lex = lexer_init(final_src);
	lexer_run(lex);
	if (debug) lexer_print(lex);
	parser_t* parse = parser_init(lex);
	parser_run(parse);
	if (debug) parser_print(parse);

#ifdef USE_VM
	if (use_vm) {
		compiler_t* c = compiler_init(parse);
		compiler_run(c);
		if (debug) chunk_print(c->root);
		if (c->errors) {
			compiler_free(c);
			parser_free(parse);
			lexer_free(lex);
			free(expanded);
			return 1;
		}
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
	free(expanded);
	return 0;
}

#endif
