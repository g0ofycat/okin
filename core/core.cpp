#include "../src/lexer/lexer.h"
#include "../src/parser/parser.h"
#include "../src/interpreter/interpreter.hpp"

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

int main(int argc, char** argv) {
	benchmark::Initialize(&argc, argv);
	if (argc > 1) {
		benchmark::RegisterBenchmark("BM_okin", BM_okin, argv[1]);
	} else {
		return 1;
	}

	benchmark::RunSpecifiedBenchmarks();

	return 0;
}

#else

int main(int argc, char** argv) {
	if (argc < 2) return 1;

	lexer_t* lex = lexer_init(argv[1]);
	lexer_run(lex);

	parser_t* parse = parser_init(lex);
	parser_run(parse);

	interpreter interp(parse);
	interp.run();

	parser_free(parse);
	lexer_free(lex);

	return 0;
}

#endif
