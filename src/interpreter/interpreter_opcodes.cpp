#include "interpreter.hpp"

interpreter::exec_fn interpreter::EXEC_TABLE[TABLE_BYTE_LIMIT];
interpreter::eval_fn interpreter::EVAL_TABLE[TABLE_BYTE_LIMIT];

// ======================
// -- UTILITY
// ======================

/// @brief Expect an integer value, throw if not
/// @param v
/// @param ctx
/// @return int64_t
int64_t interpreter::expect_int(const okin_val_t &v, const char *ctx) {
	if (v.type != val_type_t::INT)
		runtime_error(std::string(ctx) + " requires an integer");
	return std::get<int64_t>(v.data);
}

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
	std::string_view name = tok_name(node->args[0]);
	if (!env->get(name)) runtime_error("SET on undefined variable '" + std::string(name) + "'");
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
	std::string_view name = tok_name(node->args[0]);
	auto it = functions.find(std::string(name));
	if (it == functions.end())
		runtime_error("call to undefined function '" + std::string(name) + "'");

	const okin_node_t *fn = it->second;
	auto fn_env = std::make_unique<enviroment>(global_env);

	int param_count = fn->argc - 1;
	int arg_count   = node->argc - 1;
	bool has_dest   = arg_count > param_count;

	for (int i = 0; i < param_count && i < arg_count; i++) {
		std::string_view param_name = tok_name(fn->args[i + 1]);
		fn_env->declare(param_name, eval(node->args[i + 1], env));
	}

	std::string_view dest_name = has_dest ? tok_name(node->args[node->argc - 1]) : "";
	call_stack.push_back({ ip, dest_name, has_dest });

	okin_val_t ret = make_nil();
	try { execute_body(fn->body, fn->body_len, fn_env.get()); }
	catch (return_signal &r) { ret = r.value; }

	interpreter_call_frame_t frame = call_stack.back();
	call_stack.pop_back();
	ip = frame.return_ip;

	if (frame.has_dest)
		env->set(frame.dest, ret);
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
	std::string_view iter = tok_name(node->args[0]);
	int64_t start = expect_int(eval(node->args[1], env), "FOR start");
	int64_t end   = expect_int(eval(node->args[2], env), "FOR end");
	int64_t step  = expect_int(eval(node->args[3], env), "FOR step");

	if (step == 0) runtime_error("FOR step cannot be zero");

	auto loop_env = std::make_unique<enviroment>(env);
	try
	{
		for (int64_t i = start; i < end; i += step)
		{
			loop_env->declare(std::string(iter), make_int(i));
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
	int64_t idx     = expect_int(eval(node->args[1], env), "AGET index");
	const auto &arr = *std::get<okin_array_t>(arr_v.data);
	if (idx < 0 || idx >= (int64_t)arr.size()) runtime_error("AGET index out of bounds");
	env->set(std::string(tok_name(node->args[2])), arr[idx]);
}

/// @brief Set value at array index
/// @param node
/// @param env
void interpreter::exec_aset(const okin_node_t *node, enviroment *env)
{
	std::string_view name = tok_name(node->args[0]);
	okin_val_t *v         = env->get(std::string(name));
	if (!v || v->type != val_type_t::ARR) runtime_error("ASET requires an array variable");
	int64_t idx = expect_int(eval(node->args[1], env), "ASET index");
	auto &arr   = *std::get<okin_array_t>(v->data);
	if (idx < 0 || idx >= (int64_t)arr.size()) runtime_error("ASET index out of bounds");
	arr[idx]    = eval(node->args[2], env);
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
	std::string_view dest = tok_name(node->args[2]);

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
	bool taken = val_to_bool(eval(node->args[0], env));
	branch_stack.push_back(taken);
	if (taken) {
		auto if_env = std::make_unique<enviroment>(env);
		if (node->body_len > 0)
			execute_body(node->body, node->body_len, if_env.get());
		else
			runtime_error("no conditional body for IF");
	}
}

/// @brief Conditional elif block
/// @param node
/// @param env
void interpreter::exec_elif(const okin_node_t *node, enviroment *env)
{
	if (branch_stack.empty()) runtime_error("ELIF/ELSE without preceding IF");
	if (!branch_stack.back() && val_to_bool(eval(node->args[0], env))) {
		branch_stack.back() = true;
		auto elif_env = std::make_unique<enviroment>(env);
		if (node->body_len > 0)
			execute_body(node->body, node->body_len, elif_env.get());
		else
			runtime_error("no conditional body for ELIF");
	}
}

/// @brief Unconditional else block
/// @param node
/// @param env
void interpreter::exec_else(const okin_node_t *node, enviroment *env)
{
	if (branch_stack.empty()) runtime_error("ELIF/ELSE without preceding IF");
	if (!branch_stack.back()) {
		auto else_env = std::make_unique<enviroment>(env);
		if (node->body_len > 0)
			execute_body(node->body, node->body_len, else_env.get());
		else if (node->args[0])
			execute(node->args[0], else_env.get());
		else
			runtime_error("no conditional body for ELSE");
	}

	branch_stack.pop_back();
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

	double af = to_double(a), bf = to_double(b);
	if (node->opcode == JGT) take = af > bf;
	if (node->opcode == JLT) take = af < bf;

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
	std::string_view method(node->method, node->method_len);

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
	else runtime_error("unknown IO method '" + std::string(method) + "'");
}

// ======================
// -- EXEC: STRING
// ======================

/// @brief String stdlib handler
/// @param node
/// @param env
void interpreter::exec_string(const okin_node_t *node, enviroment *env)
{
	std::string_view method(node->method, node->method_len);

	if (method == "LEN")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		env->set(tok_name(node->args[1]), make_int((int64_t)s.size()));
	}
	else if (method == "CONCAT")
	{
		std::string a = val_to_string(eval(node->args[0], env));
		std::string b = val_to_string(eval(node->args[1], env));
		env->set(tok_name(node->args[2]), make_string(a + b));
	}
	else if (method == "SLICE")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		int64_t start = expect_int(eval(node->args[1], env), "SLICE start");
		int64_t end   = expect_int(eval(node->args[2], env), "SLICE end");
		if (start < 0 || end > (int64_t)s.size() || start > end)
			runtime_error("STRING SLICE index out of bounds");
		env->set(tok_name(node->args[3]), make_string(s.substr(start, end - start)));
	}
	else if (method == "FIND")
	{
		std::string s       = val_to_string(eval(node->args[0], env));
		std::string pattern = val_to_string(eval(node->args[1], env));
		size_t pos          = s.find(pattern);
		env->set(tok_name(node->args[2]), make_int(pos == std::string::npos ? -1 : (int64_t)pos));
	}
	else if (method == "UPPER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::toupper);
		env->set(tok_name(node->args[1]), make_string(s));
	}
	else if (method == "LOWER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		env->set(tok_name(node->args[1]), make_string(s));

	}
	else runtime_error("unknown STRING method '" + std::string(method) + "'");
}

// ======================
// -- EXEC: MATH
// ======================

/// @brief Math stdlib handler
/// @param node
/// @param env
void interpreter::exec_math(const okin_node_t *node, enviroment *env)
{
	std::string_view method(node->method, node->method_len);

	if (method == "POW")
	{
		double base = to_double(eval(node->args[0], env));
		double exp  = to_double(eval(node->args[1], env));
		env->set(tok_name(node->args[2]), make_float(std::pow(base, exp)));
	}
	else if (method == "SQRT")
	{
		double a = to_double(eval(node->args[0], env));
		if (a < 0.0) runtime_error("SQRT of negative number");
		env->set(tok_name(node->args[1]), make_float(std::sqrt(a)));
	}
	else if (method == "ABS")
	{
		okin_val_t a      = eval(node->args[0], env);
		okin_val_t result = (a.type == val_type_t::FLOAT)
			? make_float(std::abs(to_double(a)))
			: make_int(std::abs(std::get<int64_t>(a.data)));
		env->set(tok_name(node->args[1]), result);
	}
	else if (method == "MIN")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		okin_val_t result = use_float
			? make_float(std::min(to_double(a), to_double(b)))
			: make_int(std::min(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
		env->set(tok_name(node->args[2]), result);
	}
	else if (method == "MAX")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		okin_val_t result = use_float
			? make_float(std::max(to_double(a), to_double(b)))
			: make_int(std::max(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
		env->set(tok_name(node->args[2]), result);
	}
	else if (method == "FLOOR")
	{
		double a = to_double(eval(node->args[0], env));
		env->set(tok_name(node->args[1]), make_int((int64_t)std::floor(a)));
	}
	else if (method == "CEIL")
	{
		double a = to_double(eval(node->args[0], env));
		env->set(tok_name(node->args[1]), make_int((int64_t)std::ceil(a)));
	}
	else runtime_error("unknown MATH method '" + std::string(method) + "'");
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

	if (a.type == val_type_t::INT && b.type == val_type_t::INT)
	{
		int64_t ai = std::get<int64_t>(a.data);
		int64_t bi = std::get<int64_t>(b.data);
		switch (node->opcode)
		{
			case EQ:  return make_bool(ai == bi);
			case NEQ: return make_bool(ai != bi);
			case GT:  return make_bool(ai >  bi);
			case LT:  return make_bool(ai <  bi);
			case GTE: return make_bool(ai >= bi);
			case LTE: return make_bool(ai <= bi);
		}
	}

	if (node->opcode == EQ)  return make_bool(val_to_string(a) == val_to_string(b));
	if (node->opcode == NEQ) return make_bool(val_to_string(a) != val_to_string(b));

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

// ======================
// -- EVAL: CALL
// ======================

/// @brief Evaluate a function call expression, returning the function's return value
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_call(const okin_node_t *node, enviroment *env)
{
	std::string_view name = tok_name(node->args[0]);
	auto it = functions.find(std::string(name));
	if (it == functions.end())
		runtime_error("call to undefined function '" + std::string(name) + "'");

	const okin_node_t *fn = it->second;
	auto fn_env = std::make_unique<enviroment>(global_env);

	int param_count = fn->argc - 1;
	int arg_count   = node->argc - 1;

	for (int i = 0; i < param_count && i < arg_count; i++) {
		std::string_view param_name = tok_name(fn->args[i + 1]);
		fn_env->declare(param_name, eval(node->args[i + 1], env));
	}

	call_stack.push_back({ ip, "", false });

	okin_val_t ret = make_nil();
	try { execute_body(fn->body, fn->body_len, fn_env.get()); }
	catch (return_signal &r) { ret = r.value; }

	call_stack.pop_back();

	return ret;
}

// ======================
// -- EVAL: ARITHMETIC
// ======================

/// @brief Arithmetic operations as expressions ADD SUB MUL DIV MOD
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_arith(const okin_node_t *node, enviroment *env)
{
	okin_val_t a = eval(node->args[0], env);
	okin_val_t b = eval(node->args[1], env);

	if (a.type == val_type_t::NIL_VAL || b.type == val_type_t::NIL_VAL)
		runtime_error("arithmetic on nil");

	if (node->opcode == ADD && a.type == val_type_t::STR)
		return make_string(val_to_string(a) + val_to_string(b));

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
		return make_float(r);
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
		return make_int(r);
	}
}

// ======================
// -- EVAL: MATH
// ======================

/// @brief Math stdlib handler as expression
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_math(const okin_node_t *node, enviroment *env)
{
	std::string_view method(node->method, node->method_len);

	if (method == "POW")
	{
		double base = to_double(eval(node->args[0], env));
		double exp  = to_double(eval(node->args[1], env));
		return make_float(std::pow(base, exp));
	}
	else if (method == "SQRT")
	{
		double a = to_double(eval(node->args[0], env));
		if (a < 0.0) runtime_error("SQRT of negative number");
		return make_float(std::sqrt(a));
	}
	else if (method == "ABS")
	{
		okin_val_t a = eval(node->args[0], env);
		return (a.type == val_type_t::FLOAT)
			? make_float(std::abs(to_double(a)))
			: make_int(std::abs(std::get<int64_t>(a.data)));
	}
	else if (method == "MIN")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		return use_float
			? make_float(std::min(to_double(a), to_double(b)))
			: make_int(std::min(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
	}
	else if (method == "MAX")
	{
		okin_val_t a   = eval(node->args[0], env);
		okin_val_t b   = eval(node->args[1], env);
		bool use_float = (a.type == val_type_t::FLOAT || b.type == val_type_t::FLOAT);
		return use_float
			? make_float(std::max(to_double(a), to_double(b)))
			: make_int(std::max(std::get<int64_t>(a.data), std::get<int64_t>(b.data)));
	}
	else if (method == "FLOOR")
	{
		double a = to_double(eval(node->args[0], env));
		return make_int((int64_t)std::floor(a));
	}
	else if (method == "CEIL")
	{
		double a = to_double(eval(node->args[0], env));
		return make_int((int64_t)std::ceil(a));
	}

	runtime_error("unknown MATH method '" + std::string(method) + "'");
	return make_nil();
}

// ======================
// -- EVAL: STRING
// ======================

/// @brief String stdlib handler as expression
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_string(const okin_node_t *node, enviroment *env)
{
	std::string_view method(node->method, node->method_len);

	if (method == "LEN")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		return make_int((int64_t)s.size());
	}
	else if (method == "CONCAT")
	{
		std::string a = val_to_string(eval(node->args[0], env));
		std::string b = val_to_string(eval(node->args[1], env));
		return make_string(a + b);
	}
	else if (method == "SLICE")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		int64_t start = expect_int(eval(node->args[1], env), "SLICE start");
		int64_t end   = expect_int(eval(node->args[2], env), "SLICE end");
		if (start < 0 || end > (int64_t)s.size() || start > end)
			runtime_error("STRING SLICE index out of bounds");
		return make_string(s.substr(start, end - start));
	}
	else if (method == "FIND")
	{
		std::string s       = val_to_string(eval(node->args[0], env));
		std::string pattern = val_to_string(eval(node->args[1], env));
		size_t pos          = s.find(pattern);
		return make_int(pos == std::string::npos ? -1 : (int64_t)pos);
	}
	else if (method == "UPPER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::toupper);
		return make_string(s);
	}
	else if (method == "LOWER")
	{
		std::string s = val_to_string(eval(node->args[0], env));
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return make_string(s);
	}

	runtime_error("unknown STRING method '" + std::string(method) + "'");
	return make_nil();
}

// ======================
// -- EVAL: IO
// ======================

/// @brief IO stdlib handler as expression (READ)
/// @param node
/// @param env
/// @return okin_val_t
okin_val_t interpreter::eval_io(const okin_node_t *node, enviroment *env)
{
	std::string_view method(node->method, node->method_len);

	if (method == "READ")
	{
		std::string input;
		std::getline(std::cin, input);
		return make_string(input);
	}

	runtime_error("IO method '" + std::string(method) + "' cannot be used as expression");
	return make_nil();
}
