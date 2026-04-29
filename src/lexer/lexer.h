#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// ======================
// -- CONSTS
// ======================

#define LEXER_INIT_CAP 64

// ======================
// -- TOKEN TYPES
// ======================

typedef enum
{
	TK_OPCODE,    // integer opcode
	TK_LIB_CALL,  // opcode~METHOD
	TK_ARG_OPEN,  // <
	TK_ARG_CLOSE, // >
	TK_PIPE,      // |
	TK_COMMA,     // ,
	TK_SEMI,      // ;
	TK_VALUE,     // identifier or raw literal
	TK_EOF,
	TK_ERROR
} token_type_t;

// ======================
// -- TOKEN
// ======================

typedef struct
{
	const char  *start;
	size_t       len;
	int          line;
	int          col;
	token_type_t type;
} token_t;

// ======================
// -- LEXER
// ======================

typedef struct
{
	token_t    *tokens;
	const char *src;
	size_t      pos;
	size_t      count;
	size_t      capacity;
	int         line;
	int         col;
	int         depth;
} lexer_t;

// ======================
// -- PUBLIC
// ======================

/// @brief Initialize lexer with source string
/// @param src Source string to lex
/// @return Heap allocated lexer_t
lexer_t *lexer_init(const char *src);

/// @brief Run lexer over source, populate token list
/// @param l Lexer instance
void lexer_run(lexer_t *l);

/// @brief Free lexer and token list
/// @param l Lexer instance
void lexer_free(lexer_t *l);

/// @brief Print all tokens to stdout
/// @param l Lexer instance
void lexer_print(const lexer_t *l);

#endif
