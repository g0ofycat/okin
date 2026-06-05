#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCOPE_H
#define SCOPE_H

#include <stddef.h>

// ======================
// -- CONSTS
// ======================

#define MAX_LOCALS      256
#define MAX_SCOPE_DEPTH 64
#define MAX_GLOBALS     256

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
	const char *name;
	size_t      name_len;
} global_t;

typedef struct scope_t {
	struct scope_t *parent;

	local_t  locals[MAX_LOCALS];
	int      local_count;
	int      depth;
	global_t globals[MAX_GLOBALS];
	int      global_count;
} scope_t;

// ======================
// -- SCOPE
// ======================

/// @brief Initialize a new scope
/// @param parent: Optional parent
/// @return scope_t*
scope_t *scope_init(scope_t *parent);

/// @brief Free the given scope
/// @param s
void scope_free(scope_t *s);

/// @brief Declare a local variable in the current scope
/// @param s
/// @param name
/// @param len
/// @return Slot index
int scope_declare(scope_t *s, const char *name, size_t len);

/// @brief Resolve a local variable by name, returns slot or -1 if not found or if forced-global
/// @param s
/// @param name
/// @param len
/// @return Slot Index, or -1 if not found or forced-global
int scope_resolve(const scope_t *s, const char *name, size_t len);

/// @brief Mark a name as forced-global in the current scope
/// @param s
/// @param name
/// @param len
void scope_mark_global(scope_t *s, const char *name, size_t len);

/// @brief Begin a new block scope
/// @param s
void scope_begin(scope_t *s);

/// @brief End the current block scope, discarding its locals
/// @param s
void scope_end(scope_t *s);

#endif

#ifdef __cplusplus
}
#endif
