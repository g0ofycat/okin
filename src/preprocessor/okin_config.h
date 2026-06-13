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
#define OKIN_CONFIG_CACHE_PATH  ".okin_config_cache"

// ======================
// -- STRUCTS
// ======================

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

/// @brief Load a config from a TOML file path, or raw TOML text if the path doesn't exist
/// @param cfg: Config instance to populate
/// @param arg: File path or raw TOML string (from --config), entries formatted as "alias=opcode" or "alias=opcode~method"
/// @return int: 1 on success, 0 if no entries were parsed
int okin_config_load(okin_config_t *cfg, const char *arg);

/// @brief Persist a config so it survives between runs without --config
/// @param cfg: Config instance to write out
void okin_config_save_cache(const okin_config_t *cfg);

/// @brief Load the last persisted config
/// @param cfg: Config instance to populate
/// @return int: 1 on success, 0 if no cache exists or it's empty
int okin_config_load_cache(okin_config_t *cfg);

/// @brief Replace alias identifiers with their opcode equivalent
/// @param cfg: Config instance with alias -> opcode mappings
/// @param source: Source code using custom alias keywords
/// @return char*: Heap-allocated expanded source (caller must free), or NULL on allocation failure
char *okin_config_expand(const okin_config_t *cfg, const char *source);

#ifdef __cplusplus
}
#endif

#endif
