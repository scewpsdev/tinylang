#include "stdafx.h"

#include "fileio.h"
#include "lexer.h"
#include "parser.h"
#include "printer.h"
#include "codegen.h"

void test_lexer(Lexer* lexer) {
	while (!lexer->eof()) {
		Token token = lexer->next();
		std::cout << token.type << ": " << token.value << std::endl;
	}
	std::cout << std::endl;
}

void test_parser(Parser* parser) {
	AST* ast = parser->parse_toplevel();
	ASTPrinter printer(ast);
	printer.print(std::cout);
	std::cout << std::endl;
}

int main(int argc, char* argv[]) {
	std::map<std::string, AST*> modules;
	for (int i = 1; i < argc; i++) {
		std::string modulePath = argv[i];
		std::string moduleName = modulePath.substr(modulePath.find_last_of('/') + 1, modulePath.find_last_of('.') - modulePath.find_last_of('/') - 1);
		std::string src = read_file(modulePath);
		InputStream* in = new InputStream(src);
		Lexer lexer(in);
		test_lexer(&lexer);
		in->reset();
		Parser parser(&lexer);
		test_parser(&parser);
		in->reset();
		modules.insert(std::make_pair(moduleName, parser.parse_toplevel()));
		delete in;
	}
	Codegen gen(modules);
	gen.run();

	system("pause");
}