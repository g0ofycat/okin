#ifndef VM_VALUE_H
#define VM_VALUE_H

#include <stdint.h>
#include <stddef.h>

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

struct vm_array_t {
	vm_val_t *items;
	int       len;
	int       cap;
	int       refs;
};

struct vm_val_t {
	vm_type_t type;
	union {
		int64_t    i;
		double     f;
		char      *s;
		int        b;
		vm_array_t *arr;
	};
};

// ======================
// -- CREATE VALS
// ======================

/// @brief Turn a int into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_int   (int64_t v);

/// @brief Turn a double into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_float (double v);

/// @brief Turn a bool into a VM value
/// @param v
/// @return vm_val_t
vm_val_t vm_val_bool  (int v);

/// @brief Create a VM string value
/// @param s
/// @param len
/// @return vm_val_t
vm_val_t vm_val_str   (const char *s, size_t len);

/// @brief Create a VM nil value
/// @return vm_val_t
vm_val_t vm_val_nil   (void);

/// @brief Create a VM array object
/// @return vm_val_t
vm_val_t vm_val_array (void);

// ======================
// -- VAL UTILS
// ======================

/// @brief Check if a value is truthy
/// @param v
/// @return int: {0|1}
int         vm_val_truthy  (const vm_val_t *v);

/// @brief Turn a VM value to string type
/// @param v
/// @return const char *
const char *vm_val_type_str(const vm_val_t *v);

/// @brief Retain a VM value
/// @param v
void vm_val_retain (vm_val_t *v);

/// @brief: Release a VM value
/// @param v
void vm_val_release(vm_val_t *v);

// ======================
// -- ARRAY OPS
// ======================

/// @brief Initialize a new array object
/// @return vm_array_t
vm_array_t *vm_array_init(void);

/// @breif Push a array object to the heap
/// @param a
/// @param v
void        vm_array_push(vm_array_t *a, vm_val_t v);

/// @brief Free a given array
/// @param a
void        vm_array_free(vm_array_t *a);

#endif
