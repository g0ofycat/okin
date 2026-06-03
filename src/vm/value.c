#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "value.h"

// ======================
// -- CREATE VALS
// ======================

/// @brief Turn an int into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_int(int64_t v)
{
	vm_val_t val;
	val.type = VM_INT;
	val.i    = v;
	return val;
}

/// @brief Turn a double into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_float(double v)
{
	vm_val_t val;
	val.type = VM_FLOAT;
	val.f    = v;
	return val;
}


/// @brief Turn a bool into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_bool(int v)
{
	vm_val_t val;
	val.type = VM_BOOL;
	val.b    = v;
	return val;
}

/// @brief Create a VM string value, copies the string
/// @param s
/// @param len
/// @return vm_val_t
vm_val_t vm_val_str(const char *s, size_t len)
{
	vm_val_t val;
	val.type      = VM_STR;
	val.str       = malloc(sizeof(vm_str_t));
	val.str->data = malloc(len + 1);
	memcpy(val.str->data, s, len);
	val.str->data[len] = '\0';
	val.str->len  = len;
	val.str->refs = 1;
	return val;
}

/// @brief Create a VM nil value
/// @return vm_val_t
vm_val_t vm_val_nil(void)
{
	vm_val_t val;
	val.type = VM_NIL;
	val.i    = 0;
	return val;
}

/// @brief Create a VM array value
/// @return vm_val_t
vm_val_t vm_val_array(void)
{
	vm_val_t val;
	val.type = VM_ARRAY;
	val.arr  = vm_array_init();
	return val;
}

// ======================
// -- VAL UTILS
// ======================

/// @brief Check if a value is truthy
/// @param v
/// @return int: {0|1}
int vm_val_truthy(const vm_val_t *v)
{
	switch (v->type) {
		case VM_INT:   return v->i != 0;
		case VM_FLOAT: return v->f != 0.0;
		case VM_BOOL:  return v->b;
		case VM_STR:   return v->str && v->str->data && v->str->data[0] != '\0';
		case VM_ARRAY: return v->arr && v->arr->len > 0;
		case VM_NIL:   return 0;
	}
	return 0;
}

/// @brief Turn a VM value type to its string representation
/// @param v
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

/// @brief Retain a VM value, incrementing ref count for heap types
/// @param v
void vm_val_retain(vm_val_t *v)
{
	if (v->type == VM_STR && v->str)
		v->str->refs++;
	if (v->type == VM_ARRAY && v->arr)
		v->arr->refs++;
}

/// @brief Release a VM value, freeing heap types when ref count hits zero
/// @param v
void vm_val_release(vm_val_t *v)
{
	if (v->type == VM_STR && v->str) {
		v->str->refs--;
		if (v->str->refs <= 0) {
			free(v->str->data);
			free(v->str);
		}
		v->str = NULL;
		return;
	}
	if (v->type == VM_ARRAY && v->arr) {
		v->arr->refs--;
		if (v->arr->refs <= 0)
			vm_array_free(v->arr);
		v->arr = NULL;
	}
}

// ======================
// -- ARRAY OPS
// ======================

/// @brief Initialize a new array object
/// @return vm_array_t*
vm_array_t *vm_array_init(void)
{
	vm_array_t *a = malloc(sizeof(vm_array_t));
	a->items      = malloc(sizeof(vm_val_t) * 8);
	a->len        = 0;
	a->cap        = 8;
	a->refs       = 1;
	return a;
}

/// @brief Push a value onto the array
/// @param a
/// @param v
void vm_array_push(vm_array_t *a, vm_val_t v)
{
	if (a->len == a->cap) {
		a->cap  *= 2;
		a->items = realloc(a->items, sizeof(vm_val_t) * a->cap);
	}
	a->items[a->len++] = v;
}

/// @brief Free a given array and all its items
/// @param a
void vm_array_free(vm_array_t *a)
{
	for (int i = 0; i < a->len; i++)
		vm_val_release(&a->items[i]);
	free(a->items);
	free(a);
}
