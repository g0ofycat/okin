#ifdef __cplusplus
extern "C" {
#endif

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
	instruction_t  *code;
	vm_val_t       *constants;
	struct chunk_t **sub_chunks;
	const char     *name;

	int            code_len;
	int            code_cap;

	int            const_len;
	int            const_cap;

	int            sub_len;
	int            sub_cap;

	int            num_locals;
	int            num_params;
} chunk_t;

// ======================
// -- CHUNK
// ======================

/// @brief Initialize a new function chunk
/// @param name
/// @return chunk_t*
chunk_t *chunk_init(const char *name);

/// @brief Emit a given chunk
/// @param c
/// @param op
/// @param a
/// @return 0 on success, 1 on failure
int chunk_emit(chunk_t *c, vm_op_t op, int32_t a);

/// @brief Free the given chunk
/// @param c
void chunk_free(chunk_t *c);

/// @brief Patch a previously emitted instruction's operand
/// @param c
/// @param idx
/// @param a
void chunk_patch(chunk_t *c, int idx, int32_t a);

/// @brief Add a constant to the pool, returns its index
/// @param c
/// @param val
/// @return int: Constant Index
int chunk_add_const(chunk_t *c, vm_val_t val);

/// @brief Add a sub-chunk (function proto), returns its index
/// @param c
/// @param sub
/// @return int: Sub-chunk index
int chunk_add_sub(chunk_t *c, chunk_t *sub);

/// @brief Print chunk data recursively to stdout
/// @param chunk
void chunk_print(const chunk_t *chunk);

#endif

#ifdef __cplusplus
}
#endif
