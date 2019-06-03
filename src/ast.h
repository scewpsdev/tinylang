#pragma once

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
	Number(int value)
		: Expression("num"), value(value) {}
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
};

class Parameter {
public:
	std::string type;
	std::string name;
public:
	Parameter(const std::string& type, const std::string& name)
		: type(type), name(name) {}
};

class Extern : public Expression {
public:
	std::string funcname;
	std::vector<Parameter> args;
public:
	Extern(const std::string& funcname, std::vector<Parameter> args)
		: Expression("ext"), funcname(funcname), args(args) {}
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
			delete args[i];
			args[i] = nullptr;
		}
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