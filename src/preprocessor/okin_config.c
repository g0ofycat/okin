#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "okin_config.h"

// ======================
// -- MACROS
// ======================

#define EMIT(s)  do { if (!emit(out, out_len, cap, (s), strlen(s))) return 0; } while(0)
#define EMITC(c) do { char _c = (c); if (!emit(out, out_len, cap, &_c, 1)) return 0; } while(0)

// ======================
// -- OPCODE TABLE
// ======================

static const okin_opcode_info_t opcode_table[] = {
	{ "0",           0, 0 },
	{ "1",           0, 0 },
	{ "2",           2, 0 },
	{ "3",           2, 0 },
	{ "4",           1, 0 },
	{ "5",           0, 0 },
	{ "16",         -1, 1 },
	{ "17",         -1, 0 },
	{ "18",          1, 0 },
	{ "32",          4, 1 },
	{ "33",          1, 1 },
	{ "34",          1, 0 },
	{ "48",         -1, 0 },
	{ "49",          2, 0 },
	{ "50",          3, 0 },
	{ "51",          3, 0 },
	{ "64",          3, 0 },
	{ "65",          3, 0 },
	{ "66",          3, 0 },
	{ "67",          3, 0 },
	{ "68",          3, 0 },
	{ "80",          2, 0 },
	{ "81",          2, 0 },
	{ "82",          2, 0 },
	{ "83",          2, 0 },
	{ "84",          2, 0 },
	{ "85",          2, 0 },
	{ "96",          2, 0 },
	{ "97",          2, 0 },
	{ "98",          1, 0 },
	{ "112",         1, 1 },
	{ "113",         1, 1 },
	{ "114",         0, 1 },
	{ "128",         1, 0 },
	{ "129",         3, 0 },
	{ "130",         3, 0 },
	{ "131",         3, 0 },
	{ "132",         3, 0 },
	{ "144",         1, 0 },

	{ "192~READ",    1, 0 },
	{ "192~WRITE",   1, 0 },
	{ "192~WRITELN", 1, 0 },

	{ "208~LEN",     2, 0 },
	{ "208~CONCAT",  3, 0 },
	{ "208~SLICE",   4, 0 },
	{ "208~FIND",    3, 0 },
	{ "208~UPPER",   2, 0 },

	{ "208~LOWER",   2, 0 },
	{ "208~REPLACE", 4, 0 },
	{ "224~POW",     3, 0 },
	{ "224~SQRT",    2, 0 },
	{ "224~ABS",     2, 0 },
	{ "224~MIN",     3, 0 },
	{ "224~MAX",     3, 0 },
	{ "224~FLOOR",   2, 0 },
	{ "224~CEIL",    2, 0 },
	{ "224~ROUND",   2, 0 },

	{ NULL, 0, 0 },
};

/// @brief Look up metadata for a raw opcode string
/// @param opcode: Raw opcode string (e.g. "192~WRITELN", "64")
/// @return const okin_opcode_info_t*: Matching entry, or NULL if not found
const okin_opcode_info_t *okin_opcode_lookup(const char *opcode) {
	for (int i = 0; opcode_table[i].opcode; i++)
		if (strcmp(opcode_table[i].opcode, opcode) == 0) return &opcode_table[i];
	return NULL;
}

// ======================
// -- HELPERS
// ======================

/// @brief Read an entire file into a heap buffer
/// @param path: File path
/// @return char*: Null-terminated buffer (caller must free), or NULL on failure
static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");

	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(size + 1);
	if (!buf) { fclose(f); return NULL; }

	if (fread(buf, 1, size, f) != (size_t)size) { free(buf); fclose(f); return NULL; }

	buf[size] = '\0';
	fclose(f);
	return buf;
}

/// @brief Strip leading/trailing whitespace in place
/// @param s: String to trim
static void trim(char *s) {
	while (*s == ' ' || *s == '\t') s++;
	char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
	*end = '\0';
}

/// @brief Check if a character is valid in an identifier
/// @param c: Character to check
/// @return int: Non-zero if alphanumeric or underscore
static int is_ident_char(char c) {
	return isalnum((unsigned char)c) || c == '_';
}

/// @brief Grow output buffer to fit at least extra more bytes
/// @param out: Output buffer pointer (may be reallocated)
/// @param cap: Current capacity (updated on growth)
/// @param out_len: Bytes already written
/// @param extra: Bytes about to be written
/// @return int: 1 on success, 0 on allocation failure
static int ensure_cap(char **out, size_t *cap, size_t out_len, size_t extra) {
	if (out_len + extra + 1 <= *cap) return 1;
	size_t new_cap = *cap * 2;
	while (new_cap < out_len + extra + 1) new_cap *= 2;
	char *grown = realloc(*out, new_cap);
	if (!grown) return 0;
	*out = grown;
	*cap = new_cap;
	return 1;
}

/// @brief Append a string to the output buffer
/// @param out: Output buffer pointer
/// @param out_len: Bytes written (updated)
/// @param cap: Buffer capacity (updated on growth)
/// @param str: String to append
/// @param len: Length of str
/// @return size_t: 1 on success, 0 on allocation failure
static size_t emit(char **out, size_t *out_len, size_t *cap, const char *str, size_t len) {
	if (!ensure_cap(out, cap, *out_len, len)) return 0;
	memcpy(*out + *out_len, str, len);
	*out_len += len;
	return 1;
}

/// @brief Skip spaces, tabs, and commas
/// @param src: Source string
/// @param i: Current position
/// @return size_t: Position after separators
static size_t skip_sep(const char *src, size_t i) {
	while (src[i] == ' ' || src[i] == '\t' || src[i] == ',') i++;
	return i;
}

/// @brief Read one token (quoted string or bare word) from src into buf
/// @param src: Source string
/// @param i: Current position
/// @param buf: Output token buffer
/// @param buf_cap: Capacity of buf
/// @return size_t: Position after the token
static size_t read_token(const char *src, size_t i, char *buf, size_t buf_cap) {
	size_t n = 0;
#define PUSH(c) do { if (n + 1 < buf_cap) buf[n++] = (c); } while (0)
	if (src[i] == '"') {
		PUSH(src[i++]);
		while (src[i] && src[i] != '"') {
			if (src[i] == '\\') PUSH(src[i++]);
			PUSH(src[i++]);
		}
		if (src[i] == '"') PUSH(src[i++]);
	} else {
		while (src[i] && src[i] != ',' && src[i] != ' ' && src[i] != '\t'
				&& src[i] != '\n' && src[i] != '\r')
			PUSH(src[i++]);
	}
#undef PUSH

	buf[n] = '\0';
	return i;
}

/// @brief Parse alias=opcode lines into cfg, ignoring blank lines and # comments
/// @param cfg: Config instance to populate
/// @param src: Raw config text
/// @return int: 1 if at least one entry was parsed, 0 otherwise
static int parse_toml(okin_config_t *cfg, const char *src) {
	cfg->count = 0;
	char *copy = strdup(src);
	if (!copy) return 0;

	for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n")) {
		char *hash = strchr(line, '#');
		if (hash) *hash = '\0';
		trim(line);
		if (!*line) continue;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *alias = line, *opcode = eq + 1;
		trim(alias);
		trim(opcode);
		if (!*alias || !*opcode || cfg->count >= OKIN_CONFIG_MAX_ENTRIES) continue;
		okin_config_entry_t *e = &cfg->entries[cfg->count++];
		snprintf(e->alias,  OKIN_CONFIG_MAX_LEN, "%s", alias);
		snprintf(e->opcode, OKIN_CONFIG_MAX_LEN, "%s", opcode);
	}

	free(copy);
	return cfg->count > 0;
}

/// @brief Find the alias entry matching at pos, respecting word boundaries
/// @param cfg: Config instance to search
/// @param src: Full source string
/// @param pos: Position to check
/// @return const okin_config_entry_t*: Matching entry, or NULL if none
static const okin_config_entry_t *find_alias_at(const okin_config_t *cfg, const char *src, size_t pos) {
	if (pos > 0 && is_ident_char(src[pos - 1])) return NULL;

	for (int i = 0; i < cfg->count; i++) {
		size_t len = strlen(cfg->entries[i].alias);
		if (strncmp(src + pos, cfg->entries[i].alias, len) != 0) continue;
		if (is_ident_char(src[pos + len])) continue;
		return &cfg->entries[i];
	}

	return NULL;
}

// ======================
// -- EXPANSION
// ======================

static int expand_block(const okin_config_t *cfg, const char *source, size_t *i, char **out, size_t *out_len, size_t *cap, int in_body);
static int expand_arg(const okin_config_t *cfg, const char *source, size_t *i, char **out, size_t *out_len, size_t *cap);

/// @brief Expand a call link to its original form
/// @param cfg
/// @param entry
/// @param info
/// @param source
/// @param i
/// @param out
/// @param out_len
/// @param cap
/// @return int
static int expand_call(const okin_config_t *cfg, const okin_config_entry_t *entry, const okin_opcode_info_t *info, const char *source, size_t *i, char **out, size_t *out_len, size_t *cap) {
	EMIT(entry->opcode);
	if (info->arity == 0 && !info->has_body) return 1;

	if (info->arity != 0) {
		EMITC('<');
		for (int argc = 0; ; argc++) {
			*i = skip_sep(source, *i);
			if (info->arity == -1) {
				if (!source[*i] || source[*i] == '\n' || source[*i] == '\r' || source[*i] == ';') break;
			} else if (argc >= info->arity) break;

			if (argc > 0) EMITC(',');
			if (!expand_arg(cfg, source, i, out, out_len, cap)) return 0;
		}
	}

	if (info->has_body) {
		EMITC(info->arity == 0 ? '<' : '|');
		*i = skip_sep(source, *i);
		if (!expand_block(cfg, source, i, out, out_len, cap, 1)) return 0;

		while (*out_len > 0 && strchr(";\n\r \t", (*out)[*out_len - 1]))
			(*out_len)--;
	}

	EMITC('>');
	return 1;
}

/// @brief Read one argument: a nested alias call, a raw <...> expression, or a literal token
/// @param cfg
/// @param source
/// @param i
/// @param out
/// @param out_len
/// @param cap
/// @return int
static int expand_arg(const okin_config_t *cfg, const char *source, size_t *i, char **out, size_t *out_len, size_t *cap) {
	*i = skip_sep(source, *i);

	const okin_config_entry_t *entry = find_alias_at(cfg, source, *i);
	if (entry) {
		const okin_opcode_info_t *info = okin_opcode_lookup(entry->opcode);
		*i += strlen(entry->alias);
		if (!info) return emit(out, out_len, cap, entry->opcode, strlen(entry->opcode));
		return expand_call(cfg, entry, info, source, i, out, out_len, cap);
	}

	if (source[*i] == '<') {
		size_t start = *i;
		int depth = 0;
		do {
			if (source[*i] == '<') depth++;
			else if (source[*i] == '>') depth--;
			(*i)++;
		} while (source[*i] && depth > 0);
		return emit(out, out_len, cap, source + start, *i - start);
	}

	char tok[512];
	*i = read_token(source, *i, tok, sizeof tok);
	return emit(out, out_len, cap, tok, strlen(tok));
}

/// @brief Expand a sequence of statements; if in_body, stop and consume a trailing END
/// @param cfg
/// @param source
/// @param i
/// @param out
/// @param out_len
/// @param cap
/// @param in_body
/// @return int
static int expand_block(const okin_config_t *cfg, const char *source, size_t *i, char **out, size_t *out_len, size_t *cap, int in_body) {
	while (source[*i]) {
		const okin_config_entry_t *entry = find_alias_at(cfg, source, *i);

		if (in_body && is_ident_char(source[*i]) && (*i == 0 || !is_ident_char(source[*i - 1]))) {
			size_t j = *i;
			while (is_ident_char(source[j])) j++;

			if (j - *i == 3 && strncmp(source + *i, "END", 3) == 0) {
				*i = j;
				return 1;
			}

			if (entry && (strcmp(entry->opcode, "113") == 0 || strcmp(entry->opcode, "114") == 0))
				return 1;
		}

		if (!entry) {
			if (!ensure_cap(out, cap, *out_len, 1)) return 0;
			(*out)[(*out_len)++] = source[(*i)++];
			continue;
		}

		const okin_opcode_info_t *info = okin_opcode_lookup(entry->opcode);
		*i += strlen(entry->alias);

		if (!info) EMIT(entry->opcode);
		else if (!expand_call(cfg, entry, info, source, i, out, out_len, cap)) return 0;

		EMITC(';');
	}

	return 1;
}

// ======================
// -- CONFIG
// ======================

/// @brief Load a config from a file path or raw text, and cache it
/// @param cfg: Config instance to populate
/// @param arg: File path or raw config string
/// @return int: 1 on success, 0 if no entries were parsed
int okin_config_load(okin_config_t *cfg, const char *arg) {
	if (!arg) return 0;

	char *file_content = read_file(arg);
	int ok = parse_toml(cfg, file_content ? file_content : arg);
	free(file_content);

	if (ok) okin_config_save_cache(cfg);
	return ok;
}

/// @brief Persist a config to disk so it survives between runs
/// @param cfg: Config instance to write
void okin_config_save_cache(const okin_config_t *cfg) {
	FILE *f = fopen(OKIN_CONFIG_CACHE_PATH, "w");
	if (!f) return;

	for (int i = 0; i < cfg->count; i++)
		fprintf(f, "%s=%s\n", cfg->entries[i].alias, cfg->entries[i].opcode);

	fclose(f);
}

/// @brief Load the last persisted config from disk
/// @param cfg: Config instance to populate
/// @return int: 1 on success, 0 if no cache or it's empty
int okin_config_load_cache(okin_config_t *cfg) {
	char *content = read_file(OKIN_CONFIG_CACHE_PATH);
	if (!content) return 0;

	int ok = parse_toml(cfg, content);
	free(content);
	return ok;
}

/// @brief Replace alias identifiers in source with their expanded opcode form
/// @param cfg: Config instance with alias -> opcode mappings
/// @param source: Source code using alias keywords
/// @return char*: Heap-allocated expanded source (caller must free), or NULL on failure
char *okin_config_expand(const okin_config_t *cfg, const char *source) {
	size_t cap = strlen(source) * 2 + 64, out_len = 0, i = 0;
	char *out = malloc(cap);
	if (!out) return NULL;

	if (!expand_block(cfg, source, &i, &out, &out_len, &cap, 0)) { free(out); return NULL; }

	out[out_len] = '\0';
	return out;
}
