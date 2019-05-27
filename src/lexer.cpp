#include "lexer.h"

const Token Token::Null = { "", "" };

const std::string keywords = " if else true false null ";

namespace lexer {
	InputStream* input;
	Token current;

	bool is_keyword(const std::string& str) {
		return keywords.find(" " + str + " ", 0) != std::string::npos;
	}

	bool is_whitespace(char ch) {
		return ch == ' ' || ch == '\t' || ch == '\n';
	}

	bool is_ident(char ch) {
		return std::isdigit(ch) || std::isalpha(ch);
	}

	bool is_punc(char ch) {
	}

	std::string read_while(bool(*predicate)(char)) {
		std::stringstream str;
		while (!input->eof() && predicate(input->peek())) {
			str << input->next();
		}
		return str.str();
	}

	Token read_string() {
	}

	Token read_number() {
	}

	Token read_ident() {
	}

	void skip_line_comment() {
		read_while([](char ch) -> bool { return ch != '\n'; });
		input->next();
	}

	void skip_block_comment() {
		read_while([](char ch) -> bool { return ch != '*' || input->peek(2) != '/' });
		input->next();
		input->next();
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