#ifdef __cplusplus
extern "C" {
#endif

#ifndef VM_VALUE_H
#define VM_VALUE_H

#include <stdint.h>
#include <stddef.h>

#include "../util/arena.h"

// ======================
// -- CLASSES & STRUCTS
// ======================

typedef enum {
	VM_INT,
	VM_FLOAT,
	VM_STR,
	VM_BOOL,

	VM_ARRAY,
	VM_NIL,
} vm_type_t;

typedef struct vm_val_t   vm_val_t;
typedef struct vm_array_t vm_array_t;

typedef struct {
	char   *data;
	size_t  len;
} vm_str_t;

struct vm_array_t {
	vm_val_t *items;
	int       len;
	int       cap;
};

struct vm_val_t {
	vm_type_t type;
	union {
		int64_t     i;
		double      f;
		int         b;
		vm_str_t   *str;
		vm_array_t *arr;
	};
};

// ======================
// -- CREATE VALS
// ======================

/// @brief Create a VM int value
/// @param v: Integer value
/// @return vm_val_t
vm_val_t vm_val_int(int64_t v);

/// @brief Create a VM float value
/// @param v: Float value
/// @return vm_val_t
vm_val_t vm_val_float(double v);

/// @brief Create a VM bool value
/// @param v: Boolean value {0|1}
/// @return vm_val_t
vm_val_t vm_val_bool(int v);

/// @brief Create a VM string value, copies the string into arena memory
/// @param arena: Arena allocator
/// @param s: Source string
/// @param len: Length of source string
/// @return vm_val_t
vm_val_t vm_val_str(arena_t *arena, const char *s, size_t len);

/// @brief Create a VM nil value
/// @return vm_val_t
vm_val_t vm_val_nil(void);

/// @brief Create a VM array value
/// @param arena: Arena allocator
/// @return vm_val_t
vm_val_t vm_val_array(arena_t *arena);

// ======================
// -- VAL UTILS
// ======================

/// @brief Check if a value is truthy
/// @param v: Value to check
/// @return int: {0|1}
int vm_val_truthy(const vm_val_t *v);

/// @brief Get the type name of a VM value as a string
/// @param v: Value to inspect
/// @return const char*
const char *vm_val_type_str(const vm_val_t *v);

// ======================
// -- ARRAY OPS
// ======================

/// @brief Initialize a new heap-allocated array object
/// @param arena: Arena allocator
/// @return vm_array_t*
vm_array_t *vm_array_init(arena_t *arena);

/// @brief Push a value onto the array, growing if needed
/// @param arena: Arena allocator
/// @param a: Target array
/// @param v: Value to push
void vm_array_push(arena_t *arena, vm_array_t *a, vm_val_t v);

#endif

#ifdef __cplusplus
}
#endif
