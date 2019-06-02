#include "stdafx.h"

#include "input.h"

InputStream::InputStream(const std::string& input)
	: input(input) {
}

char InputStream::next() {
	char ch = input[pos++];
	if (ch == '\n') {
		line++;
		col = 0;
	}
	else {
		col++;
	}
	return ch;
}

char InputStream::peek(int off) {
	return input[pos + off];
}

void InputStream::reset() {
	pos = 0;
}

bool InputStream::eof() {
	return pos >= input.length();
}

void InputStream::error(const std::string& msg) {
	std::cerr << "(" << line << ":" << col << ") " << msg << std::endl;
}