#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMPILER_H
#define COMPILER_H

#include "../parser/parser.h"

#include "chunk.h"
#include "scope.h"

// ======================
// -- CONSTS
// ======================

#define MAX_FUNCTIONS 256
#define MAX_BREAK_PATCHES 64

// ======================
// -- CLASSES & STRUCTS
// ======================

typedef struct {
	const char *name;
	size_t      name_len;
	int         index;
} func_entry_t;

typedef struct {
	const parser_t *parser;
	chunk_t        *root;
	chunk_t        *current_scope;
	scope_t        *scope;
	func_entry_t    functions[MAX_FUNCTIONS];
	int             func_count;
	int             break_patches[MAX_BREAK_PATCHES];
	int             break_count;

	int             errors;
} compiler_t;

// ======================
// -- COMPILER
// ======================

/// @brief Initialize the compiler
/// @param parser
/// @return compiler_t*
compiler_t *compiler_init(const parser_t *parser);

/// @brief Run the given compiler
/// @param c
void compiler_run(compiler_t *c);

/// @brief Free the given compiler
/// @param c
void compiler_free(compiler_t *c);

#endif

#ifdef __cplusplus
}
#endif
