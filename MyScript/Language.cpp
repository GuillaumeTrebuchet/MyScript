#include "stdafx.h"
#include "Language.hpp"

namespace MyScript
{
	const std::vector<Language::TypeInfo> Language::builtInTypes =
	{
		{ MSType::MS_TYPE_INTEGER, "int" },
		{ MSType::MS_TYPE_FLOAT , "float" },
		{ MSType::MS_TYPE_BOOLEAN, "bool" },
		{ MSType::MS_TYPE_STRING, "string" },
	};

	const std::vector<llvm::StringRef> Language::keywords =
	{
		{ "int" },
		{ "float" },
		{ "bool" },
		{ "string" },
		{ "if" },
		{ "import" },
		{ "else" },
		{ "function" },
		{ "return" },
		{ "while" },
		{ "break" },
		{ "continue" },
		{ "true" },
		{ "false" },
		{ "end" },
	};

	const std::vector<Language::OperatorInfo> Language::operators =
	{
		{ MSOperator::MS_OPERATOR_ADD,				"+", 40 },
		{ MSOperator::MS_OPERATOR_SUBTRACT,			"-", 40 },
		{ MSOperator::MS_OPERATOR_MODULO,			"%", 50 },
		{ MSOperator::MS_OPERATOR_MULTIPLY,			"*", 50 },
		{ MSOperator::MS_OPERATOR_DIVIDE,			"/", 50 },
		{ MSOperator::MS_OPERATOR_AND,				"and", 10 },
		{ MSOperator::MS_OPERATOR_OR,				"or", 10 },
		{ MSOperator::MS_OPERATOR_EQUALITY,			"==", 20 },
		{ MSOperator::MS_OPERATOR_INEQUALITY,		"!=", 20 },
		{ MSOperator::MS_OPERATOR_GREATEREQUAL,		">=", 30 },
		{ MSOperator::MS_OPERATOR_LESSEREQUAL,		"<=", 30 },
		{ MSOperator::MS_OPERATOR_GREATER,			">", 30 },
		{ MSOperator::MS_OPERATOR_LESSER,			"<", 30 },
	};
}