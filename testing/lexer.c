#include "../src/lexer/lexer.h"

int main(int argc, const char** argv) {
	lexer_t* lex = lexer_init(argv[1]);

	lexer_run(lex);
	lexer_print(lex);

	lexer_free(lex);

	return 0;
}
