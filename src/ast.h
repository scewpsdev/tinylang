#pragma once

#include "pch.h"

enum Operator {
};

class Expression {
public:
	std::string type;
};

class Number : public Expression {
public:
	int value;
};

class String : public Expression {
public:
	std::string value;
};

class Boolean : public Expression {
public:
	bool value;
};

class Identifier : public Expression {
public:
	std::string value;
};

class Closure : public Expression {
public:
	std::vector<std::string> args;
	class AST body;
};

class Call : public Expression {
public:
	class AST func;
	std::vector<Expression> args;
};

class If : public Expression {
public:
	Expression cond;
	class AST then;
	class AST els;
};

class Assign : public Expression {
public:
	Operator op;
	class AST left;
	class AST right;
};

class Binary : public Expression {
public:
	Operator op;
	class AST left;
	class AST right;
};

class Program : public Expression {
public:
	class AST prog;
};

class AST {
public:
	std::vector<Expression> vars;
	class AST* body;
};