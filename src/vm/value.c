#include <stdlib.h>
#include <string.h>

#include "value.h"

// ======================
// -- CREATE VALS
// ======================

/// @brief Create a VM int value
/// @param v: Integer value
/// @return vm_val_t
vm_val_t vm_val_int(int64_t v)
{
	return (vm_val_t){.type = VM_INT, .i = v};
}

/// @brief Create a VM float value
/// @param v: Float value
/// @return vm_val_t
vm_val_t vm_val_float(double v)
{
	return (vm_val_t){.type = VM_FLOAT, .f = v};
}

/// @brief Create a VM bool value
/// @param v: Boolean value {0|1}
/// @return vm_val_t
vm_val_t vm_val_bool(int v)
{
	return (vm_val_t){.type = VM_BOOL, .b = v};
}

/// @brief Create a VM string value, copies the string into arena memory
/// @param arena: Arena allocator
/// @param s: Source string
/// @param len: Length of source string
/// @return vm_val_t
vm_val_t vm_val_str(arena_t *arena, const char *s, size_t len)
{
	vm_str_t *str = arena_alloc(arena, sizeof(vm_str_t));
	str->data = arena_alloc(arena, len + 1);
	memcpy(str->data, s, len);
	str->data[len] = '\0';
	str->len = len;
	return (vm_val_t){.type = VM_STR, .str = str};
}

/// @brief Create a VM nil value
/// @return vm_val_t
vm_val_t vm_val_nil(void)
{
	return (vm_val_t){.type = VM_NIL};
}

/// @brief Create a VM array value
/// @param arena: Arena allocator
/// @return vm_val_t
vm_val_t vm_val_array(arena_t *arena)
{
	return (vm_val_t){.type = VM_ARRAY, .arr = vm_array_init(arena)};
}

// ======================
// -- VAL UTILS
// ======================

/// @brief Check if a value is truthy
/// @param v: Value to check
/// @return int: {0|1}
int vm_val_truthy(const vm_val_t *v)
{
	switch (v->type) {
		case VM_INT:   return v->i != 0;
		case VM_FLOAT: return v->f != 0.0;
		case VM_BOOL:  return v->b;
		case VM_STR:   return v->str && v->str->len > 0;
		case VM_ARRAY: return v->arr && v->arr->len > 0;
		case VM_NIL:   return 0;
	}
	return 0;
}

/// @brief Get the type name of a VM value as a string
/// @param v: Value to inspect
/// @return const char*
const char *vm_val_type_str(const vm_val_t *v)
{
	switch (v->type) {
		case VM_INT:   return "int";
		case VM_FLOAT: return "float";
		case VM_BOOL:  return "bool";
		case VM_STR:   return "str";
		case VM_ARRAY: return "array";
		case VM_NIL:   return "nil";
	}
	return "unknown";
}

// ======================
// -- ARRAY OPS
// ======================

/// @brief Initialize a new heap-allocated array object
/// @param arena: Arena allocator
/// @return vm_array_t*
vm_array_t *vm_array_init(arena_t *arena)
{
	vm_array_t *a = arena_alloc(arena, sizeof(vm_array_t));
	a->items = arena_alloc(arena, sizeof(vm_val_t) * 8);
	a->len = 0;
	a->cap = 8;
	return a;
}

/// @brief Push a value onto the array, growing if needed
/// @param arena: Arena allocator
/// @param a: Target array
/// @param v: Value to push
void vm_array_push(arena_t *arena, vm_array_t *a, vm_val_t v)
{
	if (a->len == a->cap) {
		int new_cap = a->cap * 2;
		vm_val_t *new_items = arena_alloc(arena, sizeof(vm_val_t) * new_cap);
		memcpy(new_items, a->items, sizeof(vm_val_t) * a->len);
		a->items = new_items;
		a->cap = new_cap;
	}
	a->items[a->len++] = v;
}
