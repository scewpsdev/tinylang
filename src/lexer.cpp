#include "lexer.h"

const Token Token::Null = { "", "" };

namespace lexer {
	InputStream* input;
	Token current;

	bool is_whitespace(char ch) {
		return ch == ' ' || ch == '\t' || ch == '\n';
	}

	std::string read_while(bool(*predicate)(char)) {
		std::stringstream str;
		while (!input->eof() && predicate(input->peek())) {
			str << input->next();
		}
		return str.str();
	}

	void skip_line_comment() {
		read_while([]() -> bool {

		});
	}

	void skip_block_comment() {
	}

	Token read_string() {
	}

	Token read_next() {
		read_while(is_whitespace);
		if (input->eof()) return Token::Null;
		char ch = input->peek();
		char ch2 = input->peek(2);
		if (ch == '/' && ch2 == '/') {
			skip_line_comment();
			return read_next();
		}
		if (ch == '/' && ch2 == '*') {
			skip_block_comment();
			return read_next();
		}
		if (ch == '"') return read_string();
		if (is_digit(ch)) return read_number();
		if (is_ident(ch)) return read_ident();
		if (is_punc(ch)) return { "punc", std::to_string(input->next()) };

	}

	Token peek() {
		return current.type != "" ? current : (current = read_next());
	}

	Token next() {
		Token tok = current;
		current = Token::Null;
		return tok.type != "" ? tok : read_next();
	}
}