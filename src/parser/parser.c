#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

// ======================
// -- PRIVATE: STREAM
// ======================

/// @brief Peek current token without advancing
/// @param p
/// @return token_t*
static const token_t *peek(const parser_t *p)
{
	return &p->lexer->tokens[p->pos];
}

/// @brief Peek token at offset without advancing
/// @param p
/// @param offset
/// @return token_t*
static const token_t *peek_at(const parser_t *p, size_t offset)
{
	size_t i = p->pos + offset;
	if (i >= p->lexer->count) return &p->lexer->tokens[p->lexer->count - 1];
	return &p->lexer->tokens[i];
}

/// @brief Advance and return current token
/// @param p
/// @return token_t*
static const token_t *advance(parser_t *p)
{
	const token_t *t = &p->lexer->tokens[p->pos];
	if (t->type != TK_EOF) p->pos++;
	return t;
}

/// @brief Check if current token matches type
/// @param p
/// @param type
/// @return int
static int check(const parser_t *p, token_type_t type)
{
	return peek(p)->type == type;
}

/// @brief Advance if current token matches type, return 1 on success
/// @param p
/// @param type
/// @return int
static int match(parser_t *p, token_type_t type)
{
	if (!check(p, type)) return 0;
	advance(p);
	return 1;
}

/// @brief Emit a parse error and increment error count
/// @param p
/// @param msg
static void parse_error(parser_t *p, const char *msg)
{
	const token_t *t = peek(p);
	fprintf(stderr, "[okin parse error] line %d col %d: %s (got '%.*s')\n",
			t->line, t->col, msg, (int)t->len, t->start);
	p->errors++;
}

/// @brief Skip tokens until semicolon or EOF for error recovery
/// @param p
static void sync(parser_t *p)
{
	while (!check(p, TK_SEMI) && !check(p, TK_EOF)) advance(p);
	match(p, TK_SEMI);
}

// ======================
// -- PRIVATE: ARENA
// ======================

/// @brief Allocate a zeroed node from the arena
/// @param p
/// @return okin_node_t*
static okin_node_t *alloc_node(parser_t *p)
{
	okin_node_t *n = arena_alloc(p->arena, sizeof(okin_node_t));
	memset(n, 0, sizeof(okin_node_t));
	return n;
}

/// @brief Allocate a pointer array from the arena
/// @param p
/// @param count
/// @return okin_node_t**
static okin_node_t **alloc_node_array(parser_t *p, int count)
{
	return arena_alloc(p->arena, sizeof(okin_node_t *) * count);
}

// ======================
// -- PRIVATE: PASS 1
// ======================

/// @brief Collect all label names and their statement indices
/// @param p
static void collect_labels(parser_t *p)
{
	size_t saved_pos = p->pos;
	int    index     = 0;
	int    depth     = 0;

	while (!check(p, TK_EOF))
	{
		const token_t *t = peek(p);

		if (t->type == TK_ARG_OPEN)  { depth++; advance(p); continue; }
		if (t->type == TK_ARG_CLOSE) { depth--; advance(p); continue; }

		if (t->type == TK_SEMI && depth == 0) { index++; advance(p); continue; }

		if (t->type == TK_OPCODE && depth == 0)
		{
			char buf[16] = {0};
			size_t copy_len = t->len < 15 ? t->len : 15; // LABEL
			memcpy(buf, t->start, copy_len);
			if (atoi(buf) == LABEL)
			{
				advance(p);
				if (check(p, TK_ARG_OPEN))
				{
					advance(p);
					const token_t *name = peek(p);
					if (name->type == TK_VALUE && p->program->label_count < PARSER_MAX_LABELS)
					{
						label_entry_t *entry = &p->program->labels[p->program->label_count++];
						entry->name     = name->start;
						entry->name_len = name->len;
						entry->index    = index;
					}
				}
				continue;
			}
		}

		advance(p);
	}

	p->pos = saved_pos;
}

// ======================
// -- PRIVATE: PASS 2
// ======================

static okin_node_t *parse_node(parser_t *p);
static okin_node_t *parse_statement(parser_t *p);

/// @brief Resolve a label name to its program index, -1 if not found
/// @param p
/// @param name
/// @param len
/// @return int
static int resolve_label(const parser_t *p, const char *name, size_t len)
{
	for (int i = 0; i < p->program->label_count; i++)
	{
		label_entry_t *e = &p->program->labels[i];
		if (e->name_len == len && memcmp(e->name, name, len) == 0)
			return e->index;
	}
	return -1;
}

/// @brief Parse a comma separated arg list inside < >, return node array
/// @param p
/// @param out_argc
/// @return okin_node_t**
static okin_node_t **parse_args(parser_t *p, int *out_argc)
{
	// collect into a temp stack-local list, then copy to arena
	okin_node_t *tmp[256];
	int count = 0;

	while (!check(p, TK_ARG_CLOSE) && !check(p, TK_PIPE) && !check(p, TK_EOF))
	{
		if (count > 0 && !match(p, TK_COMMA))
		{
			parse_error(p, "expected ',' between args");
			sync(p);
			break;
		}

		tmp[count++] = parse_node(p);
	}

	*out_argc = count;
	if (count == 0) return NULL;

	okin_node_t **arr = alloc_node_array(p, count);
	memcpy(arr, tmp, sizeof(okin_node_t *) * count);
	return arr;
}

/// @brief Parse a pipe body | stmt; stmt; ... > as a node array
/// @param p
/// @param out_len
/// @return okin_node_t**
static okin_node_t **parse_body(parser_t *p, int *out_len)
{
	okin_node_t *tmp[256];
	int count = 0;

	while (!check(p, TK_ARG_CLOSE) && !check(p, TK_EOF))
	{
		tmp[count++] = parse_statement(p);
		match(p, TK_SEMI);
	}

	*out_len = count;
	if (count == 0) return NULL;

	okin_node_t **arr = alloc_node_array(p, count);
	memcpy(arr, tmp, sizeof(okin_node_t *) * count);
	return arr;
}

/// @brief Parse a single value or nested node
/// @param p
/// @return okin_node_t*
static okin_node_t *parse_node(parser_t *p)
{
	const token_t *t = peek(p);

	if (t->type == TK_OPCODE || t->type == TK_LIB_CALL)
		return parse_statement(p);

	if (t->type == TK_VALUE)
	{
		advance(p);
		okin_node_t *n  = alloc_node(p);
		n->opcode       = 0xFF;
		n->val_start    = t->start;
		n->val_len      = t->len;
		return n;
	}

	parse_error(p, "expected value or opcode");
	advance(p);
	return alloc_node(p);
}

/// @brief Parse one full statement: opcode<args|body>
/// @param p
/// @return okin_node_t*
static okin_node_t *parse_statement(parser_t *p)
{
	const token_t *t = advance(p);
	okin_node_t *n   = alloc_node(p);

	char buf[16] = {0};
	size_t copy_len = t->len < 15 ? t->len : 15;
	memcpy(buf, t->start, copy_len);
	n->opcode = (uint8_t)atoi(buf);

	if (t->type == TK_LIB_CALL)
	{
		n->val_start = t->start;
		n->val_len   = t->len;
	}

	if (!match(p, TK_ARG_OPEN))
		return n;

	n->args = parse_args(p, &n->argc);

	if (match(p, TK_PIPE))
		n->body = parse_body(p, &n->body_len);

	if (!match(p, TK_ARG_CLOSE))
		parse_error(p, "expected '>'");

	if (n->opcode == JMP || n->opcode == JEQ || n->opcode == JNE ||
			n->opcode == JGT || n->opcode == JLT)
	{
		okin_node_t *label_arg = n->args[n->argc - 1];
		int idx = resolve_label(p, label_arg->val_start, label_arg->val_len);
		if (idx == -1)
			parse_error(p, "undefined label");
		else
			label_arg->opcode = 0xFE;
	}

	return n;
}

// ======================
// -- PRIVATE: PRINT
// ======================

/// @brief Recursively print a node with indentation
/// @param n
/// @param depth
static void print_node(const okin_node_t *n, int depth)
{
	for (int i = 0; i < depth; i++) printf("  ");

	if (n->opcode == 0xFF)
		printf("VALUE '%.*s'\n", (int)n->val_len, n->val_start);
	else if (n->opcode == 0xFE)
		printf("JUMP_TARGET\n");
	else
		printf("NODE opcode=0x%02X\n", n->opcode);

	for (int i = 0; i < n->argc; i++)
		print_node(n->args[i], depth + 1);

	if (n->body_len > 0)
	{
		for (int i = 0; i < depth; i++) printf("  ");
		printf("BODY:\n");
		for (int i = 0; i < n->body_len; i++)
			print_node(n->body[i], depth + 1);
	}
}

// ======================
// -- PARSER
// ======================

/// @brief Initialize parser from a lexed token stream
/// @param lexer Completed lexer instance
/// @return Heap allocated parser_t
parser_t *parser_init(const lexer_t *lexer)
{
	parser_t *p    = calloc(1, sizeof(parser_t));
	p->lexer       = lexer;
	p->arena       = arena_init();
	p->program     = calloc(1, sizeof(okin_program_t));
	return p;
}

/// @brief Run both passes, populate program
/// @param p Parser instance
void parser_run(parser_t *p)
{
	collect_labels(p);

	okin_node_t *tmp[4096];
	int count = 0;

	while (!check(p, TK_EOF))
	{
		if (check(p, TK_SEMI)) { advance(p); continue; }
		if (check(p, TK_ERROR)) { advance(p); continue; }
		tmp[count++] = parse_statement(p);
		match(p, TK_SEMI);
	}

	p->program->len   = count;
	p->program->nodes = arena_alloc(p->arena, sizeof(okin_node_t *) * count);
	memcpy(p->program->nodes, tmp, sizeof(okin_node_t *) * count);
}

/// @brief Free parser, arena, and program
/// @param p Parser instance
void parser_free(parser_t *p)
{
	arena_free(p->arena);
	free(p->program);
	free(p);
}

/// @brief Print all nodes to stdout
/// @param p Parser instance
void parser_print(const parser_t *p)
{
	for (int i = 0; i < p->program->len; i++)
	{
		printf("[%d] ", i);
		print_node(p->program->nodes[i], 0);
	}
}
