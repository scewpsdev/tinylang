#include "stdafx.h"

#include "lexer.h"

const Token Token::Null = { "", "" };

const std::string keywords = " ext def if else while loop for break continue class true false null ";

namespace lexer {
	InputStream* input;
	Token current;
	int line = 1, col = 1;

	void error(const std::string& msg);

	bool is_keyword(const std::string& str) {
		return keywords.find(" " + str + " ", 0) != std::string::npos;
	}

	bool is_whitespace(char ch) {
		return ch == ' ' || ch == '\t' || ch == '\n';
	}

	bool is_number(char ch1, char ch2) {
		return std::isdigit(ch1) || ch1 == '-' && std::isdigit(ch2);
	}

	bool is_ident(char ch) {
		return std::isdigit(ch) || std::isalpha(ch) || ch == '_';
	}

	bool is_punc(char ch) {
		std::string chars = ".,;(){}[]'";
		return chars.find(ch) != std::string::npos;
	}

	bool is_op(char ch) {
		std::string chars = "+-*/%=&|<>!";
		return chars.find(ch) != std::string::npos;
	}

	std::string read_while(bool(*predicate)(char)) {
		std::stringstream str;
		while (!input->eof() && predicate(input->peek())) {
			str << input->next();
		}
		return str.str();
	}

	std::string read_escaped(char end) {
		bool esc = false;
		std::stringstream str;
		input->next();
		while (!input->eof()) {
			char ch = input->next();
			if (esc) {
				str << ch;
				esc = false;
			}
			else if (ch == '\\')
				esc = true;
			else if (ch == end)
				break;
			else str << ch;
		}
		return str.str();
	}

	Token read_string() {
		return { "str", read_escaped('"') };
	}

	Token read_number() {
		std::string numstr = read_while([](char ch) -> bool { return std::isdigit(ch) || ch == '-'; });
		return { "num", numstr };
	}

	Token read_char() {
		return { "char", read_escaped('\'') };
	}

	Token read_ident() {
		std::string ident = read_while(is_ident);
		return { is_keyword(ident) ? "kw" : "var", ident };
	}

	void skip_line_comment() {
		input->next();
		input->next();
		read_while([](char ch) -> bool { return ch != '\n'; });
		input->next();
	}

	void skip_block_comment() {
		input->next();
		input->next();
		read_while([](char ch) -> bool { return ch != '*' || input->peek(1) != '/';	});
		input->next();
		input->next();
	}

	Token read_next() {
		read_while(is_whitespace);
		if (input->eof()) return Token::Null;
		char ch = input->peek();
		char ch2 = input->peek(1);
		if (ch == '/' && ch2 == '/') {
			skip_line_comment();
			return read_next();
		}
		if (ch == '/' && ch2 == '*') {
			skip_block_comment();
			return read_next();
		}
		if (ch == '"') return read_string();
		if (ch == '\'') return read_char();
		if (is_number(ch, ch2)) return read_number();
		if (is_ident(ch)) return read_ident();
		if (is_punc(ch)) return { "punc", std::string(1, input->next()) };
		if (is_op(ch)) return { "op", read_while(is_op) };
		error("Can't handle character '" + std::string(1, ch) + "'");
		return Token::Null;
	}

	Token peek() {
		return current.type != "" ? current : (current = read_next());
	}

	Token next() {
		Token tok = current;
		current = Token::Null;
		line = input->getLine();
		col = input->getCol();
		return tok.type != "" ? tok : read_next();
	}

	bool eof() {
		return peek().type == "";
	}

	void error(const std::string& msg) {
		input->error(msg, line, col);
	}
}

Lexer::Lexer(InputStream* input) {
	lexer::input = input;
}

Token Lexer::peek() {
	return lexer::peek();
}

Token Lexer::next() {
	return lexer::next();
}

bool Lexer::eof() {
	return lexer::eof();
}

void Lexer::error(const std::string& msg) {
	lexer::error(msg);
}