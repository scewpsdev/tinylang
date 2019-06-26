#pragma once

class Expression {
public:
	std::string type;
public:
	Expression(std::string type)
		: type(type) {}
	virtual bool lvalue() { return false; }
};

class Number : public Expression {
public:
	int value;
public:
	Number(int value)
		: Expression("num"), value(value) {}
};

class Character : public Expression {
public:
	int value;
public:
	Character(int value)
		: Expression("char"), value(value) {}
};

class String : public Expression {
public:
	std::string value;
public:
	String(std::string value)
		: Expression("str"), value(value) {}
};

class Boolean : public Expression {
public:
	bool value;
public:
	Boolean(bool value)
		: Expression("bool"), value(value) {}
};

class Identifier : public Expression {
public:
	std::string value;
public:
	Identifier(std::string value)
		: Expression("var"), value(value) {}
	virtual bool lvalue() override { return true; }
};

struct Typename {
	std::string name = "";
	bool pointer = false;
};

class Closure : public Expression {
public:
	std::vector<std::string> args;
	Expression* body;
public:
	Closure(std::vector<std::string> args, Expression* body)
		: Expression("cls"), args(args), body(body) {}
	~Closure() {
		delete body; body = nullptr;
	}
};

class Array : public Expression {
public:
	std::vector<Expression*> elements;
public:
	Array(std::vector<Expression*> elements)
		: Expression("arr"), elements(elements) {}
};

class Call : public Expression {
public:
	Expression* func;
	std::vector<Expression*> args;
public:
	Call(Expression* func, std::vector<Expression*> args)
		: Expression("call"), func(func), args(args) {}
	~Call() {
		delete func; func = nullptr;
		for (int i = 0; i < args.size(); i++) {
			delete args[i]; args[i] = nullptr;
		}
	}
};

class Index : public Expression {
public:
	Expression* expr;
	std::vector<Expression*> args;
public:
	Index(Expression* expr, std::vector<Expression*> args)
		: Expression("idx"), expr(expr), args(args) {}
	~Index() {
		delete expr; expr = nullptr;
		for (int i = 0; i < args.size(); i++) {
			delete args[i]; args[i] = nullptr;
		}
	}
	virtual bool lvalue() override { return true; }
};

class Member : public Expression {
public:
	Expression* expr;
	Expression* member;
public:
	Member(Expression* expr, Expression* member)
		: Expression("member"), expr(expr), member(member) {}
	~Member() {
		delete expr; expr = nullptr;
	}
	virtual bool lvalue() override { return true; }
};

class Cast : public Expression {
public:
	Expression* expr;
	Typename t;
public:
	Cast(Expression* expr, Typename type)
		: Expression("cast"), expr(expr), t(type) {}
	~Cast() {
		delete expr; expr = nullptr;
	}
};

class If : public Expression {
public:
	Expression* cond;
	Expression* then;
	Expression* els;
public:
	If(Expression* cond, Expression* then, Expression* els)
		: Expression("if"), cond(cond), then(then), els(els) {}
	~If() {
		delete cond; cond = nullptr;
		delete then; then = nullptr;
		if (els) {
			delete els; els = nullptr;
		}
	}
};

class Loop : public Expression {
public:
	Expression* init;
	Expression* iterate;
	Expression* cond;
	Expression* body;
public:
	Loop(Expression* init, Expression* iterate, Expression* cond, Expression* body)
		: Expression("loop"), init(init), iterate(iterate), cond(cond), body(body) {}
	~Loop() {
		delete init; init = nullptr;
		delete iterate; iterate = nullptr;
		delete cond; cond = nullptr;
		delete body; body = nullptr;
	}
};

class Assign : public Expression {
public:
	std::string op;
	Expression* left;
	Expression* right;
public:
	Assign(std::string op, Expression* left, Expression* right)
		: Expression("assign"), op(op), left(left), right(right) {}
	~Assign() {
		delete left; left = nullptr;
		delete right; right = nullptr;
	}
};

class Binary : public Expression {
public:
	std::string op;
	Expression* left;
	Expression* right;
public:
	Binary(std::string op, Expression* left, Expression* right)
		: Expression("binary"), op(op), left(left), right(right) {}
	~Binary() {
		delete left; left = nullptr;
		delete right; right = nullptr;
	}
};

class Unary : public Expression {
public:
	std::string op;
	bool position;
	Expression* expr;
public:
	Unary(std::string op, bool position, Expression* expr)
		: Expression("unary"), op(op), position(position), expr(expr) {}
	~Unary() {
		delete expr; expr = nullptr;
	}
	virtual bool lvalue() override {
		return ((op == "++" || op == "--") && !position) || (op == "*" && !position);
	}
};

class Break : public Expression {
public:
	Break()
		: Expression("break") {}
	~Break() {}
};

class Continue : public Expression {
public:
	Continue()
		: Expression("continue") {}
	~Continue() {}
};

class Class : public Expression {
public:
	std::string name;
	std::vector<std::tuple<Typename, std::string>> elements;
	std::vector<class Function*> methods;
public:
	Class(const std::string& name, std::vector<std::tuple<Typename, std::string>> types, std::vector<Function*> methods)
		: Expression("class"), name(name), elements(types), methods(methods) {}
};

class Function : public Expression {
public:
	std::string funcname;
	std::vector<std::tuple<Typename, std::string>> params;
	Expression* body;
public:
	Function(const std::string& funcname, std::vector<std::tuple<Typename, std::string>> args, Expression* body)
		: Expression("func"), funcname(funcname), params(args), body(body) {}
	~Function() {
		delete body; body = nullptr;
	}
};

class AST {
public:
	std::vector<Expression*> vars;
public:
	AST() {}
	AST(std::vector<Expression*> vars) : vars(vars) {}
	~AST() {
		for (int i = 0; i < vars.size(); i++) {
			delete vars[i];
			vars[i] = nullptr;
		}
	}

	inline void push(Expression* expr) {
		vars.push_back(expr);
	}
};

class Program : public Expression {
public:
	AST* ast;
public:
	Program(AST* ast)
		: Expression("prog"), ast(ast) {}
	~Program() {
		delete ast; ast = nullptr;
	}
};