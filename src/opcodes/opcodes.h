#ifndef OPCODES_H
#define OPCODES_H

// -- RANGES: --
// 0x00 - 0x3F: General
//   0x00 - 0x0F: Memory
//   0x10 - 0x1F: Functions
//   0x20 - 0x2F: Iterators
//   0x30 - 0x3F: Data
// 0x40 - 0x7F: Arithmetic & Conditionals
//   0x40 - 0x4F: Arithmetic
//   0x50 - 0x5F: Comparison
//   0x60 - 0x6F: Logical
//   0x70 - 0x7F: Branching
// 0x80 - 0xBF: Jumps
//   0x80 - 0x8F: Jump Ops
//   0x90 - 0x9F: Labels
// 0xC0 - 0xFF: Std Libs
//   0xC0 - 0xCF: IO
//   0xD0 - 0xDF: String
//   0xE0 - 0xEF: Math

#include <stdint.h>

//--============
// -- OPCODES
//--============

// -- GENERAL --

// memory
#define FALSE    0x00  // 0x00: boolean false literal; "0<empty>"
#define TRUE     0x01  // 0x01: boolean true literal; "1<empty>"

#define VAR      0x02  // 0x02: init mem location; "2<NAME, VALUE>"
#define SET      0x03  // 0x03: reassign existing var; "3<NAME, VALUE>"
#define GLOBAL   0x04  // 0x04: mark var as writable from inner scope; "4<NAME>"

#define NIL      0x05  // 0x05: ptr to invalid mem addr; "5<empty>"

// functions
#define FUNCTION 0x10  // 0x10: function label with body; "16<NAME, ...PARAMS|<LOGIC>>"
#define CALL     0x11  // 0x11: invoke function; "17<FUNC_NAME, ...ARGS, DEST>"
#define RET      0x12  // 0x12: return value from function; "18<VALUE>"

// iterators
#define FOR      0x20  // 0x20: range iterator; "32<ITER, START, END, STEP|<LOGIC>>"
#define WHILE    0x21  // 0x21: conditional iterator; "33<COND|<LOGIC>>"
#define BREAK    0x22  // 0x22: break out of iterator; "34<empty>"

// data
#define ARRAY    0x30  // 0x30: init contiguous memory block; "48<VAL, VAL, ...>"; dynamic, out of bounds gets replaced by 0
#define IN       0x31  // 0x31: membership check inside memory block; "49<ITEM, ARRAY>"
#define AGET     0x32  // 0x32: get value at array index; "50<ARRAY, INDEX, DEST>"
#define ASET     0x33  // 0x33: set value at array index; "51<ARRAY, INDEX, VALUE>"

// -- ARITHMETIC & CONDITIONALS --

// arithmetic
#define ADD      0x40  // 0x40: addition; "64<A, B, DEST>"
#define SUB      0x41  // 0x41: subtraction; "65<A, B, DEST>"
#define MUL      0x42  // 0x42: multiplication; "66<A, B, DEST>"
#define DIV      0x43  // 0x43: division; "67<A, B, DEST>"
#define MOD      0x44  // 0x44: modulo; "68<A, B, DEST>"

// comparison
#define EQ       0x50  // 0x50: equals; "80<A, B>". true if A == B
#define NEQ      0x51  // 0x51: not equals; "81<A, B>". true if A != B
#define GT       0x52  // 0x52: greater than; "82<A, B>". true if A > B
#define LT       0x53  // 0x53: less than; "83<A, B>". true if A < B
#define GTE      0x54  // 0x54: greater than or equal; "84<A, B>". true if A >= B
#define LTE      0x55  // 0x55: less than or equal; "85<A, B>". true if A <= B

// logical
#define AND      0x60  // 0x60: logical and; "96<COND_A, COND_B>". true if both
#define OR       0x61  // 0x61: logical or; "97<COND_A, COND_B>". true if either
#define NOT      0x62  // 0x62: logical not; "98<COND>". inverts cond.

// branching
#define IF       0x70  // 0x70: conditional if; "112<COND|<LOGIC>>"

// -- JUMPS --

// jump ops
#define JMP      0x80  // 0x80: unconditional jump to label; "128<LABEL>"
#define JEQ      0x81  // 0x81: jump if A == B; "129<A, B, LABEL>"
#define JNE      0x82  // 0x82: jump if A != B; "130<A, B, LABEL>"
#define JGT      0x83  // 0x83: jump if A > B; "131<A, B, LABEL>"
#define JLT      0x84  // 0x84: jump if A < B; "132<A, B, LABEL>"

// labels
#define LABEL    0x90  // 0x90: define jump target; "144<NAME>"

// -- STD LIBS --

#define IO       0xC0  // 0xC0: basic io lib; "192~<METHOD, ...ARGS>"
					   //   ~READ<DEST>                   - read input into var
					   //   ~WRITE<VALUE>                 - write value to stdout
					   //   ~WRITELN<VALUE>               - write value to stdout with newline

					   // string
#define STRING   0xD0  // 0xD0: string lib; "208~<METHOD, ...ARGS>"
					   //   ~LEN<STR, DEST>               - length of string
					   //   ~CONCAT<A, B, DEST>           - concatenate two strings
					   //   ~SLICE<STR, START, END, DEST> - substring
					   //   ~FIND<STR, PATTERN, DEST>     - index of pattern, -1 if not found
					   //   ~UPPER<STR, DEST>             - uppercase
					   //   ~LOWER<STR, DEST>             - lowercase

					   // math
#define MATH     0xE0  // 0xE0: math lib; "224~<METHOD, ...ARGS>"
					   //   ~POW<BASE, EXP, DEST>         - exponentiation
					   //   ~SQRT<A, DEST>                - square root
					   //   ~ABS<A, DEST>                 - absolute value
					   //   ~MIN<A, B, DEST>              - minimum of two values
					   //   ~MAX<A, B, DEST>              - maximum of two values
					   //   ~FLOOR<A, DEST>               - floor
					   //   ~CEIL<A, DEST>                - ceiling

#endif
