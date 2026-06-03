#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "vm.h"

// ======================
// -- MACROS
// ======================

#define PUSH(v) (vm->stack[vm->stack_top++] = (v))
#define POP()   (vm->stack[--vm->stack_top])
#define PEEK()  (vm->stack[vm->stack_top - 1])

#define CURRENT_FRAME() (&vm->frames[vm->frame_count - 1])

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
// -- CONSTANTS & LOCALS
// ======================

/// @brief Push a constant from the pool onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index)
static void op_load_const(vm_t *vm, const instruction_t *inst) {}

/// @brief Push a local variable (stack slot relative to frame base) onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = slot index)
static void op_load_local(vm_t *vm, const instruction_t *inst) {}

/// @brief Store top of stack into a local slot (does not pop)
/// @param vm: VM instance
/// @param inst: Instruction (a = slot index)
static void op_store_local(vm_t *vm, const instruction_t *inst) {}

/// @brief Load a global by name (constant pool holds the name string) onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for name)
static void op_load_global(vm_t *vm, const instruction_t *inst) {}

/// @brief Pop and store a value into a global by name
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for name)
static void op_store_global(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- LITERALS
// ======================

/// @brief Push true onto the stack
static void op_true(vm_t *vm, const instruction_t *inst) {}

/// @brief Push false onto the stack
static void op_false(vm_t *vm, const instruction_t *inst) {}

/// @brief Push nil onto the stack
static void op_nil(vm_t *vm, const instruction_t *inst) {}

/// @brief Discard the top of stack
static void op_pop(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- ARITHMETIC
// ======================

/// @brief Binary arithmetic: pops b then a, pushes result
/// @param vm: VM instance
/// @param inst: Instruction (op selects operation)
static void op_arith(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- COMPARISON
// ======================

/// @brief Binary comparison: pops b then a, pushes bool result
/// @param vm: VM instance
/// @param inst: Instruction (op selects comparison)
static void op_cmp(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- LOGICAL
// ======================

/// @brief Logical AND: short-circuits, pops both, pushes bool
static void op_and(vm_t *vm, const instruction_t *inst) {}

/// @brief Logical OR: short-circuits, pops both, pushes bool
static void op_or(vm_t *vm, const instruction_t *inst) {}

/// @brief Logical NOT: pops one, pushes inverted bool
static void op_not(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- JUMPS
// ======================

/// @brief Unconditional jump
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jmp(vm_t *vm, const instruction_t *inst) {}

/// @brief Jump if top of stack is falsy, pops condition
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jmp_false(vm_t *vm, const instruction_t *inst) {}

/// @brief Jump if popped a == b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jeq(vm_t *vm, const instruction_t *inst) {}

/// @brief Jump if popped a != b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jne(vm_t *vm, const instruction_t *inst) {}

/// @brief Jump if popped a > b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jgt(vm_t *vm, const instruction_t *inst) {}

/// @brief Jump if popped a < b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jlt(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- FUNCTIONS
// ======================

/// @brief Push a function (sub-chunk) reference onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = sub_chunk index)
static void op_load_func(vm_t *vm, const instruction_t *inst) {}

/// @brief Call a function: expects callee below args on stack
/// @param vm: VM instance
/// @param inst: Instruction (a = arg count)
static void op_call(vm_t *vm, const instruction_t *inst) {}

/// @brief Return from a function, restoring caller frame
/// @param vm: VM instance
/// @param inst: Instruction
static void op_ret(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- ARRAYS
// ======================

/// @brief Build an array from the top N stack values, pushes array
/// @param vm: VM instance
/// @param inst: Instruction (a = element count)
static void op_make_array(vm_t *vm, const instruction_t *inst) {}

/// @brief Index into an array: pops index then array, pushes element
/// @param vm: VM instance
/// @param inst: Instruction
static void op_array_get(vm_t *vm, const instruction_t *inst) {}

/// @brief Set array element: pops value, index, array (array is mutated in-place)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_array_set(vm_t *vm, const instruction_t *inst) {}

/// @brief Membership check: pops array then item, pushes bool
/// @param vm: VM instance
/// @param inst: Instruction
static void op_in(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- IO
// ======================

/// @brief Write top of stack to stdout (no newline), pops value
/// @param vm: VM instance
/// @param inst: Instruction
static void op_io_write(vm_t *vm, const instruction_t *inst) {}

/// @brief Write top of stack to stdout with newline, pops value
/// @param vm: VM instance
/// @param inst: Instruction
static void op_io_writeln(vm_t *vm, const instruction_t *inst) {}

/// @brief Read a line from stdin, store into global named by constant[a]
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for global name)
static void op_io_read(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- STRING
// ======================

/// @brief Push string length of top of stack, pops string
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_len(vm_t *vm, const instruction_t *inst) {}

/// @brief Concatenate two strings: pops b then a, pushes result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_concat(vm_t *vm, const instruction_t *inst) {}

/// @brief Slice a string: pops end, start, string; pushes substring
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_slice(vm_t *vm, const instruction_t *inst) {}

/// @brief Find substring: pops pattern then string, pushes index or -1
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_find(vm_t *vm, const instruction_t *inst) {}

/// @brief Uppercase: pops string, pushes uppercased copy
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_upper(vm_t *vm, const instruction_t *inst) {}

/// @brief Lowercase: pops string, pushes lowercased copy
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_lower(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- MATH
// ======================

/// @brief Power: pops exp then base, pushes float result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_pow(vm_t *vm, const instruction_t *inst) {}

/// @brief Square root: pops value, pushes float result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_sqrt(vm_t *vm, const instruction_t *inst) {}

/// @brief Absolute value: pops value, pushes abs (preserves int/float type)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_abs(vm_t *vm, const instruction_t *inst) {}

/// @brief Minimum: pops b then a, pushes smaller (preserves int if both int)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_min(vm_t *vm, const instruction_t *inst) {}

/// @brief Maximum: pops b then a, pushes larger (preserves int if both int)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_max(vm_t *vm, const instruction_t *inst) {}

/// @brief Floor: pops float, pushes int
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_floor(vm_t *vm, const instruction_t *inst) {}

/// @brief Ceil: pops float, pushes int
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_ceil(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- CONTROL
// ======================

/// @brief Halt execution
/// @param vm: VM instance
/// @param inst: Instruction
static void op_halt(vm_t *vm, const instruction_t *inst) {}

/// @brief Break out of loop by jumping to patch target
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_break(vm_t *vm, const instruction_t *inst) {}

// ======================
// -- DISPATCH
// ======================

typedef void (*vm_fn)(vm_t *, const instruction_t *);
static vm_fn VM_TABLE[256];

/// @brief Initialize the VM dispatch table
static void init_vm_table(void)
{
	memset(VM_TABLE, 0, sizeof(VM_TABLE));

	VM_TABLE[OP_LOAD_CONST]   = op_load_const;
	VM_TABLE[OP_LOAD_LOCAL]   = op_load_local;
	VM_TABLE[OP_STORE_LOCAL]  = op_store_local;
	VM_TABLE[OP_LOAD_GLOBAL]  = op_load_global;
	VM_TABLE[OP_STORE_GLOBAL] = op_store_global;

	VM_TABLE[OP_TRUE]         = op_true;
	VM_TABLE[OP_FALSE]        = op_false;
	VM_TABLE[OP_NIL]          = op_nil;
	VM_TABLE[OP_POP]          = op_pop;

	VM_TABLE[OP_ADD]          = op_arith;
	VM_TABLE[OP_SUB]          = op_arith;
	VM_TABLE[OP_MUL]          = op_arith;
	VM_TABLE[OP_DIV]          = op_arith;
	VM_TABLE[OP_MOD]          = op_arith;

	VM_TABLE[OP_EQ]           = op_cmp;
	VM_TABLE[OP_NEQ]          = op_cmp;
	VM_TABLE[OP_GT]           = op_cmp;
	VM_TABLE[OP_LT]           = op_cmp;
	VM_TABLE[OP_GTE]          = op_cmp;
	VM_TABLE[OP_LTE]          = op_cmp;

	VM_TABLE[OP_AND]          = op_and;
	VM_TABLE[OP_OR]           = op_or;
	VM_TABLE[OP_NOT]          = op_not;

	VM_TABLE[OP_JMP]          = op_jmp;
	VM_TABLE[OP_JMP_FALSE]    = op_jmp_false;
	VM_TABLE[OP_JEQ]          = op_jeq;
	VM_TABLE[OP_JNE]          = op_jne;
	VM_TABLE[OP_JGT]          = op_jgt;
	VM_TABLE[OP_JLT]          = op_jlt;

	VM_TABLE[OP_LOAD_FUNC]    = op_load_func;
	VM_TABLE[OP_CALL]         = op_call;
	VM_TABLE[OP_RET]          = op_ret;

	VM_TABLE[OP_MAKE_ARRAY]   = op_make_array;
	VM_TABLE[OP_ARRAY_GET]    = op_array_get;
	VM_TABLE[OP_ARRAY_SET]    = op_array_set;
	VM_TABLE[OP_IN]           = op_in;

	VM_TABLE[OP_IO_WRITE]     = op_io_write;
	VM_TABLE[OP_IO_WRITELN]   = op_io_writeln;
	VM_TABLE[OP_IO_READ]      = op_io_read;

	VM_TABLE[OP_STR_LEN]      = op_str_len;
	VM_TABLE[OP_STR_CONCAT]   = op_str_concat;
	VM_TABLE[OP_STR_SLICE]    = op_str_slice;
	VM_TABLE[OP_STR_FIND]     = op_str_find;
	VM_TABLE[OP_STR_UPPER]    = op_str_upper;
	VM_TABLE[OP_STR_LOWER]    = op_str_lower;

	VM_TABLE[OP_MATH_POW]     = op_math_pow;
	VM_TABLE[OP_MATH_SQRT]    = op_math_sqrt;
	VM_TABLE[OP_MATH_ABS]     = op_math_abs;
	VM_TABLE[OP_MATH_MIN]     = op_math_min;
	VM_TABLE[OP_MATH_MAX]     = op_math_max;
	VM_TABLE[OP_MATH_FLOOR]   = op_math_floor;
	VM_TABLE[OP_MATH_CEIL]    = op_math_ceil;

	VM_TABLE[OP_HALT]         = op_halt;
	VM_TABLE[OP_BREAK]        = op_break;
}

// ======================
// -- VM
// ======================

/// @brief Initialize the VM with a root chunk
/// @param root: Root chunk to execute
/// @return vm_t*
vm_t *vm_init(chunk_t *root)
{
	static int initialized = 0;
	if (!initialized) { init_vm_table(); initialized = 1; }

	vm_t *vm = calloc(1, sizeof(vm_t));

	vm_call_frame_t *frame = &vm->frames[vm->frame_count++];
	frame->chunk = root;
	frame->return_ip = 0;
	frame->stack_base = 0;

	vm->chunk = root;
	vm->ip = 0;

	return vm;
}

/// @brief Run the VM
/// @param vm: VM instance
void vm_run(vm_t *vm)
{
	while (vm->ip < vm->chunk->code_len) {
		instruction_t *inst = &vm->chunk->code[vm->ip++];
		vm_fn handler = VM_TABLE[inst->op];
		if (handler) {
			handler(vm, inst);
		} else {
			char buf[64];
			snprintf(buf, sizeof(buf), "unhandled opcode: %d", inst->op);
			vm_error(buf);
		}
	}
}

/// @brief Free the VM
/// @param vm: VM instance
void vm_free(vm_t *vm)
{
	for (int i = 0; i < vm->stack_top; i++)
		vm_val_release(&vm->stack[i]);
	for (int i = 0; i < vm->global_count; i++)
		vm_val_release(&vm->globals[i].val);
	free(vm);
}
