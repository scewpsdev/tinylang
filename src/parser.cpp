#include "parser.h"

#include "ast.h"
#include "pch.h"

namespace parser {
	const Expression Null = { "" };
	const std::map<std::string, int> OP_PRECEDENCE = {
		{"=", 1}, {"||", 2}, {"&&", 3}, {"<", 7}, {">", 7}, {"<=", 7}, {">=", 7}, {"==", 7}, {"!=", 7}, {"+", 10}, {"-", 10}, {"*", 20}, {"/", 20}, {"%", 20},
	};

	Lexer* input;

	Expression parse_expr();

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

	Operator op_from_str(const std::string & str) {
	}

	bool is_op(const std::string & op) {
		Token tok = input->peek();
		return tok.type != "" && tok.type == "op" && (op == "" || tok.value == op);
	}

	void skip_punc(char ch) {
		if (is_punc(ch)) input->next();
		else input->error("Token '" + std::to_string(ch) + "' expected");
	}

	void skip_kw(std::string kw) {
		if (is_kw(kw)) input->next();
		else input->error("Keyword \"" + kw + "\" expected");
	}

	void skip_op(const std::string op) {
		if (is_op(op)) input->next();
		else input->error("Operator '" + op + "' expected");
	}

	Expression maybe_binary(Expression left, int prec) {
		if (is_op("")) {
			Token tok = input->peek();
			int tokprec = OP_PRECEDENCE.at(tok.value);
			if (tokprec > prec) {
				input->next();
				if (tok.value == "=")
					return maybe_binary(Assign("assign", tok.value, left, maybe_binary(parse_atom(), tokprec)), prec);
				else
					return maybe_binary(Binary("binary", tok.value, left, maybe_binary(parse_atom(), tokprec)), prec);
			}
		}
		return left;
	}

	template<typename T>
	std::vector<T> delimited(char start, char end, char separator, T(*parser)()) {
		std::vector<T> list;
		bool first = true;
		skip_punc(start);
		while (!input->eof()) {
			if (is_punc(stop)) break;
			if (first) first = false;
			else skip_punc(separator);
			if (is_punc(stop)) break;
			list.push_back(parser());
		}
		skip_punc(stop);
		return list;
	}

	Call parse_call(Expression func) {
		return Call("call", func, delimited('(', ')', ',', parse_expr));
	}

	std::string parse_varname() {
		Token name = input->next();
		if (name.type != "var") input->error("Variable name expected");
		return name.value;
	}

	If parse_if() {
		skip_kw("if");
		Expression cond = parse_expr();
		Expression then = parse_expr();
		Expression els = Null;
		if (is_kw("else")) {
			input->next();
			els = parse_expr();
		}
		return If("if", cond, then, els);
	}

	Closure parse_closure() {
		return Closure("closure", delimited('(', ')', ',', parse_varname), parse_ast());
	}

	Boolean parse_bool() {
		return Boolean("bool", input->next().value == "true");
	}

	Expression maybe_call(Expression(*call)()) {
		Expression result = call();
		return is_punc('(') ? parse_call(result) : result;
	}

	Expression parse_atom() {
		return maybe_call([]() -> Expression {
			});
	}

	Expression parse_prog() {
		std::vector<Expression> vars = delimited('{', '}', ';', parse_expr);
		if (vars.empty()) return Null;
		if (vars.size() == 1) return vars[0];
		return Program("prog", AST(vars));
	}

	Program parse_toplevel() {
		AST prog;
		while (!input->eof()) {
			prog.push(parse_expr());
			if (!input->eof()) skip_punc(';');
		}
		return { "prog", prog };
	}
}

Parser::Parser(Lexer lexer)
	: lexer(lexer) {
	parser::input = &this->lexer;
}