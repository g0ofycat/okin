#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "vm.h"

// ======================
// -- MACROS
// ======================

#define PUSH(v)  (vm->stack[vm->stack_top++] = (v))
#define POP()    (vm->stack[--vm->stack_top])
#define PEEK()   (vm->stack[vm->stack_top - 1])

// ======================
// -- UTILITY
// ======================

/// @brief Emit a VM runtime error and exit
/// @param msg: Error message
static void vm_error(const char *msg)
{
	fprintf(stderr, "[okin vm error] %s\n", msg);
	exit(1);
}

/// @brief Get a global by name, returns NULL if not found
/// @param vm: VM instance
/// @param key: Global name
/// @param len: Name length
/// @return vm_val_t*
static vm_val_t *vm_get_global(vm_t *vm, const char *key, size_t len)
{
	for (int i = 0; i < vm->global_count; i++) {
		global_entry_t *e = &vm->globals[i];
		if (e->key_len == len && memcmp(e->key, key, len) == 0)
			return &e->val;
	}
	return NULL;
}

/// @brief Set a global by name, creates if not found
/// @param vm: VM instance
/// @param key: Global name
/// @param len: Name length
/// @param val: Value to set
static void vm_set_global(vm_t *vm, const char *key, size_t len, vm_val_t val)
{
	for (int i = 0; i < vm->global_count; i++) {
		global_entry_t *e = &vm->globals[i];
		if (e->key_len == len && memcmp(e->key, key, len) == 0) {
			e->val = val;
			return;
		}
	}
	if (vm->global_count >= VM_GLOBALS_CAP) { vm_error("too many globals"); return; }
	global_entry_t *e = &vm->globals[vm->global_count++];
	e->key     = key;
	e->key_len = len;
	e->val     = val;
}

// ======================
// -- VM
// ======================

/// @brief Initialize the VM with a root chunk
/// @param root: Root chunk to execute
/// @return vm_t*
vm_t *vm_init(chunk_t *root)
{
	vm_t *vm  = calloc(1, sizeof(vm_t));
	vm->chunk = root;
	vm->ip    = 0;
	return vm;
}

/// @brief Run the VM
/// @param vm: VM instance
void vm_run(vm_t *vm)
{
	vm_execute(vm);
}

/// @brief Free the VM
/// @param vm: VM instance
void vm_free(vm_t *vm)
{
	free(vm);
}
