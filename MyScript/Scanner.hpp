#pragma once
#include "stdafx.h"

#include "Language.hpp"

namespace MyScript
{
	/*
	Just a scanner, nothing special
	*/
	enum class TokenType
	{
		Unknown = 0,
		Whitespace,
		Identifier,
		Keyword,
		Integer,
		Decimal,
		String,
		Boolean,
		Operator,
		Comment,
	};
	struct Token
	{
		TokenType		type;
		llvm::StringRef	text;
		int				index;
		int				length;
	};

	enum class ScannerState
	{
		None,
	};

	class  Scanner
	{
		llvm::StringRef m_source;
		unsigned int m_index = 0;
		ScannerState m_state = ScannerState::None;

	public:
		Scanner()
		{

		}

		llvm::StringRef GetSource()
		{
			return m_source;
		}
		void SetSource(const char* source, int length)
		{
			m_source = llvm::StringRef(source, length);
		}

		void SetIndex(int index)
		{
			m_index = index;
		}
		int GetIndex()
		{
			return m_index;
		}

		ScannerState GetState()
		{
			return m_state;
		}
		void SetState(ScannerState state)
		{
			m_state = state;
		}

		bool IsHex(const char c)
		{
			return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}
		//	Return true if token is valid. False if EOF, token is not valid.
		bool GetNextToken(Token* pToken)
		{
			//	EOF
			if (m_index >= m_source.size())
				return false;

			unsigned int i = m_index;

			if (isspace(m_source[i]))
			{
				//	Whitespace token
				++i;
				while (i < m_source.size() && isspace(m_source[i]))
					++i;

				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::Whitespace;
				pToken->text = m_source.substr(m_index, pToken->length);

				m_index = i;
				return true;
			}
			else if (isalpha(m_source[i]) || m_source[i] == '_')
			{
				//	Identifier or keyword
				++i;
				while (i < m_source.size() && (isalnum(m_source[i]) || m_source[i] == '_'))
					++i;

				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::Identifier;
				pToken->text = m_source.substr(m_index, pToken->length);

				//	Check if boolean
				if ((pToken->text == "true") || (pToken->text == "false"))
				{
					pToken->type = TokenType::Boolean;
				}
				else
				{
					//	Check if keyword
					if (std::find(Language::keywords.begin(), Language::keywords.end(), pToken->text) != Language::keywords.end())
						pToken->type = TokenType::Keyword;

				}

				m_index = i;
				return true;
			}
			else if (isdigit(m_source[i]))
			{
				//	Hexa prefix
				if(i + 1 < m_source.size() && m_source[i] == '0' && m_source[i + 1] == 'x')
				{
					i += 2;
					while (i < m_source.size() && IsHex(m_source[i]))
						++i;
				}
				else
				{
					//	base 10
					++i;
					while (i < m_source.size() && isdigit(m_source[i]))
						++i;

					//	Check decimal
					if (i < m_source.size() && m_source[i] == '.')
					{
						++i;
						while (i < m_source.size() && isdigit(m_source[i]))
							++i;

						pToken->index = m_index;
						pToken->length = i - m_index;
						pToken->type = TokenType::Decimal;
						pToken->text = m_source.substr(m_index, pToken->length);

						m_index = i;
						return true;
					}
				}
				

				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::Integer;
				pToken->text = m_source.substr(m_index, pToken->length);

				m_index = i;
				return true;
			}
			else if (m_source[i] == '"')
			{
				//	string
				++i;

				bool escaped = false;
				while (i < m_source.size())
				{
					escaped = (m_source[i] == '\\');

					if (m_source[i] == '"' && !escaped)
					{
						++i;
						break;
					}
					++i;
				}
				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::String;
				pToken->text = m_source.substr(m_index, pToken->length);

				m_index = i;
				return true;
			}
			else if (m_source.substr(i, 2) == "//")
			{
				//	comment
				i += 2;

				while (i < m_source.size())
				{
					if (m_source[i] == '\n')
					{
						++i;
						break;
					}
					++i;
				}
				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::Comment;
				pToken->text = m_source.substr(m_index, pToken->length);

				m_index = i;
				return true;
			}
			else
			{
				//	Check operators
				for (auto op : Language::operators)
				{
					llvm::StringRef& s = op.text;
					if (m_source.substr(i, s.size()) == s)
					{
						i += s.size();
						pToken->index = m_index;
						pToken->length = i - m_index;
						pToken->type = TokenType::Operator;
						pToken->text = m_source.substr(m_index, pToken->length);

						m_index = i;
						return true;
					}
				}

				++i;
				pToken->index = m_index;
				pToken->length = i - m_index;
				pToken->type = TokenType::Unknown;
				pToken->text = m_source.substr(m_index, pToken->length);

				m_index = i;
				return true;
			}
		}


	};
}