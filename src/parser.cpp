#include "stdafx.h"

#include "parser.h"

namespace parser {
	const std::map<std::string, int> OP_PRECEDENCE = {
		{"=", 1}, {"+=", 1}, {"-=", 1}, {"*=", 1}, {"/=", 1}, {"%=", 1}, {"||", 2}, {"&&", 3}, {"<", 7}, {">", 7}, {"<=", 7}, {">=", 7}, {"==", 7}, {"!=", 7}, {"+", 10}, {"-", 10}, {"*", 20}, {"/", 20}, {"%", 20},
	};

	Lexer* input;

	Function* parse_func();
	Expression* parse_atom();
	Expression* parse_expr();
	Expression* parse_prog();

	Expression* maybe_unary(Expression* e);

	void unexpected() {
		input->error("Unexpected token \"" + input->peek().value + "\"");
	}

	bool is_punc(char ch) {
		Token tok = input->peek();
		return tok.type != "" && tok.type == "punc" && (ch == 0 || tok.value[0] == ch);
	}

	bool is_kw(std::string kw) {
		Token tok = input->peek();
		return tok.type != "" && tok.type == "kw" && (kw == "" || kw == tok.value);
	}

	bool is_op(const std::string& op) {
		Token tok = input->peek();
		return tok.type != "" && tok.type == "op" && (op == "" || tok.value == op);
	}

	void skip_punc(char ch) {
		if (is_punc(ch)) input->next();
		else input->error("Token '" + std::string(1, ch) + "' expected");
	}

	void skip_kw(std::string kw) {
		if (is_kw(kw)) input->next();
		else input->error("Keyword \"" + kw + "\" expected");
	}

	void skip_op(const std::string op) {
		if (is_op(op)) input->next();
		else input->error("Operator '" + op + "' expected");
	}

	Expression* maybe_binary(Expression* left, int prec) {
		if (is_op("")) {
			Token tok = input->peek();
			int tokprec = OP_PRECEDENCE.at(tok.value);
			if (tokprec > prec) {
				input->next();
				if (tok.value == "=" || tok.value.length() == 2 && (std::string("+-*/%").find(tok.value[0]) != std::string::npos) && tok.value[1] == '=') {
					return maybe_unary(maybe_binary(new Assign(tok.value, left, maybe_binary(maybe_unary(parse_atom()), tokprec)), prec));
				}
				else
					return maybe_unary(maybe_binary(new Binary(tok.value, left, maybe_binary(maybe_unary(parse_atom()), tokprec)), prec));
			}
		}
		return left;
	}

	template<typename T>
	std::vector<T> delimited(char start, char end, char separator, T(*parser)()) {
		std::vector<T> list;
		bool first = true;
		if (start) skip_punc(start);
		while (!input->eof()) {
			if (is_punc(end)) break;
			if (first) first = false;
			else skip_punc(separator);
			if (is_punc(end)) break;
			list.push_back(parser());
		}
		skip_punc(end);
		return list;
	}

	Boolean* parse_bool() {
		return new Boolean(input->next().value == "true");
	}

	std::string parse_ident() {
		Token name = input->next();
		if (name.type != "var") input->error("Variable name expected");
		return name.value;
	}

	Typename parse_typename() {
		Typename type;
		Token nametok = input->next();
		if (nametok.type != "var") input->error("Type name expected");
		type.name = nametok.value;
		if (is_punc('*')) {
			skip_punc('*');
			type.pointer = true;
		}
		return type;
	}

	Array* parse_array() {
		return new Array(delimited('[', ']', ',', parse_expr));
	}

	Closure* parse_closure() {
		return new Closure(delimited('(', ')', ',', parse_ident), parse_prog());
	}

	Call* parse_call(Expression* func) {
		return new Call(func, delimited('(', ')', ',', parse_expr));
	}

	Index* parse_index(Expression* expr) {
		return new Index(expr, delimited('[', ']', ',', parse_expr));
	}

	Member* parse_member(Expression* expr) {
		skip_punc('.');
		Expression* member = parse_atom();
		return new Member(expr, member);
		/*
		std::string name = input->next().value;
		return new Member(expr, name);
		*/
	}

	Expression* maybe_unary(Expression* e) {
		if (is_punc('(')) return maybe_unary(parse_call(e));
		if (is_punc('[')) return maybe_unary(parse_index(e));
		if (is_punc('.')) return maybe_unary(parse_member(e));
		if (is_op("++")) {
			skip_op("++");
			return new Unary("++", true, e);
		}
		if (is_op("--")) {
			skip_op("--");
			return new Unary("--", true, e);
		}
		return e;
	}

	Expression* maybe_unary(Expression* (*call)()) {
		Expression* e = call();
		return maybe_unary(e);
	}

	If* parse_if() {
		skip_kw("if");
		Expression* cond = parse_expr();
		Expression* then = parse_expr();
		Expression* els = nullptr;
		if (is_kw("else")) {
			input->next();
			els = parse_expr();
		}
		return new If(cond, then, els);
	}

	Loop* parse_loop() {
		skip_kw("loop");
		Expression* body = parse_expr();
		return new Loop(nullptr, nullptr, new Boolean(true), body);
	}

	Loop* parse_while() {
		skip_kw("while");
		Expression* cond = parse_expr();
		Expression* body = parse_expr();
		return new Loop(nullptr, nullptr, cond, body);
	}

	Loop* parse_for() {
		skip_kw("for");
		Expression* first = parse_expr();
		Expression* second = nullptr;
		Expression* third = nullptr;
		Expression* fourth = nullptr;
		if (is_punc(',')) {
			skip_punc(',');
			second = parse_expr();
		}
		if (is_punc(',')) {
			skip_punc(',');
			third = parse_expr();
		}
		if (is_punc(',')) {
			skip_punc(',');
			fourth = parse_expr();
		}
		if (is_punc(',')) {
			// TODO ERROR
		}
		Expression* body = parse_expr();
		if (first && !second) {
			if (first->type != "num") {
				// TODO ERROR
			}
			int numits = ((Number*)first)->value;
			return new Loop(new Assign("=", new Identifier("__it"), new Number(0)), new Unary("++", true, new Identifier("__it")), new Binary("<", new Identifier("__it"), new Number(numits)), body);
		}
		if (first && second && !third) {
			if (first->type != "var") {
				// TODO ERROR
			}
			if (second->type != "num") {
				// TODO ERROR
			}
			std::string itname = ((Identifier*)first)->value;
			int numits = ((Number*)second)->value;
			return new Loop(new Assign("=", new Identifier(itname), new Number(0)), new Unary("++", true, new Identifier(itname)), new Binary("<", new Identifier(itname), new Number(numits)), body);
		}
		if (first && second && third && !fourth) {
			std::string itname = ((Identifier*)first)->value;
			int start = ((Number*)second)->value;
			int numits = ((Number*)third)->value;
			return new Loop(new Assign("=", new Identifier(itname), new Number(start)), new Unary("++", true, new Identifier(itname)), new Binary("<", new Identifier(itname), new Number(numits)), body);
		}
		if (first && second && third && fourth) {
			std::string itname = ((Identifier*)first)->value;
			int start = ((Number*)second)->value;
			int numits = ((Number*)third)->value;
			int step = ((Number*)fourth)->value;
			return new Loop(new Assign("=", new Identifier(itname), new Number(0)), new Assign("+=", new Identifier(itname), new Number(step)), new Binary("<", new Identifier(itname), new Number(numits)), body);
		}
		else {
			// TODO ERROR
		}
		return nullptr;
	}

	std::tuple<Typename, std::string> parse_type_and_name() {
		Typename type = parse_typename();
		Token name = input->next();
		if (name.type != "var") input->error("Variable name expected");
		return std::make_tuple(type, name.value);
	}

	Class* parse_type() {
		skip_kw("class");
		std::string name = input->next().value;
		skip_punc('{');

		// Member variables
		std::vector<std::tuple<Typename, std::string>> elements = delimited(0, ';', ',', parse_type_and_name);

		// Member functions
		std::vector<Function*> methods;
		while (is_kw("def")) {
			Function* func = parse_func();
			methods.push_back(func);
		}

		skip_punc('}');
		return new Class(name, elements, methods);
	}

	Cast* parse_typecast() {
		skip_op("<");
		std::string type = input->next().value;
		bool pointer = is_op("*");
		if (pointer) skip_op("*");
		skip_op(">");
		Expression* expr = parse_expr();
		return new Cast(expr, Typename{ type, pointer });
	}

	Function* parse_ext() {
		skip_kw("ext");
		Token funcname = input->next();
		if (funcname.type != "var") input->error("Function name expected");
		return new Function(funcname.value, delimited('(', ')', ',', parse_type_and_name), nullptr);
	}

	Function* parse_func() {
		input->next();
		Token tok = input->next();
		std::string funcname = tok.value;
		auto params = delimited('(', ')', ',', parse_type_and_name);
		Expression* body = parse_expr();
		return new Function(funcname, params, body);
	}

	Expression* parse_atom() {
		//return maybe_unary([]() -> Expression * {
		if (is_kw("ext")) return parse_ext();
		if (is_kw("def")) return parse_func();
		if (is_punc('(')) {
			input->next();
			Expression* expr = parse_expr();
			skip_punc(')');
			return expr;
		}
		if (is_op("<")) return parse_typecast();
		if (is_punc('{')) return parse_prog();
		if (is_punc('[')) return parse_array();
		if (is_op("++") || is_op("--") || is_op("!") || is_op("&") || is_op("*")) {
			std::string op = input->next().value;
			return new Unary(op, false, maybe_unary(parse_atom()));
		}
		if (is_kw("if")) return parse_if();
		if (is_kw("loop")) return parse_loop();
		if (is_kw("while")) return parse_while();
		if (is_kw("for")) return parse_for();
		if (is_kw("true") || is_kw("false")) return parse_bool();
		if (is_kw("cls")) {
			input->next();
			return parse_closure();
		}
		if (is_kw("class")) return parse_type();
		Token tok = input->next();
		if (tok.type == "kw") {
			if (tok.value == "break") return new Break();
			if (tok.value == "continue") return new Continue();
		}
		if (tok.type == "var") return new Identifier(tok.value);
		if (tok.type == "num") return new Number(std::stoi(tok.value));
		if (tok.type == "char") return new Character((int)tok.value[0]);
		if (tok.type == "str") return new String(tok.value);
		unexpected();
		return nullptr;
		//});
	}

	Expression* parse_expr() {
		return maybe_binary(maybe_unary([]() -> Expression * { return parse_atom(); }), 0);
	}

	Expression* parse_prog() {
		std::vector<Expression*> vars = delimited('{', '}', ';', parse_expr);
		if (vars.empty()) return nullptr;
		//if (vars.size() == 1) return vars[0];
		return new Program(new AST(vars));
	}

	AST* parse_toplevel() {
		AST* ast = new AST();
		while (!input->eof()) {
			ast->push(parse_expr());
			if (!input->eof()) skip_punc(';');
		}
		return ast;
	}
}

Parser::Parser(Lexer* lexer) {
	parser::input = lexer;
}

AST* Parser::parse_toplevel() {
	return parser::parse_toplevel();
}