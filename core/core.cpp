#include "../src/lexer/lexer.h"
#include "../src/parser/parser.h"
#include "../src/interpreter/interpreter.hpp"

int main(int argc, const char** argv) {
	lexer_t* lex = lexer_init(argv[1]);
	lexer_run(lex);
	// lexer_print(lex);

	parser_t* parse = parser_init(lex);
	parser_run(parse);
	// parser_print(parse);

	interpreter interp(parse);
	interp.run();

	parser_free(parse);
	lexer_free(lex);

	return 0;
}
