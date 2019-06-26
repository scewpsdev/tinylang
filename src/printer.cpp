#include "stdafx.h"

#include "printer.h"

namespace printer {
	AST* input;
	int numIndents = 0;

	void print_func(Function* func, std::ostream& out);
	void print_expr(Expression* expr, std::ostream& out);
	void print_ast(AST* ast, std::ostream& out);

	std::string indent() {
		return std::string((size_t)numIndents * 4, ' ');
	}

	void print_num(Number* num, std::ostream& out) {
		out << std::to_string(num->value);
	}

	void print_char(Character* ch, std::ostream& out) {
		out << '\'' << (wchar_t)ch->value << '\'';
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

	void print_array(Array* array, std::ostream& out) {
		out << "[ ";
		for (int i = 0; i < array->elements.size(); i++) {
			print_expr(array->elements[i], out);
			out << (i < array->elements.size() - 1 ? ", " : "");
		}
		out << " ]";
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

	void print_index(Index* idx, std::ostream& out) {
		print_expr(idx->expr, out);
		out << "[";
		for (int i = 0; i < idx->args.size(); i++) {
			print_expr(idx->args[i], out);
			if (i < idx->args.size() - 1) out << ", ";
		}
		out << "]";
	}

	void print_member(Member* member, std::ostream& out) {
		print_expr(member->expr, out);
		out << ".";
		print_expr(member->member, out);
	}

	void print_if(If* ifexpr, std::ostream& out) {
		out << "if ";
		print_expr(ifexpr->cond, out);
		out << " ";
		print_expr(ifexpr->then, out);
		if (ifexpr->els) {
			out << " else ";
			print_expr(ifexpr->els, out);
		}
	}

	void print_loop(Loop* loop, std::ostream& out) {
		out << "loop ";
		if (loop->init) {
			print_expr(loop->init, out);
			out << "; ";
		}
		print_expr(loop->cond, out);
		if (loop->iterate) {
			out << "; ";
			print_expr(loop->iterate, out);
		}
		out << " ";
		print_expr(loop->body, out);
	}

	void print_type_and_name(std::tuple<Typename, std::string> t, std::ostream& out) {
		out << std::get<0>(t).name << (std::get<0>(t).pointer ? "*" : "") << " " << std::get<1>(t);
	}

	void print_class(Class* type, std::ostream& out) {
		out << "type " << type->name << " {" << std::endl;
		for (int i = 0; i < type->elements.size(); i++) {
			print_type_and_name(type->elements[i], out);
			out << (i < type->elements.size() - 1 ? "," : ";") << std::endl;
		}
		for (int i = 0; i < type->methods.size(); i++) {
			print_func(type->methods[i], out);
			out << ";" << std::endl;
		}
		out << "}";
	}

	void print_assign(Assign* assign, std::ostream& out) {
		print_expr(assign->left, out);
		out << " " << assign->op << " ";
		print_expr(assign->right, out);
	}

	void print_binary(Binary* binary, std::ostream& out) {
		out << "(";
		print_expr(binary->left, out);
		out << " " << binary->op << " ";
		print_expr(binary->right, out);
		out << ")";
	}

	void print_unary(Unary* unary, std::ostream& out) {
		if (!unary->position) out << unary->op;
		print_expr(unary->expr, out);
		if (unary->position) out << unary->op;
	}

	void print_break(std::ostream& out) {
		out << "break";
	}

	void print_continue(std::ostream& out) {
		out << "continue";
	}

	void print_prog(Program* prog, std::ostream& out) {
		out << indent() << "{" << std::endl;
		numIndents++;
		print_ast(prog->ast, out);
		numIndents--;
		out << indent() << "}";
	}

	void print_func(Function* func, std::ostream& out) {
		out << (func->body ? "def " : "ext ") << func->funcname << "(";
		for (int i = 0; i < func->params.size(); i++) {
			print_type_and_name(func->params[i], out);
		}
		out << ")";
		if (func->body) print_expr(func->body, out);
	}

	void print_expr(Expression* expr, std::ostream& out) {
		if (expr->type == "prog") print_prog((Program*)expr, out);
		if (expr->type == "break") print_break(out);
		if (expr->type == "continue") print_continue(out);
		if (expr->type == "binary") print_binary((Binary*)expr, out);
		if (expr->type == "unary") print_unary((Unary*)expr, out);
		if (expr->type == "assign") print_assign((Assign*)expr, out);
		if (expr->type == "if") print_if((If*)expr, out);
		if (expr->type == "loop") print_loop((Loop*)expr, out);
		if (expr->type == "class") print_class((Class*)expr, out);
		if (expr->type == "call") print_call((Call*)expr, out);
		if (expr->type == "idx") print_index((Index*)expr, out);
		if (expr->type == "member") print_member((Member*)expr, out);
		if (expr->type == "cls") print_closure((Closure*)expr, out);
		if (expr->type == "arr") print_array((Array*)expr, out);
		if (expr->type == "var") print_ident((Identifier*)expr, out);
		if (expr->type == "bool") print_bool((Boolean*)expr, out);
		if (expr->type == "str") print_str((String*)expr, out);
		if (expr->type == "num") print_num((Number*)expr, out);
		if (expr->type == "char") print_char((Character*)expr, out);
		if (expr->type == "func") print_func((Function*)expr, out);
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