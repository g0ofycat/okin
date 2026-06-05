#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

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

/// @brief Hash function (FNV-1a)
/// @param key
/// @param len
/// @return uint32_t
static uint32_t hash_key(const char *key, size_t len) {
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < len; i++)
		h = (h ^ (uint8_t)key[i]) * 16777619u;
	return h;
}

/// @brief Find given slot in hash table
/// @param vm
/// @param key
/// @param len
/// @return global_entry_t
static global_entry_t *globals_find_slot(vm_t *vm, const char *key, size_t len) {
	uint32_t idx = hash_key(key, len) & (VM_GLOBALS_CAP - 1);
	for (;;) {
		global_entry_t *e = &vm->globals[idx];
		if (!e->occupied || (e->key_len == len && memcmp(e->key, key, len) == 0))
			return e;
		idx = (idx + 1) & (VM_GLOBALS_CAP - 1);
	}
}

/// @brief Get a global by name, returns NULL if not found
/// @param vm: VM instance
/// @param key: Global name
/// @param len: Name length
/// @return vm_val_t*
static vm_val_t *vm_get_global(vm_t *vm, const char *key, size_t len)
{
	global_entry_t *e = globals_find_slot(vm, key, len);
	return e->occupied ? &e->val : NULL;
}

/// @brief Set a global by name, creates if not found
/// @param vm: VM instance
/// @param key: Global name
/// @param len: Name length
/// @param val: Value to set
static void vm_set_global(vm_t *vm, const char *key, size_t len, vm_val_t val)
{
	if (vm->global_count >= VM_GLOBALS_CAP * 3 / 4) vm_error("too many globals");
	global_entry_t *e = globals_find_slot(vm, key, len);

	if (e->occupied) {
		vm_val_release(&e->val);
		e->val = val;
		return;
	}

	char *key_copy = malloc(len + 1);
	memcpy(key_copy, key, len);
	key_copy[len] = '\0';

	e->key      = key_copy;
	e->key_len  = len;
	e->val      = val;
	e->occupied = 1;
	vm->global_count++;
}

// ======================
// -- VALUE HELPERS
// ======================

/// @brief Equal check between two values
/// @param a: Left operand
/// @param b: Right operand
/// @return int (1 = equal)
static int val_equal(const vm_val_t *a, const vm_val_t *b)
{
	if (a->type != b->type) return 0;
	switch (a->type) {
		case VM_NIL:   return 1;
		case VM_BOOL:  return a->b == b->b;
		case VM_INT:   return a->i == b->i;
		case VM_FLOAT: return a->f == b->f;
		case VM_STR:   return strcmp(a->str->data, b->str->data) == 0;
		case VM_ARRAY: return a->arr == b->arr;
	}
	return 0;
}

/// @brief Value to float
/// @param v: Value to convert
/// @return double
static double val_to_float(const vm_val_t *v)
{
	if (v->type == VM_INT)   return (double)v->i;
	if (v->type == VM_FLOAT) return v->f;
	vm_error("expected numeric value");
	return 0;
}

/// @brief Print a value to stdout
/// @param v: Value to print
static void val_print(const vm_val_t *v)
{
	switch (v->type) {
		case VM_NIL:   printf("nil"); break;
		case VM_BOOL:  printf("%s", v->b ? "true" : "false"); break;
		case VM_INT:   printf("%lld", (long long)v->i); break;
		case VM_FLOAT: printf("%g", v->f); break;
		case VM_STR:   printf("%s", v->str->data); break;
		case VM_ARRAY:
					   printf("[");
					   for (int i = 0; i < v->arr->len; i++) {
						   if (i) printf(", ");
						   val_print(&v->arr->items[i]);
					   }
					   printf("]");
					   break;
	}
}

// ======================
// -- CONSTANTS & LOCALS
// ======================

/// @brief Get current frame chunk or chunk
/// @param vm
/// @return chunk_t
static inline chunk_t *current_chunk(vm_t *vm) {
	return vm->frame_count > 0 ? CURRENT_FRAME()->chunk : vm->chunk;
}

/// @brief Push a constant from the pool onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index)
static void op_load_const(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = current_chunk(vm)->constants[inst->a];
	vm_val_retain(&v);
	PUSH(v);
}

/// @brief Push a local variable (stack slot relative to frame base) onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = slot index)
static void op_load_local(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = CURRENT_FRAME()->locals[inst->a];
	vm_val_retain(&v);
	PUSH(v);
}

/// @brief Pop and store a value into a local slot
/// @param vm: VM instance
/// @param inst: Instruction (a = slot index)
static void op_store_local(vm_t *vm, const instruction_t *inst)
{
	if (inst->a > CURRENT_FRAME()->max_local)
		CURRENT_FRAME()->max_local = inst->a;
	vm_val_t *slot = &CURRENT_FRAME()->locals[inst->a];
	vm_val_release(slot);
	*slot = POP();
}

/// @brief Load a global by name (constant pool holds the name string) onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for name)
static void op_load_global(vm_t *vm, const instruction_t *inst)
{
	vm_val_t *name = &current_chunk(vm)->constants[inst->a];
	if (name->type != VM_STR || !name->str) vm_error("global lookup requires string identifier");
	vm_val_t *val = vm_get_global(vm, name->str->data, name->str->len);
	vm_val_t  v   = val ? *val : vm_val_nil();
	vm_val_retain(&v);
	PUSH(v);
}

/// @brief Pop and store a value into a global by name
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for name)
static void op_store_global(vm_t *vm, const instruction_t *inst)
{
	vm_val_t *name = &current_chunk(vm)->constants[inst->a];
	if (name->type != VM_STR || !name->str) vm_error("global storage requires string identifier");
	vm_val_t val = POP();
	vm_set_global(vm, name->str->data, name->str->len, val);
}

// ======================
// -- LITERALS
// ======================

/// @brief Push true onto the stack
static void op_true(vm_t *vm, const instruction_t *inst)  { PUSH(vm_val_bool(1)); }

/// @brief Push false onto the stack
static void op_false(vm_t *vm, const instruction_t *inst) { PUSH(vm_val_bool(0)); }

/// @brief Push nil onto the stack
static void op_nil(vm_t *vm, const instruction_t *inst)   { PUSH(vm_val_nil()); }

/// @brief Discard the top of stack
static void op_pop(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	vm_val_release(&v);
}

/// @brief Binary arithmetic: pops b then a, pushes result
/// @param vm: VM instance
/// @param inst: Instruction (op selects operation)
static void op_arith(vm_t *vm, const instruction_t *inst)
{
	vm_val_t a = POP(), b = POP();
	int both_int = (a.type == VM_INT && b.type == VM_INT);

	if (inst->op == OP_MOD) {
		if (!both_int) vm_error("mod requires integers");
		if (b.i == 0)  vm_error("modulo by zero");
		PUSH(vm_val_int(a.i % b.i));
		return;
	}

	if (both_int) {
		switch (inst->op) {
			case OP_ADD: PUSH(vm_val_int(a.i + b.i)); return;
			case OP_SUB: PUSH(vm_val_int(a.i - b.i)); return;
			case OP_MUL: PUSH(vm_val_int(a.i * b.i)); return;
			case OP_DIV:
						 if (b.i == 0) vm_error("division by zero");
						 PUSH(vm_val_int(a.i / b.i));
						 return;
			default: break;
		}
	}

	double fa = val_to_float(&a), fb = val_to_float(&b);

	switch (inst->op) {
		case OP_ADD: PUSH(vm_val_float(fa + fb)); break;
		case OP_SUB: PUSH(vm_val_float(fa - fb)); break;
		case OP_MUL: PUSH(vm_val_float(fa * fb)); break;
		case OP_DIV:
					 if (fb == 0.0) vm_error("division by zero");
					 PUSH(vm_val_float(fa / fb));
					 break;
		default: vm_error("unknown arith op"); break;

	}
}

// ======================
// -- COMPARISON
// ======================

/// @brief Binary comparison: pops b then a, pushes bool result
/// @param vm: VM instance
/// @param inst: Instruction (op selects comparison)
static void op_cmp(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();

	if (inst->op == OP_EQ)  { PUSH(vm_val_bool( val_equal(&a, &b))); return; }
	if (inst->op == OP_NEQ) { PUSH(vm_val_bool(!val_equal(&a, &b))); return; }

	double fa = val_to_float(&a), fb = val_to_float(&b);

	switch (inst->op) {
		case OP_GT:  PUSH(vm_val_bool(fa >  fb)); break;
		case OP_LT:  PUSH(vm_val_bool(fa <  fb)); break;
		case OP_GTE: PUSH(vm_val_bool(fa >= fb)); break;
		case OP_LTE: PUSH(vm_val_bool(fa <= fb)); break;
		default: vm_error("unknown cmp op"); break;
	}
}

// ======================
// -- LOGICAL
// ======================

/// @brief Logical AND: short-circuits, pops both, pushes bool
static void op_and(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	PUSH(vm_val_bool(vm_val_truthy(&a) && vm_val_truthy(&b)));
}

/// @brief Logical OR: short-circuits, pops both, pushes bool
static void op_or(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	PUSH(vm_val_bool(vm_val_truthy(&a) || vm_val_truthy(&b)));
}

/// @brief Logical NOT: pops one, pushes inverted bool
static void op_not(vm_t *vm, const instruction_t *inst)
{
	vm_val_t a = POP();
	PUSH(vm_val_bool(!vm_val_truthy(&a)));
}

// ======================
// -- JUMPS
// ======================

/// @brief General jump
/// @param vm
/// @param target
static inline void vm_jump(vm_t *vm, int target)
{
	CURRENT_FRAME()->local_ip = target;
}

/// @brief Unconditional jump
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jmp(vm_t *vm, const instruction_t *inst)
{
	vm_jump(vm, inst->a);
}

/// @brief Jump if top of stack is falsy, pops condition
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jmp_false(vm_t *vm, const instruction_t *inst)
{
	vm_val_t cond = POP();
	if (!vm_val_truthy(&cond))
		vm_jump(vm, inst->a);
}

/// @brief Jump if popped a == b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jeq(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (val_equal(&a, &b))
		vm_jump(vm, inst->a);
}

/// @brief Jump if popped a != b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jne(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (!val_equal(&a, &b))
		vm_jump(vm, inst->a);
}

/// @brief Jump if popped a > b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jgt(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (val_to_float(&a) > val_to_float(&b))
		vm_jump(vm, inst->a);
}

/// @brief Jump if popped a < b
/// @param vm: VM instance
/// @param inst: Instruction (a = target ip)
static void op_jlt(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (val_to_float(&a) < val_to_float(&b))
		vm_jump(vm, inst->a);
}

// ======================
// -- FUNCTIONS
// ======================

/// @brief Push a function (sub-chunk) reference onto the stack
/// @param vm: VM instance
/// @param inst: Instruction (a = sub_chunk index)
static void op_load_func(vm_t *vm, const instruction_t *inst)
{
	PUSH(vm_val_int(inst->a));
}

/// @brief Call a function: expects callee below args on stack
/// @param vm: VM instance
/// @param inst: Instruction (a = arg count)
static void op_call(vm_t *vm, const instruction_t *inst)
{
	int arg_count = inst->a;
	if (vm->stack_top < arg_count + 1) vm_error("stack underflow in call");

	vm_val_t callee = vm->stack[vm->stack_top - 1];

	if (callee.type != VM_INT) vm_error("call on non-function");
	if (vm->frame_count >= VM_CALL_STACK_MAX) vm_error("call stack overflow");

	chunk_t *fn_chunk = current_chunk(vm)->sub_chunks[callee.i];

	int return_ip = CURRENT_FRAME()->local_ip;

	vm_call_frame_t *frame = &vm->frames[vm->frame_count++];
	frame->chunk      = fn_chunk;
	frame->return_ip  = return_ip;
	frame->local_ip   = 0;
	frame->stack_base = vm->stack_top - arg_count - 1;
	frame->max_local  = arg_count > 0 ? arg_count - 1 : 0;

	memset(frame->locals, 0, sizeof(frame->locals));

	for (int i = 0; i < arg_count; i++) {
		frame->locals[i] = vm->stack[frame->stack_base + i];
		vm_val_retain(&frame->locals[i]);
	}

	vm->stack_top = frame->stack_base;
}

/// @brief Return from a function, restoring caller frame
/// @param vm: VM instance
/// @param inst: Instruction
static void op_ret(vm_t *vm, const instruction_t *inst)
{
	vm_val_t retval = POP();
	vm_call_frame_t *frame = &vm->frames[--vm->frame_count];

	for (int i = 0; i <= frame->max_local; i++)
		vm_val_release(&frame->locals[i]);

	vm->stack_top = frame->stack_base;

	if (vm->frame_count > 0)
		vm->frames[vm->frame_count - 1].local_ip = frame->return_ip;

	PUSH(retval);
}

// ======================
// -- ARRAYS
// ======================

/// @brief Build an array from the top N stack values, pushes array
/// @param vm: VM instance
/// @param inst: Instruction (a = element count)
static void op_make_array(vm_t *vm, const instruction_t *inst)
{
	int count = inst->a;
	vm_val_t arr = vm_val_array();
	for (int i = 0; i < count; i++) {
		vm_val_t element = POP(); 
		vm_array_push(arr.arr, element);
	}
	PUSH(arr);
}

/// @brief Index into an array: pops index then array, pushes element
/// @param vm: VM instance
/// @param inst: Instruction
static void op_array_get(vm_t *vm, const instruction_t *inst)
{
	vm_val_t idx = POP(), arr = POP();
	if (arr.type != VM_ARRAY) vm_error("array_get on non-array");
	if (idx.type != VM_INT)   vm_error("array index must be integer");
	if (idx.i < 0 || idx.i >= arr.arr->len) vm_error("array index out of bounds");
	vm_val_t v = arr.arr->items[idx.i];
	vm_val_retain(&v);
	vm_val_release(&arr);
	PUSH(v);
}

/// @brief Set array element: pops value, index, array (array is mutated in-place)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_array_set(vm_t *vm, const instruction_t *inst)
{
	vm_val_t val = POP(), idx = POP(), arr = POP();
	if (arr.type != VM_ARRAY) vm_error("array_set on non-array");
	if (idx.type != VM_INT)   vm_error("array index must be integer");
	if (idx.i < 0 || idx.i >= arr.arr->len) vm_error("array index out of bounds");
	vm_val_release(&arr.arr->items[idx.i]);
	arr.arr->items[idx.i] = val;
	vm_val_release(&arr);
}

/// @brief Membership check: pops array then item, pushes bool
/// @param vm: VM instance
/// @param inst: Instruction
static void op_in(vm_t *vm, const instruction_t *inst)
{
	vm_val_t arr = POP(), item = POP();
	if (arr.type != VM_ARRAY) vm_error("'in' requires array");
	for (int i = 0; i < arr.arr->len; i++) {
		if (val_equal(&item, &arr.arr->items[i])) {
			vm_val_release(&arr);
			PUSH(vm_val_bool(1));
			return;
		}
	}
	vm_val_release(&arr);
	PUSH(vm_val_bool(0));
}

// ======================
// -- IO
// ======================

/// @brief Write top of stack to stdout (no newline), pops value
/// @param vm: VM instance
/// @param inst: Instruction
static void op_io_write(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	val_print(&v);
	vm_val_release(&v);
}

/// @brief Write top of stack to stdout with newline, pops value
/// @param vm: VM instance
/// @param inst: Instruction
static void op_io_writeln(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	val_print(&v);
	printf("\n");
	vm_val_release(&v);
}

/// @brief Read a line from stdin, store into global named by constant[a]
/// @param vm: VM instance
/// @param inst: Instruction (a = constant index for global name)
static void op_io_read(vm_t *vm, const instruction_t *inst)
{
	char buf[1024];
	if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
	size_t len = strlen(buf);
	if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
	vm_val_t *name = &vm->chunk->constants[inst->a];
	if (name->type != VM_STR || !name->str) vm_error("io_read target variable key must be string");
	vm_set_global(vm, name->str->data, name->str->len, vm_val_str(buf, len));
}

// ======================
// -- STRING
// ======================

/// @brief Push string length of top of stack, pops string
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_len(vm_t *vm, const instruction_t *inst)
{
	vm_val_t s = POP();
	if (s.type != VM_STR || !s.str) vm_error("str_len on non-string");
	int64_t len = (int64_t)s.str->len;
	vm_val_release(&s);
	PUSH(vm_val_int(len));
}

/// @brief Concatenate two strings: pops b then a, pushes result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_concat(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (a.type != VM_STR || b.type != VM_STR || !a.str || !b.str) vm_error("str_concat on non-string");
	size_t la = a.str->len, lb = b.str->len;
	char *buf = malloc(la + lb + 1);
	memcpy(buf, a.str->data, la);
	memcpy(buf + la, b.str->data, lb);
	buf[la + lb] = '\0';
	vm_val_release(&a); vm_val_release(&b);
	vm_val_t result = vm_val_str(buf, la + lb);
	free(buf);
	PUSH(result);
}

/// @brief Slice a string: pops end, start, string; pushes substring
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_slice(vm_t *vm, const instruction_t *inst)
{
	vm_val_t end = POP(), start = POP(), s = POP();
	if (s.type != VM_STR || !s.str)                                 vm_error("str_slice on non-string");
	if (start.type != VM_INT || end.type != VM_INT) vm_error("str_slice indices must be integers");
	int64_t slen = (int64_t)s.str->len;
	int64_t lo   = start.i, hi = end.i;
	if (lo < 0 || hi > slen || lo > hi) vm_error("str_slice out of bounds");
	vm_val_t result = vm_val_str(s.str->data + lo, (size_t)(hi - lo));
	vm_val_release(&s);
	PUSH(result);
}

/// @brief Find substring: pops pattern then string, pushes index or -1
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_find(vm_t *vm, const instruction_t *inst)
{
	vm_val_t pat = POP(), s = POP();
	if (s.type != VM_STR || pat.type != VM_STR || !s.str || !pat.str) vm_error("str_find on non-string");
	char    *found = strstr(s.str->data, pat.str->data);
	int64_t  idx   = found ? (int64_t)(found - s.str->data) : -1;
	vm_val_release(&s); vm_val_release(&pat);
	PUSH(vm_val_int(idx));
}

/// @brief For upper and lower string logic
/// @param vm
/// @param transform_fn
static inline void op_str_transform(vm_t *vm, int (*transform_fn)(int))
{
	vm_val_t s = POP();
	if (s.type != VM_STR || !s.str) vm_error("str_transform on non-string");

	size_t len = s.str->len;
	char *buf = malloc(len + 1);
	for (size_t i = 0; i <= len; i++) {
		buf[i] = (char)transform_fn((unsigned char)s.str->data[i]);
	}

	vm_val_release(&s);
	vm_val_t result = vm_val_str(buf, len);
	free(buf);
	PUSH(result);
}

/// @brief Uppercase: pops string, pushes uppercased copy
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_upper(vm_t *vm, const instruction_t *inst) { op_str_transform(vm, toupper); }

/// @brief Lowercase: pops string, pushes lowercased copy
/// @param vm: VM instance
/// @param inst: Instruction
static void op_str_lower(vm_t *vm, const instruction_t *inst) { op_str_transform(vm, tolower); }

// ======================
// -- MATH
// ======================

/// @brief Power: pops exp then base, pushes float result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_pow(vm_t *vm, const instruction_t *inst)
{
	vm_val_t exp = POP(), base = POP();
	PUSH(vm_val_float(pow(val_to_float(&base), val_to_float(&exp))));
}

/// @brief Square root: pops value, pushes float result
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_sqrt(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	PUSH(vm_val_float(sqrt(val_to_float(&v))));
}

/// @brief Absolute value: pops value, pushes abs (preserves int/float type)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_abs(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	if (v.type == VM_INT)   { PUSH(vm_val_int(v.i < 0 ? -v.i : v.i)); return; }
	if (v.type == VM_FLOAT) { PUSH(vm_val_float(fabs(v.f))); return; }
	vm_error("abs on non-numeric");
}

/// @brief Minimum: pops b then a, pushes smaller (preserves int if both int)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_min(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (a.type == VM_INT && b.type == VM_INT) { PUSH(a.i <= b.i ? a : b); return; }
	PUSH(val_to_float(&a) <= val_to_float(&b) ? a : b);
}

/// @brief Maximum: pops b then a, pushes larger (preserves int if both int)
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_max(vm_t *vm, const instruction_t *inst)
{
	vm_val_t b = POP(), a = POP();
	if (a.type == VM_INT && b.type == VM_INT) { PUSH(a.i >= b.i ? a : b); return; }
	PUSH(val_to_float(&a) >= val_to_float(&b) ? a : b);
}

/// @brief Floor: pops float, pushes int
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_floor(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	PUSH(vm_val_int((int64_t)floor(val_to_float(&v))));
}

/// @brief Ceil: pops float, pushes int
/// @param vm: VM instance
/// @param inst: Instruction
static void op_math_ceil(vm_t *vm, const instruction_t *inst)
{
	vm_val_t v = POP();
	PUSH(vm_val_int((int64_t)ceil(val_to_float(&v))));
}

// ======================
// -- VM
// ======================

/// @brief Initialize the VM with a root chunk
/// @param root: Root chunk to execute
/// @return vm_t*
vm_t *vm_init(chunk_t *root)
{
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
	if (vm->frame_count == 0) return;

#define NEXT() \
	do { \
		frame = CURRENT_FRAME(); \
		if (frame->local_ip >= frame->chunk->code_len) goto halt; \
		inst = &frame->chunk->code[frame->local_ip++]; \
		goto *dispatch[inst->op]; \
	} while (0)

	vm_call_frame_t *frame = CURRENT_FRAME();
	instruction_t   *inst;

	static const void *dispatch[] = {
		[OP_LOAD_CONST]   = &&op_load_const,
		[OP_LOAD_LOCAL]   = &&op_load_local,
		[OP_STORE_LOCAL]  = &&op_store_local,
		[OP_LOAD_GLOBAL]  = &&op_load_global,
		[OP_STORE_GLOBAL] = &&op_store_global,
		[OP_TRUE]         = &&op_true,
		[OP_FALSE]        = &&op_false,
		[OP_NIL]          = &&op_nil,
		[OP_POP]          = &&op_pop,
		[OP_ADD]          = &&op_arith,
		[OP_SUB]          = &&op_arith,
		[OP_MUL]          = &&op_arith,
		[OP_DIV]          = &&op_arith,
		[OP_MOD]          = &&op_arith,
		[OP_EQ]           = &&op_cmp,
		[OP_NEQ]          = &&op_cmp,
		[OP_GT]           = &&op_cmp,
		[OP_LT]           = &&op_cmp,
		[OP_GTE]          = &&op_cmp,
		[OP_LTE]          = &&op_cmp,
		[OP_AND]          = &&op_and,
		[OP_OR]           = &&op_or,
		[OP_NOT]          = &&op_not,
		[OP_JMP]          = &&op_jmp,
		[OP_JMP_FALSE]    = &&op_jmp_false,
		[OP_JEQ]          = &&op_jeq,
		[OP_JNE]          = &&op_jne,
		[OP_JGT]          = &&op_jgt,
		[OP_JLT]          = &&op_jlt,
		[OP_LOAD_FUNC]    = &&op_load_func,
		[OP_CALL]         = &&op_call,
		[OP_RET]          = &&op_ret,
		[OP_MAKE_ARRAY]   = &&op_make_array,
		[OP_ARRAY_GET]    = &&op_array_get,
		[OP_ARRAY_SET]    = &&op_array_set,
		[OP_IN]           = &&op_in,
		[OP_IO_WRITE]     = &&op_io_write,
		[OP_IO_WRITELN]   = &&op_io_writeln,
		[OP_IO_READ]      = &&op_io_read,
		[OP_STR_LEN]      = &&op_str_len,
		[OP_STR_CONCAT]   = &&op_str_concat,
		[OP_STR_SLICE]    = &&op_str_slice,
		[OP_STR_FIND]     = &&op_str_find,
		[OP_STR_UPPER]    = &&op_str_upper,
		[OP_STR_LOWER]    = &&op_str_lower,
		[OP_MATH_POW]     = &&op_math_pow,
		[OP_MATH_SQRT]    = &&op_math_sqrt,
		[OP_MATH_ABS]     = &&op_math_abs,
		[OP_MATH_MIN]     = &&op_math_min,
		[OP_MATH_MAX]     = &&op_math_max,
		[OP_MATH_FLOOR]   = &&op_math_floor,
		[OP_MATH_CEIL]    = &&op_math_ceil,
		[OP_BREAK]        = &&op_jmp,
		[OP_HALT]         = &&halt,
	};

	NEXT();

op_load_const:   op_load_const(vm, inst);   NEXT();
op_load_local:   op_load_local(vm, inst);   NEXT();
op_store_local:  op_store_local(vm, inst);  NEXT();
op_load_global:  op_load_global(vm, inst);  NEXT();
op_store_global: op_store_global(vm, inst); NEXT();
op_true:         op_true(vm, inst);         NEXT();
op_false:        op_false(vm, inst);        NEXT();
op_nil:          op_nil(vm, inst);          NEXT();
op_pop:          op_pop(vm, inst);          NEXT();
op_arith:        op_arith(vm, inst);        NEXT();
op_cmp:          op_cmp(vm, inst);          NEXT();
op_and:          op_and(vm, inst);          NEXT();
op_or:           op_or(vm, inst);           NEXT();
op_not:          op_not(vm, inst);          NEXT();
op_jmp:          op_jmp(vm, inst);          NEXT();
op_jmp_false:    op_jmp_false(vm, inst);    NEXT();
op_jeq:          op_jeq(vm, inst);          NEXT();
op_jne:          op_jne(vm, inst);          NEXT();
op_jgt:          op_jgt(vm, inst);          NEXT();
op_jlt:          op_jlt(vm, inst);          NEXT();
op_load_func:    op_load_func(vm, inst);    NEXT();
op_call:         op_call(vm, inst);         frame = CURRENT_FRAME(); NEXT();
op_ret:          op_ret(vm, inst);          frame = CURRENT_FRAME(); NEXT();
op_make_array:   op_make_array(vm, inst);   NEXT();
op_array_get:    op_array_get(vm, inst);    NEXT();
op_array_set:    op_array_set(vm, inst);    NEXT();
op_in:           op_in(vm, inst);           NEXT();
op_io_write:     op_io_write(vm, inst);     NEXT();
op_io_writeln:   op_io_writeln(vm, inst);   NEXT();
op_io_read:      op_io_read(vm, inst);      NEXT();
op_str_len:      op_str_len(vm, inst);      NEXT();
op_str_concat:   op_str_concat(vm, inst);   NEXT();
op_str_slice:    op_str_slice(vm, inst);    NEXT();
op_str_find:     op_str_find(vm, inst);     NEXT();
op_str_upper:    op_str_upper(vm, inst);    NEXT();
op_str_lower:    op_str_lower(vm, inst);    NEXT();
op_math_pow:     op_math_pow(vm, inst);     NEXT();
op_math_sqrt:    op_math_sqrt(vm, inst);    NEXT();
op_math_abs:     op_math_abs(vm, inst);     NEXT();
op_math_min:     op_math_min(vm, inst);     NEXT();
op_math_max:     op_math_max(vm, inst);     NEXT();
op_math_floor:   op_math_floor(vm, inst);   NEXT();
op_math_ceil:    op_math_ceil(vm, inst);    NEXT();

halt:
				 return;

#undef NEXT
}

/// @brief Free the VM
/// @param vm: VM instance
void vm_free(vm_t *vm)
{
	for (int i = 0; i < vm->stack_top; i++)
		vm_val_release(&vm->stack[i]);
	for (int i = 0; i < VM_GLOBALS_CAP; i++) {
		if (!vm->globals[i].occupied) continue;
		vm_val_release(&vm->globals[i].val);
		free((char *)vm->globals[i].key);
	}
	free(vm);
}
