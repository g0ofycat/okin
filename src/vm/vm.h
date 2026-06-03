#ifdef __cplusplus
extern "C" {
#endif

#ifndef VM_H
#define VM_H

#include <stddef.h>

#include "value.h"
#include "opcode.h"
#include "../compiler/chunk.h"

// ======================
// -- CONSTS
// ======================

#define VM_STACK_MAX      4096
#define VM_CALL_STACK_MAX 256
#define VM_GLOBALS_CAP    256
#define VM_LOCALS_CAP 256

// ======================
// -- STRUCTS
// ======================

typedef struct {
	chunk_t *chunk;
	int      return_ip;
	int      stack_base;
	vm_val_t locals[VM_LOCALS_CAP];
} vm_call_frame_t;

typedef struct {

	const char *key;
	size_t      key_len;
	vm_val_t    val;
} global_entry_t;

typedef struct {
	vm_val_t       stack[VM_STACK_MAX];

	int            stack_top;

	vm_call_frame_t   frames[VM_CALL_STACK_MAX];
	int               frame_count;

	global_entry_t globals[VM_GLOBALS_CAP];
	int            global_count;

	chunk_t       *chunk;
	int            ip;
} vm_t;

// ======================
// -- VM
// ======================

/// @brief Initialize the VM with a root chunk
/// @param root: The root chunk to execute
/// @return vm_t*
vm_t *vm_init(chunk_t *root);

/// @brief Run the VM
/// @param vm: The VM instance
void  vm_run(vm_t *vm);

/// @brief Free the VM
/// @param vm: The VM instance
void  vm_free(vm_t *vm);

#endif

#ifdef __cplusplus
}
#endif
