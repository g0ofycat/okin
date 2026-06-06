#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "compiler.h"

// ======================
// -- UTILITY
// ======================

/// @brief Emit a compiler error
/// @param c: Compiler instance
/// @param msg: Error message
static void compiler_error(compiler_t *c, const char *msg)
{
	fprintf(stderr, "[okin compiler error] %s\n", msg);
	c->errors++;
}

/// @brief Resolve a name to a local slot or emit a global load
/// @param c: Compiler instance
/// @param name: Variable name
/// @param len: Name length
static void load_name(compiler_t *c, const char *name, size_t len)
{
	int slot = scope_resolve(c->scope, name, len);
	if (slot >= 0) { chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot); return; }
	if (slot == SCOPE_NOT_FOUND && c->current_scope != c->root) {
		char msg[256];
		snprintf(msg, sizeof(msg), "undefined variable '%.*s'", (int)len, name);
		compiler_error(c, msg);
		return;
	}
	chunk_emit(c->current_scope, OP_LOAD_GLOBAL, chunk_add_const(c->current_scope, vm_val_str(name, len)));
}

/// @brief Store a value into a local slot or global by name
/// @param c: Compiler instance
/// @param name: Variable name
/// @param len: Name length
static void store_name(compiler_t *c, const char *name, size_t len)
{
	int slot = scope_resolve(c->scope, name, len);
	if (slot >= 0) { chunk_emit(c->current_scope, OP_STORE_LOCAL, slot); return; }
	if (slot == SCOPE_NOT_FOUND && c->current_scope != c->root) {
		char msg[256];
		snprintf(msg, sizeof(msg), "undefined variable '%.*s'", (int)len, name);
		compiler_error(c, msg);
		return;
	}
	chunk_emit(c->current_scope, OP_STORE_GLOBAL, chunk_add_const(c->current_scope, vm_val_str(name, len)));
}

/// @brief Look up a registered function index by name, returns -1 if not found
/// @param c: Compiler instance
/// @param name: Function name
/// @param len: Name length
/// @return int: Function Index, or -1 if not found
static int resolve_func(const compiler_t *c, const char *name, size_t len)
{
	for (int i = 0; i < c->func_count; i++) {
		const func_entry_t *e = &c->functions[i];
		if (e->name_len == len && memcmp(e->name, name, len) == 0)
			return e->index;
	}
	return -1;
}

/// @brief Register a function name to a sub-chunk index
/// @param c: Compiler instance
/// @param name: Function name
/// @param len: Name length
/// @param index: Sub-chunk Index
static void register_func(compiler_t *c, const char *name, size_t len, int index)
{
	if (c->func_count >= MAX_FUNCTIONS) { compiler_error(c, "too many functions"); return; }
	func_entry_t *e = &c->functions[c->func_count++];
	e->name     = name;
	e->name_len = len;
	e->index    = index;
}

/// @brief Emit optional names
/// @param c
/// @param node
/// @param arg_index
/// @return 1 on success, 0 on failure
static inline int emit_optional_store(compiler_t *c, const okin_node_t *node, int arg_index)
{
	if (node->argc > arg_index && node->args[arg_index]->tok == TK_VALUE) {
		store_name(c, node->args[arg_index]->val_start, node->args[arg_index]->val_len);
		return 1;
	}
	return 0;
}

// ======================
// -- FORWARD DECL
// ======================

static void compile_node(compiler_t *c, const okin_node_t *node);
static void compile_body(compiler_t *c, okin_node_t **nodes, int len);

// ======================
// -- CORE
// ======================

/// @brief Compile a leaf node to a constant load or name load
/// @param c: Compiler instance
/// @param node: Leaf node
static void compile_leaf(compiler_t *c, const okin_node_t *node)
{
	if (node->tok == TK_VALUE) { load_name(c, node->val_start, node->val_len); return; }

	char buf[32];
	vm_val_t val;

	if (node->tok == TK_INT_LIT || node->tok == TK_FLOAT_LIT) {
		size_t len = (node->val_len - 1) < 31 ? (node->val_len - 1) : 31;
		memcpy(buf, node->val_start + 1, len); buf[len] = '\0';
		val = node->tok == TK_INT_LIT ? vm_val_int(atoll(buf)) : vm_val_float(atof(buf));
	} else if (node->tok == TK_INT) {
		size_t len = node->val_len < 31 ? node->val_len : 31;
		memcpy(buf, node->val_start, len); buf[len] = '\0';
		val = memchr(buf, '.', len) ? vm_val_float(atof(buf)) : vm_val_int(atoll(buf));
	} else if (node->tok == TK_STRING) {
		val = vm_val_str(node->val_start, node->val_len);
	} else {
		val = vm_val_nil();
	}

	chunk_emit(c->current_scope, OP_LOAD_CONST, chunk_add_const(c->current_scope, val));
}

/// @brief Compile VAR declaration
/// @param c: Compiler instance
/// @param node: VAR node - args: NAME, VALUE
static void compile_var(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[1]);

	if (c->current_scope == c->root) {
		chunk_emit(c->current_scope, OP_STORE_GLOBAL,
				chunk_add_const(c->current_scope,
					vm_val_str(node->args[0]->val_start, node->args[0]->val_len)));
		return;
	}

	int slot = scope_declare(c->scope, node->args[0]->val_start, node->args[0]->val_len);
	if (slot == -1) { compiler_error(c, "too many locals"); return; }
	c->current_scope->num_locals++;
	chunk_emit(c->current_scope, OP_STORE_LOCAL, slot);
}

/// @brief Compile SET reassignment
/// @param c: Compiler instance
/// @param node: SET node - args: NAME, VALUE
static void compile_set(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[1]);
	store_name(c, node->args[0]->val_start, node->args[0]->val_len);
}

/// @brief Compile GLOBAL - marks names as forced-global
/// @param c: Compiler instance
/// @param node: GLOBAL node - args: ...NAMES
static void compile_global(compiler_t *c, const okin_node_t *node)
{
	for (int i = 0; i < node->argc; i++)
		scope_mark_global(c->scope, node->args[i]->val_start, node->args[i]->val_len);
}

/// @brief Compile RET
/// @param c: Compiler instance
/// @param node: RET node - args: VALUE
static void compile_ret(compiler_t *c, const okin_node_t *node)
{
	if (node->argc > 0) compile_node(c, node->args[0]);
	else chunk_emit(c->current_scope, OP_NIL, 0);
	chunk_emit(c->current_scope, OP_RET, 0);
}

/// @brief Compile BREAK
/// @param c: Compiler instance
/// @param node: BREAK node
static void compile_break(compiler_t *c, const okin_node_t *node)
{
	if (c->break_count >= MAX_BREAK_PATCHES) { compiler_error(c, "too many breaks"); return; }
	c->break_patches[c->break_count++] = chunk_emit(c->current_scope, OP_JMP, 0);
}

// ======================
// -- ARITHMETIC
// ======================

/// @brief Compile arithmetic node, storing result into DEST
/// @param c: Compiler instance
/// @param node: Arith node - args: A, B, DEST
static void compile_arith(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	switch (node->opcode) {
		case ADD: chunk_emit(c->current_scope, OP_ADD, 0); break;
		case SUB: chunk_emit(c->current_scope, OP_SUB, 0); break;
		case MUL: chunk_emit(c->current_scope, OP_MUL, 0); break;
		case DIV: chunk_emit(c->current_scope, OP_DIV, 0); break;
		case MOD: chunk_emit(c->current_scope, OP_MOD, 0); break;
	}
	emit_optional_store(c, node, 2);
}

// ======================
// -- COMPARISON
// ======================

/// @brief Compile comparison node (EQ, NEQ, GT, LT, GTE, LTE)
/// @param c: Compiler instance
/// @param node: Cmp node - args: A, B
static void compile_cmp(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	switch (node->opcode) {
		case EQ:  chunk_emit(c->current_scope, OP_EQ,  0); break;
		case NEQ: chunk_emit(c->current_scope, OP_NEQ, 0); break;
		case GT:  chunk_emit(c->current_scope, OP_GT,  0); break;
		case LT:  chunk_emit(c->current_scope, OP_LT,  0); break;
		case GTE: chunk_emit(c->current_scope, OP_GTE, 0); break;
		case LTE: chunk_emit(c->current_scope, OP_LTE, 0); break;
	}
}

// ======================
// -- LOGICAL
// ======================

/// @brief Compile logical node (AND, OR, NOT)
/// @param c: Compiler instance
/// @param node: Logical node - args: A[, B]
static void compile_logical(compiler_t *c, const okin_node_t *node)
{
	if (node->opcode == NOT) {
		compile_node(c, node->args[0]);
		chunk_emit(c->current_scope, OP_NOT, 0);
		return;
	}
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	switch (node->opcode) {
		case AND: chunk_emit(c->current_scope, OP_AND, 0); break;
		case OR:  chunk_emit(c->current_scope, OP_OR,  0); break;

	}
}

// ======================
// -- CONTROL FLOW
// ======================

/// @brief Add a patch entry
/// @param pl
/// @param idx
static void pl_add(patch_list_t *pl, int idx) {
	pl->entries[pl->count++] = idx;
}

/// @brief Resolve a patch
/// @param pl
/// @param chunk
static void pl_resolve(patch_list_t *pl, chunk_t *chunk) {
	int target = chunk->code_len;
	for (int i = 0; i < pl->count; i++)
		chunk_patch(chunk, pl->entries[i], target);
}

/// @brief Compile a CALL instruction
/// @param c: Compiler instance
/// @param node: CALL node - args: NAME, ...ARGS, DEST
/// @param as_expr: Call without storing / emitting
static void compile_call(compiler_t *c, const okin_node_t *node, int as_expr)
{
	int argc      = node->argc;
	int has_dest  = !as_expr && (argc > 1 && node->args[argc - 1]->tok == TK_VALUE);
	int arg_count = has_dest ? argc - 2 : argc - 1;

	const char *name = node->args[0]->val_start;
	size_t      len  = node->args[0]->val_len;
	int fn_idx = resolve_func(c, name, len);
	if (fn_idx != -1)
		chunk_emit(c->current_scope, OP_LOAD_FUNC, fn_idx);
	else
		load_name(c, name, len);

	for (int i = 1; i <= arg_count; i++)
		compile_node(c, node->args[i]);

	chunk_emit(c->current_scope, OP_CALL, arg_count);

	if (as_expr)  return;
	if (has_dest) emit_optional_store(c, node, argc - 1);
	else          chunk_emit(c->current_scope, OP_POP, 0);
}

/// @brief Add a scope to a body before compiling
/// @param c
/// @param node
static void compile_scoped_body(compiler_t *c, okin_node_t *node) {
	scope_begin(c->scope);
	compile_body(c, node->body, node->body_len);
	scope_end(c->scope);
}

/// @brief Compile body
/// @param c
/// @param nodes
/// @param len
static void compile_body(compiler_t *c, okin_node_t **nodes, int len) {
	for (int i = 0; i < len; ) {
		if (nodes[i]->opcode == CALL) { compile_call(c, nodes[i++], 0); continue; }
		if (nodes[i]->opcode != IF) { compile_node(c, nodes[i++]); continue; }
		patch_list_t ends = {0};
		while (i < len && (nodes[i]->opcode == IF || nodes[i]->opcode == ELIF)) {
			compile_node(c, nodes[i]->args[0]);
			int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);
			compile_scoped_body(c, nodes[i]);
			pl_add(&ends, chunk_emit(c->current_scope, OP_JMP, 0));
			chunk_patch(c->current_scope, jf, c->current_scope->code_len);
			i++;
		}
		if (i < len && nodes[i]->opcode == ELSE) {
			okin_node_t *else_node = nodes[i++];
			if (else_node->body_len > 0)
				compile_scoped_body(c, else_node);
			else if (else_node->args[0])
				compile_node(c, else_node->args[0]);
		}
		pl_resolve(&ends, c->current_scope);
	}
}

/// @brief Compile WHILE loop with backpatching
/// @param c: Compiler instance
/// @param node: WHILE node - args: COND | body
static void compile_while(compiler_t *c, const okin_node_t *node)
{
	int loop_start = c->current_scope->code_len;
	compile_node(c, node->args[0]);

	scope_begin(c->scope);

	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);
	int incoming_break_start = c->break_count;

	compile_body(c, node->body, node->body_len);

	chunk_emit(c->current_scope, OP_JMP, loop_start);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);

	scope_end(c->scope);

	for (int i = incoming_break_start; i < c->break_count; i++)
		chunk_patch(c->current_scope, c->break_patches[i], c->current_scope->code_len);

	c->break_count = incoming_break_start;
}

/// @brief Compile FOR range loop
/// @param c: Compiler instance
/// @param node: FOR node - args: INDEX, START, END, STEP | body
static void compile_for(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[1]);

	scope_begin(c->scope);

	int slot = scope_declare(c->scope, node->args[0]->val_start, node->args[0]->val_len);
	if (slot == -1) { compiler_error(c, "too many locals"); return; }

	c->current_scope->num_locals++;
	chunk_emit(c->current_scope, OP_STORE_LOCAL, slot);

	int loop_start = c->current_scope->code_len;
	int incoming_break_start = c->break_count;

	chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot);
	compile_node(c, node->args[2]);
	chunk_emit(c->current_scope, OP_LT, 0);

	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);

	compile_body(c, node->body, node->body_len);

	chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot);
	compile_node(c, node->args[3]);
	chunk_emit(c->current_scope, OP_ADD, 0);
	chunk_emit(c->current_scope, OP_STORE_LOCAL, slot);
	chunk_emit(c->current_scope, OP_JMP, loop_start);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);

	scope_end(c->scope);

	for (int i = incoming_break_start; i < c->break_count; i++)
		chunk_patch(c->current_scope, c->break_patches[i], c->current_scope->code_len);

	c->break_count = incoming_break_start;
}

// ======================
// -- JUMPS
// ======================

/// @brief Compile jump instructions using parser-resolved label indices
/// @param c: Compiler instance
/// @param node: Jump node
static void compile_jmp(compiler_t *c, const okin_node_t *node)
{
	if (node->opcode == JMP) {
		chunk_emit(c->current_scope, OP_JMP, node->args[0]->jump_index);
		return;
	}
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	int target = node->args[2]->jump_index;
	switch (node->opcode) {
		case JEQ: chunk_emit(c->current_scope, OP_JEQ, target); break;
		case JNE: chunk_emit(c->current_scope, OP_JNE, target); break;
		case JGT: chunk_emit(c->current_scope, OP_JGT, target); break;
		case JLT: chunk_emit(c->current_scope, OP_JLT, target); break;
	}
}

// ======================
// -- FUNCTIONS
// ======================

/// @brief Pre-registration pass, registers all function names before compilation
/// @param c: Compiler instance
static void pre_register_functions(compiler_t *c)
{
	for (int i = 0; i < c->parser->program->len; i++) {
		const okin_node_t *node = c->parser->program->nodes[i];
		if (node->opcode != FUNCTION) continue;
		chunk_t *fn_chunk = chunk_init(node->args[0]->val_start);
		int sub_idx       = chunk_add_sub(c->root, fn_chunk);
		register_func(c, node->args[0]->val_start, node->args[0]->val_len, sub_idx);
	}
}

/// @brief Compile a FUNCTION definition into its pre-registered sub-chunk
/// @param c: Compiler instance
/// @param node: FUNCTION node - args: NAME, ...PARAMS | body
static void compile_function(compiler_t *c, const okin_node_t *node)
{
	int sub_idx = resolve_func(c, node->args[0]->val_start, node->args[0]->val_len);
	if (sub_idx < 0 || sub_idx >= c->root->sub_len) {
		compiler_error(c, "invalid function index");
		return;
	}

	chunk_t  *fn_chunk     = c->root->sub_chunks[sub_idx];
	chunk_t  *parent       = c->current_scope;
	scope_t  *parent_scope = c->scope;

	c->current_scope = fn_chunk;
	c->scope         = scope_init(parent_scope);
	scope_begin(c->scope);

	for (int i = 1; i < node->argc; i++) {
		int slot = scope_declare(c->scope, node->args[i]->val_start, node->args[i]->val_len);
		if (slot == -1) {
			compiler_error(c, "too many local parameters");
			scope_end(c->scope);
			scope_free(c->scope);
			c->scope         = parent_scope;
			c->current_scope = parent;
			return;
		}
		fn_chunk->num_params++;
		fn_chunk->num_locals++;
	}

	compile_body(c, node->body, node->body_len);

	chunk_emit(c->current_scope, OP_NIL, 0);
	chunk_emit(c->current_scope, OP_RET, 0);

	scope_end(c->scope);
	scope_free(c->scope);

	c->scope         = parent_scope;
	c->current_scope = parent;
}

// ======================
// -- ARRAYS
// ======================

/// @brief Compile an ARRAY literal
/// @param c: Compiler instance
/// @param node: ARRAY node - args: ...VALUES
static void compile_array(compiler_t *c, const okin_node_t *node)
{
	for (int i = 0; i < node->argc; i++) compile_node(c, node->args[i]);
	chunk_emit(c->current_scope, OP_MAKE_ARRAY, node->argc);
}

/// @brief Compile AGET (array get)
/// @param c: Compiler instance
/// @param node: AGET node - args: ARRAY, INDEX, DEST
static void compile_aget(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	chunk_emit(c->current_scope, OP_ARRAY_GET, 0);
	emit_optional_store(c, node, 2);
}

/// @brief Compile ASET (array set)
/// @param c: Compiler instance
/// @param node: ASET node - args: ARRAY, INDEX, VALUE
static void compile_aset(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	compile_node(c, node->args[2]);
	chunk_emit(c->current_scope, OP_ARRAY_SET, 0);
}

/// @brief Compile IN membership check
/// @param c: Compiler instance
/// @param node: IN node - args: ITEM, ARRAY
static void compile_in(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	compile_node(c, node->args[1]);
	chunk_emit(c->current_scope, OP_IN, 0);
}

// ======================
// -- STDLIB
// ======================

/// @brief Compile an IO lib call
/// @param c: Compiler instance
/// @param node: IO node
static void compile_io(compiler_t *c, const okin_node_t *node)
{
	if (!node->method) { compiler_error(c, "IO requires a method"); return; }
	if (memcmp(node->method, "WRITELN", node->method_len) == 0) {
		compile_node(c, node->args[0]);
		chunk_emit(c->current_scope, OP_IO_WRITELN, 0);
	} else if (memcmp(node->method, "WRITE", node->method_len) == 0) {
		compile_node(c, node->args[0]);
		chunk_emit(c->current_scope, OP_IO_WRITE, 0);
	} else if (memcmp(node->method, "READ", node->method_len) == 0) {
		chunk_emit(c->current_scope, OP_IO_READ,
				chunk_add_const(c->current_scope, vm_val_str(node->args[0]->val_start, node->args[0]->val_len)));
	} else compiler_error(c, "unknown IO method");
}

/// @brief Compile a STRING lib call
/// @param c: Compiler instance
/// @param node: STRING node
static void compile_string(compiler_t *c, const okin_node_t *node)
{
	if (!node->method) { compiler_error(c, "STRING requires a method"); return; }
	for (int i = 0; i < node->argc - 1; i++) compile_node(c, node->args[i]);
	vm_op_t op;
	if      (memcmp(node->method, "LEN",    node->method_len) == 0) op = OP_STR_LEN;
	else if (memcmp(node->method, "CONCAT", node->method_len) == 0) op = OP_STR_CONCAT;
	else if (memcmp(node->method, "SLICE",  node->method_len) == 0) op = OP_STR_SLICE;
	else if (memcmp(node->method, "FIND",   node->method_len) == 0) op = OP_STR_FIND;
	else if (memcmp(node->method, "UPPER",  node->method_len) == 0) op = OP_STR_UPPER;
	else if (memcmp(node->method, "LOWER",  node->method_len) == 0) op = OP_STR_LOWER;
	else { compiler_error(c, "unknown STRING method"); return; }

	chunk_emit(c->current_scope, op, 0);
	emit_optional_store(c, node, node->argc - 1);
}

/// @brief Compile a MATH lib call
/// @param c: Compiler instance
/// @param node: MATH node
static void compile_math(compiler_t *c, const okin_node_t *node)
{
	if (!node->method) { compiler_error(c, "MATH requires a method"); return; }
	for (int i = 0; i < node->argc - 1; i++) compile_node(c, node->args[i]);
	vm_op_t op;

	if      (memcmp(node->method, "POW",   node->method_len) == 0) op = OP_MATH_POW;
	else if (memcmp(node->method, "SQRT",  node->method_len) == 0) op = OP_MATH_SQRT;
	else if (memcmp(node->method, "ABS",   node->method_len) == 0) op = OP_MATH_ABS;
	else if (memcmp(node->method, "MIN",   node->method_len) == 0) op = OP_MATH_MIN;
	else if (memcmp(node->method, "MAX",   node->method_len) == 0) op = OP_MATH_MAX;
	else if (memcmp(node->method, "FLOOR", node->method_len) == 0) op = OP_MATH_FLOOR;
	else if (memcmp(node->method, "CEIL",  node->method_len) == 0) op = OP_MATH_CEIL;
	else { compiler_error(c, "unknown MATH method"); return; }

	chunk_emit(c->current_scope, op, 0);
	emit_optional_store(c, node, node->argc - 1);
}

// ======================
// -- DISPATCH
// ======================

typedef void (*compile_fn)(compiler_t *, const okin_node_t *);
static compile_fn COMPILE_TABLE[256];

/// @brief Initialize the compile dispatch table
static void init_compile_table(void)
{
	memset(COMPILE_TABLE, 0, sizeof(COMPILE_TABLE));

	COMPILE_TABLE[VAR]      = compile_var;
	COMPILE_TABLE[SET]      = compile_set;
	COMPILE_TABLE[GLOBAL]   = compile_global;
	COMPILE_TABLE[FUNCTION] = compile_function;
	COMPILE_TABLE[RET]      = compile_ret;
	COMPILE_TABLE[FOR]      = compile_for;
	COMPILE_TABLE[WHILE]    = compile_while;
	COMPILE_TABLE[BREAK]    = compile_break;

	COMPILE_TABLE[ADD]      = compile_arith;
	COMPILE_TABLE[SUB]      = compile_arith;
	COMPILE_TABLE[MUL]      = compile_arith;
	COMPILE_TABLE[DIV]      = compile_arith;
	COMPILE_TABLE[MOD]      = compile_arith;
	COMPILE_TABLE[JMP]      = compile_jmp;
	COMPILE_TABLE[JEQ]      = compile_jmp;
	COMPILE_TABLE[JNE]      = compile_jmp;
	COMPILE_TABLE[JGT]      = compile_jmp;
	COMPILE_TABLE[JLT]      = compile_jmp;
	COMPILE_TABLE[ARRAY]    = compile_array;
	COMPILE_TABLE[AGET]     = compile_aget;
	COMPILE_TABLE[ASET]     = compile_aset;
	COMPILE_TABLE[IN]       = compile_in;
	COMPILE_TABLE[IO]       = compile_io;
	COMPILE_TABLE[STRING]   = compile_string;
	COMPILE_TABLE[MATH]     = compile_math;

	COMPILE_TABLE[EQ]  = compile_cmp;
	COMPILE_TABLE[NEQ] = compile_cmp;
	COMPILE_TABLE[GT]  = compile_cmp;
	COMPILE_TABLE[LT]  = compile_cmp;
	COMPILE_TABLE[GTE] = compile_cmp;
	COMPILE_TABLE[LTE] = compile_cmp;
	COMPILE_TABLE[AND] = compile_logical;
	COMPILE_TABLE[OR]  = compile_logical;
	COMPILE_TABLE[NOT] = compile_logical;
}

/// @brief Compile a single AST node
/// @param c: Compiler instance
/// @param node: Node to compile
static void compile_node(compiler_t *c, const okin_node_t *node)
{
	if (node->opcode == NODE_LEAF)        { compile_leaf(c, node); return; }
	if (node->opcode == NODE_JUMP_TARGET) return;
	if (node->opcode == LABEL)            return;
	if (node->opcode == TRUE)             { chunk_emit(c->current_scope, OP_TRUE,  0); return; }
	if (node->opcode == FALSE)            { chunk_emit(c->current_scope, OP_FALSE, 0); return; }
	if (node->opcode == NIL)              { chunk_emit(c->current_scope, OP_NIL,   0); return; }
	if (node->opcode == CALL)             { compile_call(c, node, 1); return; }

	compile_fn fn = COMPILE_TABLE[node->opcode];
	if (!fn) {
		char msg[256];
		snprintf(msg, sizeof(msg), "unknown opcode 0x%d", node->opcode);
		compiler_error(c, msg);
		return;
	}
	fn(c, node);
}

// ======================
// -- PUBLIC
// ======================

/// @brief Initialize the compiler
/// @param parser
/// @return compiler_t*
compiler_t *compiler_init(const parser_t *parser)
{
	static int initialized = 0;
	if (!initialized) { init_compile_table(); initialized = 1; }
	compiler_t *c  = calloc(1, sizeof(compiler_t));
	c->parser      = parser;
	c->root        = chunk_init("__main__");
	c->current_scope = c->root;
	c->scope       = scope_init(NULL);
	return c;
}

/// @brief Run the given compiler
/// @param c
void compiler_run(compiler_t *c)
{
	pre_register_functions(c);
	compile_body(c, c->parser->program->nodes, c->parser->program->len);
	chunk_emit(c->current_scope, OP_HALT, 0);
}

/// @brief Free the given compiler
/// @param c
void compiler_free(compiler_t *c)
{
	scope_free(c->scope);
	free(c);
}
