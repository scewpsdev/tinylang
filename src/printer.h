#pragma once

#include "ast.h"

class ASTPrinter {
public:
	ASTPrinter(AST* input);

	void print(std::ostream& out);
};