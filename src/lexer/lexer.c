#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"

// ======================
// -- DATA TABLES
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
	"EOF",
	"ERROR"
};

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

/// @brief Return 1 if c is a valid value character (not a delimiter)
/// @param c
/// @return int
static int is_value_char(int c) { return c && !strchr("<>|,; \t\r\n", c); }

// ======================
// -- LEXER
// ======================

/// @brief Initialize lexer with source string
/// @param src Source string to lex
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
/// @param l Lexer instance
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
			advance(l);
			token_type_t type = SINGLE_CHAR_TYPES[sc - SINGLE_CHAR_TOKENS];
			if (type == TK_COMMA || type == TK_ARG_CLOSE)
			{
				if (l->count > 0)
				{
					token_type_t prev = l->tokens[l->count - 1].type;
					if (prev == TK_COMMA || prev == TK_ARG_OPEN)
						push_error(l, start, 1);
				}
			}
			push(l, type, start, 1, line, col);

			if (type == TK_ARG_OPEN)  l->depth++;
			if (type == TK_ARG_CLOSE) l->depth--;

			continue;
		}

		if (isdigit((unsigned char)c) || (c == '-' && isdigit((unsigned char)l->src[l->pos + 1])))
		{
			if (c == '-') advance(l);
			read_while(l, isdigit);

			if (peek(l) == '.')
			{
				advance(l);
				read_while(l, isdigit);
				push(l, TK_VALUE, start, (l->src + l->pos) - start, line, col);
				continue;
			}
			if (peek(l) == '~')
			{
				advance(l);
				read_while(l, is_value_char);
				push(l, TK_LIB_CALL, start, (l->src + l->pos) - start, line, col);
			}
			else
			{
				push(l, TK_OPCODE, start, (l->src + l->pos) - start, line, col);
			}
			continue;
		}

		if (isalpha((unsigned char)c) || c == '_' || c == '$')
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
/// @param l Lexer instance
void lexer_free(lexer_t *l)
{
	free(l->tokens);
	free(l);
}

/// @brief Print all tokens to stdout
/// @param l Lexer instance
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
