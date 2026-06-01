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
scope_t *scope_init   (void);

/// @brief Free the given scope
/// @param s
void     scope_free   (scope_t *s);

/// @brief Declare a new scope
/// @param s
/// @param name
/// @param len
/// @return Slot index
int      scope_declare(scope_t *s, const char *name, size_t len);

/// @brief Resolve any issues in the given scope
/// @param s
/// @param name
/// @param len
/// @return Slot index
int      scope_resolve(const scope_t *s, const char *name, size_t len);

/// @brief Begin the given scope
/// @param s
void     scope_begin  (scope_t *s);

/// @brief End the given scope
/// @param s
void     scope_end    (scope_t *s);

#endif
