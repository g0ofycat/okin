#include <stdlib.h>

#include "arena.h"

// ======================
// -- PRIVATE
// ======================

/// @brief Allocate and link a new chunk onto the arena
/// @param a Arena instance
static void arena_grow(arena_t *a)
{
	arena_chunk_t *chunk = malloc(sizeof(arena_chunk_t));
	chunk->offset = 0;
	chunk->next   = a->head;
	a->head       = chunk;
}

// ======================
// -- ARENA
// ======================

/// @brief Initialize a new arena
/// @return Heap allocated arena_t
arena_t *arena_init()
{
	arena_t *a = calloc(1, sizeof(arena_t));
	arena_grow(a);
	return a;
}

/// @brief Allocate size bytes from the arena
/// @param a Arena instance
/// @param size Bytes to allocate
/// @return Pointer to allocated memory
void *arena_alloc(arena_t *a, size_t size)
{
	if (a->head->offset + size > ARENA_CHUNK_SIZE)
		arena_grow(a);

	void *ptr = a->head->data + a->head->offset;
	a->head->offset += size;
	return ptr;
}

/// @brief Free all chunks in the arena
/// @param a Arena instance
void arena_free(arena_t *a)
{
	arena_chunk_t *chunk = a->head;
	while (chunk)
	{
		arena_chunk_t *next = chunk->next;
		free(chunk);
		chunk = next;
	}
	free(a);
}
