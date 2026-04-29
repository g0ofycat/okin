#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

// ======================
// -- CONSTS
// ======================

#define ARENA_CHUNK_SIZE 65536

// ======================
// -- ARENA CHUNK
// ======================

typedef struct arena_chunk_t
{
	size_t                offset;
	struct arena_chunk_t *next;
	uint8_t               data[ARENA_CHUNK_SIZE];
} arena_chunk_t;

// ======================
// -- ARENA
// ======================

typedef struct
{
	arena_chunk_t *head;
} arena_t;

// ======================
// -- PUBLIC
// ======================

/// @brief Initialize a new arena
/// @return Heap allocated arena_t
arena_t *arena_init();

/// @brief Allocate size bytes from the arena
/// @param a Arena instance
/// @param size Bytes to allocate
/// @return Pointer to allocated memory
void *arena_alloc(arena_t *a, size_t size);

/// @brief Free all chunks in the arena
/// @param a Arena instance
void arena_free(arena_t *a);

#endif
