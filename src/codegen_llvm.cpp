#include "stdafx.h"

#include "codegen.h"

#define max(l, r) (l > r ? l : r)

namespace codegen {
	struct StructInfo {
		std::string name;
		llvm::StructType* type;
		std::vector<std::string> membernames;
		std::map<std::string, llvm::Function*> methods;
	};
	struct ModuleData {
		//std::map<std::string, CodeBlock> blocks;
		std::map<std::string, llvm::Constant*> globals;
		std::map<std::string, StructInfo> types;
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
	llvm::Value* object;
	llvm::Value* lastobject;

	llvm::Value* gen_expr(Expression* expr);

	std::string mangle(const std::string& name, StructInfo* parent) {
		return parent ? "__" + parent->name + "_" + name : name;
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

	llvm::Value* rval(llvm::Value* val, bool lvalue) {
		return lvalue ? builder.CreateLoad(val) : val;
	}

	llvm::Type* llvm_type(Typename type) {
		llvm::Type* t = nullptr;
		if (type.name.length() >= 2 && type.name[0] == 'i') {
			int bitsize = std::stoi(type.name.substr(1));
			t = builder.getIntNTy(bitsize);
		}
		if (module->types.count(type.name)) t = module->types[type.name].type;
		return type.pointer ? t->getPointerTo() : t;
	}

	int member_index(llvm::Type* type, const std::string& name) {
		for (std::pair<std::string, StructInfo> info : module->types) {
			if (info.second.type == type) {
				for (int i = 0; i < info.second.membernames.size(); i++) {
					if (info.second.membernames[i] == name) {
						return i;
					}
				}
				// TODO ERROR
				return -1;
			}
		}
		// TODO ERROR
		return -1;
	}

	StructInfo* find_struct_info(llvm::Type* structtype) {
		for (std::pair<std::string, StructInfo> pair : module->types) {
			if (pair.second.type == structtype) return &module->types[pair.first];
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

	llvm::AllocaInst* alloc_var(const std::string& name, llvm::Type* type, llvm::Value* arrsize, CodeBlock* block) {
		llvm::BasicBlock* entry = &module->llvmFunc->getEntryBlock();
		llvm::IRBuilder<> builder(entry, entry->begin());
		llvm::AllocaInst* alloc = builder.CreateAlloca(type, arrsize, name);
		block->locals.insert(std::make_pair(name, alloc));
		return alloc;
	}

	llvm::Value* find_var(std::string name, llvm::Value* parent, CodeBlock* block) {
		if (parent) {
			// Member variable
			int memberid = -1;
			if ((memberid = member_index(parent->getType()->getPointerElementType(), name)) != -1) {
				return builder.CreateGEP(parent, { builder.getInt32(0), builder.getInt32(memberid) });
			}
			// Member function
			StructInfo* info = find_struct_info(parent->getType()->getPointerElementType());
			for (std::pair<std::string, llvm::Function*> pair : info->methods) {
				if (pair.first == name) return pair.second;
			}
		}
		if (!block) return nullptr;
		if (block->locals.count(name)) return block->locals[name];
		if (llvm::Value * val = find_var(name, object, block->parent)) return val;
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

	llvm::Function* create_func(Function* func, StructInfo* parent) {
		std::string funcname = mangle(func->funcname, parent);

		std::vector<llvm::Type*> params;
		for (int i = 0; i < func->params.size(); i++) {
			params.push_back(llvm_type(std::get<0>(func->params[i])));
		}
		if (parent) params.insert(params.begin(), parent->type->getPointerTo());
		llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt32Ty(), params, false);
		llvm::Function* llvmfunc = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, funcname, module->llvmMod);

		if (parent) parent->methods.insert(std::make_pair(func->funcname, llvmfunc));
		else module->globals.insert(std::make_pair(funcname, llvmfunc));

		int i = 0;
		for (llvm::Argument& arg : llvmfunc->args()) {
			if (parent && i == 0) arg.setName("this");
			else arg.setName(std::get<1>(func->params[i - (parent ? 1 : 0)]));
			i++;
		}

		if (func->body) {
			llvm::BasicBlock* parentBlock = builder.GetInsertBlock();
			llvm::Function* parentFunc = module->llvmFunc;
			module->llvmFunc = llvmfunc;

			CodeBlock* parent = block;
			block = new CodeBlock(parent);

			llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", llvmfunc);
			builder.SetInsertPoint(entry);

			for (llvm::Argument& arg : llvmfunc->args()) {
				llvm::AllocaInst* alloc = alloc_var(arg.getName(), arg.getType(), nullptr, block);
				builder.CreateStore(&arg, alloc, false);
				block->locals.insert(std::make_pair((std::string)arg.getName(), alloc));
			}

			gen_expr(func->body);
			builder.CreateRet(builder.getInt32(0));

			delete block;
			block = parent;

			module->llvmFunc = parentFunc;
			builder.SetInsertPoint(parentBlock);
		}

		return llvmfunc;
	}

	/*
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
	*/

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
		return find_var(ident->value, object, block);
	}

	llvm::Value* gen_array(Array* array) {
		std::vector<llvm::Value*> elements;
		llvm::Type* type = builder.getInt32Ty();
		for (int i = 0; i < array->elements.size(); i++) {
			llvm::Value* val = rval(gen_expr(array->elements[i]), array->elements[i]->lvalue());
			if (i == 0) type = val->getType();
			else val = cast(val, type);
			elements.push_back(val);
		}
		llvm::AllocaInst* alloc = builder.CreateAlloca(type, builder.getInt32(elements.size()));
		for (int i = 0; i < elements.size(); i++) {
			llvm::Value* elementptr = builder.CreateGEP(alloc, builder.getInt32(i));
			builder.CreateStore(elements[i], elementptr, false);
		}
		return alloc;
		//return builder.CreateLoad(alloc);
	}

	llvm::Value* gen_closure(Closure* cls) {
		return nullptr;
	}

	llvm::Value* gen_call(Call* call) {
		// Constructor
		if (call->func->type == "var") {
			if (llvm::Type * type = llvm_type(Typename{ ((Identifier*)call->func)->value, false })) {
				if (call->args.size() == 0) {
					llvm::AllocaInst* alloc = alloc_var("", type, nullptr, block);
					return builder.CreateLoad(alloc);
				}
				else {
					// TODO assign struct members
					return nullptr;
				}
			}
		}
		// Function call
		llvm::Value* callee = gen_expr(call->func);
		std::vector<llvm::Value*> args;
		int i = 0;
		for (llvm::Argument& arg : ((llvm::Function*)callee)->args()) {
			if (call->func->type == "member") {
				llvm::Value* val = i == 0 ? lastobject : cast(rval(gen_expr(call->args[i - 1]), call->args[i - 1]->lvalue()), arg.getType());
				args.push_back(val);
			}
			else {
				llvm::Value* val = cast(rval(gen_expr(call->args[i]), call->args[i]->lvalue()), arg.getType());
				args.push_back(val);
			}
			i++;
		}
		/*
		for (int i = 0; i < call->args.size(); i++) {
			llvm::Argument* arg = ((llvm::Function*)callee)->arg_begin() + (i + (call->func->type == "member" ? 1 : 0)) * sizeof(llvm::Argument*);
			args.push_back(cast(rval(gen_expr(call->args[i]), call->args[i]->lvalue()), arg->getType()));
		}
		if (call->func->type == "member") {
			args.insert(args.begin(), lastobject);
		}
		*/
		return builder.CreateCall(callee, args);
	}

	llvm::Value* gen_index(Index* idx) {
		if (idx->args.size() != 1) {
			// TODO ERROR
			return nullptr;
		}
		llvm::Value* expr = rval(gen_expr(idx->expr), idx->expr->lvalue());
		llvm::Value* index = gen_expr(idx->args[0]);
		llvm::Value* alloc = builder.CreateGEP(expr, index);
		return alloc;
	}

	llvm::Value* gen_member(Member* member) {
		object = gen_expr(member->expr);
		lastobject = object;
		llvm::Value* memberptr = gen_expr(member->member);
		object = nullptr;
		return memberptr;
		/*
		if (member->member->type == "var") {
			std::string membername = ((Identifier*)member->member)->value;
			return builder.CreateGEP(expr, { builder.getInt32(0), builder.getInt32(member_index(expr->getType()->getPointerElementType(), membername)) });
		}
		*/
		/*
		else if (member->member->type == "call") {
			Call* call = (Call*)member->member;
			llvm::Type* structtype = expr->getType()->getPointerElementType();
			StructInfo* info = find_struct_info(structtype);
			llvm::Value* callee = find_method(structtype, call.)
				std::vector<llvm::Value*> args;
			for (int i = 0; i < call->args.size(); i++) {
				llvm::Argument* arg = ((llvm::Function*)callee)->arg_begin() + i * sizeof(llvm::Argument*);
				args.push_back(cast(rval(gen_expr(call->args[i]), call->args[i]->lvalue()), arg->getType()));
			}
			return builder.CreateCall(callee, args);
		}
		*/
		//else {
		// TODO ERROR
		//return nullptr;
		//}
	}

	llvm::Value* gen_cast(Cast* c) {
		return cast(rval(gen_expr(c->expr), c->expr->lvalue()), llvm_type(c->t));
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

		if (loop->init) gen_expr(loop->init);
		builder.CreateBr(headb);

		builder.SetInsertPoint(headb);
		llvm::Value* cond = cast(rval(gen_expr(loop->cond), loop->lvalue()), llvm::Type::getInt1Ty(context));
		builder.CreateCondBr(cond, loopb, mergeb);

		module->llvmFunc->getBasicBlockList().push_back(loopb);
		builder.SetInsertPoint(loopb);
		gen_expr(loop->body);
		if (loop->iterate) gen_expr(loop->iterate);
		builder.CreateBr(headb);

		module->llvmFunc->getBasicBlockList().push_back(mergeb);
		builder.SetInsertPoint(mergeb);

		block->loopbegin = nullptr;
		block->loopend = nullptr;

		return builder.getInt32(0);
	}

	llvm::Value* gen_type(Class* type) {
		if (llvm_type(Typename{ type->name, false })) {
			// TODO ERROR
			return nullptr;
		}

		StructInfo info;
		info.name = type->name;

		std::vector<llvm::Type*> elements;
		std::vector<std::string> members;
		for (int i = 0; i < type->elements.size(); i++) {
			elements.push_back(llvm_type(std::get<0>(type->elements[i])));
			members.push_back(std::get<1>(type->elements[i]));
		}
		info.membernames = members;
		info.type = llvm::StructType::create(elements, type->name, false);
		module->types.insert(std::make_pair(type->name, info));

		std::map<std::string, llvm::Function*> methods;
		for (int i = 0; i < type->methods.size(); i++) {
			Function* method = type->methods[i];
			methods.insert(std::make_pair(method->funcname, create_func(method, &info)));
		}
		module->types[type->name].methods = methods;

		return nullptr;
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
		//return call_func(op, { left, right }, block);
		// TODO ERROR
		return nullptr;
	}

	llvm::Value* gen_assign(Assign* assign) {
		llvm::Value* left = gen_expr(assign->left);
		llvm::Value* right = gen_expr(assign->right);
		if (assign->op == "=" && !left) {
			if (assign->left->lvalue()) {
				left = alloc_var(((Identifier*)assign->left)->value, assign->right->lvalue() ? right->getType()->getPointerElementType() : right->getType(), nullptr, block);
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



		if (unary->op == "++") {
			llvm::Value* val = builder.CreateLoad(expr);
			llvm::Value* result = gen_binary("+", val, builder.getInt32(1));
			builder.CreateStore(cast(result, expr->getType()->getPointerElementType()), expr, false);
			return unary->position ? val : expr;
		}
		if (unary->op == "--") {
			llvm::Value* val = builder.CreateLoad(expr);
			llvm::Value* result = gen_binary("-", val, builder.getInt32(1));
			builder.CreateStore(cast(result, expr->getType()->getPointerElementType()), expr, false);
			return unary->position ? val : result;
		}
		if (unary->op == "!") {
			llvm::Value* val = builder.CreateLoad(expr);
			return builder.CreateNeg(val);
		}
		if (unary->op == "&") return expr;
		if (unary->op == "*") return builder.CreateLoad(expr);

		// TODO ERROR
		return nullptr;
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

	llvm::Function* gen_func(Function* func) {
		return create_func(func, nullptr);
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
		if (expr->type == "class") return gen_type((Class*)expr);
		if (expr->type == "call") return gen_call((Call*)expr);
		if (expr->type == "idx") return gen_index((Index*)expr);
		if (expr->type == "member") return gen_member((Member*)expr);
		if (expr->type == "cast") return gen_cast((Cast*)expr);
		if (expr->type == "cls") return gen_closure((Closure*)expr);
		if (expr->type == "arr") return gen_array((Array*)expr);
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

		Function mainFunc("mainCRTStartup", std::vector<std::tuple<Typename, std::string>>(), new Program(ast));
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