#ifdef __cplusplus
extern "C" {
#endif

#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>

#include "util/arena.h"
#include "../lexer/lexer.h"
#include "../opcodes/opcodes.h"

// ======================
// -- CONSTS
// ======================

#define PARSER_MAX_LABELS 256
#define PARSER_MAX_ARGS   256

#define BODY_MAX_BYTES    4096
#define PARSER_MAX_NODES  4096

#define NODE_LEAF         0xFF
#define NODE_JUMP_TARGET  0xFE

// ======================
// -- STRUCTS
// ======================

typedef struct okin_node_t
{
	const char         *val_start;
	const char         *method;
	size_t              val_len;
	size_t              method_len;
	struct okin_node_t **args;
	struct okin_node_t **body;
	int                 argc;
	int                 body_len;
	int                 jump_index;
	uint8_t             opcode;
	token_type_t        tok;
} okin_node_t;

typedef struct
{
	const char *name;
	size_t      name_len;
	int         index;
} label_entry_t;

typedef struct
{
	okin_node_t  **nodes;
	int            len;
	label_entry_t  labels[PARSER_MAX_LABELS];
	int            label_count;
} okin_program_t;

// ======================
// -- PARSER
// ======================

typedef struct
{
	const lexer_t  *lexer;
	size_t          pos;
	arena_t        *arena;
	okin_program_t *program;
	int             errors;
} parser_t;

// ======================
// -- PUBLIC
// ======================

/// @brief Initialize parser from a lexed token stream
/// @param lexer Completed lexer instance
/// @return Heap allocated parser_t
parser_t *parser_init(const lexer_t *lexer);

/// @brief Run both passes, populate program
/// @param p Parser instance
void parser_run(parser_t *p);

/// @brief Free parser, arena, and program
/// @param p Parser instance
void parser_free(parser_t *p);

/// @brief Print all nodes to stdout
/// @param p Parser instance
void parser_print(const parser_t *p);

#endif

#ifdef __cplusplus
}
#endif
