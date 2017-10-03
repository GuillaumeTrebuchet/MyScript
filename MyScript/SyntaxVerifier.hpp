#pragma once
#include "stdafx.h"
#include "MyScript.h"

#include "IASTNode.hpp"

#include "Utility.hpp"

/*
Not used for now
*/
namespace MyScript
{
	class SyntaxVerifier
	{
		SyntaxErrorCallback m_callback;
		bool m_hasErrors = false;
	public:
		SyntaxVerifier()
		{

		}

		void SetCallback(SyntaxErrorCallback callback)
		{
			m_callback = callback;
		}

		bool HasErrors()
		{
			return m_hasErrors;
		}
		
		//template <typename Iter>
		//void VerifyAll(Iter begin, Iter end = begin + 1)
		void VerifyAll(pool_vector<IASTNode*> tree)
		{
			/*for (auto it = begin; it != end; ++it)
			{
				if(it)
			}*/
			for (auto node : tree)
			{
				if (is_type<ASTFunctionNode>(node))
				{
					//ASTFunctionNode* funcNode = dynamic_cast<ASTFunctionNode*>(node);
					//funcNode->
				}
			}
		}
	};
}