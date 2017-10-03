#pragma once
#include "stdafx.h"

#include "Scanner.hpp"

#include "MemoryPool.hpp"

#include "IASTNode.hpp"

namespace MyScript
{
	enum class ParseResult
	{
		Success,
		Error,			//	Error when parsing, error is raised by the callee
		ErrorAtStart,	//	Error on the first token, error must be raised by the caller
	};
	class Parser
		: mystd::NonCopyable
	{
		Scanner* m_pScanner = nullptr;

		Token m_currentToken;

		bool m_eof = false;

		MemoryPool*					m_pMemoryPool = nullptr;
		MSSyntaxErrorCallback		m_errorCallback = nullptr;

		int m_line = 0;
		int m_column = 0;

		void AcceptToken()
		{
			//	Update line/column for error reporting
			if (!m_eof)
			{
				if (m_currentToken.type == TokenType::Whitespace)
				{
					int i = 0;

					while ((i = m_currentToken.text.find_first_of('\n', i + 1)) != llvm::StringRef::npos)
					{
						++m_line;
						m_column = m_currentToken.length - (i + 1);
					}
				}
				else
				{
					m_column += m_currentToken.length;
				}
			}

			m_eof = !m_pScanner->GetNextToken(&m_currentToken);
			
		}

		void Error(const char* msg)
		{
			//	Should get line/col
			m_errorCallback("", m_line, m_column, msg);
		}

		Language::OperatorInfo GetOperatorInfo(llvm::StringRef s)
		{
			for (int i = 0; i < Language::operators.size(); ++i)
			{
				if (Language::operators[i].text == s)
					return Language::operators[i];
			}
			throw std::exception();
		}


		//	Required cause some keyword are also operators
		bool IsOperator(const llvm::StringRef& s)
		{
			for (auto op : Language::operators)
			{
				if (op.text == s)
					return true;
			}

			return false;
		}
		float StringToFloat(const llvm::StringRef& s)
		{
			auto it = s.begin();

			double r = 0.0;
			bool neg = false;
			if (*it == '-') {
				neg = true;
				++it;
			}
			while (*it >= '0' && *it <= '9' && it != s.end())
			{
				r = (r*10.0) + (*it - '0');
				++it;
			}
			if (*it == '.')
			{
				double f = 0.0;
				int n = 0;
				++it;
				while (*it >= '0' && *it <= '9')
				{
					f = (f*10.0) + (*it - '0');
					++it;
					++n;
				}
				r += f / std::pow(10.0, n);
			}
			if (neg)
			{
				r = -r;
			}
			return r;
		}
		int StringToInt(const llvm::StringRef& s)
		{
			if(s.startswith("0x"))
			{
				int val = 0;
				for (auto it = s.begin() + 2; it != s.end(); ++it)
				{
					auto c = *it;

					if (c >= '0' && c <= '9')
						val = val * 16 + (c - '0');
					else if (c >= 'a' && c <= 'f')
						val = val * 16 + ((c - 'a') + 10);
					else if (c >= 'A' && c <= 'F')
						val = val * 16 + ((c - 'A') + 10);
					else
						throw std::exception();
				}
				return val;
			}
			else
			{
				int val = 0;
				for (auto c : s)
					val = val * 10 + (c - '0');
				return val;
			}
			
		}
		ParseResult EvaluateString(const llvm::StringRef& s, pool_vector<uint16_t>& s_utf16)
		{
			s_utf16.reserve(s.size());

			if (s.size() < 1 || s[0] != '\"')
				throw std::exception();

			if (s.size() < 2 || s[s.size() - 1] != '\"' || s[s.size() - 2] == '\\')
			{
				Error("missing closing quote");
				return ParseResult::Error;
			}

			bool escaped = false;
			for (int i = 1; i < s.size() - 1; ++i)
			{
				char c = s[i];
				if (c == '\\')
				{
					escaped = true;
				}
				else if (escaped)
				{
					if (c == 'a')
						s_utf16.push_back('\a');
					else if (c == 'b')
						s_utf16.push_back('\b');
					else if (c == 'f')
						s_utf16.push_back('\f');
					else if (c == 'n')
						s_utf16.push_back('\n');
					else if (c == 'r')
						s_utf16.push_back('\r');
					else if (c == 't')
						s_utf16.push_back('\t');
					else if (c == 'v')
						s_utf16.push_back('\v');
					else if (c == '\'')
						s_utf16.push_back('\'');
					else if (c == '"')
						s_utf16.push_back('\"');
					else if (c == '\\')
						s_utf16.push_back('\\');
					else if (c == '?')
						s_utf16.push_back('?');
					else
						s_utf16.push_back(c);
				}
				else
				{
					s_utf16.push_back(c);
				}
			}

			s_utf16.push_back(0);

			return ParseResult::Success;
		}
	public:
		Parser()
		{
		}
		void SkipWhitespaces()
		{
			while (!m_eof && (m_currentToken.type == TokenType::Whitespace ||
				m_currentToken.type == TokenType::Comment))
			{
				AcceptToken();
			}
		}

		
		ParseResult ParseTokenByText(const char* text)
		{
			SkipWhitespaces();
			if (!m_eof && m_currentToken.text == text)
			{
				AcceptToken();
				return ParseResult::Success;
			}
			else
				return ParseResult::Error;
		}
		ParseResult ParseTokenByType(TokenType type, llvm::StringRef* pText = nullptr)
		{
			SkipWhitespaces();
			if (!m_eof && m_currentToken.type == type)
			{
				if (pText != nullptr)
					*pText = m_currentToken.text;

				AcceptToken();
				return ParseResult::Success;
			}
			else
				return ParseResult::Error;
		}

		ParseResult ParseType(MSType* pType)
		{
			if (ParseTokenByText("bool") == ParseResult::Success)
			{
				*pType = MSType::MS_TYPE_BOOLEAN;
				return ParseResult::Success;
			}
			else if (ParseTokenByText("int") == ParseResult::Success)
			{
				*pType = MSType::MS_TYPE_INTEGER;
				return ParseResult::Success;
			}
			else if (ParseTokenByText("float") == ParseResult::Success)
			{
				*pType = MSType::MS_TYPE_FLOAT;
				return ParseResult::Success;
			}
			else if (ParseTokenByText("string") == ParseResult::Success)
			{
				*pType = MSType::MS_TYPE_STRING;
				return ParseResult::Success;
			}
			else if (ParseTokenByText("void") == ParseResult::Success)
			{
				*pType = MSType::MS_TYPE_VOID;
				return ParseResult::Success;
			}
			else
			{
				return ParseResult::Error;
			}
		}

		ParseResult ParseArguments(pool_vector<ASTFunctionNode::Argument>& args)
		{
			//	Parse '('
			if (ParseTokenByText("(") != ParseResult::Success)
			{
				Error("'(' expected");
				return ParseResult::Error;
			}

			//	Parse ')'
			if (ParseTokenByText(")") == ParseResult::Success)
				return ParseResult::Success;

			while (true)
			{
				//	Parse type
				MSType type;
				if (ParseType(&type) != ParseResult::Success)
				{
					Error("type expected");
					return ParseResult::Error;
				}

				//	Parse name
				llvm::StringRef name;
				if (ParseTokenByType(TokenType::Identifier, &name) != ParseResult::Success)
				{
					Error("identifier expected");
					return ParseResult::Error;
				}

				args.push_back(ASTFunctionNode::Argument(type, name));

				//	Parse ')'
				if (ParseTokenByText(")") == ParseResult::Success)
					break;

				//	Parse ')'
				if (ParseTokenByText(",") != ParseResult::Success)
				{
					Error("',' expected");
					return ParseResult::Error;
				}
			}

			return ParseResult::Success;
		}
		ParseResult ParseFunction(ASTFunctionNode** ppNode)
		{
			//	Parse 'function'
			if (ParseTokenByText("function") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTFunctionNode* pNode = m_pMemoryPool->Alloc<ASTFunctionNode>()(m_pMemoryPool);
			*ppNode = pNode;

			//	Parse name
			if (ParseTokenByType(TokenType::Identifier, &pNode->m_name) != ParseResult::Success)
			{
				Error("identifier expected");
				return ParseResult::Error;
			}

			//	Parse arguments
			if (ParseArguments(pNode->m_arguments) != ParseResult::Success)
				return ParseResult::Error;

			//	Parse ':'
			if (ParseTokenByText(":") == ParseResult::Success)
			{
				//	Parse return type
				if (ParseType(&pNode->m_retType) != ParseResult::Success)
				{
					Error("type expected");
					return ParseResult::Error;
				}
			}
			else
			{
				pNode->m_retType = MSType::MS_TYPE_VOID;
			}

			//	Parse block
			IASTNode* pStatementNode = nullptr;

			ParseResult result;
			while ((result = ParseStatement(&pStatementNode)) == ParseResult::Success)
			{
				pNode->m_statements.push_back(pStatementNode);
			}

			//	Parse 'end'
			if (ParseTokenByText("end") != ParseResult::Success)
			{
				Error("'end' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}

		//	This parse simple expressions
		ParseResult ParseSimpleExpression(IASTNode** ppNode)
		{
			ASTCallNode* pCallNode = nullptr;
			ParseResult result;

			llvm::StringRef text;
			if (ParseTokenByText("null") == ParseResult::Success)
			{
				ASTNullNode* pNode = m_pMemoryPool->Alloc<ASTNullNode>()();
				*ppNode = pNode;
			}
			else if (ParseTokenByType(TokenType::Boolean, &text) == ParseResult::Success)
			{
				ASTBooleanNode* pNode = m_pMemoryPool->Alloc<ASTBooleanNode>()();
				*ppNode = pNode;

				if (text == "true")
					pNode->m_value = true;
				else if (text == "false")
					pNode->m_value = false;
				else
					throw std::exception();
			}
			else if (ParseTokenByType(TokenType::Integer, &text) == ParseResult::Success)
			{
				ASTIntegerNode* pNode = m_pMemoryPool->Alloc<ASTIntegerNode>()();
				*ppNode = pNode;

				pNode->m_value = StringToInt(text);
			}
			else if (ParseTokenByType(TokenType::Decimal, &text) == ParseResult::Success)
			{
				ASTFloatNode* pNode = m_pMemoryPool->Alloc<ASTFloatNode>()();
				*ppNode = pNode;

				pNode->m_value = StringToFloat(text);
			}
			else if ((result = ParseFunctionCall(&pCallNode)) != ParseResult::ErrorAtStart)
			{
				*ppNode = pCallNode;
				return result;
			}
			else if (ParseTokenByType(TokenType::String, &text) == ParseResult::Success)
			{
				ASTStringNode* pNode = m_pMemoryPool->Alloc<ASTStringNode>()(m_pMemoryPool);
				*ppNode = pNode;

				return EvaluateString(text, pNode->m_value);
			}
			else if (ParseTokenByType(TokenType::Identifier, &text) == ParseResult::Success)
			{
				ASTNameNode* pNode = m_pMemoryPool->Alloc<ASTNameNode>()();
				*ppNode = pNode;

				pNode->m_name = text;
			}
			else
			{
				*ppNode = nullptr;
				return ParseResult::ErrorAtStart;
			}

			return ParseResult::Success;
		}

		//	This parse more complex expressions (binary operations) using simple expressions.
		ParseResult ParseExpression(IASTNode** ppNode, int minPrecedence = 0)
		{
			/*
			Algorithm is as follow :
				- parse simple exp
			  loop:
				- parse operator (1)
				- parse exp, and continue as long as following operators have precedence over (1)
					- parse simple exp
					...
				- back to loop


				First operator ALWAYS has precedence. (thats why default value for minPrecedence is 0)
			eg.:
				'a + b * c / d - e'
				- parse a
				- parse +
				- '+' has precedence over 0
				- parse 'b * c / d - e' :
					- parse b
					- parse *
					- '*' has precedence over '+', so go on
					- parse c
					- back to loop
					- parse '/'
					- '/' has precedence over '+'
					- parse d
					- back to loop
					- parse '-'
					- '-' doesnt have precedence over '+'
				- parse '-'
				- '-' has precedence over 0
				- parse e
				- done !
			*/


			//	Parse a simple expression
			IASTNode* pExpression1 = nullptr;
			if (ParseSimpleExpression(&pExpression1) != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			//	As long as we get an operator, we parse another simple expression.
			while (true)
			{
				SkipWhitespaces();

				//	End of file, just return expression
				if (m_eof)
				{
					*ppNode = pExpression1;
					return ParseResult::Success;
				}

				//	No operator, probably just end of expression
				if (!IsOperator(m_currentToken.text))
				{
					*ppNode = pExpression1;
					return ParseResult::Success;
				}

				Language::OperatorInfo opInfo = GetOperatorInfo(m_currentToken.text);

				//	If current operator precedence is smaller than the given minimum precedence, we return
				if (opInfo.precedence <= minPrecedence)
				{
					*ppNode = pExpression1;
					return ParseResult::Success;
				}
				else
				{
					//	Otherwise, we go on
					AcceptToken();

					ASTBinaryOperationNode* pNode = m_pMemoryPool->Alloc<ASTBinaryOperationNode>()();

					pNode->m_expression1 = pExpression1;
					pNode->m_operator = opInfo.op;

					IASTNode* pExpression2 = nullptr;
					if (ParseExpression(&pExpression2, opInfo.precedence) != ParseResult::Success)
						return ParseResult::Error;

					pNode->m_expression2 = pExpression2;
					pExpression1 = pNode;
				}
			}

			return ParseResult::Success;
		}

		ParseResult ParseFunctionCall(ASTCallNode** ppNode)
		{
			int index = m_currentToken.index;

			//	Parse name
			llvm::StringRef name;
			if (ParseTokenByType(TokenType::Identifier, &name) != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			//	Parse '('
			if (ParseTokenByText("(") != ParseResult::Success)
			{
				m_pScanner->SetIndex(index);
				AcceptToken();
				return ParseResult::ErrorAtStart;
			}

			ASTCallNode* pNode = m_pMemoryPool->Alloc<ASTCallNode>()(m_pMemoryPool);
			*ppNode = pNode;

			pNode->m_name = name;

			//	Parse ')'
			if (ParseTokenByText(")") == ParseResult::Success)
				return ParseResult::Success;

			while (true)
			{
				//	Parse expression
				IASTNode* pExpressionNode = nullptr;
				if (ParseExpression(&pExpressionNode) != ParseResult::Success)
					return ParseResult::Error;

				pNode->m_arguments.push_back(pExpressionNode);

				//	Parse ')'
				if (ParseTokenByText(")") == ParseResult::Success)
					break;

				//	Parse ')'
				if (ParseTokenByText(",") != ParseResult::Success)
				{
					Error("',' expected");
					return ParseResult::Error;
				}
			}

			return ParseResult::Success;
		}
		ParseResult ParseVarAssignment(ASTAssignmentNode** ppNode)
		{
			//	Parse type
			MSType type;
			bool hasType = (ParseType(&type) == ParseResult::Success);

			//	Parse name
			llvm::StringRef name;
			if (ParseTokenByType(TokenType::Identifier, &name) != ParseResult::Success)
			{
				if (!hasType)
				{
					return ParseResult::ErrorAtStart;
				}
				else
				{
					Error("identifier expected");
					return ParseResult::Error;
				}
			}

			ASTAssignmentNode* pNode = m_pMemoryPool->Alloc<ASTAssignmentNode>()();
			*ppNode = pNode;

			pNode->m_name = name;

			if (hasType)
				pNode->m_type = type;
			else
				pNode->m_type = MSType::MS_TYPE_VOID;

			//	Parse ';'
			if (ParseTokenByText(";") == ParseResult::Success)
			{
				pNode->m_expression = nullptr;
				if (hasType)
				{
					//	<identifier>;
					//	no type and no assignment, syntax error
					Error("'=' expected");
					return ParseResult::Error;
				}

				return ParseResult::Success;
			}

			//	Parse '='
			if (ParseTokenByText("=") != ParseResult::Success)
			{
				Error("'=' expected");
				return ParseResult::Error;
			}

			//	Parse expression
			if (ParseExpression(&pNode->m_expression) != ParseResult::Success)
				return ParseResult::Error;

			//	Parse ';'
			if (ParseTokenByText(";") != ParseResult::Success)
			{
				Error("';' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseIf(ASTIfNode** ppNode)
		{
			//	Parse 'if'
			if (ParseTokenByText("if") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTIfNode* pNode = m_pMemoryPool->Alloc<ASTIfNode>()(m_pMemoryPool);
			*ppNode = pNode;

			//	Parse '('
			if (ParseTokenByText("(") != ParseResult::Success)
			{
				Error("'(' expected");
				return ParseResult::Error;
			}

			//	Parse expression
			if (ParseExpression(&pNode->m_expression) != ParseResult::Success)
				return ParseResult::Error;

			//	Parse '('
			if (ParseTokenByText(")") != ParseResult::Success)
			{
				Error("')' expected");
				return ParseResult::Error;
			}

			//	Parse 'then'
			if (ParseTokenByText("then") != ParseResult::Success)
			{
				Error("'then' expected");
				return ParseResult::Error;
			}

			//	Parse block
			IASTNode* pStatementNode = nullptr;

			ParseResult result;
			while ((result = ParseStatement(&pStatementNode)) == ParseResult::Success)
			{
				pNode->m_statements.push_back(pStatementNode);
			}

			//	Parse 'then'
			if (ParseTokenByText("else") == ParseResult::Success)
			{
				pStatementNode = nullptr;
				while (ParseStatement(&pStatementNode) == ParseResult::Success)
				{
					pNode->m_elseStatements.push_back(pStatementNode);
				}
			}

			//	Parse 'end'
			if (ParseTokenByText("end") != ParseResult::Success)
			{
				Error("'end' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseWhile(ASTWhileNode** ppNode)
		{
			//	Parse 'while'
			if (ParseTokenByText("while") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTWhileNode* pNode = m_pMemoryPool->Alloc<ASTWhileNode>()(m_pMemoryPool);
			*ppNode = pNode;

			//	Parse '('
			if (ParseTokenByText("(") != ParseResult::Success)
			{
				Error("'(' expected");
				return ParseResult::Error;
			}

			//	Parse expression
			if (ParseExpression(&pNode->m_expression) != ParseResult::Success)
				return ParseResult::Error;

			//	Parse '('
			if (ParseTokenByText(")") != ParseResult::Success)
			{
				Error("')' expected");
				return ParseResult::Error;
			}

			//	Parse 'do'
			if (ParseTokenByText("do") != ParseResult::Success)
			{
				Error("'then' expected");
				return ParseResult::Error;
			}

			//	Parse block
			IASTNode* pStatementNode = nullptr;

			ParseResult result;
			while ((result = ParseStatement(&pStatementNode)) == ParseResult::Success)
			{
				pNode->m_statements.push_back(pStatementNode);
			}

			//	Parse 'end'
			if (ParseTokenByText("end") != ParseResult::Success)
			{
				Error("'end' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseReturn(ASTReturnNode** ppNode)
		{
			//	Parse 'return'
			if (ParseTokenByText("return") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTReturnNode* pNode = m_pMemoryPool->Alloc<ASTReturnNode>()();
			*ppNode = pNode;

			//	Parse expression
			if (ParseExpression(&pNode->m_expression) != ParseResult::Success)
				return ParseResult::Error;

			//	Parse ';'
			if (ParseTokenByText(";") != ParseResult::Success)
			{
				Error("';' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseImport()
		{
			//	Parse 'import'
			if (ParseTokenByText("import") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			//	Do not construct a node, cause we dont care about imports
			llvm::StringRef text;
			if (ParseTokenByType(TokenType::String, &text) != ParseResult::Success)
			{
				Error("string expected");
				return ParseResult::Error;
			}

			//	Parse ';'
			if (ParseTokenByText(";") != ParseResult::Success)
			{
				Error("';' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseBreak(ASTBreakNode** ppNode)
		{
			//	Parse 'break'
			if (ParseTokenByText("break") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTBreakNode* pNode = m_pMemoryPool->Alloc<ASTBreakNode>()();
			*ppNode = pNode;

			//	Parse ';'
			if (ParseTokenByText(";") != ParseResult::Success)
			{
				Error("';' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseContinue(ASTContinueNode** ppNode)
		{
			//	Parse 'continue'
			if (ParseTokenByText("continue") != ParseResult::Success)
				return ParseResult::ErrorAtStart;

			ASTContinueNode* pNode = m_pMemoryPool->Alloc<ASTContinueNode>()();
			*ppNode = pNode;

			//	Parse ';'
			if (ParseTokenByText(";") != ParseResult::Success)
			{
				Error("';' expected");
				return ParseResult::Error;
			}

			return ParseResult::Success;
		}
		ParseResult ParseStatement(IASTNode** ppNode)
		{
			ASTCallNode* pCallNode = nullptr;
			ParseResult result = ParseFunctionCall(&pCallNode);
			if (result != ParseResult::ErrorAtStart)
			{
				//	Parse ';'
				if (result == ParseResult::Success && ParseTokenByText(";") != ParseResult::Success)
				{
					Error("';' expected");
					return ParseResult::Error;
				}
				*ppNode = pCallNode;
				return result;
			}

			ASTAssignmentNode* pAssignmentNode = nullptr;
			result = ParseVarAssignment(&pAssignmentNode);
			if (result != ParseResult::ErrorAtStart)
			{
				*ppNode = pAssignmentNode;
				return result;
			}

			ASTIfNode* pIfNode = nullptr;
			result = ParseIf(&pIfNode);
			if (result != ParseResult::ErrorAtStart)
			{
				*ppNode = pIfNode;
				return result;
			}

			ASTWhileNode* pWhileNode = nullptr;
			result = ParseWhile(&pWhileNode);
			if (result != ParseResult::ErrorAtStart)
			{
				*ppNode = pWhileNode;
				return result;
			}

			ASTReturnNode* pReturnNode = nullptr;
			result = ParseReturn(&pReturnNode);
			if (result != ParseResult::ErrorAtStart)
			{
				*ppNode = pReturnNode;
				return result;
			}

			ASTBreakNode* pBreakNode = nullptr;
			result = ParseBreak(&pBreakNode);
			if (result != ParseResult::ErrorAtStart)
			{
				*ppNode = pBreakNode;
				return result;
			}

			return ParseResult::ErrorAtStart;
		}

		void SetScanner(Scanner* pScanner)
		{
			m_pScanner = pScanner;
			AcceptToken();
		}
		void SetMemoryPool(MemoryPool* pMemoryPool)
		{
			m_pMemoryPool = pMemoryPool;
		}
		void SetErrorCallback(MSSyntaxErrorCallback callback)
		{
			m_errorCallback = callback;
		}

		ParseResult ParseAll(pool_vector<IASTNode*>& tree)
		{
			//	Parse block
			while (true)
			{
				IASTNode* pStatementNode = nullptr;
				ParseResult result = ParseStatement(&pStatementNode);
				if (result == ParseResult::Success)
				{
					tree.push_back(pStatementNode);
					continue;
				}
				else if (result == ParseResult::Error)
				{
					return result;
				}

				ASTFunctionNode* pFunctionNode = nullptr;
				result = ParseFunction(&pFunctionNode);
				if (result == ParseResult::Success)
				{
					tree.push_back(pFunctionNode);
					continue;
				}
				else if (result == ParseResult::Error)
				{
					return result;
				}

				result = ParseImport();
				if (result == ParseResult::Success)
				{
					continue;
				}
				else if (result == ParseResult::Error)
				{
					return result;
				}

				if (m_eof && result == ParseResult::ErrorAtStart)
					return ParseResult::Success;


				Error("statement expected");
				return ParseResult::Error;
			}

		}
	};

}