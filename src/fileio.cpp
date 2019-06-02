#include "stdafx.h"

#include "fileio.h"

std::string read_file(const std::string& path) {
	std::ifstream in(path);
	std::stringstream stream;
	stream << in.rdbuf();
	return stream.str();
}