#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"

// ======================
// -- DATA
// ======================

static const char SINGLE_CHAR_TOKENS[] = "<>|,;";

static const token_type_t SINGLE_CHAR_TYPES[] = {
	TK_ARG_OPEN,
	TK_ARG_CLOSE,
	TK_PIPE,
	TK_COMMA,
	TK_SEMI
};

static const char *TOKEN_TYPE_NAMES[] = {
	"OPCODE",
	"LIB_CALL",
	"ARG_OPEN",
	"ARG_CLOSE",
	"PIPE",
	"COMMA",
	"SEMI",
	"VALUE",
	"STRING",
	"INT",
	"INT_LIT",
	"FLOAT_LIT",
	"EOF",
	"ERROR"
};

static const char STR_DELIM[] = "\"'";

// ======================
// -- PRIVATE
// ======================

/// @brief Peek current character without advancing
/// @param l
/// @return char
static char peek(const lexer_t *l) { return l->src[l->pos]; }

/// @brief Advance and return current character, tracking line/col
/// @param l
/// @return char
static char advance(lexer_t *l)
{
	char c = l->src[l->pos++];
	c == '\n' ? (l->line++, l->col = 1) : l->col++;
	return c;
}

/// @brief Push a token onto the token list, growing if needed
/// @param l
/// @param type
/// @param start
/// @param len
/// @param col
static void push(lexer_t *l, token_type_t type, const char *start, size_t len, int line, int col)
{
	if (l->count == l->capacity)
	{
		l->capacity *= 2;
		l->tokens = realloc(l->tokens, l->capacity * sizeof(token_t));
	}
	l->tokens[l->count++] = (token_t){ start, len, line, col, type };
}

/// @brief Emit an error token and print to stderr
/// @param l
/// @param start
/// @param len
static void push_error(lexer_t *l, const char *start, size_t len)
{
	fprintf(stderr, "[okin lex error] line %d col %d: unexpected '%.*s'\n",
			l->line, l->col, (int)len, start);
	push(l, TK_ERROR, start, len, l->line, l->col);
}

/// @brief Advance while predicate is true, return span length
/// @param l
/// @param pred
/// @return size_t
static size_t read_while(lexer_t *l, int (*pred)(int))
{
	size_t len = 0;
	while (peek(l) && pred((unsigned char)peek(l))) { advance(l); len++; }
	return len;
}

/// @brief Read a quoted string, return span inside quotes
/// @param l
/// @param delim: Opening delimiter character (' or ")
static void read_string(lexer_t *l, char delim)
{
	const char *str_start = l->src + l->pos;
	int line = l->line, col = l->col;

	while (peek(l) && peek(l) != delim) {
		if (peek(l) == '\\' && l->src[l->pos + 1]) advance(l);
		advance(l);
	}

	size_t str_len = (l->src + l->pos) - str_start;

	if (peek(l) == delim)
		advance(l);
	else
		fprintf(stderr, "[okin lex error] line %d col %d: unclosed string\n", line, col);

	push(l, TK_STRING, str_start, str_len, line, col);
}

/// @brief Return 1 if c is a valid value character (not a delimiter)
/// @param c
/// @return int
static int is_value_char(int c) { return c && !strchr("<>|,; \t\r\n\"'@$", c); }

/// @brief Handle single character token
/// @param l
/// @param start
/// @param line
/// @param col
static void handle_single_char_token(lexer_t *l, const char *start, int line, int col)
{
	advance(l);
	token_type_t type = SINGLE_CHAR_TYPES[strchr(SINGLE_CHAR_TOKENS, *start) - SINGLE_CHAR_TOKENS];

	if ((type == TK_COMMA || type == TK_ARG_CLOSE) && l->count > 0)
	{
		token_type_t prev = l->tokens[l->count - 1].type;
		if (prev == TK_COMMA || prev == TK_ARG_OPEN) {
			push_error(l, start, 1);
			return;
		}
	}

	if (type == TK_ARG_OPEN) l->depth++;
	if (type == TK_ARG_CLOSE) {
		if (l->depth == 0) { push_error(l, start, 1); return; }
		l->depth--;
	}

	push(l, type, start, 1, line, col);
}

/// @brief Handle numeric token (including lib call)
/// @param l
/// @param start
/// @param line
/// @param col
static void handle_number_token(lexer_t *l, const char *start, int line, int col)
{
	if (*start == '-') advance(l);
	read_while(l, isdigit);

	size_t len = (l->src + l->pos) - start;
	if (l->depth == 0 && fast_atoi(start, len) == 0x90) // label opcode
		l->has_labels = 1;

	if (peek(l) == '.')
	{
		advance(l);
		read_while(l, isdigit);
		push(l, TK_VALUE, start, (l->src + l->pos) - start, line, col);
		return;
	}

	if (peek(l) == '~')
	{
		advance(l);
		read_while(l, is_value_char);
		push(l, TK_LIB_CALL, start, (l->src + l->pos) - start, line, col);
		return;
	}

	token_type_t type = (l->depth > 0 && peek(l) != '<') ? TK_INT : TK_OPCODE;
	push(l, type, start, (l->src + l->pos) - start, line, col);
}

/// @brief Handle integer literal token ($123)
/// @param l
/// @param start
/// @param line
/// @param col
static void handle_int_literal(lexer_t *l, const char *start, int line, int col)
{
	advance(l);

	read_while(l, isdigit);
	push(l, TK_INT_LIT, start, (l->src + l->pos) - start, line, col);
}

/// @brief Handle float literal token (@3.14)
/// @param l
/// @param start
/// @param line
/// @param col
static void handle_float_literal(lexer_t *l, const char *start, int line, int col)
{
	advance(l);

	read_while(l, isdigit);
	if (peek(l) == '.')
	{
		advance(l);
		read_while(l, isdigit);
	}
	push(l, TK_FLOAT_LIT, start, (l->src + l->pos) - start, line, col);
}

/// @brief Try to handle a literal prefix ($, @, etc), return 1 if handled
/// @param l
/// @param c: Character to check
/// @param start: Start position
/// @param line: Line number
/// @param col: Column number
/// @return 1 if handled as literal, 0 otherwise
static int try_handle_literal(lexer_t *l, char c, const char *start, int line, int col)
{
	if (!l->src[l->pos + 1] || !isdigit((unsigned char)l->src[l->pos + 1]))
		return 0;

	switch(c) {
		case '$': handle_int_literal(l, start, line, col); return 1;
		case '@': handle_float_literal(l, start, line, col); return 1;
		default: return 0;
	}
}

// ======================
// -- LEXER
// ======================

/// @brief Initialize lexer with source string
/// @param src: Source string to lex
/// @return Heap allocated lexer_t
lexer_t *lexer_init(const char *src)
{
	lexer_t *l  = calloc(1, sizeof(lexer_t));
	l->src      = src;
	l->line     = 1;
	l->col      = 1;
	l->capacity = LEXER_INIT_CAP;
	l->tokens   = malloc(LEXER_INIT_CAP * sizeof(token_t));
	return l;
}

/// @brief Run lexer over source, populate token list
/// @param l: Lexer instance
void lexer_run(lexer_t *l)
{
	while (peek(l))
	{
		if (isspace((unsigned char)peek(l))) { advance(l); continue; }

		int line = l->line;
		int col = l->col;
		const char *start = l->src + l->pos;
		char c = peek(l);

		if (c == '-' && l->src[l->pos + 1] == '-')
		{
			while (peek(l) && peek(l) != '\n') advance(l);
			continue;
		}

		const char *sc = strchr(SINGLE_CHAR_TOKENS, c);
		if (sc)
		{
			handle_single_char_token(l, start, line, col);
			continue;
		}

		if (isdigit((unsigned char)c) || (c == '-' && isdigit((unsigned char)l->src[l->pos + 1])))
		{
			handle_number_token(l, start, line, col);
			continue;
		}

		if (strchr(STR_DELIM, c))
		{
			advance(l);
			read_string(l, c);
			continue;
		}

		if (try_handle_literal(l, c, start, line, col))
		{
			continue;
		}

		if (isalpha((unsigned char)c) || c == '_')
		{
			size_t len = read_while(l, is_value_char);
			push(l, TK_VALUE, start, len, line, col);
			continue;
		}

		advance(l);
		push_error(l, start, 1);
	}

	if (l->depth != 0)
		fprintf(stderr, "[okin lex error] line %d: unclosed '<', depth %d\n", l->line, l->depth);

	push(l, TK_EOF, l->src + l->pos, 0, l->line, l->col);
}

/// @brief Free lexer and token list
/// @param l: Lexer instance
void lexer_free(lexer_t *l)
{
	free(l->tokens);
	free(l);
}

/// @brief Print all tokens to stdout
/// @param l: Lexer instance
void lexer_print(const lexer_t *l)
{
	for (size_t i = 0; i < l->count; i++)
	{
		const token_t *t = &l->tokens[i];

		printf("[%d:%d] %-12s %.*s\n",
				t->line, t->col,
				TOKEN_TYPE_NAMES[t->type],
				(int)t->len, t->start);
	}
}
