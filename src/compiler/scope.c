#include <stdlib.h>
#include <string.h>

#include "scope.h"

// ======================
// -- SCOPE
// ======================

/// @brief Initialize a new scope
/// @param parent: Optional parent
/// @return scope_t*
scope_t *scope_init(scope_t *parent)
{
	scope_t *s = calloc(1, sizeof(scope_t));
	s->parent = parent;
	return s;
}

/// @brief Free the given scope
/// @param s
void scope_free(scope_t *s)
{
	free(s);
}

/// @brief Declare a local variable in the current scope
/// @param s
/// @param name
/// @param len
/// @return Slot Index on success, -1 if full
int scope_declare(scope_t *s, const char *name, size_t len)
{
	if (s->local_count >= MAX_LOCALS) return -1;

	int slot = s->local_count++;

	local_t *l = &s->locals[slot];
	l->name     = name;
	l->name_len = len;
	l->slot     = slot;
	l->depth    = s->depth;

	return slot;
}

/// @brief Resolve a local variable by name, returns slot or -1 if not found or forced-global
/// @param s
/// @param name
/// @param len
/// @return Slot Index, or -1 if not found or forced-global
int scope_resolve(const scope_t *s, const char *name, size_t len)
{
	const scope_t *curr = s;
	while (curr) {
		for (int i = 0; i < curr->global_count; i++) {
			const global_t *g = &curr->globals[i];
			if (g->name_len == len && memcmp(g->name, name, len) == 0)
				return -1;
		}
		curr = curr->parent;
	}
	for (int i = s->local_count - 1; i >= 0; i--) {
		const local_t *l = &s->locals[i];
		if (l->name_len == len && memcmp(l->name, name, len) == 0)
			return l->slot;
	}
	return -1;
}

/// @brief Mark a name as forced-global in the current scope
/// @param s
/// @param name
/// @param len
void scope_mark_global(scope_t *s, const char *name, size_t len)
{
	if (s->global_count >= MAX_GLOBALS) return;
	global_t *g = &s->globals[s->global_count++];
	g->name = name;
	g->name_len = len;
}

/// @brief Begin a new block scope
/// @param s
void scope_begin(scope_t *s)
{
	if (s->depth >= MAX_SCOPE_DEPTH) return;
	s->depth++;
}

/// @brief End the current block scope, discarding its locals
/// @param s
void scope_end(scope_t *s)
{
	while (s->local_count > 0 && s->locals[s->local_count - 1].depth == s->depth)
		s->local_count--;
	s->depth--;
}
