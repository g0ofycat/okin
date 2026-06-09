#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"

// ======================
// -- CHUNK
// ======================

/// @brief Initialize a new function chunk
/// @param name
/// @return chunk_t*
chunk_t *chunk_init(const char *name)
{
	chunk_t *c    = calloc(1, sizeof(chunk_t));
	c->name       = name;
	c->code       = malloc(sizeof(instruction_t) * CHUNK_INIT_CAP);
	c->code_cap   = CHUNK_INIT_CAP;

	c->constants  = malloc(sizeof(vm_val_t) * CONST_INIT_CAP);
	c->const_cap  = CONST_INIT_CAP;
	c->sub_chunks = malloc(sizeof(chunk_t *) * SUB_CHUNK_INIT_CAP);
	c->sub_cap    = SUB_CHUNK_INIT_CAP;

	return c;
}

/// @brief Free the given chunk and all sub-chunks
/// @param c
void chunk_free(chunk_t *c)
{
	for (int i = 0; i < c->sub_len; i++)
		chunk_free(c->sub_chunks[i]);
	free(c->sub_chunks);
	free(c->constants);
	free(c->code);
	free(c);
}

/// @brief Emit an instruction into the chunk, returns instruction index
/// @param c
/// @param op
/// @param a
/// @return int: instruction index
int chunk_emit(chunk_t *c, vm_op_t op, int32_t a)
{
	if (c->code_len == c->code_cap) {
		c->code_cap *= 2;
		c->code      = realloc(c->code, sizeof(instruction_t) * c->code_cap);
	}
	c->code[c->code_len] = (instruction_t){ op, a };
	return c->code_len++;
}

/// @brief Patch a previously emitted instruction's operand
/// @param c
/// @param idx
/// @param a
void chunk_patch(chunk_t *c, int idx, int32_t a)
{
	c->code[idx].a = a;
}

/// @brief Add a constant to the pool, returns its index
/// @param c
/// @param val
/// @return int: Constant Index
int chunk_add_const(chunk_t *c, vm_val_t val)
{
	if (c->const_len == c->const_cap) {
		c->const_cap *= 2;
		c->constants  = realloc(c->constants, sizeof(vm_val_t) * c->const_cap);
	}
	c->constants[c->const_len] = val;
	return c->const_len++;
}

/// @brief Add a sub-chunk (function proto), returns its index
/// @param c
/// @param sub
/// @return int: Sub-chunk index
int chunk_add_sub(chunk_t *c, chunk_t *sub)
{
	if (c->sub_len == c->sub_cap) {
		c->sub_cap   *= 2;
		c->sub_chunks = realloc(c->sub_chunks, sizeof(chunk_t *) * c->sub_cap);
	}
	c->sub_chunks[c->sub_len] = sub;
	return c->sub_len++;
}

// ======================
// -- DEBUG
// ======================

/// @brief Print VM value
/// @param v
static void vm_val_print(vm_val_t v) {
	if (v.type == VM_INT)        printf("%lld", (long long)v.i);
	else if (v.type == VM_FLOAT) printf("%f", v.f);
	else if (v.type == VM_STR)   printf("\"%s\"", v.str->data);
	else if (v.type == VM_BOOL)  printf("%s", v.b ? "true" : "false");
	else if (v.type == VM_ARRAY) printf("[array]");
	else                         printf("nil");
}

/// @brief Print chunk data recursively to stdout
/// @param chunk
void chunk_print(const chunk_t *chunk) {
	printf("\n<CHUNK: '%s'>\n", chunk->name);

	printf("- CODE\n");
	for (int i = 0; i < chunk->code_len; i++) {
		const instruction_t *inst = &chunk->code[i];
		printf("%04d | %s %d\n", i, opcode_names[inst->op], inst->a);
	}

	if (chunk->const_len > 0) {
		printf("\n- CONSTANTS\n");
		for (int i = 0; i < chunk->const_len; i++) {
			printf("%04d | ", i);
			vm_val_print(chunk->constants[i]);
			printf("\n");
		}
	}

	if (chunk->sub_len > 0) {
		for (int i = 0; i < chunk->sub_len; i++) {
			chunk_print(chunk->sub_chunks[i]);
		}
	}

	printf("<END>\n");
}
