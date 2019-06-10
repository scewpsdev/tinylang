#pragma once

#include "input.h"

class Token {
public:
	const static Token Null;
public:
	std::string type;
	std::string value;
};

class Lexer {
public:
	Lexer(InputStream* input);

	Token peek();
	Token next();
	bool eof();
	void error(const std::string& msg);
};

namespace lexer {
	extern InputStream* input;

	bool is_whitespace(char ch);
	std::string read_while(bool(*predicate)(char));
	void skip_line_comment();
	void skip_block_comment();

	Token read_next();
}