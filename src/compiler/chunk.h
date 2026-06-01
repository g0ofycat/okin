#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>
#include <stddef.h>

#include "../vm/value.h"
#include "../vm/opcode.h"

// ======================
// -- CONSTS
// ======================

#define CHUNK_INIT_CAP 64
#define CONST_INIT_CAP 256
#define SUB_CHUNK_INIT_CAP 8

// ======================
// -- CLASSES & STRUCTS
// ======================

typedef struct {
	vm_op_t op;
	int32_t a;
} instruction_t;

typedef struct chunk_t {
	instruction_t code;
	int           code_len;
	int           code_cap;

	vm_val_t      constants;
	int           const_len;
	int           const_cap;

	struct chunk_t **sub_chunks;
	int              sub_len;
	int              sub_cap;

	int              num_locals;
	int              num_params;
	const char      *name;
} chunk_t;

// ======================
// -- CHUNK
// ======================

/// @brief Initialize a new function chunk
/// @param name: The name of the chunk
/// @return chunk_t*
chunk_t *chunk_init(const char *name);

/// @brief Run a given chunk
/// @param c
/// @param op
/// @param a
/// @return 0 on success, 1 on failure
int chunk_run(chunk_t *c, vm_op_t op, int32_t a);

/// @brief Free the named chunk
/// @param name
void chunk_free(const char* name);

/// @brief Patch a given chunk
/// @param c
/// @param idx
/// @param a
void chunk_patch(chunk_t *c, int idx, int32_t a);

/// @brief Add a constant to the given chunk
/// @param c
/// @param val
void chunk_add_const(chunk_t *c, vm_val_t val);

/// @brief Add a sub-chunk to the main chunk
/// @param c
/// @param sub
void chunk_add_sub(chunk_t *c, chunk_t *sub);

#endif
