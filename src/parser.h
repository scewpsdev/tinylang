#pragma once

#include "lexer.h"
#include "ast.h"

class Parser {
public:
	Parser(Lexer* lexer);

	AST* parse_toplevel();
};
