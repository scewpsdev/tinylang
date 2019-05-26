#include "input.h"
#include "pch.h"

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

bool InputStream::eof() {
	return pos >= input.length();
}

void InputStream::error(const std::string& msg) {
	std::cerr << msg << " (" << line << ":" << col << ")";
}