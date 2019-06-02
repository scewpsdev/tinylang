#pragma once

#include "ast.h"

class Codegen {
public:
	Codegen(std::map<std::string, AST*> modules);

	void run();
};