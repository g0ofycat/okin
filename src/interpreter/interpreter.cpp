#include "interpreter.hpp"

// ======================
// -- enviroment
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

/// @brief Set a variable value
/// @param name
/// @param val
void enviroment::set(const std::string &name, const okin_val_t &val)
{
	if (globals.count(name))
	{
		enviroment *env = this;
		while (env->parent) env = env->parent;
		env->vars[name] = val;
		return;
	}

	auto it = vars.find(name);
	if (it != vars.end())
	{
		it->second = val;
		return;
	}

	if (!parent)
	{
		vars[name] = val;
		return;
	}

	okin_val_t *v = parent->get(name);
	if (v)
	{
		*v = val;
		return;
	}

	vars[name] = val;
}

/// @brief Declare a variable (current scope)
/// @param name
/// @param val
void enviroment::declare(const std::string &name, const okin_val_t &val)
{
	vars[name] = val;
}

/// @brief Mark variable as global
/// @param name
void enviroment::mark_global(const std::string &name)
{
	globals[name] = true;
}

// ======================
// -- interpreter
// ======================

/// @brief Initalize interpreter from a parsed lexer
/// @param parser
interpreter::interpreter(const parser_t *parser) : 
	program(parser->program),
	global_env(new enviroment(nullptr)),
	ip(0)
{}

/// @brief Interpreter destructor
interpreter::~interpreter() {
	delete global_env;
};

/// @brief Throw runtime error
/// @param msg: Message to display
[[ noreturn ]] void interpreter::runtime_error(const std::string &msg) {
	throw std::runtime_error("[okin runtime error] " + msg);
}

// ======================
// -- RESOLVE
// ======================

/// @brief Resolve leaf node value to okin_val_t
/// @param node
/// @param env
okin_val_t interpreter::resolve(const okin_node_t *node, enviroment *env)
{
	std::string raw(node->val_start, node->val_len);

	if (node->tok == TK_STRING)
		return { val_type_t::STR, raw };

	if (node->tok == TK_INT)
	{
		if (raw.find('.') != std::string::npos)
			return { val_type_t::FLOAT, std::stod(raw) };
		return { val_type_t::INT, (int64_t)std::stoll(raw) };
	}

	okin_val_t *v = env->get(raw);
	if (!v)                              runtime_error("undefined variable '" + raw + "'");
	if (v->type == val_type_t::NIL_VAL)  runtime_error("operation on nil '" + raw + "'");
	return *v;
}

// ======================
// -- EVAL
// ======================

/// @brief Evaluate a node to a value
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval(const okin_node_t *node, enviroment *env)
{
	if (node->opcode == NODE_LEAF)
		return resolve(node, env);

	eval_fn fn = EVAL_TABLE[node->opcode];
	if (!fn) runtime_error("non-evaluable opcode 0x" + std::to_string(node->opcode));
	return (this->*fn)(node, env);
}

// ======================
// -- EXECUTE BODY
// ======================

/// @brief Execute a body block
/// @param nodes
/// @param len
/// @param env
void interpreter::execute_body(okin_node_t **nodes, int len, enviroment *env)
{
	for (int i = 0; i < len; i++)
		execute(nodes[i], env);
}

// ======================
// -- EXECUTE
// ======================

/// @brief Execute a node
/// @param node
/// @param env
void interpreter::execute(const okin_node_t *node, enviroment *env)
{
	exec_fn fn = EXEC_TABLE[node->opcode];
	if (!fn) runtime_error("unknown opcode 0x" + std::to_string(node->opcode));
	(this->*fn)(node, env);
}

// ======================
// -- PUBLIC
// ======================

/// @brief Interpret and run token stream
void interpreter::run() {
	ip = 0;

	while (ip < program->len)
	{
		execute(program->nodes[ip], global_env);
		ip++;
	}
}
