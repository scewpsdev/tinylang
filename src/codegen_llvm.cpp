#include "stdafx.h"

#include "codegen.h"

namespace codegen {
	struct ModuleData {
		//std::map<std::string, CodeBlock> blocks;
		std::map<std::string, llvm::Value*> globals;
		llvm::Module* llvmMod;
		llvm::Function* llvmFunc;
	};
	struct CodeBlock {
		CodeBlock* parent;
		std::map<std::string, llvm::AllocaInst*> locals;
		CodeBlock(CodeBlock* parent) : parent(parent) {}
	};

	llvm::LLVMContext context;
	llvm::IRBuilder<> builder(context);

	std::map<std::string, AST*> moduleList;
	ModuleData* module;
	CodeBlock* block;

	llvm::Value* gen_expr(Expression* expr);

	llvm::Value* rval(llvm::Value* val) {
		if (val->getType()->isPointerTy()) {
			return builder.CreateLoad(val);
		}
		return val;
	}

	llvm::Type* llvm_type(std::string name) {
		// TODO
		if (name.length() >= 2 && name[0] == 'i') {
			int bitsize = std::stoi(name.substr(1));
			return llvm::Type::getIntNTy(context, bitsize);
		}
		return nullptr;
	}

	llvm::Value* cast(llvm::Value* val, llvm::Type* type) {
		if (val->getType() == type) return val;
		if (val->getType()->isIntegerTy() && type->isIntegerTy()) return builder.CreateSExtOrTrunc(val, type);
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* find_var(const std::string& name, CodeBlock* block) {
		if (!block) return nullptr;
		if (block->locals.count(name)) return block->locals[name];
		if (llvm::Value * val = find_var(name, block->parent)) return val;
		if (module->globals.count(name)) return module->globals[name];
		return nullptr;
	}

	llvm::AllocaInst* alloc_var(const std::string& name, llvm::Type* type) {
		llvm::BasicBlock* entry = &module->llvmFunc->getEntryBlock();
		llvm::IRBuilder<> builder(entry, entry->begin());
		llvm::AllocaInst* alloc = builder.CreateAlloca(type, nullptr, name);
		block->locals.insert(std::make_pair(name, alloc));
		return alloc;
	}

	llvm::Value* gen_num(Number* num) {
		return llvm::ConstantInt::get(context, llvm::APInt(32, num->value));
	}

	llvm::Value* gen_char(Character* ch) {
		return llvm::ConstantInt::get(context, llvm::APInt(8, ch->value));
	}

	llvm::Value* gen_str(String* str) {
		return nullptr;
	}

	llvm::Value* gen_bool(Boolean* boolexpr) {
		return nullptr;
	}

	llvm::Value* gen_ident(Identifier* ident) {
		return find_var(ident->value, block);
	}

	llvm::Value* gen_closure(Closure* cls) {
		return nullptr;
	}

	llvm::Value* gen_call(Call* call) {
		// Cast
		if (call->func->type == "var" && call->args.size() == 1) {
			if (llvm::Type * type = llvm_type(((Identifier*)call->func)->value)) {
				return cast(gen_expr(call->args[0]), type);
			}
		}
		// Function call
		llvm::Value* callee = gen_expr(call->func);
		std::vector<llvm::Value*> args;
		for (int i = 0; i < call->args.size(); i++) {
			args.push_back(rval(gen_expr(call->args[i])));
		}
		return builder.CreateCall(callee, args);
	}

	llvm::Value* gen_if(If* ifexpr) {
		return nullptr;
	}

	llvm::Value* gen_assign(Assign* assign) {
		llvm::Value* left = gen_expr(assign->left);
		llvm::Value* right = gen_expr(assign->right);
		if (!left) {
			if (assign->left->type == "var") {
				left = alloc_var(((Identifier*)assign->left)->value, right->getType());
			}
			else {
				// TODO ERROR
				return nullptr;
			}
		}
		builder.CreateStore(cast(rval(right), left->getType()->getPointerElementType()), left, false);
		return right;
	}

	llvm::Value* gen_binary(Binary* binary) {
		llvm::Value* left = rval(gen_expr(binary->left));
		llvm::Value* right = rval(gen_expr(binary->right));
		if (binary->op == "+") return builder.CreateAdd(left, right);
		if (binary->op == "-") return builder.CreateSub(left, right);
		if (binary->op == "*") return builder.CreateMul(left, right);
		if (binary->op == "/") return builder.CreateSDiv(left, right);
		if (binary->op == "%") return builder.CreateSRem(left, right);
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* gen_ast(AST* ast) {
		CodeBlock* parent = block;
		block = new CodeBlock(parent);
		for (int i = 0; i < ast->vars.size(); i++) {
			gen_expr(ast->vars[i]);
		}
		delete block;
		block = parent;
		return nullptr;
	}

	llvm::Value* gen_prog(Program* prog) {
		return gen_ast(prog->ast);
	}

	llvm::Value* gen_func(Function* func) {
		std::vector<llvm::Type*> params;
		for (int i = 0; i < func->params.size(); i++) {
			params.push_back(llvm_type(func->params[i].type));
		}
		llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), params, false);
		llvm::Function* llvmfunc = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, func->funcname, module->llvmMod);
		module->globals.insert(std::make_pair(func->funcname, llvmfunc));
		int i = 0;
		for (llvm::Argument& arg : llvmfunc->args()) {
			arg.setName(func->params[i++].name);
		}
		if (func->body) {
			llvm::BasicBlock* parentBlock = builder.GetInsertBlock();
			llvm::Function* parentFunc = module->llvmFunc;
			module->llvmFunc = llvmfunc;

			CodeBlock* parent = block;
			block = new CodeBlock(parent);
			for (llvm::Argument& arg : llvmfunc->args()) {
				block->locals.insert(std::make_pair((std::string)arg.getName(), (llvm::AllocaInst*) & arg));
			}

			llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", llvmfunc);
			builder.SetInsertPoint(entry);

			gen_expr(func->body);
			builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));

			delete block;
			block = parent;

			module->llvmFunc = parentFunc;
			builder.SetInsertPoint(parentBlock);
		}

		return nullptr;
	}

	llvm::Value* gen_expr(Expression* expr) {
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
		if (expr->type == "char") return gen_char((Character*)expr);
		if (expr->type == "func") return gen_func((Function*)expr);
		return nullptr;
	}

	void gen_module(std::string name, AST* ast) {
		module = new ModuleData();
		module->llvmMod = new llvm::Module(name, context);

		Function mainFunc("mainCRTStartup", std::vector<Parameter>(), new Program(ast));
		gen_func(&mainFunc);

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
		linkCmd << "msvcrt.lib /subsystem:console /out:a.exe";
		//std::cout << linkCmd.str() << std::endl;
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