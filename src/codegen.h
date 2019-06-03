#pragma once

#include "input.h"
#include "ast.h"

class Codegen {
public:
	Codegen(std::map<std::string, AST*> modules);

	void run();
};