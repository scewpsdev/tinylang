#pragma once

#include "pch.h"
#include "input.h"

class Token {
public:
	const static Token Null;
public:
	std::string type;
	std::string value;
};

namespace lexer {
	extern InputStream* input;

	bool is_whitespace(char ch);
	std::string read_while(bool(*predicate)(char));
	void skip_line_comment();
	void skip_block_comment();

	Token read_next();
}