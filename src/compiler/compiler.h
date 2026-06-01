#ifndef COMPILER_H
#define COMPILER_H

#include "../parser/parser.h"
#include "../vm/vm.h"

#include "chunk.h"
#include "scope.h"

// ======================
// -- CLASSES & STRUCTS
// ======================

typedef struct {
	const parser_t *parser;
	chunk_t        *root;
	chunk_t        *current_scope;
	scope_t         scope;

	int errors;
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

/// @brief Run all chunks in the given compiler
/// @param c
/// @return chunk_t*: The root chunk
chunk_t *compile_chunks(compiler_t *c);

#endif
