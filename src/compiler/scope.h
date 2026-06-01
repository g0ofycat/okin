#ifndef SCOPE_H
#define SCOPE_H

#include <stddef.h>

// ======================
// -- CONSTS
// ======================

#define MAX_LOCALS      256
#define MAX_SCOPE_DEPTH 64

// ======================
// -- CLASSES & STRUCTS
// ======================

typedef struct {
	const char *name;
	size_t      name_len;
	int         slot;
	int         depth;
} local_t;

typedef struct {
	local_t locals[MAX_LOCALS];
	int     local_count;
	int     depth;
} scope_t;

// ======================
// -- SCOPE
// ======================

/// @brief Initialize a new scope
/// @return scope_t*
scope_t *scope_init(void);

/// @brief Free the given scope
/// @param s
void scope_free(scope_t *s);

/// @brief Declare a local variable in the current scope
/// @param s
/// @param name
/// @param len
/// @return Slot Index on success, -1 if full
int scope_declare(scope_t *s, const char *name, size_t len);

/// @brief Resolve a local variable by name, returns slot or -1 if not found
/// @param s
/// @param name
/// @param len
/// @return Slot Index, or -1 if not found
int scope_resolve(const scope_t *s, const char *name, size_t len);

/// @brief Begin a new block scope
/// @param s
void scope_begin(scope_t *s);

/// @brief End the current block scope, discarding its locals
/// @param s
void scope_end(scope_t *s);

#endif
