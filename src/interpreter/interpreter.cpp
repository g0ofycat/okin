#include "interpreter.hpp"

// ======================
// -- SIGNALS
// ======================

struct break_signal {};
struct return_signal { okin_val_t value; };

// ======================
// -- CREATE VALS
// ======================

static okin_val_t make_int(int64_t v) { return { val_type_t::INT, v }; };
static okin_val_t make_float(double v) { return { val_type_t::FLOAT, v }; };
static okin_val_t make_bool(bool v) { return { val_type_t::BOOL, v}; };
static okin_val_t make_string(std::string v) { return { val_type_t::STR, std::move(v) }; };
static okin_val_t make_array() { return { val_type_t::ARR, std::make_shared<std::vector<okin_val_t>>() }; };
static okin_val_t make_nil() { return { val_type_t::NIL_VAL, false}; };

/// @brief Convert okin_val_t to string
/// @param v
/// @return std::string
static std::string val_to_string(const okin_val_t &v) {
	switch (v.type) {
		case val_type_t::INT:     return std::to_string(std::get<int64_t>(v.data));
		case val_type_t::FLOAT:   return std::to_string(std::get<double>(v.data));
		case val_type_t::BOOL:    return std::get<bool>(v.data) ? "true" : "false";
		case val_type_t::STR:     return std::get<std::string>(v.data);
		case val_type_t::ARR:     return "[array]";
		case val_type_t::NIL_VAL: return "nil";
	}

	return UNK_VAL;
}

/// @brief Convert okin_val_t to bool
/// @param v
/// @return bool
static bool val_to_bool(const okin_val_t &v) {
	switch (v.type) {
		case val_type_t::INT:     return std::get<int64_t>(v.data) != 0;
		case val_type_t::FLOAT:   return std::get<double>(v.data) != 0;
		case val_type_t::BOOL:    return std::get<bool>(v.data);
		case val_type_t::STR:     return !std::get<std::string>(v.data).empty();
		case val_type_t::ARR:     {
									  const auto &arr = std::get<okin_array_t>(v.data);
									  return arr && !arr->empty();
								  }
		case val_type_t::NIL_VAL: return false;
	}

	return UNK_VAL;
}

/// @brief Get token name
/// @param n
/// @return std::string
static std::string tok_name(const okin_node_t *n) {
	return std::string(n->val_start, n->val_len);
}

// ======================
// -- interpreter
// ======================

/// @brief Enviroment constructor
/// @param parent: Parent enviroment (default nullptr)
enviroment::enviroment(enviroment *parent) : parent(parent) {}

/// @brief Get variable value by name
/// @param name
/// @return okin_val_t*
okin_val_t *enviroment::get(const std::string &name) {
	auto it = vars.find(name);
	if (it != vars.end()) return &it->second;
	if (parent) return parent->get(name);
	return nullptr;
};
