#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "okin_config.h"

// ======================
// -- HELPERS
// ======================

/// @brief Read an entire file into a heap buffer
/// @param path: File path
/// @return char*: Null-terminated buffer (caller must free), or NULL if the file can't be opened or read
static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");

	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(size + 1);
	if (!buf) { fclose(f); return NULL; }

	if (fread(buf, 1, size, f) != (size_t)size) {
		free(buf);
		fclose(f);
		return NULL;
	}

	buf[size] = '\0';
	fclose(f);
	return buf;
}

/// @brief Strip leading / trailing whitespace from a string in place
/// @param s: String to trim
static void trim(char *s) {
	while (*s == ' ' || *s == '\t') s++;
	char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		end--;
	*end = '\0';
}

/// @brief Parse "alias=opcode" or "alias=opcode~method" lines ('#' comments allowed) into a config
/// @param cfg: Config instance to populate
/// @param toml: TOML source text
/// @return int: 1 if at least one entry was parsed, 0 otherwise
static int parse_toml(okin_config_t *cfg, const char *toml) {
	cfg->count = 0;
	char *copy = strdup(toml);
	if (!copy) return 0;

	for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n")) {
		char *hash = strchr(line, '#');
		if (hash) *hash = '\0';
		trim(line);
		if (!*line) continue;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *alias = line;
		char *opcode = eq + 1;
		trim(alias);
		trim(opcode);
		if (!*alias || !*opcode || cfg->count >= OKIN_CONFIG_MAX_ENTRIES) continue;
		okin_config_entry_t *e = &cfg->entries[cfg->count++];
		snprintf(e->alias, OKIN_CONFIG_MAX_LEN, "%s", alias);
		snprintf(e->opcode, OKIN_CONFIG_MAX_LEN, "%s", opcode);
	}

	free(copy);
	return cfg->count > 0;
}

/// @brief Check if a character can be part of an identifier
/// @param c: Character to check
/// @return int: Non-zero if c is alphanumeric or underscore
static int is_ident_char(char c) {
	return isalnum((unsigned char)c) || c == '_';
}

/// @brief Find the entry whose alias matches an identifier starting at pos, respecting word boundaries
/// @param cfg: Config instance to search
/// @param src: Full source string
/// @param pos: Position to check for a match
/// @return const okin_config_entry_t*: Matching entry, or NULL if none matches at pos
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

/// @brief Ensure the output buffer has room for at least `extra` more bytes (plus terminator)
/// @param out: Pointer to the output buffer (may be reallocated)
/// @param cap: Pointer to the current buffer capacity (updated on growth)
/// @param out_len: Current number of bytes written
/// @param extra: Number of additional bytes about to be written
/// @return int: 1 on success, 0 on allocation failure
static int ensure_cap(char **out, size_t *cap, size_t out_len, size_t extra) {
	if (out_len + extra + 1 <= *cap) return 1;

	size_t new_cap = (*cap) * 2;
	while (new_cap < out_len + extra + 1) new_cap *= 2;

	char *grown = realloc(*out, new_cap);
	if (!grown) return 0;

	*out = grown;
	*cap = new_cap;
	return 1;
}

// ======================
// -- CONFIG
// ======================

/// @brief Load a config from a TOML file path, or raw TOML text if the path doesn't exist
/// @param cfg: Config instance to populate
/// @param arg: File path or raw TOML string (from --config), entries formatted as "alias=opcode" or "alias=opcode~method"
/// @return int: 1 on success, 0 if no entries were parsed
int okin_config_load(okin_config_t *cfg, const char *arg) {
	if (!arg) return 0;

	char *file_content = read_file(arg);
	int ok = parse_toml(cfg, file_content ? file_content : arg);
	free(file_content);

	if (ok) okin_config_save_cache(cfg);
	return ok;
}

/// @brief Persist a config so it survives between runs without --config
/// @param cfg: Config instance to write out
void okin_config_save_cache(const okin_config_t *cfg) {
	FILE *f = fopen(OKIN_CONFIG_CACHE_PATH, "w");
	if (!f) return;

	for (int i = 0; i < cfg->count; i++)
		fprintf(f, "%s=%s\n", cfg->entries[i].alias, cfg->entries[i].opcode);

	fclose(f);
}

/// @brief Load the last persisted config
/// @param cfg: Config instance to populate
/// @return int: 1 on success, 0 if no cache exists or it's empty
int okin_config_load_cache(okin_config_t *cfg) {
	char *content = read_file(OKIN_CONFIG_CACHE_PATH);
	if (!content) return 0;

	int ok = parse_toml(cfg, content);
	free(content);
	return ok;
}

/// @brief Replace alias identifiers with their opcode equivalent
/// @param cfg: Config instance with alias -> opcode mappings
/// @param source: Source code using custom alias keywords
/// @return char*: Heap-allocated expanded source (caller must free), or NULL on allocation failure
char *okin_config_expand(const okin_config_t *cfg, const char *source) {
	size_t len = strlen(source);
	size_t cap = len * 2 + 64;

	char *out = malloc(cap);
	if (!out) return NULL;

	size_t out_len = 0;

	for (size_t i = 0; i < len;) {
		const okin_config_entry_t *entry = find_alias_at(cfg, source, i);
		if (entry) {
			size_t opcode_len = strlen(entry->opcode);
			if (!ensure_cap(&out, &cap, out_len, opcode_len)) {
				free(out);
				return NULL;
			}
			memcpy(out + out_len, entry->opcode, opcode_len);
			out_len += opcode_len;
			i += strlen(entry->alias);
			continue;
		}

		out[out_len++] = source[i++];
	}

	out[out_len] = '\0';
	return out;
}
