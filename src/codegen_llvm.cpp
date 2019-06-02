#include "stdafx.h"

#include "codegen.h"

namespace codegen {
	struct CodeBlock {
		AST* ast;
		llvm::Function* func;
	};

	struct ModuleData {
		std::map<std::string, CodeBlock> blocks;
		llvm::Module* llvmMod;
	};

	llvm::LLVMContext context;
	llvm::IRBuilder<> builder(context);

	std::map<std::string, AST*> moduleList;
	ModuleData* module;
	CodeBlock* block;

	std::string gen_expr(Expression* expr);

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

	void gen_ast(CodeBlock* block, const std::string& name) {
		codegen::block = block;
		AST* ast = block->ast;

		std::vector<llvm::Type*> params;
		llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), params, false);
		block->func = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, "", module->llvmMod);

		llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, name, block->func);
		builder.SetInsertPoint(entry);
		llvm::Value* retVal = nullptr;

		builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));

		codegen::block = nullptr;
	}

	void gen_module(std::string name, AST* ast) {
		module = new ModuleData();
		module->llvmMod = new llvm::Module(name, context);

		module->blocks.insert(std::make_pair("_" + name + "_init", CodeBlock{ ast }));
		for (auto& pair : module->blocks) {
			gen_ast(&pair.second, "main");
			llvm::verifyFunction(*pair.second.func);
		}

		// Generate binary
		module->llvmMod->print(llvm::errs(), nullptr);
		module->llvmMod->setTargetTriple(llvm::sys::getDefaultTargetTriple());
		std::string error;
		const llvm::Target* target = llvm::TargetRegistry::lookupTarget(module->llvmMod->getTargetTriple(), error);
		if (!target)
		{
			llvm::errs() << error;
			return;
		}

		std::string cpu = "generic";
		std::string features = "";
		llvm::TargetOptions options;
		llvm::Optional<llvm::Reloc::Model> relocModel;
		llvm::TargetMachine* machine = target->createTargetMachine(module->llvmMod->getTargetTriple(), "generic", features, options, relocModel);
		module->llvmMod->setDataLayout(machine->createDataLayout());

		std::string output = name + ".o";
		std::error_code errcode;
		llvm::raw_fd_ostream out(output, errcode, llvm::sys::fs::F_None);
		if (errcode)
		{
			llvm::errs() << "Could not open file " + errcode.message();
			return;
		}

		llvm::legacy::PassManager passManager;
		llvm::TargetMachine::CodeGenFileType filetype = llvm::TargetMachine::CGFT_ObjectFile;
		bool failed = machine->addPassesToEmitFile(passManager, out, nullptr, filetype);
		if (failed)
		{
			llvm::errs() << "Target machine can't emit file of this filetype";
			return;
		}

		passManager.run(*module->llvmMod);
		out.flush();

		// Clean up
		delete module->llvmMod;
		delete module;
	}

	void error_callback(void* opaque, const char* msg) {
		std::cerr << "TCC: " << msg << std::endl;
	}

	void run() {
		LLVMInitializeX86TargetInfo();
		LLVMInitializeX86Target();
		LLVMInitializeX86TargetMC();
		LLVMInitializeX86AsmParser();
		LLVMInitializeX86AsmPrinter();

		// TODO resolve exports
		for (std::pair<std::string, AST*> pair : moduleList) {
			std::string moduleName = pair.first;
			AST* ast = pair.second;
			gen_module(moduleName, ast);
		}

		// Run
		std::stringstream linkCmd;
		linkCmd << "lld-link ";
		for (std::pair<std::string, AST*> pair : moduleList) {
			linkCmd << pair.first << ".o ";
		}
		linkCmd << "/subsystem:windows /out:a.exe";
		system(linkCmd.str().c_str());

		std::cout << "### Test ###" << std::endl;
		system("a.exe");
	}
}

Codegen::Codegen(std::map<std::string, AST*> modules) {
	codegen::moduleList = modules;
}

void Codegen::run() {
	codegen::run();
}