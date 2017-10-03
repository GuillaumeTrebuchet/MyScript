#pragma once
#include "stdafx.h"

#include "MemoryPool.hpp"
#include "Parser.hpp"
#include "MSIRCompiler.hpp"

#include "IMSBase.h"

/*
Represents a script. It actually only holds symbols and llvm module instance.
*/
namespace MyScript
{
	class MSScript
		: public IMSBase
	{
		MSScript() = delete;
		MSScript(const MSScript&) = delete;
		MSScript& operator=(const MSScript&) = delete;

		std::vector<MSSymbol>					m_importedSymbols;
		std::vector<MSSymbol>					m_exportedSymbols;

		std::string m_name;

		std::vector<std::unique_ptr<std::string>>	m_exportedNames;
		//	Not sure if i can free this after compiling by the context.
	public:
		MSScript(std::string name)
			: m_name(name)
		{

		}

		std::vector<MSSymbol>& GetImportedSymbols()
		{
			return m_importedSymbols;
		}

		std::vector<MSSymbol>& GetExportedSymbols()
		{
			return m_exportedSymbols;
		}

		const std::string& GetName() const
		{
			return m_name;
		}

		void RegisterExportedFunctions(const pool_vector<IASTNode*>& tree)
		{
			for (auto node : tree)
			{
				if (is_type<ASTFunctionNode>(node))
				{
					ASTFunctionNode* functionNode = dynamic_cast<ASTFunctionNode*>(node);
					MSSymbol s;
					s.type = MSSymbolType::MS_SYMBOL_FUNCTION;

					//	Need to allocate memory for names since AST is allocated in memory pool and will be freed
					m_exportedNames.push_back(std::make_unique<std::string>(functionNode->m_name.str()));
					s.name = m_exportedNames.back()->c_str();

					s.address = NULL;
					s.functionData.callingConvention = MSCallingConvention::MS_CC_CDECL;
					s.functionData.resultType = functionNode->m_retType;
					s.functionData.count = functionNode->m_arguments.size();
					for (int i = 0; i < s.functionData.count; ++i)
						s.functionData.parameterTypes[i] = functionNode->m_arguments[i].first;

					m_exportedSymbols.push_back(s);
				}
			}
		}
	};
}