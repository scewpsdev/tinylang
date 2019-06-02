#include "stdafx.h"

#include "codegen.h"

namespace codegen {
	struct CodeBlock {
		AST* ast;
		std::stringstream out;
	};

	struct ModuleData {
		std::map<std::string, CodeBlock> blocks;
	};

	TCCState* tcc;
	std::map<std::string, AST*> moduleList;
	ModuleData* module;
	CodeBlock* block;
	int numIndents;

	std::string gen_expr(Expression* expr);

	std::string indent() {
		return std::string(numIndents, '\t');
	}

	std::string arg_list(std::vector<Expression*>& args) {
		std::stringstream s;
		for (int i = 0; i < args.size(); i++) {
			s << gen_expr(args[i]) << (i < args.size() - 1 ? ", " : "");
		}
		return s.str();
	}

	std::string gen_num(Number* num) {
		return std::to_string(num->value);
	}

	std::string gen_str(String* str) {
		return "";
	}

	std::string gen_bool(Boolean* boolexpr) {
		return "";
	}

	std::string gen_ident(Identifier* ident) {
		return ident->value;
	}

	std::string gen_closure(Closure* cls) {
		return "";
	}

	std::string gen_call(Call* call) {
		return gen_expr(call->func) + "(" + arg_list(call->args) + ")";
	}

	std::string gen_if(If* ifexpr) {
		return "";
	}

	std::string gen_assign(Assign* assign) {
		return "";
	}

	std::string gen_binary(Binary* binary) {
		return "";
	}

	std::string gen_prog(Program* prog) {
		return "";
	}

	std::string gen_expr(Expression* expr) {
		if (expr->type == "prog") return gen_prog((Program*)expr);
		if (expr->type == "binary") return gen_binary((Binary*)expr);
		if (expr->type == "assign") return gen_assign((Assign*)expr);
		if (expr->type == "if") return gen_if((If*)expr);
		if (expr->type == "call") return gen_call((Call*)expr);
		if (expr->type == "cls") return gen_closure((Closure*)expr);
		if (expr->type == "var") return gen_ident((Identifier*)expr);
		if (expr->type == "bool") return gen_bool((Boolean*)expr);
		if (expr->type == "str") return gen_str((String*)expr);
		if (expr->type == "num") return gen_num((Number*)expr);
		return "";
	}

	void gen_ast(CodeBlock* block) {
		codegen::block = block;
		AST* ast = block->ast;
		std::stringstream& out = block->out;

		out << '{' << std::endl;
		numIndents++;
		for (int i = 0; i < ast->vars.size(); i++) {
			out << indent();
			out << gen_expr(ast->vars[i]);
			out << ';' << std::endl;
		}
		out << '\r';
		numIndents--;
		out << indent() << '}';
		codegen::block = nullptr;
	}

	void gen_module(std::string name, AST* ast) {
		module = new ModuleData();

		module->blocks.insert(std::make_pair("_" + name + "_init", CodeBlock{ ast }));
		for (auto& pair : module->blocks) {
			gen_ast(&pair.second);
		}
		std::stringstream code;
		code << "#include <stdlib.h>" << std::endl;
		code << "#include <stdio.h>" << std::endl;
		for (auto& pair : module->blocks) {
			code << "int " << pair.first << "() {" << std::endl;
			code << pair.second.out.str() << "}" << std::endl;
		}
		std::string src = code.str();
		std::cout << src << std::endl;
		tcc_compile_string(tcc, src.c_str());

		delete module;
	}

	void error_callback(void* opaque, const char* msg) {
		std::cerr << "TCC: " << msg << std::endl;
	}

	void run() {
		tcc = tcc_new();
		tcc_set_error_func(tcc, nullptr, error_callback);
		tcc_add_include_path(tcc, "lib/include/");
		tcc_add_include_path(tcc, "./");
		tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

		// TODO resolve exports
		for (std::pair<std::string, AST*> pair : moduleList) {
			std::string moduleName = pair.first;
			AST* ast = pair.second;
			gen_module(moduleName, ast);
		}

		tcc_compile_string(tcc,
			"int _main_init();"
			"int main(int argc, char *argv[]) { return _main_init(); }"
		);

		std::cout << "### Test ###" << std::endl;
		tcc_run(tcc, 0, nullptr);
	}
}

Codegen::Codegen(std::map<std::string, AST*> modules) {
	codegen::moduleList = modules;
}

void Codegen::run() {
	codegen::run();
}