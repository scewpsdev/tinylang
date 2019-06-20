#include "stdafx.h"

#include "codegen.h"

#define max(l, r) (l > r ? l : r)

namespace codegen {
	struct ModuleData {
		//std::map<std::string, CodeBlock> blocks;
		std::map<std::string, llvm::Value*> globals;
		llvm::Module* llvmMod;
		llvm::Function* llvmFunc;
	};
	struct CodeBlock {
		CodeBlock* parent = nullptr;
		std::map<std::string, llvm::AllocaInst*> locals;
		llvm::BasicBlock* loopbegin = nullptr;
		llvm::BasicBlock* loopend = nullptr;
		CodeBlock(CodeBlock* parent) : parent(parent) {}
	};

	llvm::LLVMContext context;
	llvm::IRBuilder<> builder(context);

	std::map<std::string, AST*> moduleList;
	ModuleData* module;
	CodeBlock* block;

	llvm::Value* gen_expr(Expression* expr);

	llvm::Value* rval(llvm::Value* val, bool lvalue) {
		return lvalue ? builder.CreateLoad(val) : val;
	}

	llvm::Type* llvm_type(std::string name) {
		// TODO
		if (name.length() >= 2 && name[0] == 'i') {
			int bitsize = std::stoi(name.substr(1));
			return builder.getIntNTy(bitsize);
		}
		return nullptr;
	}

	llvm::Value* cast(llvm::Value* val, llvm::Type* type) {
		if (val->getType() == type) return val;
		if (val->getType()->isIntegerTy() && type->isIntegerTy()) return builder.CreateSExtOrTrunc(val, type);
		// TODO ERROR
		return nullptr;
	}

	void binary_type(llvm::Value*& left, llvm::Value*& right) {
		if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
			llvm::Type* type = builder.getIntNTy(max(left->getType()->getIntegerBitWidth(), right->getType()->getIntegerBitWidth()));
			left = cast(left, type);
			right = cast(right, type);
		}
		// TODO ERROR
	}

	llvm::Value* find_var(std::string name, CodeBlock* block) {
		if (!block) return nullptr;
		if (block->locals.count(name)) return block->locals[name];
		if (llvm::Value * val = find_var(name, block->parent)) return val;
		if (module->globals.count(name)) return module->globals[name];
		return nullptr;
	}

	llvm::BasicBlock* find_loopbegin(CodeBlock* block) {
		if (!block) return nullptr;
		if (block->loopbegin) return block->loopbegin;
		if (llvm::BasicBlock * b = find_loopbegin(block->parent)) return b;
		return nullptr;
	}

	llvm::BasicBlock* find_loopend(CodeBlock* block) {
		if (!block) return nullptr;
		if (block->loopend) return block->loopend;
		if (llvm::BasicBlock * b = find_loopend(block->parent)) return b;
		return nullptr;
	}

	llvm::AllocaInst* alloc_var(const std::string& name, llvm::Type* type) {
		llvm::BasicBlock* entry = &module->llvmFunc->getEntryBlock();
		llvm::IRBuilder<> builder(entry, entry->begin());
		llvm::AllocaInst* alloc = builder.CreateAlloca(type, nullptr, name);
		block->locals.insert(std::make_pair(name, alloc));
		return alloc;
	}

	std::string mangle(const std::string& name, std::vector<llvm::Type*> args) {
		std::stringstream result;
		result << "__" << name;
		for (int i = 0; i < args.size(); i++) {
			result << "_" << (int)args[i]->getTypeID();
		}
		return result.str();
	}

	std::string mangle(const std::string& name, std::vector<llvm::Value*> args) {
		std::vector<llvm::Type*> argtypes(args.size());
		for (int i = 0; i < args.size(); i++) {
			argtypes[i] = args[i]->getType();
		}
		return mangle(name, argtypes);
	}

	llvm::Value* call_func(const std::string& name, std::vector<llvm::Value*> args, CodeBlock* block) {
		llvm::Value* callee = nullptr;
		if (callee = find_var(name, block));
		else callee = find_var(mangle(name, args), block);
		if (!callee);// TODO ERROR
		if (callee->getType()->isFunctionTy()) {
			llvm::Function* func = static_cast<llvm::Function*>(callee);
			return builder.CreateCall(func, args);
		}
		// TODO ERROR
		return nullptr;
	}

	void gen_core_defs() {
	}

	llvm::Value* gen_num(Number* num) {
		return builder.getInt32(num->value);
	}

	llvm::Value* gen_char(Character* ch) {
		return builder.getInt8(ch->value);
	}

	llvm::Value* gen_str(String* str) {
		return nullptr;
	}

	llvm::Value* gen_bool(Boolean* boolexpr) {
		return builder.getInt1(boolexpr->value);
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
				return cast(rval(gen_expr(call->args[0]), call->lvalue()), type);
			}
		}
		// Function call
		llvm::Value* callee = gen_expr(call->func);
		std::vector<llvm::Value*> args;
		for (int i = 0; i < call->args.size(); i++) {
			llvm::Argument* arg = ((llvm::Function*)callee)->arg_begin() + i * sizeof(llvm::Argument*);
			args.push_back(cast(rval(gen_expr(call->args[i]), call->args[i]->lvalue()), arg->getType()));
		}
		return builder.CreateCall(callee, args);
	}

	llvm::Value* gen_if(If* ifexpr) {
		llvm::Value* cond = cast(rval(gen_expr(ifexpr->cond), ifexpr->lvalue()), llvm::Type::getInt1Ty(context));
		llvm::BasicBlock* thenb = llvm::BasicBlock::Create(context, "then", module->llvmFunc);
		llvm::BasicBlock* elseb = llvm::BasicBlock::Create(context, "else");
		llvm::BasicBlock* mergeb = llvm::BasicBlock::Create(context, "merge");
		builder.CreateCondBr(cond, thenb, elseb);

		builder.SetInsertPoint(thenb);
		llvm::Value* thenres = gen_expr(ifexpr->then);
		builder.CreateBr(mergeb);
		thenb = builder.GetInsertBlock();

		module->llvmFunc->getBasicBlockList().push_back(elseb);
		builder.SetInsertPoint(elseb);
		llvm::Value* elseres = ifexpr->els ? gen_expr(ifexpr->els) : nullptr;
		builder.CreateBr(mergeb);
		elseb = builder.GetInsertBlock();

		module->llvmFunc->getBasicBlockList().push_back(mergeb);
		builder.SetInsertPoint(mergeb);

		if (elseres) {
			if (thenres->getType() != elseres->getType()) {
				elseres = cast(elseres, thenres->getType());
			}
			llvm::PHINode* phi = builder.CreatePHI(thenres->getType(), 2);
			phi->addIncoming(thenres, thenb);
			phi->addIncoming(elseres, elseb);
			return phi;
		}
		return thenres;
	}

	llvm::Value* gen_loop(Loop* loop) {
		llvm::BasicBlock* headb = llvm::BasicBlock::Create(context, "head", module->llvmFunc);
		llvm::BasicBlock* loopb = llvm::BasicBlock::Create(context, "loop");
		llvm::BasicBlock* mergeb = llvm::BasicBlock::Create(context, "merge");
		block->loopbegin = headb;
		block->loopend = mergeb;

		builder.CreateBr(headb);

		builder.SetInsertPoint(headb);
		llvm::Value* cond = cast(rval(gen_expr(loop->cond), loop->lvalue()), llvm::Type::getInt1Ty(context));
		builder.CreateCondBr(cond, loopb, mergeb);

		module->llvmFunc->getBasicBlockList().push_back(loopb);
		builder.SetInsertPoint(loopb);
		gen_expr(loop->body);
		builder.CreateBr(headb);

		module->llvmFunc->getBasicBlockList().push_back(mergeb);
		builder.SetInsertPoint(mergeb);

		block->loopbegin = nullptr;
		block->loopend = nullptr;

		return builder.getInt32(0);
	}

	llvm::Value* gen_binary(const std::string& op, llvm::Value* left, llvm::Value* right) {
		if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
			llvm::Type* type = builder.getIntNTy(max(left->getType()->getIntegerBitWidth(), right->getType()->getIntegerBitWidth()));
			left = cast(left, type);
			right = cast(right, type);
			if (op == "+") return builder.CreateAdd(left, right);
			if (op == "-") return builder.CreateSub(left, right);
			if (op == "*") return builder.CreateMul(left, right);
			if (op == "/") return builder.CreateSDiv(left, right);
			if (op == "%") return builder.CreateSRem(left, right);
			if (op == "==") return builder.CreateICmpEQ(left, right);
			if (op == "!=") return builder.CreateICmpNE(left, right);
			if (op == "<") return builder.CreateICmpSLT(left, right);
			if (op == ">") return builder.CreateICmpSGT(left, right);
			if (op == "<=") return builder.CreateICmpEQ(left, right);
			if (op == ">=") return builder.CreateICmpSGE(left, right);
			if (op == "&&") return builder.CreateAnd(left, right);
			if (op == "||") return builder.CreateOr(left, right);
		}
		return call_func(op, { left, right }, block);
	}

	llvm::Value* gen_assign(Assign* assign) {
		llvm::Value* left = gen_expr(assign->left);
		llvm::Value* right = gen_expr(assign->right);
		if (assign->op == "=" && !left) {
			if (assign->left->lvalue()) {
				left = alloc_var(((Identifier*)assign->left)->value, assign->right->lvalue() ? right->getType()->getPointerElementType() : right->getType());
			}
			else {
				// TODO ERROR
				return nullptr;
			}
		}
		if (assign->op == "=") right = rval(right, assign->right->lvalue());
		if (assign->op == "+=") right = gen_binary("+", builder.CreateLoad(left), rval(right, assign->right->lvalue()));
		if (assign->op == "-=") right = gen_binary("-", builder.CreateLoad(left), rval(right, assign->right->lvalue()));
		if (assign->op == "*=") right = gen_binary("*", builder.CreateLoad(left), rval(right, assign->right->lvalue()));
		if (assign->op == "/=") right = gen_binary("/", builder.CreateLoad(left), rval(right, assign->right->lvalue()));
		if (assign->op == "%=") right = gen_binary("%", builder.CreateLoad(left), rval(right, assign->right->lvalue()));

		builder.CreateStore(cast(right, left->getType()->getPointerElementType()), left, false);
		return right;
	}

	llvm::Value* gen_binary(Binary* binary) {
		llvm::Value* left = rval(gen_expr(binary->left), binary->left->lvalue());
		llvm::Value* right = rval(gen_expr(binary->right), binary->right->lvalue());
		return gen_binary(binary->op, left, right);
	}

	llvm::Value* gen_unary(Unary* unary) {
		llvm::Value* expr = gen_expr(unary->expr);
		llvm::Value* val = builder.CreateLoad(expr);
		llvm::Value* result = nullptr;

		if (unary->op == "++") {
			result = gen_binary("+", val, builder.getInt32(1));
			builder.CreateStore(cast(result, expr->getType()->getPointerElementType()), expr, false);
		}
		if (unary->op == "--") {
			result = gen_binary("-", val, builder.getInt32(1));
			builder.CreateStore(cast(result, expr->getType()->getPointerElementType()), expr, false);
		}
		if (unary->op == "!") result = builder.CreateNeg(val);
		if (unary->op == "&") result = expr;

		return unary->position ? val : result;
	}

	llvm::Value* gen_break() {
		if (llvm::BasicBlock * end = find_loopend(block)) {
			builder.CreateBr(end);
			llvm::BasicBlock* afterbreakb = llvm::BasicBlock::Create(context, "afterbreak", module->llvmFunc);
			builder.SetInsertPoint(afterbreakb);
			return nullptr;
		}
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* gen_continue() {
		if (llvm::BasicBlock * begin = find_loopbegin(block)) {
			builder.CreateBr(begin);
			llvm::BasicBlock* afterbreakb = llvm::BasicBlock::Create(context, "afterbreak", module->llvmFunc);
			builder.SetInsertPoint(afterbreakb);
			return nullptr;
		}
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* gen_ast(AST* ast) {
		CodeBlock* parent = block;
		block = new CodeBlock(parent);
		llvm::Value* retval = nullptr;
		for (int i = 0; i < ast->vars.size(); i++) {
			llvm::Value* val = gen_expr(ast->vars[i]);
			if (!retval && i == ast->vars.size() - 1) retval = val;
		}
		delete block;
		block = parent;
		return retval;
	}

	llvm::Value* gen_prog(Program* prog) {
		return gen_ast(prog->ast);
	}

	llvm::Value* gen_func(Function* func) {
		std::vector<llvm::Type*> params;
		for (int i = 0; i < func->params.size(); i++) {
			params.push_back(llvm_type(func->params[i].type));
		}
		llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt32Ty(), params, false);
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
			builder.CreateRet(builder.getInt32(0));

			/*
			for (llvm::BasicBlock& block : llvmfunc->getBasicBlockList()) {
				bool terminated = false;
				for (llvm::Instruction& inst : block.getInstList()) {
					if (inst.getType()->getTypeID() == llvm::Instruction::Br) {
						__debugbreak();
						terminated = true;
					}
					else if (terminated) {
						block.getInstList().remove(inst);
					}
				}
			}
			*/

			delete block;
			block = parent;

			module->llvmFunc = parentFunc;
			builder.SetInsertPoint(parentBlock);
		}

		return llvmfunc;
	}

	llvm::Value* gen_expr(Expression* expr) {
		if (expr->type == "prog") return gen_prog((Program*)expr);
		if (expr->type == "break") return gen_break();
		if (expr->type == "continue") return gen_continue();
		if (expr->type == "binary") return gen_binary((Binary*)expr);
		if (expr->type == "unary") return gen_unary((Unary*)expr);
		if (expr->type == "assign") return gen_assign((Assign*)expr);
		if (expr->type == "if") return gen_if((If*)expr);
		if (expr->type == "loop") return gen_loop((Loop*)expr);
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

		gen_core_defs();

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