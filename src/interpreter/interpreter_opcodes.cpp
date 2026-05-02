#include "interpreter.hpp"

interpreter::exec_fn interpreter::EXEC_TABLE[TABLE_BYTE_LIMIT];
interpreter::eval_fn interpreter::EVAL_TABLE[TABLE_BYTE_LIMIT];

// ======================
// -- EXEC: MEMORY
// ======================

/// @brief Declare and assign a variable
/// @param node
/// @param env
void interpreter::exec_var(const okin_node_t *node, enviroment *env)
{
	env->declare(tok_name(node->args[0]), eval(node->args[1], env));
}

/// @brief Reassign an existing variable
/// @param node
/// @param env
void interpreter::exec_set(const okin_node_t *node, enviroment *env)
{
	std::string name = tok_name(node->args[0]);
	if (!env->get(name)) runtime_error("SET on undefined variable '" + name + "'");
	env->set(name, eval(node->args[1], env));
}

/// @brief Mark variable as writable from inner scope
/// @param node
/// @param env
void interpreter::exec_global(const okin_node_t *node, enviroment *env)
{
	env->mark_global(tok_name(node->args[0]));
}

// ======================
// -- EXEC: FUNCTIONS
// ======================

/// @brief Register a function definition
/// @param node
/// @param env
void interpreter::exec_function(const okin_node_t *node, enviroment *env)
{
	functions[tok_name(node->args[0])] = node;
}

/// @brief Invoke a function
/// @param node
/// @param env
void interpreter::exec_call(const okin_node_t *node, enviroment *env)
{
	std::string name = tok_name(node->args[0]);
	auto it = functions.find(name);
	if (it == functions.end())
		runtime_error("call to undefined function '" + name + "'");

	const okin_node_t *fn = it->second;
	auto fn_env = std::make_unique<enviroment>(global_env);

	int param_count = fn->argc - 1;
	int arg_count   = node->argc - 1;
	bool has_dest   = arg_count > param_count;
	int val_count   = has_dest ? arg_count - 1 : arg_count;

	for (int i = 0; i < param_count && i < val_count; i++)
		fn_env->declare(tok_name(fn->args[i + 1]), eval(node->args[i + 1], env));

	call_stack.push_back({ ip, has_dest ? tok_name(node->args[node->argc - 1]) : "", has_dest });

	okin_val_t ret = make_nil();
	try { execute_body(fn->body, fn->body_len, fn_env.get()); }
	catch (return_signal &r) { ret = r.value; }

	call_frame_t frame = call_stack.back();
	call_stack.pop_back();
	ip = frame.return_ip;

	if (frame.has_dest)
		env->declare(frame.dest, ret);
}

/// @brief Return a value from a function
/// @param node
/// @param env
void interpreter::exec_ret(const okin_node_t *node, enviroment *env)
{
	throw return_signal{ node->argc > 0 ? eval(node->args[0], env) : make_nil() };
}

// ======================
// -- EXEC: ITERATORS
// ======================

/// @brief Range-based for loop
/// @param node
/// @param env
void interpreter::exec_for(const okin_node_t *node, enviroment *env)
{
	std::string iter = tok_name(node->args[0]);
	int64_t start    = std::get<int64_t>(eval(node->args[1], env).data);
	int64_t end      = std::get<int64_t>(eval(node->args[2], env).data);
	int64_t step     = std::get<int64_t>(eval(node->args[3], env).data);

	auto loop_env = std::make_unique<enviroment>(env);
	try
	{
		for (int64_t i = start; i < end; i += step)
		{
			loop_env->declare(iter, make_int(i));
			execute_body(node->body, node->body_len, loop_env.get());
		}
	}
	catch (break_signal &) {}
}

/// @brief Conditional while loop
/// @param node
/// @param env
void interpreter::exec_while(const okin_node_t *node, enviroment *env)
{
	auto loop_env = std::make_unique<enviroment>(env);
	try
	{
		while (val_to_bool(eval(node->args[0], loop_env.get())))
			execute_body(node->body, node->body_len, loop_env.get());
	}
	catch (break_signal &) {}
}

/// @brief Break out of iterator
/// @param node
/// @param env
void interpreter::exec_break(const okin_node_t *node, enviroment *env)
{
	throw break_signal{};
}

// ======================
// -- EXEC: DATA
// ======================

/// @brief Get value at array index
/// @param node
/// @param env
void interpreter::exec_aget(const okin_node_t *node, enviroment *env)
{
	okin_val_t arr_v = eval(node->args[0], env);
	if (arr_v.type != val_type_t::ARR) runtime_error("AGET requires an array");
	int64_t idx     = std::get<int64_t>(eval(node->args[1], env).data);
	const auto &arr = *std::get<okin_array_t>(arr_v.data);
	if (idx < 0 || idx >= (int64_t)arr.size()) runtime_error("AGET index out of bounds");
	env->set(tok_name(node->args[2]), arr[idx]);
}

/// @brief Set value at array index
/// @param node
/// @param env
void interpreter::exec_aset(const okin_node_t *node, enviroment *env)
{
	std::string name = tok_name(node->args[0]);
	okin_val_t *v    = env->get(name);
	if (!v || v->type != val_type_t::ARR) runtime_error("ASET requires an array variable");
	int64_t idx  = std::get<int64_t>(eval(node->args[1], env).data);
	auto &arr    = *std::get<okin_array_t>(v->data);
	if (idx < 0 || idx >= (int64_t)arr.size()) runtime_error("ASET index out of bounds");
	arr[idx]     = eval(node->args[2], env);
}

// ======================
// -- EXEC: ARITHMETIC
// ======================

/// @brief Arithmetic operations ADD SUB MUL DIV MOD
/// @param node
/// @param env
void interpreter::exec_arith(const okin_node_t *node, enviroment *env)
{
	okin_val_t a     = eval(node->args[0], env);
	okin_val_t b     = eval(node->args[1], env);
	std::string dest = tok_name(node->args[2]);

	if (a.type == val_type_t::NIL_VAL || b.type == val_type_t::NIL_VAL)
		runtime_error("arithmetic on nil");

	if (node->opcode == ADD && a.type == val_type_t::STR)
	{
		env->set(dest, make_string(val_to_string(a) + val_to_string(b)));
		return;
	}

	bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
	if (use_float)
	{
		double af = to_double(a), bf = to_double(b), r = 0;
		switch (node->opcode)
		{
			case ADD: r = af + bf; break;
			case SUB: r = af - bf; break;
			case MUL: r = af * bf; break;
			case DIV: if (bf == 0.0) runtime_error("division by zero"); r = af / bf; break;
			case MOD: r = std::fmod(af, bf); break;
		}
		env->set(dest, make_float(r));
	}
	else
	{
		int64_t ai = std::get<int64_t>(a.data);
		int64_t bi = std::get<int64_t>(b.data);
		int64_t r  = 0;
		switch (node->opcode)
		{
			case ADD: r = ai + bi; break;
			case SUB: r = ai - bi; break;
			case MUL: r = ai * bi; break;
			case DIV: if (bi == 0) runtime_error("division by zero"); r = ai / bi; break;
			case MOD: if (bi == 0) runtime_error("modulo by zero");   r = ai % bi; break;
		}
		env->set(dest, make_int(r));
	}
}

// ======================
// -- EXEC: BRANCHING
// ======================

/// @brief Conditional if block
/// @param node
/// @param env
void interpreter::exec_if(const okin_node_t *node, enviroment *env)
{
	if (val_to_bool(eval(node->args[0], env)))
	{
		auto if_env = std::make_unique<enviroment>(env);
		execute_body(node->body, node->body_len, if_env.get());
	}
}

// ======================
// -- EXEC: JUMPS
// ======================

/// @brief Unconditional and conditional jumps
/// @param node
/// @param env
void interpreter::exec_jmp(const okin_node_t *node, enviroment *env)
{
	if (node->opcode == JMP) { ip = node->args[0]->jump_index - 1; return; }

	okin_val_t a = eval(node->args[0], env);
	okin_val_t b = eval(node->args[1], env);
	bool take    = false;

	if (node->opcode == JEQ) take = val_to_string(a) == val_to_string(b);
	if (node->opcode == JNE) take = val_to_string(a) != val_to_string(b);

	if (a.type == val_type_t::INT && b.type == val_type_t::INT)
	{
		int64_t ai = std::get<int64_t>(a.data);
		int64_t bi = std::get<int64_t>(b.data);
		if (node->opcode == JGT) take = ai > bi;
		if (node->opcode == JLT) take = ai < bi;
	}

	if (take) ip = node->args[2]->jump_index - 1;
}

/// @brief Label no-op
/// @param node
/// @param env
void interpreter::exec_label(const okin_node_t *node, enviroment *env) {}

// ======================
// -- EXEC: IO
// ======================

/// @brief IO stdlib handler
/// @param node
/// @param env
void interpreter::exec_io(const okin_node_t *node, enviroment *env)
{
	std::string method(node->method, node->method_len);

	if (method == "WRITELN")
	{
		for (int i = 0; i < node->argc; i++)
			std::cout << val_to_string(eval(node->args[i], env));
		std::cout << '\n';
	}
	else if (method == "WRITE")
	{
		for (int i = 0; i < node->argc; i++)
			std::cout << val_to_string(eval(node->args[i], env));
	}
	else if (method == "READ")
	{
		std::string input;
		std::getline(std::cin, input);
		env->declare(tok_name(node->args[0]), make_string(input));
	}
	else runtime_error("unknown IO method '" + method + "'");
}

// ======================
// -- EXEC: STRING
// ======================

/// @brief String stdlib handler
/// @param node
/// @param env
void interpreter::exec_string(const okin_node_t *node, enviroment *env)
{
	std::string method(node->method, node->method_len);

	if (method == "LEN")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		env->declare(tok_name(node->args[1]), make_int((int64_t)s.size()));
	}
	else if (method == "CONCAT")
	{
		std::string a = val_to_string(eval(node->args[0], env));
		std::string b = val_to_string(eval(node->args[1], env));
		env->declare(tok_name(node->args[2]), make_string(a + b));
	}
	else if (method == "SLICE")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		int64_t start = std::get<int64_t>(eval(node->args[1], env).data);
		int64_t end   = std::get<int64_t>(eval(node->args[2], env).data);
		if (start < 0 || end > (int64_t)s.size() || start > end)
			runtime_error("STRING SLICE index out of bounds");
		env->declare(tok_name(node->args[3]), make_string(s.substr(start, end - start)));
	}
	else if (method == "FIND")
	{
		std::string s       = val_to_string(eval(node->args[0], env));
		std::string pattern = val_to_string(eval(node->args[1], env));
		size_t pos          = s.find(pattern);
		env->declare(tok_name(node->args[2]), make_int(pos == std::string::npos ? -1 : (int64_t)pos));
	}
	else if (method == "UPPER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::toupper);
		env->declare(tok_name(node->args[1]), make_string(s));
	}
	else if (method == "LOWER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		env->declare(tok_name(node->args[1]), make_string(s));
	}
	else runtime_error("unknown STRING method '" + method + "'");
}

// ======================
// -- EXEC: MATH
// ======================

/// @brief Math stdlib handler
/// @param node
/// @param env
void interpreter::exec_math(const okin_node_t *node, enviroment *env)
{
	std::string method(node->method, node->method_len);

	if (method == "POW")
	{
		double base = to_double(eval(node->args[0], env));
		double exp  = to_double(eval(node->args[1], env));
		env->declare(tok_name(node->args[2]), make_float(std::pow(base, exp)));
	}
	else if (method == "SQRT")
	{
		double a = to_double(eval(node->args[0], env));
		if (a < 0.0) runtime_error("SQRT of negative number");
		env->declare(tok_name(node->args[1]), make_float(std::sqrt(a)));
	}
	else if (method == "ABS")
	{
		okin_val_t a      = eval(node->args[0], env);
		okin_val_t result = (a.type == val_type_t::FLOAT)
			? make_float(std::abs(to_double(a)))
			: make_int(std::abs(std::get<int64_t>(a.data)));
		env->declare(tok_name(node->args[1]), result);
	}
	else if (method == "MIN")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		okin_val_t result = use_float
			? make_float(std::min(to_double(a), to_double(b)))
			: make_int(std::min(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
		env->declare(tok_name(node->args[2]), result);
	}
	else if (method == "MAX")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		okin_val_t result = use_float
			? make_float(std::max(to_double(a), to_double(b)))
			: make_int(std::max(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
		env->declare(tok_name(node->args[2]), result);
	}
	else if (method == "FLOOR")
	{
		double a = to_double(eval(node->args[0], env));
		env->declare(tok_name(node->args[1]), make_int((int64_t)std::floor(a)));
	}
	else if (method == "CEIL")
	{
		double a = to_double(eval(node->args[0], env));
		env->declare(tok_name(node->args[1]), make_int((int64_t)std::ceil(a)));
	}
	else runtime_error("unknown MATH method '" + method + "'");
}

// ======================
// -- EVAL: COMPARISON
// ======================

/// @brief Comparison operators EQ NEQ GT LT GTE LTE
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_cmp(const okin_node_t *node, enviroment *env)
{
	okin_val_t a = eval(node->args[0], env);
	okin_val_t b = eval(node->args[1], env);

	if (node->opcode == EQ)  return make_bool(val_to_string(a) == val_to_string(b));
	if (node->opcode == NEQ) return make_bool(val_to_string(a) != val_to_string(b));

	if (a.type == val_type_t::INT && b.type == val_type_t::INT)
	{
		int64_t ai = std::get<int64_t>(a.data);
		int64_t bi = std::get<int64_t>(b.data);
		switch (node->opcode)
		{
			case GT:  return make_bool(ai >  bi);
			case LT:  return make_bool(ai <  bi);
			case GTE: return make_bool(ai >= bi);
			case LTE: return make_bool(ai <= bi);
		}
	}

	double af = to_double(a), bf = to_double(b);
	switch (node->opcode)
	{
		case GT:  return make_bool(af >  bf);
		case LT:  return make_bool(af <  bf);
		case GTE: return make_bool(af >= bf);
		case LTE: return make_bool(af <= bf);
	}

	return make_bool(false);
}

// ======================
// -- EVAL: LOGICAL
// ======================

/// @brief Logical operators AND OR NOT
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_logical(const okin_node_t *node, enviroment *env)
{
	if (node->opcode == NOT)
		return make_bool(!val_to_bool(eval(node->args[0], env)));

	// short circuit
	bool a = val_to_bool(eval(node->args[0], env));
	if (node->opcode == AND && !a) return make_bool(false);
	if (node->opcode == OR  &&  a) return make_bool(true);
	return make_bool(val_to_bool(eval(node->args[1], env)));
}

// ======================
// -- EVAL: ARRAY
// ======================

/// @brief Build array from args
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_array(const okin_node_t *node, enviroment *env)
{
	okin_val_t result = make_array();
	auto &arr = *std::get<okin_array_t>(result.data);
	for (int i = 0; i < node->argc; i++)
		arr.push_back(eval(node->args[i], env));
	return result;
}

// ======================
// -- EVAL: IN
// ======================

/// @brief Membership check
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_in(const okin_node_t *node, enviroment *env)
{
	okin_val_t item  = eval(node->args[0], env);
	okin_val_t arr_v = eval(node->args[1], env);
	if (arr_v.type != val_type_t::ARR) runtime_error("IN requires an array");
	const auto &arr = *std::get<okin_array_t>(arr_v.data);
	for (const auto &v : arr)
		if (val_to_string(v) == val_to_string(item)) return make_bool(true);
	return make_bool(false);
}
