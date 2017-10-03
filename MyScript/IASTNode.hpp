#pragma once
#include "stdafx.h"

#include "MyScript.h"

#include "MemoryPool.hpp"


namespace MyScript
{
	template <typename T>
	using pool_vector = std::vector<T, PoolAllocator<T>>;

	class IASTNode
	{
	public:
		//	for dynamic_cast
		virtual ~IASTNode() {}
	};

	class ASTFunctionNode
		: public IASTNode
	{
	public:
		typedef std::pair<MSType, llvm::StringRef> Argument;

		llvm::StringRef						m_name;
		MSType								m_retType;
		pool_vector<Argument>				m_arguments;
		pool_vector<IASTNode*>				m_statements;

		template <typename T>
		ASTFunctionNode(T& t)
			: m_arguments(t),
			m_statements(t)
		{

		}
	};

	class ASTAssignmentNode
		: public IASTNode
	{
	public:
		llvm::StringRef		m_name;
		MSType				m_type;
		IASTNode*			m_expression;

	};

	class ASTCallNode
		: public IASTNode
	{
	public:
		llvm::StringRef			m_name;
		pool_vector<IASTNode*>	m_arguments;

		template <typename T>
		ASTCallNode(T& t)
			: m_arguments(t)
		{

		}

	};

	class ASTIfNode
		: public IASTNode
	{
	public:
		IASTNode*				m_expression;
		pool_vector<IASTNode*>	m_statements;
		pool_vector<IASTNode*>	m_elseStatements;

		template <typename T>
		ASTIfNode(T& t)
			: m_statements(t),
			m_elseStatements(t)
		{

		}
	};

	class ASTWhileNode
		: public IASTNode
	{
	public:
		IASTNode*				m_expression;
		pool_vector<IASTNode*>	m_statements;

		template <typename T>
		ASTWhileNode(T& t)
			: m_statements(t)
		{

		}

	};

	class ASTBreakNode
		: public IASTNode
	{
	public:
		llvm::Value* CodeGen(llvm::LLVMContext& context)
		{
			return nullptr;
		}
	};
	class ASTContinueNode
		: public IASTNode
	{
	public:
	};

	class ASTReturnNode
		: public IASTNode
	{
	public:
		IASTNode*				m_expression;

	};

	class ASTNullNode
		: public IASTNode
	{
	public:

	};

	class ASTBooleanNode
		: public IASTNode
	{
	public:
		bool m_value;

	};

	class ASTIntegerNode
		: public IASTNode
	{
	public:
		int m_value;

	};

	class ASTFloatNode
		: public IASTNode
	{
	public:
		float m_value;

	};

	class ASTStringNode
		: public IASTNode
	{
	public:
		pool_vector<uint16_t> m_value;

		template <typename T>
		ASTStringNode(T& t)
			: m_value(t)
		{

		}

	};

	class ASTNameNode
		: public IASTNode
	{
	public:
		llvm::StringRef m_name;

	};

	class ASTBinaryOperationNode
		: public IASTNode
	{
	public:
		MSOperator m_operator;

		IASTNode* m_expression1;
		IASTNode* m_expression2;

	};
}