#ifndef OKIN_CONFIG_H
#define OKIN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ======================
// -- CONSTS
// ======================

#define OKIN_CONFIG_MAX_ENTRIES 256
#define OKIN_CONFIG_MAX_LEN     32
#define OKIN_CONFIG_MAX_ARGS    64

#define OKIN_CONFIG_CACHE_PATH  ".okin_config_cache"

// ======================
// -- STRUCTS
// ======================

typedef struct {
	const char *opcode;
	int         arity;
	int         has_body;
} okin_opcode_info_t;

typedef struct {
	char alias[OKIN_CONFIG_MAX_LEN];
	char opcode[OKIN_CONFIG_MAX_LEN];
} okin_config_entry_t;

typedef struct {
	okin_config_entry_t entries[OKIN_CONFIG_MAX_ENTRIES];
	int count;
} okin_config_t;

// ======================
// -- CONFIG
// ======================

/// @brief Load a config from a file path or raw text, and cache it
/// @param cfg: Config instance to populate
/// @param arg: File path or raw config string
/// @return int: 1 on success, 0 if no entries were parsed
const okin_opcode_info_t *okin_opcode_lookup(const char *opcode);

/// @brief Load a config from a TOML file path, or raw TOML text if the path doesn't exist
/// @param cfg: Config instance to populate
/// @param arg: File path or raw TOML string (from --config), entries formatted as "alias=opcode" or "alias=opcode~method"
/// @return int: 1 on success, 0 if no entries were parsed
int okin_config_load(okin_config_t *cfg, const char *arg);

/// @brief Persist a config to disk so it survives between runs
/// @param cfg: Config instance to write
void okin_config_save_cache(const okin_config_t *cfg);

/// @brief Load the last persisted config from disk
/// @param cfg: Config instance to populate
/// @return int: 1 on success, 0 if no cache or it's empty
int okin_config_load_cache(okin_config_t *cfg);

/// @brief Replace alias identifiers in source with their expanded opcode form
/// @param cfg: Config instance with alias -> opcode mappings
/// @param source: Source code using alias keywords
/// @return char*: Heap-allocated expanded source (caller must free), or NULL on failure
char *okin_config_expand(const okin_config_t *cfg, const char *source);

#ifdef __cplusplus
}
#endif

#endif
