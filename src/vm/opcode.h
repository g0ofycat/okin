#ifndef VM_OPCODE_H
#define VM_OPCODE_H

//--============
// -- OPCODES
//--============

// TODO: use consts for opcodes and type enum as uint8_t

typedef enum {
	// -- CONSTANTS & LOCALS --
	OP_LOAD_CONST,    // a = constant pool index
	OP_LOAD_LOCAL,    // a = stack slot
	OP_STORE_LOCAL,   // a = stack slot
	OP_LOAD_GLOBAL,   // a = constant pool index (name)
	OP_STORE_GLOBAL,  // a = constant pool index (name)

	// -- LITERALS --
	OP_TRUE,
	OP_FALSE,
	OP_NIL,
	OP_POP,

	// -- ARITH -- (2 pop; 1 push)
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,

	// -- COMPARISON -- (2 pop; bool push)
	OP_EQ, OP_NEQ, OP_GT, OP_LT, OP_GTE, OP_LTE,

	// -- LOGICAL --
	OP_AND, OP_OR, OP_NOT,

	// -- JMPS --
	OP_JMP,           // a = target ip
	OP_JMP_FALSE,     // a = target ip, pops condition
	OP_JEQ,           // a = target ip, pops two values
	OP_JNE,
	OP_JGT,
	OP_JLT,

	// -- FUNCTIONS --
	OP_LOAD_FUNC,     // a = sub_chunk index
	OP_CALL,          // a = arg count
	OP_RET,

	// -- ARRAYS --
	OP_MAKE_ARRAY,    // a = element count, pops a items
	OP_ARRAY_GET,     // pops index, pops array, pushes value
	OP_ARRAY_SET,     // pops value, pops index, pops array
	OP_IN,            // pops array, pops item, pushes bool

	// -- IO --
	OP_IO_WRITE,
	OP_IO_WRITELN,
	OP_IO_READ,       // a = global name index to store into

	// -- STRING --
	OP_STR_LEN,
	OP_STR_CONCAT,
	OP_STR_SLICE,
	OP_STR_FIND,
	OP_STR_UPPER,
	OP_STR_LOWER,

	// -- MATH --
	OP_MATH_POW,
	OP_MATH_SQRT,
	OP_MATH_ABS,
	OP_MATH_MIN,
	OP_MATH_MAX,
	OP_MATH_FLOOR,
	OP_MATH_CEIL,

	OP_HALT,
} vm_op_t;

#endif
