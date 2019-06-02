#include "stdafx.h"

#include "parser.h"

namespace parser {
	const std::map<std::string, int> OP_PRECEDENCE = {
		{"=", 1}, {"||", 2}, {"&&", 3}, {"<", 7}, {">", 7}, {"<=", 7}, {">=", 7}, {"==", 7}, {"!=", 7}, {"+", 10}, {"-", 10}, {"*", 20}, {"/", 20}, {"%", 20},
	};

	Lexer* input;

	Expression* parse_atom();
	Expression* parse_expr();
	Expression* parse_prog();

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
				if (tok.value == "=")
					return maybe_binary(new Assign(tok.value, left, maybe_binary(parse_atom(), tokprec)), prec);
				else
					return maybe_binary(new Binary(tok.value, left, maybe_binary(parse_atom(), tokprec)), prec);
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
			if (is_punc(end)) break;
			if (first) first = false;
			else skip_punc(separator);
			if (is_punc(end)) break;
			list.push_back(parser());
		}
		skip_punc(end);
		return list;
	}

	Call* parse_call(Expression* func) {
		return new Call(func, delimited('(', ')', ',', parse_expr));
	}

	std::string parse_varname() {
		Token name = input->next();
		if (name.type != "var") input->error("Variable name expected");
		return name.value;
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

	Closure* parse_closure() {
		return new Closure(delimited('(', ')', ',', parse_varname), parse_prog());
	}

	Boolean* parse_bool() {
		return new Boolean(input->next().value == "true");
	}

	Expression* maybe_call(Expression* (*call)()) {
		Expression* result = call();
		return is_punc('(') ? parse_call(result) : result;
	}

	Expression* parse_atom() {
		return maybe_call([]() -> Expression * {
			if (is_punc('(')) {
				input->next();
				Expression* expr = parse_expr();
				skip_punc(')');
				return expr;
			}
			if (is_punc('{')) return parse_prog();
			if (is_kw("if")) return parse_if();
			if (is_kw("true") || is_kw("false")) return parse_bool();
			if (is_kw("cls")) {
				input->next();
				return parse_closure();
			}
			Token tok = input->next();
			if (tok.type == "var") return new Identifier(tok.value);
			if (tok.type == "num") return new Number(std::stoi(tok.value));
			if (tok.type == "str") return new String(tok.value);
			unexpected();
			return nullptr;
		});
	}

	Expression* parse_expr() {
		return maybe_call([]() -> Expression * {
			return maybe_binary(parse_atom(), 0);
		});
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