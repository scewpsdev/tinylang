#include "stdafx.h"

#include "printer.h"

namespace printer {
	AST* input;
	int numIndents = 0;

	void print_expr(Expression* expr, std::ostream& out);
	void print_ast(AST* ast, std::ostream& out);

	std::string indent() {
		return std::string((size_t)numIndents * 4, ' ');
	}

	void print_num(Number* num, std::ostream& out) {
		out << std::to_string(num->value);
	}

	void print_str(String* str, std::ostream& out) {
		out << "\"" << str->value << "\"";
	}

	void print_bool(Boolean* boolexpr, std::ostream& out) {
		out << boolexpr->value ? "true" : "false";
	}

	void print_ident(Identifier* ident, std::ostream& out) {
		out << ident->value;
	}

	void print_closure(Closure* cls, std::ostream& out) {
		out << "(";
		for (int i = 0; i < cls->args.size(); i++) {
			out << cls->args[i];
			if (i < cls->args.size() - 1) out << ", ";
		}
		out << ") ";
		print_expr(cls->body, out);
	}

	void print_call(Call* call, std::ostream& out) {
		print_expr(call->func, out);
		out << "(";
		for (int i = 0; i < call->args.size(); i++) {
			print_expr(call->args[i], out);
			if (i < call->args.size() - 1) out << ", ";
		}
		out << ")";
	}

	void print_if(If* ifexpr, std::ostream& out) {
		out << "if ";
		print_expr(ifexpr->cond, out);
		out << " ";
		print_expr(ifexpr->then, out);
		if (ifexpr->els->type != "") {
			out << " else ";
			print_expr(ifexpr->els, out);
		}
	}

	void print_assign(Assign* assign, std::ostream& out) {
		print_expr(assign->left, out);
		out << " " << assign->op << " ";
		print_expr(assign->right, out);
	}

	void print_binary(Binary* binary, std::ostream& out) {
		print_expr(binary->left, out);
		out << " " << binary->op << " ";
		print_expr(binary->right, out);
	}

	void print_prog(Program* prog, std::ostream& out) {
		out << indent() << "{" << std::endl;
		numIndents++;
		print_ast(prog->ast, out);
		numIndents--;
		out << indent() << "}";
	}

	void print_param(Parameter* param, std::ostream& out) {
		out << param->type << " " << param->name;
	}

	void print_function(Function* func, std::ostream& out) {
		out << (func->body ? "" : "ext ") << func->funcname << "(";
		for (int i = 0; i < func->params.size(); i++) {
			print_param(&func->params[i], out);
		}
		out << ")";
	}

	void print_expr(Expression* expr, std::ostream& out) {
		if (expr->type == "prog") print_prog((Program*)expr, out);
		if (expr->type == "binary") print_binary((Binary*)expr, out);
		if (expr->type == "assign") print_assign((Assign*)expr, out);
		if (expr->type == "if") print_if((If*)expr, out);
		if (expr->type == "call") print_call((Call*)expr, out);
		if (expr->type == "cls") print_closure((Closure*)expr, out);
		if (expr->type == "var") print_ident((Identifier*)expr, out);
		if (expr->type == "bool") print_bool((Boolean*)expr, out);
		if (expr->type == "str") print_str((String*)expr, out);
		if (expr->type == "num") print_num((Number*)expr, out);
		if (expr->type == "func") print_function((Function*)expr, out);
	}

	void print_ast(AST* ast, std::ostream& out) {
		for (int i = 0; i < ast->vars.size(); i++) {
			out << indent();
			print_expr(ast->vars[i], out);
			out << ";" << std::endl;
		}
	}

	void print(std::ostream& out) {
		print_ast(input, out);
		out << std::endl;
	}
}

ASTPrinter::ASTPrinter(AST* input) {
	printer::input = input;
}

void ASTPrinter::print(std::ostream& out) {
	printer::print(out);
}