#pragma once

#include "pch.h"

class InputStream {
	int pos = 0, line = 1, col = 0;
	const std::string& input;
public:
	InputStream(const std::string& input);

	char next();
	char peek(int off = 0);
	bool eof();
	void error(const std::string& msg);
};