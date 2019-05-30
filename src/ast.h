#pragma once

#include "pch.h"

class Expression {
public:
	std::string type;
public:
	Expression(std::string type)
		: type(type) {}
};

class Number : public Expression {
public:
	int value;
public:
	Number(std::string type, int value)
		: Expression(type), value(value) {}
};

class String : public Expression {
public:
	std::string value;
public:
	String(std::string type, std::string value)
		: Expression(type), value(value) {}
};

class Boolean : public Expression {
public:
	bool value;
public:
	Boolean(std::string type, bool value)
		: Expression(type), value(value) {}
};

class Identifier : public Expression {
public:
	std::string value;
public:
	Identifier(std::string type, std::string value)
		: Expression(type), value(value) {}
};

class Closure : public Expression {
public:
	std::vector<std::string> args;
	class AST body;
public:
	Closure(std::string type, std::vector<std::string> args, AST body)
		: Expression(type), args(args), body(body) {}
};

class Call : public Expression {
public:
	Expression func;
	std::vector<Expression> args;
public:
	Call(std::string type, Expression func, std::vector<Expression> args)
		: Expression(type), func(func), args(args) {}
};

class If : public Expression {
public:
	Expression cond;
	Expression then;
	Expression els;
public:
	If(std::string type, Expression cond, Expression then, Expression els)
		: Expression(type), cond(cond), then(then), els(els) {}
};

class Assign : public Expression {
public:
	std::string op;
	Expression left;
	Expression right;
public:
	Assign(std::string type, std::string op, Expression left, Expression right)
		: Expression(type), op(op), left(left), right(right) {}
};

class Binary : public Expression {
public:
	std::string op;
	Expression left;
	Expression right;
public:
	Binary(std::string type, std::string op, Expression left, Expression right)
		: Expression(type), op(op), left(left), right(right) {}
};

class Program : public Expression {
public:
	class AST prog;
public:
	Program(std::string type, AST prog)
		: Expression(type), prog(prog) {}
};

class AST {
public:
	std::vector<Expression> vars;
	class AST* body;
public:
	AST() {}
	AST(std::vector<Expression> vars) : vars(vars) {}
	inline void push(Expression expr) {
		vars.push_back(expr);
	}
};