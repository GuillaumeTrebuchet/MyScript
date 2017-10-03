#pragma once
#include "stdafx.h"

#include "MyScript.h"

/*
Contains data about the language itself. Like types, keywords, operators and such.
*/

namespace MyScript
{
	class Language
	{
	public:
		struct TypeInfo
		{
			MSType			type;
			llvm::StringRef	text;
		};
		struct OperatorInfo
		{
			MSOperator		op;
			llvm::StringRef text;
			int				precedence;
		};
		static const std::vector<TypeInfo>				builtInTypes;
		static const std::vector<llvm::StringRef>		keywords;
		static const std::vector<OperatorInfo>			operators;
	};
}