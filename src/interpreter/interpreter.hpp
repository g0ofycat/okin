#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <variant>

#include "../parser/parser.h"

// ======================
// -- ENUMS & CLASSES
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

using okin_array_t = std::vector<okin_val_t>;

struct okin_val_t {
	val_type_t type;
	std::variant<int64_t, double, std::string, bool, okin_array_t> data;
};

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
		void declare(const std::string &name);

		/// @brief Mark variable as global
		/// @param name
		void mark_global(const std::string &name);
};

// ======================
// -- interpreter
// ======================

class interpreter {
	private:
		const okin_program_t *program;
		enviroment           *global_env;
		int                   ip;

		/// @brief Evaluate a node to a value
		/// @param node
		/// @param env
		/// @return okin_val_t
		okin_val_t eval(const okin_node_t *node, enviroment *env);

		/// @brief Execute a node
		/// @param node
		/// @param env
		void execute(okin_node_t *node, enviroment *env);

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

		/// @brief Interpret and run token stream
		void run();
};

#endif
