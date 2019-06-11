#include "codegen.h"
#include "pch.h"

#include "ast.h"
#include "log.h"
#include "file.h"
#include "cast.h"

namespace sl
{
	static llvm::LLVMContext context;
	llvm::IRBuilder<> builder(context);
	static llvm::Module* module;
	static llvm::Function* function;
	static sl::Module* slModule;
	static sl::CodeBlock* slBlock;

	void error_callback(void* opaque, const char* msg)
	{
		std::cout << "[ERROR] " << msg << std::endl;
	}

	llvm::Function::LinkageTypes getLLVMLinkage(AccessModifier accessModif)
	{
		switch (accessModif)
		{
		case None: return llvm::Function::LinkageTypes::PrivateLinkage;
		case Public: return llvm::Function::LinkageTypes::ExternalLinkage;
		default: return llvm::Function::LinkageTypes::PrivateLinkage;
		}
	}

	llvm::AllocaInst* allocateValue(llvm::Function* func, const std::string& name, llvm::Type* type)
	{
		llvm::BasicBlock* entry = &func->getEntryBlock();
		llvm::IRBuilder<> entryBuilder(entry, entry->begin());
		return entryBuilder.CreateAlloca(type, nullptr, name);
	}

	llvm::Value* rVal(Expression* expr)
	{
		llvm::Value* val = expr->codegen();
		if (val)
			return expr->isLValue() ? builder.CreateLoad(val) : val;
		return nullptr;
	}

	llvm::Value* NullLiteral::codegen()
	{
		return nullptr;
	}

	llvm::Value* IntLiteral::codegen()
	{
		return llvm::ConstantInt::get(context, llvm::APInt(32, value, true));
	}

	llvm::Value* CharLiteral::codegen()
	{
		return llvm::ConstantInt::get(context, llvm::APInt(32, value, false));
	}

	llvm::Value* BoolLiteral::codegen()
	{
		return llvm::ConstantInt::get(context, llvm::APInt(1, value));
	}

	llvm::Value* StringLiteral::codegen()
	{
		return nullptr;
	}

	llvm::Value* CompoundExpression::codegen()
	{
		return expr->codegen();
	}

	llvm::Value* Identifier::codegen()
	{
		llvm::Value* ptr = slBlock->getNamedValue(name);
		if (!ptr)
		{
			logError(getToken(), "Unknown identifier '" + name + "'");
			return nullptr;
		}
		return ptr;
	}

	llvm::Value* ArrayInitializer::codegen()
	{
		return nullptr;
	}

	llvm::Value* Lambda::codegen()
	{
		return nullptr;
	}

	llvm::Value* VarDecl::codegen()
	{
		llvm::Type* type = this->getLLVMType();
		if (type)
		{
			llvm::AllocaInst* alloc = allocateValue(function, name, type);
			slBlock->addNamedValue(name, alloc);
			return alloc;
		}
		else
		{
			logError(getToken(), "Unknown typename '" + this->type + "'");
			return nullptr;
		}
	}

	std::string VarDecl::getLLVMType()
	{
		if (type[0] == 'i')
		{
			int n = std::stoi(type.substr(1));
			return llvm::Type::getIntNTy(context, n);
		}
		return nullptr;
	}

	llvm::Value* VarAssign::codegen()
	{
		if (variable->isLValue())
		{
			llvm::Value* alloc = variable->codegen();
			if (!alloc) return nullptr;
			llvm::Value* currentValue = builder.CreateLoad(alloc);
			llvm::Value* newValue = implCast(rVal(value), currentValue->getType());
			if (!newValue)
			{
				logError(getToken(), "Cannot assign value of non-matching type");
				return nullptr;
			}
			builder.CreateStore(newValue, alloc);
			return newValue;
		}
		return nullptr;
	}

	llvm::Value* VarIncrAssign::codegen()
	{
		return nullptr;
	}

	llvm::Value* VarDecrAssign::codegen()
	{
		return nullptr;
	}

	llvm::Value* IfStatement::codegen()
	{
		llvm::Value* val = rVal(expr);
		if (!val) return nullptr;
		llvm::Value* condition = implCast(val, llvm::Type::getInt1Ty(context));

		llvm::BasicBlock* trueBlock = llvm::BasicBlock::Create(context, "tb", function);
		llvm::BasicBlock* falseBlock = llvm::BasicBlock::Create(context, "fb");
		llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(context, "mg");

		builder.CreateCondBr(condition, trueBlock, falseBlock);

		builder.SetInsertPoint(trueBlock);
		llvm::Value* trueResult = thenB->codegen();
		if (!trueResult) trueResult = llvm::ConstantInt::get(context, llvm::APInt(32, 0));
		builder.CreateBr(mergeBlock);
		trueBlock = builder.GetInsertBlock();

		function->getBasicBlockList().push_back(falseBlock);
		builder.SetInsertPoint(falseBlock);
		llvm::Value* falseResult = elseB ? elseB->codegen() : llvm::ConstantInt::get(context, llvm::APInt(32, 0));
		if (!falseResult) falseResult = llvm::ConstantInt::get(context, llvm::APInt(32, 0));
		builder.CreateBr(mergeBlock);
		falseBlock = builder.GetInsertBlock();

		function->getBasicBlockList().push_back(mergeBlock);
		builder.SetInsertPoint(mergeBlock);

		if (!trueResult->getType()->isVoidTy())
		{
			llvm::PHINode* phi = builder.CreatePHI(trueResult->getType(), 2);
			phi->addIncoming(trueResult, trueBlock);
			phi->addIncoming(falseResult, falseBlock);

			return phi;
		}
		else
		{
			return nullptr;
		}
	}

	llvm::Value* WhileLoop::codegen()
	{
		return nullptr;
	}

	llvm::Value* Return::codegen()
	{
		return slBlock->returnVal = rVal(value);
	}

	llvm::Value* UnaryOperation::codegen()
	{
		switch (op)
		{
		case OperatorType::LogicalNot:
		{
			llvm::Value* val = rVal(expr);
			if (val)
			{
				llvm::Value* condition = implCast(val, llvm::Type::getInt1Ty(context));
				return builder.CreateICmpEQ(condition, llvm::ConstantInt::get(context, llvm::APInt(1, 0)));
			}
			return nullptr;
		}
		case OperatorType::FunctionOp:
		{
			if (Identifier * funcIdentifier = dynamic_cast<Identifier*>(expr))
			{
				std::string funcName = funcIdentifier->name;
				llvm::Function* func = module->getFunction(funcName);
				if (func)
				{
					std::vector<llvm::Value*> argList;
					for (int i = 0; i < (int)args.size(); i++)
					{
						llvm::Argument* arg = func->arg_begin() + i;
						llvm::Value* argVal = implCast(rVal(args[i]), arg->getType());
						if (argVal) argList.push_back(argVal);
						else return nullptr;
					}
					return builder.CreateCall(func, argList);
				}
				else
				{
					logError(getToken(), "Unknown function '" + funcIdentifier->name + "'");
					return nullptr;
				}
			}
			break;
		}
		}
		return nullptr;
	}

	llvm::Value* BinaryOperation::codegen()
	{
		llvm::Value* l = rVal(left);
		llvm::Value* r = rVal(right);

		switch (op)
		{
		case OperatorType::Add: return builder.CreateAdd(l, r);
		case OperatorType::Subtract: return builder.CreateSub(l, r);
		case OperatorType::Multiply: return builder.CreateMul(l, r);
		case OperatorType::Divide: return builder.CreateUDiv(l, r);
		case OperatorType::Modulo: return builder.CreateURem(l, r);
		case OperatorType::LessThan: return builder.CreateICmpULT(l, r);
		case OperatorType::LessThanEqual: return builder.CreateICmpULE(l, r);
		case OperatorType::GreaterThan: return builder.CreateICmpUGT(l, r);
		case OperatorType::GreaterThanEqual: return builder.CreateICmpUGE(l, r);
		default: return nullptr;
		}
	}

	llvm::Value* CompoundStatement::codegen()
	{
		return block->codegen();
	}

	llvm::Value* ExprStatement::codegen()
	{
		return expression->codegen();
	}

	void MetaDirective::codegen()
	{
	}

	llvm::Value* CodeBlock::codegen()
	{
		CodeBlock* lastBlock = slBlock;
		slBlock = this;
		llvm::Value* result = nullptr;

		for (int i = 0; i < (int)statements.size(); i++)
		{
			statements[i]->codegen();

			// Check for return
			if (slBlock->returnVal)
			{
				result = slBlock->returnVal;
				break;
			}
		}
		slBlock = lastBlock;

		return result;
	}

	llvm::Function* FunctionHead::codegen()
	{
		std::vector<llvm::Type*> argTypes;
		if (args)
		{
			for (int i = 0; i < (int)args->args.size(); i++)
			{
				argTypes.push_back(args->args[i]->getLLVMType());
			}
		}

		llvm::FunctionType* funcType = llvm::FunctionType::get(retType ? retType->getLLVMType() : llvm::Type::getVoidTy(context), argTypes, false);
		llvm::Function* func = llvm::Function::Create(funcType, getLLVMLinkage(access), name, module);

		int i = 0;
		for (llvm::Argument& arg : func->args())
		{
			arg.setName(args->args[i++]->getName());
		}

		return func;
	}

	llvm::Function* Function::codegen()
	{
		llvm::Function* func = module->getFunction(head->getName());
		if (!func) func = head->codegen();

		if (body)
		{
			sl::function = func;

			for (llvm::Argument& arg : function->args())
				body->addNamedValue(arg.getName(), &arg);

			llvm::BasicBlock * entry = llvm::BasicBlock::Create(context, "entry", func);
			builder.SetInsertPoint(entry);
			llvm::Value * retValue = body->codegen();

			if (retValue) builder.CreateRet(retValue);
			else builder.CreateRetVoid();

			sl::function = nullptr;
		}

		return func;
	}

	llvm::Function* Function::declgen()
	{
		llvm::Function* func = head->codegen();
		return func;
	}

	void Module::codegen(std::map<std::string, Module*> modules)
	{
		llvm::Module module(name, context);
		sl::module = &module;
		slModule = this;

		// Meta directives
		for (int i = 0; i < (int)directives.size(); i++)
		{
			directives[i]->codegen();
		}

		// Import declarations
		for (int i = 0; i < (int)imports.size(); i++)
		{
			std::string importName = imports[i]->getName();
			if (modules.count(importName)) modules[imports[i]->getName()]->declgen();
			else
			{
				logError(imports[i]->getToken(), "Unkown module '" + imports[i]->getName() + "'");
			}
		}

		// Module declarations
		this->declgen();

		// Module functions
		for (int i = 0; i < (int)functions.size(); i++)
		{
			llvm::Function* func = functions[i]->codegen();
			llvm::verifyFunction(*func);
		}

		generateBinary(name, module);

		sl::module = nullptr;
		slModule = nullptr;
	}

	void Module::declgen()
	{
		for (int i = 0; i < (int)functions.size(); i++)
		{
			llvm::Function* func = functions[i]->declgen();
			llvm::verifyFunction(*func);
		}
	}

	void generateBinary(const std::string & name, llvm::Module & module)
	{
		module.print(llvm::errs(), nullptr);

		// Create binary
		module.setTargetTriple(llvm::sys::getDefaultTargetTriple());
		std::string error;
		const llvm::Target* target = llvm::TargetRegistry::lookupTarget(module.getTargetTriple(), error);
		if (!target)
		{
			llvm::errs() << error;
			return;
		}

		std::string cpu = "generic";
		std::string features = "";
		llvm::TargetOptions options;
		llvm::Optional<llvm::Reloc::Model> relocModel;
		llvm::TargetMachine* machine = target->createTargetMachine(module.getTargetTriple(), cpu, features, options, relocModel);
		module.setDataLayout(machine->createDataLayout());

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

		passManager.run(module);
		out.flush();
	}

	void initCodegen()
	{
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();
	}

	void runBinary(std::map<std::string, sl::Module*> modules)
	{
		std::stringstream objFiles;
		for (std::pair<std::string, sl::Module*> pair : modules)
			objFiles << pair.first << ".o ";
		std::string cmd = "gcc stdlib.c " + objFiles.str() + "-o a.exe";
		system(cmd.c_str());
		std::cout << "/////////////" << std::endl;
		system("a.exe");
	}

	void terminateCodegen()
	{
	}
}
