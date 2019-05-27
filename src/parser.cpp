#include "parser.h"

#include "ast.h"

namespace parser {


	Expression parseExpression();

	template<typename T>
	std::vector<T> delimited(char start, char end, char separator, T(*parser)()) {
		std::vector<T> list;
		bool first = true;
		skip_punc(start);
		while(!input->eof())
	}

	std::string parse_varname() {

	}

	AST parse_ast() {
	}

	Closure parse_closure() {
		return { delimited('(', ')', ',', parse_varname), parse_ast() };
	}
}