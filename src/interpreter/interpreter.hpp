#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <variant>

#include "../parser/parser.h"

// ======================
// -- CONSTS
// ======================

#define UNK_VAL "<UNK>"
#define TABLE_BYTE_LIMIT 256

// ======================
// -- CLASSES & STRUCTS
// ======================

enum class val_type_t {
	INT,
	FLOAT,
	STR,
	BOOL,
	ARR,
	NIL_VAL
};

struct okin_val_t;

using okin_array_t = std::shared_ptr<std::vector<okin_val_t>>;

struct okin_val_t {
	val_type_t type;
	std::variant<int64_t, double, std::string, bool, okin_array_t> data;
};

struct break_signal {};
struct return_signal { okin_val_t value; };

// ======================
// -- CALL FRAME
// ======================

struct call_frame_t
{
	int         return_ip;
	std::string dest;
	bool        has_dest;
};

// ======================
// -- CREATE VALS
// ======================

inline okin_val_t make_int(int64_t v) { return { val_type_t::INT, v }; };
inline okin_val_t make_float(double v) { return { val_type_t::FLOAT, v }; };
inline okin_val_t make_bool(bool v) { return { val_type_t::BOOL, v}; };
inline okin_val_t make_string(std::string v) { return { val_type_t::STR, std::move(v) }; };
inline okin_val_t make_array() { return { val_type_t::ARR, std::make_shared<std::vector<okin_val_t>>() }; };
inline okin_val_t make_nil() { return { val_type_t::NIL_VAL, false}; };

/// @brief Convert okin_val_t to string
/// @param v
/// @return std::string
inline std::string val_to_string(const okin_val_t &v) {
	switch (v.type) {
		case val_type_t::INT:
			return std::to_string(std::get<int64_t>(v.data));
		case val_type_t::FLOAT:
			return std::to_string(std::get<double>(v.data));
		case val_type_t::BOOL:
			return std::get<bool>(v.data) ? "true" : "false";
		case val_type_t::STR:
			return std::get<std::string>(v.data);
		case val_type_t::ARR:
			return "[array]";
		case val_type_t::NIL_VAL:
			return "nil";
	}
	return UNK_VAL;
}

/// @brief Convert okin_val_t to bool
/// @param v
/// @return bool
inline bool val_to_bool(const okin_val_t &v) {
	switch (v.type) {
		case val_type_t::INT:
			return std::get<int64_t>(v.data) != 0;
		case val_type_t::FLOAT:
			return std::get<double>(v.data) != 0;
		case val_type_t::BOOL:
			return std::get<bool>(v.data);
		case val_type_t::STR:
			return !std::get<std::string>(v.data).empty();
		case val_type_t::ARR: {
								  const auto &arr = std::get<okin_array_t>(v.data);
								  return arr && !arr->empty();
							  }
		case val_type_t::NIL_VAL:
							  return false;
	}
	return false;
}

/// @brief Get token name
/// @param n
/// @return std::string
inline std::string tok_name(const okin_node_t *n) {
	return std::string(n->val_start, n->val_len);
}

/// @brief ::FLOAT or int64_t to double
/// @param v
/// @return double
inline double to_double(const okin_val_t &v)
{
	if (v.type == val_type_t::FLOAT) return std::get<double>(v.data);
	return (double)std::get<int64_t>(v.data);
}

// ======================
// -- enviroment
// ======================

class enviroment {
	public:
		enviroment *parent;
		std::unordered_map<std::string, okin_val_t> vars;
		std::unordered_map<std::string, bool>       globals;

		/// @brief Enviroment constructor
		/// @param parent: Parent enviroment (default nullptr)
		explicit enviroment(enviroment *parent = nullptr);

		/// @brief Get variable value by name
		/// @param name
		/// @return okin_val_t*
		okin_val_t *get(const std::string &name);

		/// @brief Set a variable value
		/// @param name
		/// @param val
		void set(const std::string &name, const okin_val_t &val);

		/// @brief Declare a variable (current scope)
		/// @param name
		/// @param val
		void declare(const std::string &name, const okin_val_t &val);

		/// @brief Mark variable as global
		/// @param name
		void mark_global(const std::string &name);
};

// ======================
// -- interpreter
// ======================

class interpreter {
	private:
		// ======================
		// -- TYPES
		// ======================

		using exec_fn = void (interpreter::*)(const okin_node_t*, enviroment*);
		using eval_fn = okin_val_t (interpreter::*)(const okin_node_t*, enviroment*);

		// ======================
		// -- DATA
		// ======================

		std::unordered_map<std::string, const okin_node_t *> functions;
		std::vector<call_frame_t>                            call_stack;

		const okin_program_t *program;
		enviroment           *global_env;
		int                   ip;

		// ======================
		// -- DISPATCH TABLES
		// ======================

		static exec_fn EXEC_TABLE[TABLE_BYTE_LIMIT];
		static eval_fn EVAL_TABLE[TABLE_BYTE_LIMIT];

		/// @brief Initalize all opcode logic lookups
		static void init_tables();

		// ======================
		// -- CORE
		// ======================

		/// @brief Execute a body block
		/// @param nodes
		/// @param len
		/// @param env
		void execute_body(okin_node_t **nodes, int len, enviroment *env);

		/// @brief Resolve leaf node value to okin_val_t
		/// @param node
		/// @param env
		okin_val_t resolve(const okin_node_t *node, enviroment *env);

		/// @brief Throw runtime error
		/// @param msg: Message to display
		[[ noreturn ]] void runtime_error(const std::string &msg);

		// ======================
		// -- EXEC HANDLERS
		// ======================

		void exec_var(const okin_node_t *node, enviroment *env);
		void exec_set(const okin_node_t *node, enviroment *env);
		void exec_global(const okin_node_t *node, enviroment *env);
		void exec_function(const okin_node_t *node, enviroment *env);
		void exec_call(const okin_node_t *node, enviroment *env);
		void exec_ret(const okin_node_t *node, enviroment *env);
		void exec_for(const okin_node_t *node, enviroment *env);
		void exec_while(const okin_node_t *node, enviroment *env);
		void exec_break(const okin_node_t *node, enviroment *env);
		void exec_aget(const okin_node_t *node, enviroment *env);
		void exec_aset(const okin_node_t *node, enviroment *env);
		void exec_arith(const okin_node_t *node, enviroment *env);
		void exec_if(const okin_node_t *node, enviroment *env);
		void exec_jmp(const okin_node_t *node, enviroment *env);
		void exec_label(const okin_node_t *node, enviroment *env);
		void exec_io(const okin_node_t *node, enviroment *env);
		void exec_string(const okin_node_t *node, enviroment *env);
		void exec_math(const okin_node_t *node, enviroment *env);

		// ======================
		// -- EVAL HANDLERS
		// ======================

		okin_val_t eval_cmp(const okin_node_t *node, enviroment *env);
		okin_val_t eval_logical(const okin_node_t *node, enviroment *env);
		okin_val_t eval_array(const okin_node_t *node, enviroment *env);
		okin_val_t eval_in(const okin_node_t *node, enviroment *env);

	public:
		// ======================
		// -- { CON | DE}STRUCTOR
		// ======================

		/// @brief Initalize interpreter from a parsed lexer
		/// @param parser
		explicit interpreter(const parser_t *parser);

		/// @brief Interpreter destructor
		~interpreter();

		// ======================
		// -- SPECIAL
		// ======================

		/// @brief Prevent copies
		interpreter(const interpreter &) = delete;

		/// @brief Prevent copies
		interpreter &operator=(const interpreter &) = delete;

		// ======================
		// -- PUBLIC
		// ======================

		/// @brief Evaluate a node to a value
		/// @param node
		/// @param env
		/// @return okin_val_t
		okin_val_t eval(const okin_node_t *node, enviroment *env);

		/// @brief Execute a node
		/// @param node
		/// @param env
		void execute(const okin_node_t *node, enviroment *env);

		/// @brief Interpret and run token stream
		void run();
};

#endif
