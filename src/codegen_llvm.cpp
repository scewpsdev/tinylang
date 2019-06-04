#include "stdafx.h"

#include "codegen.h"

namespace codegen {
	struct CodeBlock {
		AST* ast;
		llvm::Function* func;
	};

	struct ModuleData {
		//std::map<std::string, CodeBlock> blocks;
		std::map<std::string, llvm::Value*> globals;
		llvm::Module* llvmMod;
	};

	llvm::LLVMContext context;
	llvm::IRBuilder<> builder(context);

	std::map<std::string, AST*> moduleList;
	ModuleData* module;
	CodeBlock* block;

	llvm::Value* gen_expr(Expression* expr);

	llvm::Type* llvm_type(std::string name) {
		// TODO
		return llvm::Type::getInt32Ty(context);
	}

	llvm::Value* gen_num(Number* num) {
		return llvm::ConstantInt::get(context, llvm::APInt(32, num->value));
	}

	llvm::Value* gen_str(String* str) {
		return nullptr;
	}

	llvm::Value* gen_bool(Boolean* boolexpr) {
		return nullptr;
	}

	llvm::Value* gen_ident(Identifier* ident) {
		// Global
		if (module->globals.count(ident->value)) return module->globals[ident->value];
		return nullptr;
	}

	llvm::Value* gen_closure(Closure* cls) {
		return nullptr;
	}

	llvm::Value* gen_call(Call* call) {
		llvm::Value* callee = gen_expr(call->func);
		//if (funcExpr->getType()->isFunctionTy()) {
			//llvm::Function* func = static_cast<llvm::Function*>(funcExpr);
		std::vector<llvm::Value*> args;
		for (int i = 0; i < call->args.size(); i++) {
			args.push_back(gen_expr(call->args[i]));
		}
		builder.CreateCall(callee, args);
		//}
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* gen_extern(Extern* ext) {
		std::vector<llvm::Type*> params;
		for (int i = 0; i < ext->args.size(); i++) {
			params.push_back(llvm_type(ext->args[i].type));
		}
		llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), params, false);
		llvm::Function* func = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, ext->funcname, module->llvmMod);
		module->globals.insert(std::make_pair(ext->funcname, func));
		return nullptr;
	}

	llvm::Value* gen_if(If* ifexpr) {
		return nullptr;
	}

	llvm::Value* gen_assign(Assign* assign) {
		return nullptr;
	}

	llvm::Value* gen_binary(Binary* binary) {
		return nullptr;
	}

	llvm::Value* gen_prog(Program* prog) {
		return nullptr;
	}

	llvm::Value* gen_expr(Expression* expr) {
		if (expr->type == "prog") return gen_prog((Program*)expr);
		if (expr->type == "binary") return gen_binary((Binary*)expr);
		if (expr->type == "assign") return gen_assign((Assign*)expr);
		if (expr->type == "if") return gen_if((If*)expr);
		if (expr->type == "call") return gen_call((Call*)expr);
		if (expr->type == "cls") return gen_closure((Closure*)expr);
		if (expr->type == "ext") return gen_extern((Extern*)expr);
		if (expr->type == "var") return gen_ident((Identifier*)expr);
		if (expr->type == "bool") return gen_bool((Boolean*)expr);
		if (expr->type == "str") return gen_str((String*)expr);
		if (expr->type == "num") return gen_num((Number*)expr);
		return nullptr;
	}

	void gen_ast(CodeBlock * block, const std::string & name) {
		codegen::block = block;
		AST* ast = block->ast;

		std::vector<llvm::Type*> params;
		llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), params, false);
		llvm::Function* func = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, name, nullptr);

		llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
		builder.SetInsertPoint(entry);
		llvm::Value* retVal = nullptr;
		for (int i = 0; i < ast->vars.size(); i++) {
			gen_expr(ast->vars[i]);
		}

		builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));
		module->llvmMod->getFunctionList().push_back(func);

		codegen::block = nullptr;
	}

	void gen_module(std::string name, AST * ast) {
		module = new ModuleData();
		module->llvmMod = new llvm::Module(name, context);

		//module->blocks.insert(std::make_pair("_" + name + "_init", CodeBlock{ ast }));
		//for (auto& pair : module->blocks) {
		//	gen_ast(&pair.second, "mainCRTStartup");
		//	llvm::verifyFunction(*pair.second.func);
		//}
		CodeBlock initBlock = { ast };
		gen_ast(&initBlock, "mainCRTStartup");

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

		std::string output = "out/" + name + ".o";
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
			linkCmd << "out/" << pair.first << ".o ";
		}
		linkCmd << "/subsystem:console /out:out/a.exe";
		//std::cout << linkCmd.str() << std::endl;
		system(linkCmd.str().c_str());

		std::cout << "### Test ###" << std::endl;
		system("/out/a.exe");
	}
}

Codegen::Codegen(std::map<std::string, AST*> modules) {
	codegen::moduleList = modules;
}

void Codegen::run() {
	codegen::run();
}