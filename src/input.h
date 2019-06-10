#pragma once

class InputStream {
	int pos = 0, line = 1, col = 1;
	const std::string& input;
public:
	InputStream(const std::string& input);

	char next();
	char peek(int off = 0);
	void reset();
	bool eof();
	void error(const std::string& msg, int line, int col);

	int getLine() { return line; }
	int getCol() { return col; }
};