#include "pch.h"
#include "fileio.h"
#include "lexer.h"

int main(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		char* modulePath = argv[i];
		std::string src = read_file(modulePath);
		lexer::input = new InputStream(src);
		
	}
}