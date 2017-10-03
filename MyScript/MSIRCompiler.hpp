#pragma once
#include "stdafx.h"

#include "IASTNode.hpp"

#include "Utility.hpp"


namespace MyScript
{
	template <typename T>
	struct MSPointerValueComparator
	{
		bool operator()(T t1, T t2) const {
			return *t1 == *t2;
		}
	};

	class MSCompileException
		: public std::exception
	{
	public:
		MSCompileException(const char* message)
			: std::exception(message)
		{
		}
	};

	enum class ScopeType
	{
		Global,
		Function,
		While,
		If,
	};
	struct ScopeInfo
	{
		ScopeInfo(llvm::BasicBlock* startBlock, llvm::BasicBlock* outBlock, ScopeType type)
			: startBlock(startBlock),
			outBlock(outBlock),
			type(type)
		{
		}
		llvm::BasicBlock*						startBlock;
		llvm::BasicBlock*						outBlock;
		ScopeType								type;
		std::map<llvm::StringRef, llvm::Value*>	localSymbols;
	};
	/*
	This translate an AST to LLVM IR
	*/
	class MSIRCompiler
		: mystd::NonCopyable
	{
		template<typename T>
		class OrderableArrayRef
			: public llvm::ArrayRef<T>
		{
		public:
			using llvm::ArrayRef<T>::ArrayRef;
			bool operator<(const OrderableArrayRef<T>& a) const
			{
				return std::lexicographical_compare(this->begin(), this->end(), a.begin(), a.end());
			}
		};

		llvm::LLVMContext* m_pContext;
		std::unique_ptr<llvm::IRBuilder<>> m_pBuilder;
		std::unique_ptr<llvm::Module> m_pModule;
		//std::unique_ptr<llvm::FunctionPassManager> m_pFPM;

		std::vector<ScopeInfo> m_scopes;

		llvm::Type* m_floatType = nullptr;
		llvm::Type* m_intType = nullptr;
		llvm::Type* m_boolType = nullptr;
		llvm::Type* m_voidType = nullptr;
		
		// { i32 size, [i16 x 0] data }
		llvm::Type* m_stringType = nullptr;
		//	i16
		llvm::Type* m_charType = nullptr;
		//	i8*
		llvm::Type* m_pointerType = nullptr;
		// { i32 refcount, i8* ptr }
		llvm::StructType* m_handleType = nullptr;

		llvm::Function* m_handleIncrementFunc = nullptr;
		llvm::Function* m_handleDecrementFunc = nullptr;
		llvm::Function* m_getstrptrFunc = nullptr;
				
		std::map<OrderableArrayRef<uint16_t>, llvm::Constant*> m_stringConstants;
		std::string m_name;

		//	Return type name
		std::string DebugTypeName(llvm::Type* type)
		{
			std::string s;
			llvm::raw_string_ostream ss(s);
			type->print(ss);
			ss.flush();
			return s;
		}

		//	Get handle ptr if string already exist(to avoid duplicate) or allocate new string
		llvm::Constant* GetConstantString(const OrderableArrayRef<uint16_t>& s)
		{
			//	Must update refcount when constant referenced
			auto it = m_stringConstants.find(s);
			if (it != m_stringConstants.end())
				return it->second;

			//	Allocate a new string
			llvm::Constant* data = AllocConstantStringData(s);

			//	Use refcount = 2 so it is never destroyed
			llvm::Constant* handle = AllocConstantHandle(llvm::ConstantInt::get(m_intType, 2), llvm::ConstantExpr::getPointerCast(data, m_pointerType));

			m_stringConstants[s] = handle;
			return handle;
		}

		llvm::Constant* AllocConstantStringData(const OrderableArrayRef<uint16_t>& s)
		{			
			/*
			Create a new type as follow, that is compatible with m_stringType since array can be indexed beyond bounds.
			{
				i32,
				[i16 x s.size()],
			}
			
			Create a global constant of this type with initialized data.
			Then return a pointer to it, and cast it to a pointer to m_stringType
			*/
			//	Type
			std::vector<llvm::Type*> elements =
			{
				m_intType,
				llvm::ArrayType::get(m_charType, s.size()),
			};

			llvm::StructType* type = llvm::StructType::get(*m_pContext, elements);

			//	Data
			std::vector<llvm::Constant*> values =
			{
				llvm::ConstantInt::get(m_intType, s.size() - 1),
				llvm::ConstantDataArray::get(*m_pContext, s),
			};

			llvm::GlobalVariable* str_data_var = new llvm::GlobalVariable(*m_pModule,
				type,
				false,
				llvm::GlobalValue::InternalLinkage,
				0);

			str_data_var->setInitializer(llvm::ConstantStruct::get(type, values));
			return llvm::ConstantExpr::getPointerCast(str_data_var, m_stringType->getPointerTo());
		}
				
		//	This is not really constant, just static memory. But can be read/written.
		//	refcount should be +1 though, so it never reaches 0, and memory is never freed.
		llvm::Constant* AllocConstantHandle(llvm::Constant* refcount, llvm::Constant* ptr)
		{
			std::vector<llvm::Constant*> values =
			{
				refcount,
				ptr,
			};
			llvm::GlobalVariable* var = new llvm::GlobalVariable(*m_pModule,
				m_handleType,
				false,
				llvm::GlobalValue::InternalLinkage,
				0);

			var->setInitializer(llvm::ConstantStruct::get(m_handleType, values));
			return var;
		}

		//	True if value is a handle pointer
		bool IsHandlePtr(llvm::Value* value)
		{
			if (value->getType() == m_handleType->getPointerTo())
				return true;
			else
				return false;
		}
		bool IsHandlePtrPtr(llvm::Value* value)
		{
			if (value->getType() == m_handleType->getPointerTo()->getPointerTo())
				return true;
			else
				return false;
		}

		//	This performs type conversion for binary operations. (eg.: 'int + float' => '(float)int + float')
		llvm::Type* ConvertValuesForOperation(llvm::Value** pLhs, llvm::Value** pRhs)
		{
			llvm::Type* lhsType = (*pLhs)->getType();
			llvm::Type* rhsType = (*pRhs)->getType();

			//	No conversion, types are equals
			if (lhsType == rhsType)
				return lhsType;

			if (lhsType == m_floatType)
			{
				if (rhsType == m_intType)
				{
					*pRhs = m_pBuilder->CreateSIToFP(*pRhs, m_floatType);
					return m_floatType;
				}
				else if (rhsType == m_boolType)
				{
					*pRhs = m_pBuilder->CreateSIToFP(*pRhs, m_floatType);
					return m_floatType;
				}
			}
			else if (lhsType == m_intType)
			{
				if (rhsType == m_floatType)
				{
					*pLhs = m_pBuilder->CreateSIToFP(*pLhs, m_floatType);
					return m_floatType;
				}
				else if (rhsType == m_boolType)
				{
					*pRhs = m_pBuilder->CreateZExt(*pRhs, m_intType);
					return m_intType;
				}
			}
			else if (lhsType == m_boolType)
			{
				if (rhsType == m_floatType)
				{
					*pLhs = m_pBuilder->CreateSIToFP(*pLhs, m_floatType);
					return m_floatType;
				}
				else if (rhsType == m_intType)
				{
					*pLhs = m_pBuilder->CreateZExt(*pLhs, m_intType);
					return m_intType;
				}
			}

			std::string str;
			llvm::raw_string_ostream rso(str);
			rso << "Cannot compute operation between types ";
			lhsType->print(rso);
			rso << " and ";
			rhsType->print(rso);
			rso.flush();
			throw MSCompileException(str.c_str());
		}

		//	This generate code for type cast(float to int, etc.)
		llvm::Value* Convert(llvm::Value* value, llvm::Type* type)
		{
			llvm::Type* valueType = value->getType();
			if (valueType == type)
				return value;

			if (type == m_floatType)
			{
				if (valueType == m_intType)
				{
					return m_pBuilder->CreateSIToFP(value, m_floatType);
				}
				else if (valueType == m_boolType)
				{
					return m_pBuilder->CreateSIToFP(value, m_floatType);
				}
				else
					throw std::exception();
			}
			else if (type == m_intType)
			{
				if (valueType == m_floatType)
				{
					return m_pBuilder->CreateFPToSI(value, m_intType);
				}
				else if (valueType == m_boolType)
				{
					return m_pBuilder->CreateZExt(value, m_intType);
				}
				else
					throw std::exception();
			}
			else if (type == m_boolType)
			{
				if (valueType == m_floatType)
				{
					return m_pBuilder->CreateFPToSI(value, m_boolType);
				}
				else if (valueType == m_intType)
				{
					return m_pBuilder->CreateZExt(value, m_boolType);
				}
				else
					throw std::exception();
			}
			else
				throw std::exception();
		}

		/*
		All these generate code for binary operations
		*/
		llvm::Value* ComputeAdd(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFAdd(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateAdd(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateAdd(lhs, rhs);
			}
			else
				throw MSCompileException("addition operands not supported");
		}
		llvm::Value* ComputeSubtract(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFSub(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateSub(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateSub(lhs, rhs);
			}
			else
				throw MSCompileException("subtraction operands not supported");
		}
		llvm::Value* ComputeMultiply(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFMul(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateMul(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateMul(lhs, rhs);
			}
			else
				throw MSCompileException("multiplication operands not supported");
		}
		llvm::Value* ComputeDivide(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFDiv(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateSDiv(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateUDiv(lhs, rhs);
			}
			else
				throw MSCompileException("division operands not supported");
		}
		llvm::Value* ComputeModulo(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFRem(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateSRem(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateURem(lhs, rhs);
			}
			else
				throw MSCompileException("modulo operands not supported");
		}
		llvm::Value* ComputeLesserOrEqual(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpOLE(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpSLE(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpULE(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeLesser(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpOLT(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpSLT(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpULT(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeInequality(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpONE(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpNE(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpNE(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeGreaterOrEqual(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpOGE(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpSGE(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpUGE(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeGreater(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpOGT(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpSGT(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpUGT(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeEquality(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			if (type == m_floatType)
			{
				return m_pBuilder->CreateFCmpOEQ(lhs, rhs);
			}
			else if (type == m_intType)
			{
				return m_pBuilder->CreateICmpEQ(lhs, rhs);
			}
			else if (type == m_boolType)
			{
				return m_pBuilder->CreateICmpEQ(lhs, rhs);
			}
			else
				throw MSCompileException("comparison operands not supported");
		}
		llvm::Value* ComputeAnd(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* lhsType = lhs->getType();
			llvm::Type* rhsType = lhs->getType();

			if (lhsType == m_floatType)
			{
				lhs = m_pBuilder->CreateFCmpONE(lhs, llvm::ConstantFP::get(*m_pContext, llvm::APFloat(0.0f)));
			}
			else if (lhsType == m_intType)
			{
				lhs = m_pBuilder->CreateICmpNE(lhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(32, 0)));
			}
			else if (lhsType == m_boolType)
			{
				lhs = m_pBuilder->CreateICmpNE(lhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(1, 0)));
			}
			else
				throw MSCompileException("comparison operands not supported");

			if (rhsType == m_floatType)
			{
				rhs = m_pBuilder->CreateFCmpONE(rhs, llvm::ConstantFP::get(*m_pContext, llvm::APFloat(0.0f)));
			}
			else if (rhsType == m_intType)
			{
				rhs = m_pBuilder->CreateICmpNE(rhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(32, 0)));
			}
			else if (rhsType == m_boolType)
			{
				rhs = m_pBuilder->CreateICmpNE(rhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(1, 0)));
			}
			else
				throw std::exception();

			return m_pBuilder->CreateAnd(lhs, rhs);
		}
		llvm::Value* ComputeOr(llvm::Value* lhs, llvm::Value* rhs)
		{
			llvm::Type* lhsType = lhs->getType();
			llvm::Type* rhsType = rhs->getType();


			if (lhsType == m_floatType)
			{
				lhs = m_pBuilder->CreateFCmpONE(lhs, llvm::ConstantFP::get(*m_pContext, llvm::APFloat(0.0f)));
			}
			else if (lhsType == m_intType)
			{
				lhs = m_pBuilder->CreateICmpNE(lhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(32, 0)));
			}
			else if (lhsType == m_boolType)
			{
				lhs = m_pBuilder->CreateICmpNE(lhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(1, 0)));
			}
			else
				throw MSCompileException("comparison operands not supported");

			if (rhsType == m_floatType)
			{
				rhs = m_pBuilder->CreateFCmpONE(rhs, llvm::ConstantFP::get(*m_pContext, llvm::APFloat(0.0f)));
			}
			else if (rhsType == m_intType)
			{
				rhs = m_pBuilder->CreateICmpNE(rhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(32, 0)));
			}
			else if (rhsType == m_boolType)
			{
				rhs = m_pBuilder->CreateICmpNE(rhs, llvm::ConstantInt::get(*m_pContext, llvm::APInt(1, 0)));
			}
			else
				throw MSCompileException("comparison operands not supported");

			return m_pBuilder->CreateOr(lhs, rhs);
		}

		/*
		Generate code for handle reference counting
		*/
		void ComputeHandleIncrement(llvm::Value* ptr)
		{

			std::vector<llvm::Value*> args =
			{
				ptr
			};
			llvm::Value* result = m_pBuilder->CreateCall(m_handleIncrementFunc, args);

			/*std::vector<llvm::Value*> idx =
			{
				llvm::ConstantInt::get(m_intType, 0),
				llvm::ConstantInt::get(m_intType, 0),
			};
			llvm::Value* pRefcount = m_pBuilder->CreateGEP(ptr, idx);
			m_pBuilder*/
		}
		void ComputeHandleDecrement(llvm::Value* ptr)
		{
			std::vector<llvm::Value*> args =
			{
				ptr
			};
			llvm::Value* result = m_pBuilder->CreateCall(m_handleDecrementFunc, args);
		}

		bool IsSymbolDefined(const llvm::StringRef& s)
		{
			for (auto itScope = m_scopes.rbegin(); itScope != m_scopes.rend(); ++itScope)
			{
				auto it = itScope->localSymbols.find(s);
				if (it != itScope->localSymbols.end())
					return true;
			}

			return false;
		}

		//	Get symbol with name. Throw Symbol not found exception on failure.
		llvm::Value* GetSymbol(const llvm::StringRef& s)
		{
			for (auto itScope = m_scopes.rbegin(); itScope != m_scopes.rend(); ++itScope)
			{
				auto it = itScope->localSymbols.find(s);
				if (it != itScope->localSymbols.end())
					return it->second;
			}
			
			throw MSCompileException(("unresolved symbol " + s).str().c_str());
		}

		llvm::GlobalVariable* CreateGlobal(llvm::Type* type, llvm::Constant* initializer, const llvm::Twine& name)
		{
			auto var = new llvm::GlobalVariable(*m_pModule,
				type,
				false,
				llvm::GlobalValue::InternalLinkage,
				0,
				name);

			if(initializer)
				var->setInitializer(initializer);

			return var;
		}
		//	Create stack allocation for given type
		llvm::AllocaInst* CreateAlloca(llvm::Type* type, llvm::Value* arraySize = __nullptr, const llvm::Twine& name = "")
		{
			llvm::Function* func = m_pBuilder->GetInsertBlock()->getParent();
			llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
			return tmpBuilder.CreateAlloca(type, arraySize, name);
		}

		llvm::Type* GetLLVMType(MSType type)
		{
			switch (type)
			{
			case MSType::MS_TYPE_BOOLEAN:
				return m_boolType;
			case MSType::MS_TYPE_FLOAT:
				return m_floatType;
			case MSType::MS_TYPE_INTEGER:
				return m_intType;
			case MSType::MS_TYPE_VOID:
				return m_voidType;
			case MSType::MS_TYPE_STRING:
				return m_handleType->getPointerTo();
			default:
				throw std::exception();
			}
		}

		void InitBuiltinTypes()
		{
			m_floatType = llvm::Type::getFloatTy(*m_pContext);
			m_intType = llvm::IntegerType::getInt32Ty(*m_pContext);
			m_boolType = llvm::Type::getInt1Ty(*m_pContext);
			m_voidType = llvm::Type::getVoidTy(*m_pContext);
			m_charType = llvm::Type::getInt16Ty(*m_pContext);
			m_pointerType = llvm::Type::getInt8PtrTy(*m_pContext);

			std::vector<llvm::Type*> elements =
			{
				m_intType,		//	refcount
				m_pointerType,	//	ptr
			};
			m_handleType = llvm::StructType::get(*m_pContext, elements);

			elements =
			{
				m_intType,
				llvm::ArrayType::get(m_charType, 0),
			};
			m_stringType = llvm::StructType::get(*m_pContext, elements);
		}

		//	Create runtime function prototypes
		void DeclareInternalFunctions()
		{
			//	Register handle functions
			std::array<llvm::Type*, 1> argTypes;
			argTypes[0] = m_handleType->getPointerTo();
			
			llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(*m_pContext), argTypes, false);

			m_handleIncrementFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "hdlinc", m_pModule.get());
			m_handleDecrementFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "hdldec", m_pModule.get());

			//	Register string functions
			m_scopes[0].localSymbols["strlen"] = llvm::Function::Create(llvm::FunctionType::get(m_intType, argTypes, false), llvm::Function::ExternalLinkage, "strlen", m_pModule.get());

			std::array<llvm::Type*, 2> strconcat_argTypes;
			strconcat_argTypes[0] = m_handleType->getPointerTo();
			strconcat_argTypes[1] = m_handleType->getPointerTo();

			llvm::FunctionType* strconcat_funcType = llvm::FunctionType::get(m_handleType->getPointerTo(), strconcat_argTypes, false);

			m_scopes[0].localSymbols["strcat"] = llvm::Function::Create(strconcat_funcType, llvm::Function::ExternalLinkage, "strcat", m_pModule.get());

			llvm::FunctionType* strcmp_funcType = llvm::FunctionType::get(m_intType, strconcat_argTypes, false);

			m_scopes[0].localSymbols["strcmp"] = llvm::Function::Create(strcmp_funcType, llvm::Function::ExternalLinkage, "strcmp", m_pModule.get());


			std::array<llvm::Type*, 3> substr_argTypes;
			substr_argTypes[0] = m_handleType->getPointerTo();
			substr_argTypes[1] = m_intType;
			substr_argTypes[2] = m_intType;

			llvm::FunctionType* substr_funcType = llvm::FunctionType::get(m_handleType->getPointerTo(), substr_argTypes, false);
			
			m_scopes[0].localSymbols["substr"] = llvm::Function::Create(substr_funcType, llvm::Function::ExternalLinkage, "substr", m_pModule.get());


			llvm::FunctionType* strgetptr_funcType = llvm::FunctionType::get(m_charType->getPointerTo(), argTypes, false);
			m_getstrptrFunc = llvm::Function::Create(strgetptr_funcType, llvm::Function::ExternalLinkage, "strgetptr", m_pModule.get());
		}

		llvm::Value* ConvertStringToCString(llvm::Value* ptr)
		{
			std::vector<llvm::Value*> args =
			{
				ptr
			};
			return m_pBuilder->CreateCall(m_getstrptrFunc, args);
		}
	public:
		MSIRCompiler(llvm::LLVMContext* pContext, const char* name)
			: m_pContext(pContext),
			m_name(name)
		{
			m_pModule = llvm::make_unique<llvm::Module>(name, *m_pContext);
			m_pBuilder = llvm::make_unique<llvm::IRBuilder<>>(*m_pContext);

			InitBuiltinTypes();

			PushScope(nullptr, nullptr, ScopeType::Global);

			DeclareInternalFunctions();


			// Create a new pass manager attached to it.
			/*m_pFPM = llvm::make_unique<llvm::FunctionPassManager>(m_pModule.get());

			// Do simple "peephole" optimizations and bit-twiddling optzns.
			m_pFPM->addPass(llvm::createInstructionCombiningPass());
			// Reassociate expressions.
			m_pFPM->addPass(llvm::createReassociatePass());
			// Eliminate Common SubExpressions.
			m_pFPM->addPass(llvm::createGVNPass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			m_pFPM->addPass(llvm::createCFGSimplificationPass());

			m_pFPM->doInitialization();*/

		}

		llvm::Value* CompileExpression(ASTNullNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			return llvm::ConstantPointerNull::get(m_handleType->getPointerTo());
		}
		llvm::Value* CompileExpression(ASTBooleanNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			if (pNode->m_value)
				return llvm::ConstantInt::getTrue(*m_pContext);
			else
				return llvm::ConstantInt::getFalse(*m_pContext);
		}
		llvm::Value* CompileExpression(ASTIntegerNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			return llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(*m_pContext), pNode->m_value);
		}
		llvm::Value* CompileExpression(ASTFloatNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			return llvm::ConstantFP::get(llvm::Type::getFloatTy(*m_pContext), pNode->m_value);
		}
		llvm::Value* CompileExpression(ASTStringNode* pNode, bool* isRValue)
		{
			//	Is not an R-value cause string constants are actually hidden global variables
			*isRValue = false;

			return GetConstantString(pNode->m_value);
		}
		llvm::Value* CompileExpression(ASTNameNode* pNode, bool* isRValue)
		{
			*isRValue = false;

			llvm::Value* val = GetSymbol(pNode->m_name);
			return m_pBuilder->CreateLoad(val);
		}
		llvm::Value* CompileExpression(ASTBinaryOperationNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			bool lhsIsRValue;
			bool rhsIsRValue;
			llvm::Value* lhs = CompileExpression(pNode->m_expression1, &lhsIsRValue);
			llvm::Value* rhs = CompileExpression(pNode->m_expression2, &rhsIsRValue);

			llvm::Type* type = ConvertValuesForOperation(&lhs, &rhs);

			switch (pNode->m_operator)
			{
			case MSOperator::MS_OPERATOR_ADD:
				return ComputeAdd(lhs, rhs);
			case MSOperator::MS_OPERATOR_AND:
				return ComputeAnd(lhs, rhs);
			case MSOperator::MS_OPERATOR_DIVIDE:
				return ComputeDivide(lhs, rhs);
			case MSOperator::MS_OPERATOR_EQUALITY:
				return ComputeEquality(lhs, rhs);
			case MSOperator::MS_OPERATOR_GREATER:
				return ComputeGreater(lhs, rhs);
			case MSOperator::MS_OPERATOR_GREATEREQUAL:
				return ComputeGreaterOrEqual(lhs, rhs);
			case MSOperator::MS_OPERATOR_INEQUALITY:
				return ComputeInequality(lhs, rhs);
			case MSOperator::MS_OPERATOR_LESSER:
				return ComputeLesser(lhs, rhs);
			case MSOperator::MS_OPERATOR_LESSEREQUAL:
				return ComputeLesserOrEqual(lhs, rhs);
			case MSOperator::MS_OPERATOR_MODULO:
				return ComputeModulo(lhs, rhs);
			case MSOperator::MS_OPERATOR_MULTIPLY:
				return ComputeMultiply(lhs, rhs);
			case MSOperator::MS_OPERATOR_OR:
				return ComputeOr(lhs, rhs);
			case MSOperator::MS_OPERATOR_SUBTRACT:
				return ComputeSubtract(lhs, rhs);
			default:
				throw std::exception();
			}

			//	Now that we dont need it anymore, destroy if RValue
			if (lhsIsRValue && IsHandlePtr(lhs))
			{
				ComputeHandleDecrement(lhs);
			}
			if (rhsIsRValue && IsHandlePtr(rhs))
			{
				ComputeHandleDecrement(rhs);
			}
		}
		llvm::Value* CompileExpression(ASTCallNode* pNode, bool* isRValue)
		{
			*isRValue = true;

			llvm::Function* func = llvm::dyn_cast<llvm::Function>(GetSymbol(pNode->m_name));
			if (!func)
				throw std::exception();

			std::vector<llvm::Value*> args(pNode->m_arguments.size());
			std::vector<bool> argRValues(pNode->m_arguments.size());

			auto argIt = func->arg_begin();

			for (int i = 0; i < pNode->m_arguments.size(); ++i)
			{
				//	fuck vector<bool>
				bool tmpArgRValue;
				args[i] = CompileExpression(pNode->m_arguments[i], &tmpArgRValue);

				
				//	Convert to C-string if function wants a const wchar_t*
				if (args[i]->getType() == m_handleType->getPointerTo() && argIt->getType() == m_charType->getPointerTo())
					args[i] = ConvertStringToCString(args[i]);

				argRValues[i] = tmpArgRValue;

				++argIt;
			}

			llvm::Value* result = m_pBuilder->CreateCall(func, args);

			//	Now that we dont need it anymore, destroy if RValue
			for (int i = 0; i < args.size(); ++i)
			{
				if (argRValues[i] && IsHandlePtr(args[i]))
				{
					ComputeHandleDecrement(args[i]);
				}
			}

			return result;
		}
		llvm::Value* CompileExpression(IASTNode* pNode, bool* isRValue)
		{
			if (is_type<ASTNullNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTNullNode*>(pNode), isRValue);
			}
			else if (is_type<ASTBooleanNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTBooleanNode*>(pNode), isRValue);
			}
			else if (is_type<ASTIntegerNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTIntegerNode*>(pNode), isRValue);
			}
			else if (is_type<ASTFloatNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTFloatNode*>(pNode), isRValue);
			}
			else if (is_type<ASTStringNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTStringNode*>(pNode), isRValue);
			}
			else if (is_type<ASTNameNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTNameNode*>(pNode), isRValue);
			}
			else if (is_type<ASTBinaryOperationNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTBinaryOperationNode*>(pNode), isRValue);
			}
			else if (is_type<ASTCallNode>(pNode))
			{
				return CompileExpression(dynamic_cast<ASTCallNode*>(pNode), isRValue);
			}
			else
			{
				throw std::exception();
			}
		}

		void PushScope(llvm::BasicBlock* startBlock, llvm::BasicBlock* outBlock, ScopeType type)
		{
			m_scopes.emplace_back(startBlock, outBlock, type);
		}
		//	Pop all scopes until it
		void PopScope(int count = 1)
		{
			//	Remove the local scope
			for (int i = 0; i<count; ++i)
				m_scopes.pop_back();
		}
		//	Generate code for variables destruction(decrement, destructor call, etc.)
		void DestroyScopeVariables(int iScope)
		{
			for (auto it : m_scopes[iScope].localSymbols)
			{
				if (IsHandlePtrPtr(it.second))
				{
					llvm::Value* ptr = m_pBuilder->CreateLoad(it.second);
					ComputeHandleDecrement(ptr);
				}
			}
		}

		//	Get index of last loop scope we are in. -1 if not found
		int GetCurrentLoopScope()
		{
			for (int i = m_scopes.size() - 1; i >= 0; --i)
			{
				if (m_scopes[i].type == ScopeType::While)
					return i;
			}

			return -1;
		}
		int GetCurrentFunctionScope()
		{
			for (int i = m_scopes.size() - 1; i >= 0; --i)
			{
				if (m_scopes[i].type == ScopeType::Function)
					return i;
			}

			return -1;
		}

		template <typename Iter>
		bool CompileBlock(Iter begin, Iter end = begin + 1)
		{
			//	Compile statements
			bool unreachable = false;

			for (Iter it = begin; it != end; ++it)
			{
				if (!CompileStatement(*it))
				{
					unreachable = true;
					break;
				}
			}
						
			return !unreachable;
		}

		
		/*
		All these return true if compilation should keep going with following statements.
		False if following statements are unreachable.
		eg. : return, break, and continue always return false. if and while may return false or true.
		*/
		bool CompileStatement(ASTAssignmentNode* pNode)
		{
			//	if void, there is no type, so variable was already declared.
			if (pNode->m_type == MSType::MS_TYPE_VOID)
			{
				llvm::Value* ptr = GetSymbol(pNode->m_name);
				if (!ptr)
					throw std::exception();

				//	Compile expression before we call destructor.
				//	So we get correct result if we got stuff like : 'string s = substr(s, 1)'
				bool isRValue;
				llvm::Value* expr = CompileExpression(pNode->m_expression, &isRValue);
				
				//	Decrement old handle
				if (IsHandlePtrPtr(ptr))
				{
					llvm::Value* old_ptr = m_pBuilder->CreateLoad(ptr);
					ComputeHandleDecrement(old_ptr);
				}

				//	Increment if expr is an object and its an LValue
				if (!isRValue && IsHandlePtr(expr))
				{
					ComputeHandleIncrement(expr);
				}
				
				m_pBuilder->CreateStore(expr, ptr);
			}
			else
			{
				if (IsSymbolDefined(pNode->m_name))
					throw MSCompileException((pNode->m_name + " redefinition").str().c_str());

				bool isRValue;
				llvm::Value* expr = CompileExpression(pNode->m_expression, &isRValue);

				llvm::Value* ptr = nullptr;
				if (m_scopes.size() > 1)
				{
					ptr = CreateAlloca(GetLLVMType(pNode->m_type));
					m_scopes.back().localSymbols[pNode->m_name] = ptr;
					m_pBuilder->CreateStore(expr, ptr);
				}
				else
				{
					//	We are in global scope, so need to create a global variable
					//	We use a global because since global scope IS a function, but is not available in another function
					
					if (auto cst = llvm::dyn_cast<llvm::Constant>(expr))
					{
						ptr = CreateGlobal(GetLLVMType(pNode->m_type), cst, pNode->m_name);
						m_scopes.back().localSymbols[pNode->m_name] = ptr;
					}
					else
					{
						throw MSCompileException("global variable with non constant initialization not supported");
					}

					
				}


				//	Increment if expr is an object and its an LValue
				if (!isRValue && IsHandlePtr(expr))
				{
					ComputeHandleIncrement(expr);
				}

			}

			return true;
		}
		bool CompileStatement(ASTIfNode* pNode)
		{
			bool isRValue;
			llvm::Value* condition = CompileExpression(pNode->m_expression, &isRValue);
			condition = Convert(condition, m_boolType);

			//	Now that we dont need it anymore, destroy if RValue
			if (isRValue && IsHandlePtr(condition))
			{
				ComputeHandleDecrement(condition);
			}

			llvm::Function* function = m_pBuilder->GetInsertBlock()->getParent();

			llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*m_pContext, "", function);
			llvm::BasicBlock* elseBlock = nullptr;
			llvm::BasicBlock* mergeBlock = nullptr;

			//	Create branch instruction
			if (pNode->m_elseStatements.size() > 0)
			{
				elseBlock = llvm::BasicBlock::Create(*m_pContext);
				m_pBuilder->CreateCondBr(condition, thenBlock, elseBlock);
			}
			else
			{
				//	There is no else, jump to merge block directly
				mergeBlock = llvm::BasicBlock::Create(*m_pContext);
				m_pBuilder->CreateCondBr(condition, thenBlock, mergeBlock);
			}

			//	Move builder to then block
			m_pBuilder->SetInsertPoint(thenBlock);

			//	Push local scope
			PushScope(nullptr, nullptr, ScopeType::If);

			bool ifUnreachable = false;

			//	Compile statements
			if (CompileBlock(pNode->m_statements.begin(), pNode->m_statements.end()))
			{
				//	Jump to merge block cause code is reachable
				if (mergeBlock == nullptr)
					mergeBlock = llvm::BasicBlock::Create(*m_pContext);

				DestroyScopeVariables(m_scopes.size() - 1);
				m_pBuilder->CreateBr(mergeBlock);

				ifUnreachable = false;
			}
			else
				ifUnreachable = true;

			//	Pop local scope
			PopScope();


			bool elseUnreachable = false;

			if (elseBlock != nullptr)
			{
				function->getBasicBlockList().push_back(elseBlock);
				m_pBuilder->SetInsertPoint(elseBlock);

				//	Push local scope
				PushScope(nullptr, nullptr, ScopeType::If);

				if (CompileBlock(pNode->m_elseStatements.begin(), pNode->m_elseStatements.end()))
				{
					if (mergeBlock == nullptr)
						mergeBlock = llvm::BasicBlock::Create(*m_pContext);

					DestroyScopeVariables(m_scopes.size() - 1);
					m_pBuilder->CreateBr(mergeBlock);

					elseUnreachable = false;
				}
				else
					elseUnreachable = true;

				//	Pop local scope
				PopScope();
			}

			if (mergeBlock != nullptr)
			{
				function->getBasicBlockList().push_back(mergeBlock);
				m_pBuilder->SetInsertPoint(mergeBlock);
			}

			bool unreachable = ifUnreachable && elseUnreachable;
			return !unreachable;
		}
		bool CompileStatement(ASTReturnNode* pNode)
		{
			int iScope = GetCurrentFunctionScope();
			if(iScope == -1)
				throw MSCompileException("illegal return");		//	Not in a function

			//	Must compute expression before we destroy the scope
			bool isRValue;
			llvm::Value* value = CompileExpression(pNode->m_expression, &isRValue);
			
			//	/!\ Could be optimize cause if we return a local variable, we increment, then PopScope decrement
			//	Increment because Call statements, are ALWAYS considered RValue, that means if we return an LValue, its gonna be decremented.
			if (!isRValue && IsHandlePtr(value))
			{
				ComputeHandleIncrement(value);
			}

			//	Must destroy local variables before branching
			for (int i = m_scopes.size() - 1; i >= iScope; --i)
				DestroyScopeVariables(i);

			m_pBuilder->CreateRet(value);
			return false;
		}
		bool CompileStatement(ASTBreakNode* pNode)
		{
			int iScope = GetCurrentLoopScope();
			if (iScope == -1)
				throw MSCompileException("illegal break");	//	Not in a loop

			//	Must destroy variables before branching
			for (int i = m_scopes.size() - 1; i >= iScope; --i)
				DestroyScopeVariables(i);

			m_pBuilder->CreateBr(m_scopes[iScope].outBlock);
			return false;
		}
		bool CompileStatement(ASTContinueNode* pNode)
		{
			int iScope = GetCurrentLoopScope();
			if (iScope == -1)
				throw MSCompileException("illegal continue");	//	We are not in a loop

			//	Must destroy variables before branching
			for (int i = m_scopes.size() - 1; i >= iScope; --i)
				DestroyScopeVariables(i);

			m_pBuilder->CreateBr(m_scopes[iScope].startBlock);
			return false;
		}
		bool CompileStatement(ASTWhileNode* pNode)
		{
			bool isRValue;
			llvm::Value* condition = CompileExpression(pNode->m_expression, &isRValue);
			condition = Convert(condition, m_boolType);

			//	Now that we dont need it anymore, destroy if RValue
			if (isRValue && IsHandlePtr(condition))
			{
				ComputeHandleDecrement(condition);
			}

			llvm::Function* function = m_pBuilder->GetInsertBlock()->getParent();

			llvm::BasicBlock* conditionBlock = llvm::BasicBlock::Create(*m_pContext, "", function);
			llvm::BasicBlock* block = llvm::BasicBlock::Create(*m_pContext);
			llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*m_pContext);

			m_pBuilder->CreateBr(conditionBlock);

			m_pBuilder->SetInsertPoint(conditionBlock);

			m_pBuilder->CreateCondBr(condition, block, mergeBlock);

			function->getBasicBlockList().push_back(block);
			m_pBuilder->SetInsertPoint(block);

			//	Push local scope
			PushScope(conditionBlock, mergeBlock, ScopeType::While);

			//	Compile statements
			if (CompileBlock(pNode->m_statements.begin(), pNode->m_statements.end()))
			{
				DestroyScopeVariables(m_scopes.size() - 1);
				//	Jump to merge block
				m_pBuilder->CreateBr(mergeBlock);
			}

			//	Pop local scope
			PopScope();

			function->getBasicBlockList().push_back(mergeBlock);
			m_pBuilder->SetInsertPoint(mergeBlock);

			return true;
		}
		bool CompileStatement(IASTNode* pNode)
		{
			if (is_type<ASTAssignmentNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTAssignmentNode*>(pNode));
			}
			else if (is_type<ASTIfNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTIfNode*>(pNode));
			}
			else if (is_type<ASTReturnNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTReturnNode*>(pNode));
			}
			else if (is_type<ASTBreakNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTBreakNode*>(pNode));
			}
			else if (is_type<ASTContinueNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTContinueNode*>(pNode));
			}
			else if (is_type<ASTWhileNode>(pNode))
			{
				return CompileStatement(dynamic_cast<ASTWhileNode*>(pNode));
			}
			else if (is_type<ASTCallNode>(pNode))
			{
				bool isRValue;
				llvm::Value* result = CompileExpression(dynamic_cast<ASTCallNode*>(pNode), &isRValue);

				if (isRValue && IsHandlePtr(result))
					ComputeHandleDecrement(result);

				return true;
			}
			else
			{
				throw std::exception();
			}
		}

		void CompileFunction(ASTFunctionNode* pNode)
		{
			//	Resolve the return type
			llvm::Type* retType = GetLLVMType(pNode->m_retType);
			
			//	Resolve the argument types
			std::vector<llvm::Type*> argTypes(pNode->m_arguments.size());
			for (int i = 0; i < pNode->m_arguments.size(); ++i)
				argTypes[i] = GetLLVMType(pNode->m_arguments[i].first);

			//	Get function type
			llvm::FunctionType* funcType = llvm::FunctionType::get(retType, argTypes, false);

			llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, m_name + "::" + pNode->m_name, m_pModule.get());
			
			llvm::BasicBlock* block = llvm::BasicBlock::Create(*m_pContext, "", func);
			auto oldInsertBlock = m_pBuilder->GetInsertBlock();
			auto oldInsertPoint = m_pBuilder->GetInsertPoint();
			m_pBuilder->SetInsertPoint(block);

			//	Push local scope
			PushScope(nullptr, nullptr, ScopeType::Function);

			//	Create arguments
			int i = 0;
			for (auto& arg : func->args())
			{
				llvm::Value* argValue = CreateAlloca(argTypes[i]);
				m_scopes.back().localSymbols[pNode->m_arguments[i].second] = argValue;
				m_pBuilder->CreateStore(&arg, argValue);
				++i;
			}

			//	Compile statements

			bool unreachable = false;
			for (auto statement : pNode->m_statements)
			{
				if (!CompileStatement(statement))
				{
					unreachable = true;
					break;
				}
			}

			if (!unreachable)
			{
				DestroyScopeVariables(m_scopes.size() - 1);
				m_pBuilder->CreateRetVoid();
			}

			PopScope();

#ifdef _DEBUG
			std::string error;
			llvm::raw_string_ostream os(error);

			if (llvm::verifyFunction(*func, &os))
			{
				m_pModule->dump();
				os.flush();
				throw MSCompileException(error.c_str());
			}
#endif
			m_scopes[0].localSymbols[pNode->m_name] = func;

			m_pBuilder->SetInsertPoint(oldInsertBlock, oldInsertPoint);
		}

		void CompileAll(const pool_vector<IASTNode*>& tree)
		{
			llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(*m_pContext), false);

			llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, m_name + "::$", m_pModule.get());

			llvm::BasicBlock* block = llvm::BasicBlock::Create(*m_pContext, "", func);
			m_pBuilder->SetInsertPoint(block);
			
			for (auto pNode : tree)
			{
				if (is_type<ASTFunctionNode>(pNode))
				{
					CompileFunction(dynamic_cast<ASTFunctionNode*>(pNode));
				}
				else
				{
					CompileStatement(pNode);
				}
			}

			m_pBuilder->CreateRetVoid();

			if (llvm::verifyFunction(*func))
				throw std::exception();
		}

		//	Create a declaration for external symbols
		void CreateImportDeclaration(MSSymbol* pSymbol)
		{
			if (pSymbol->type == MSSymbolType::MS_SYMBOL_FUNCTION)
			{
				llvm::Type* retType = GetLLVMType(pSymbol->functionData.resultType);

				//	Get function name

				//	Resolve the argument types
				std::vector<llvm::Type*> argTypes(pSymbol->functionData.count);
				for (int i = 0; i < pSymbol->functionData.count; ++i)
				{
					if (pSymbol->functionData.parameterTypes[i] == MSType::MS_TYPE_STRING)
					{
						argTypes[i] = m_charType->getPointerTo();
					}
					else
						argTypes[i] = GetLLVMType(pSymbol->functionData.parameterTypes[i]);
				}

				//	Get function type
				llvm::FunctionType* funcType = llvm::FunctionType::get(retType, argTypes, false);

				llvm::Function* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, m_name + "::" + pSymbol->name, m_pModule.get());

				if (pSymbol->functionData.callingConvention == MSCallingConvention::MS_CC_CDECL)
					func->setCallingConv(llvm::CallingConv::C);
				else if (pSymbol->functionData.callingConvention == MSCallingConvention::MS_CC_STDCALL)
					func->setCallingConv(llvm::CallingConv::X86_StdCall);
				else throw std::exception();

				m_scopes[0].localSymbols[pSymbol->name] = func;
			}
		}

		std::unique_ptr<llvm::Module>& GetModule()
		{
			return m_pModule;
		}

	};
}