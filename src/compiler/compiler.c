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
	if (slot != -1) { chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot); return; }
	chunk_emit(c->current_scope, OP_LOAD_GLOBAL, chunk_add_const(c->current_scope, vm_val_str(name, len)));
}

/// @brief Store a value into a local slot or global by name
/// @param c: Compiler instance
/// @param name: Variable name
/// @param len: Name length
static void store_name(compiler_t *c, const char *name, size_t len)
{
	int slot = scope_resolve(c->scope, name, len);
	if (slot != -1) { chunk_emit(c->current_scope, OP_STORE_LOCAL, slot); return; }
	chunk_emit(c->current_scope, OP_STORE_GLOBAL, chunk_add_const(c->current_scope, vm_val_str(name, len)));
}

/// @brief Patch all pending break jumps to current ip
/// @param c: Compiler instance
static void patch_breaks(compiler_t *c)
{
	for (int i = 0; i < c->break_count; i++)
		chunk_patch(c->current_scope, c->break_patches[i], c->current_scope->code_len);
	c->break_count = 0;
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

// ======================
// -- FORWARD DECL
// ======================

static void compile_node(compiler_t *c, const okin_node_t *node);

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
	c->break_patches[c->break_count++] = chunk_emit(c->current_scope, OP_BREAK, 0);
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
	if (node->argc >= 3 && node->args[2]->tok == TK_VALUE)
		store_name(c, node->args[2]->val_start, node->args[2]->val_len);
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

/// @brief Compile IF with backpatching
/// @param c: Compiler instance
/// @param node: IF node - args: COND | body
static void compile_if(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);
	scope_begin(c->scope);
	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);
	scope_end(c->scope);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);
}

/// @brief Compile ELIF with backpatching
/// @param c: Compiler instance
/// @param node: ELIF node - args: COND | body
static void compile_elif(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[0]);
	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);
	scope_begin(c->scope);
	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);
	scope_end(c->scope);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);
}

/// @brief Compile ELSE body
/// @param c: Compiler instance
/// @param node: ELSE node - body only
static void compile_else(compiler_t *c, const okin_node_t *node)
{
	scope_begin(c->scope);
	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);
	scope_end(c->scope);
}

/// @brief Compile WHILE loop with backpatching
/// @param c: Compiler instance
/// @param node: WHILE node - args: COND | body
static void compile_while(compiler_t *c, const okin_node_t *node)
{
	int loop_start = c->current_scope->code_len;
	compile_node(c, node->args[0]);
	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);
	scope_begin(c->scope);
	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);
	scope_end(c->scope);
	chunk_emit(c->current_scope, OP_JMP, loop_start);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);
	patch_breaks(c);
}

/// @brief Compile FOR range loop
/// @param c: Compiler instance
/// @param node: FOR node - args: INDEX, START, END, STEP | body
static void compile_for(compiler_t *c, const okin_node_t *node)
{
	compile_node(c, node->args[1]);
	int slot = scope_declare(c->scope, node->args[0]->val_start, node->args[0]->val_len);

	if (slot == -1) { compiler_error(c, "too many locals"); return; }
	c->current_scope->num_locals++;
	chunk_emit(c->current_scope, OP_STORE_LOCAL, slot);

	int loop_start = c->current_scope->code_len;
	chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot);
	compile_node(c, node->args[2]);

	chunk_emit(c->current_scope, OP_LT, 0);
	int jf = chunk_emit(c->current_scope, OP_JMP_FALSE, 0);

	scope_begin(c->scope);
	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);
	scope_end(c->scope);

	chunk_emit(c->current_scope, OP_LOAD_LOCAL, slot);
	compile_node(c, node->args[3]);
	chunk_emit(c->current_scope, OP_ADD, 0);
	chunk_emit(c->current_scope, OP_STORE_LOCAL, slot);

	chunk_emit(c->current_scope, OP_JMP, loop_start);
	chunk_patch(c->current_scope, jf, c->current_scope->code_len);
	patch_breaks(c);
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
	int sub_idx       = resolve_func(c, node->args[0]->val_start, node->args[0]->val_len);
	chunk_t *fn_chunk = c->root->sub_chunks[sub_idx];
	chunk_t *parent   = c->current_scope;
	c->current_scope  = fn_chunk;
	scope_begin(c->scope);

	for (int i = 1; i < node->argc; i++) {
		int slot = scope_declare(c->scope, node->args[i]->val_start, node->args[i]->val_len);
		fn_chunk->num_params++;
		fn_chunk->num_locals++;
		(void)slot;
	}

	for (int i = 0; i < node->body_len; i++) compile_node(c, node->body[i]);

	chunk_emit(c->current_scope, OP_NIL, 0);
	chunk_emit(c->current_scope, OP_RET, 0);

	scope_end(c->scope);
	c->current_scope = parent;
}

/// @brief Compile a CALL instruction
/// @param c: Compiler instance
/// @param node: CALL node - args: NAME, ...ARGS, DEST
static void compile_call(compiler_t *c, const okin_node_t *node)
{
	int arg_count = node->argc - 2;
	if (arg_count < 0) arg_count = 0;

	for (int i = 1; i <= arg_count; i++) compile_node(c, node->args[i]);

	int fn_idx = resolve_func(c, node->args[0]->val_start, node->args[0]->val_len);
	if (fn_idx != -1)
		chunk_emit(c->current_scope, OP_LOAD_FUNC, fn_idx);
	else
		load_name(c, node->args[0]->val_start, node->args[0]->val_len);

	chunk_emit(c->current_scope, OP_CALL, arg_count);

	if (node->argc >= 2 && node->args[node->argc - 1]->tok == TK_VALUE)
		store_name(c, node->args[node->argc - 1]->val_start, node->args[node->argc - 1]->val_len);
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
	if (node->argc >= 3 && node->args[2]->tok == TK_VALUE)
		store_name(c, node->args[2]->val_start, node->args[2]->val_len);
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
	if (node->args[node->argc - 1]->tok == TK_VALUE)
		store_name(c, node->args[node->argc - 1]->val_start, node->args[node->argc - 1]->val_len);
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
	if (node->args[node->argc - 1]->tok == TK_VALUE)
		store_name(c, node->args[node->argc - 1]->val_start, node->args[node->argc - 1]->val_len);
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
	COMPILE_TABLE[FUNCTION] = compile_function;
	COMPILE_TABLE[CALL]     = compile_call;
	COMPILE_TABLE[RET]      = compile_ret;
	COMPILE_TABLE[FOR]      = compile_for;
	COMPILE_TABLE[WHILE]    = compile_while;
	COMPILE_TABLE[BREAK]    = compile_break;
	COMPILE_TABLE[IF]       = compile_if;
	COMPILE_TABLE[ELIF]     = compile_elif;
	COMPILE_TABLE[ELSE]     = compile_else;
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
	if (node->opcode == GLOBAL)           return;
	if (node->opcode == TRUE)             { chunk_emit(c->current_scope, OP_TRUE,  0); return; }
	if (node->opcode == FALSE)            { chunk_emit(c->current_scope, OP_FALSE, 0); return; }
	if (node->opcode == NIL)              { chunk_emit(c->current_scope, OP_NIL,   0); return; }

	compile_fn fn = COMPILE_TABLE[node->opcode];
	if (!fn) { compiler_error(c, "unknown opcode"); return; }
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

	c->scope       = scope_init();
	return c;
}

/// @brief Run the given compiler
/// @param c
void compiler_run(compiler_t *c)
{
	pre_register_functions(c);
	for (int i = 0; i < c->parser->program->len; i++)
		compile_node(c, c->parser->program->nodes[i]);
	chunk_emit(c->current_scope, OP_HALT, 0);
}

/// @brief Free the given compiler
/// @param c
void compiler_free(compiler_t *c)
{
	scope_free(c->scope);
	free(c);
}

/// @brief Return the root chunk after compilation
/// @param c
/// @return chunk_t*
chunk_t *compiler_chunk(compiler_t *c)
{
	return c->root;
}
