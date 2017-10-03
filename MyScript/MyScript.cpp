#include "stdafx.h"

#include "MyScript.h"

#include "MSScript.hpp"
#include "MSContext.hpp"

#include "Parser.hpp"
#include "MSIRCompiler.hpp"
#include "Utility.hpp"

#include "MSRuntime.h"

using namespace MyScript;

MSEXPORT HANDLE MSAPI MSCompile(
	HANDLE hContext,
	LPCSTR id,
	LPCSTR source,
	DWORD sourceLength,
	MSSymbol* pSymbols,
	DWORD nSymbols,
	//MSCacheCallback cacheCallback,
	MSSyntaxErrorCallback errorCallback)
{
	try
	{
		MSContext* pContext = reinterpret_cast<MSContext*>(hContext);

		//	Parse code with parser + scanner
		std::unique_ptr<MemoryPool> memoryPool = std::make_unique<MemoryPool>();

		Scanner scanner;
		scanner.SetSource(source, sourceLength);
		scanner.SetIndex(0);

		Parser parser;
		parser.SetScanner(&scanner);
		parser.SetMemoryPool(memoryPool.get());
		parser.SetErrorCallback(errorCallback);

		std::unique_ptr<pool_vector<IASTNode*>> pTree = std::make_unique<pool_vector<IASTNode*>>(memoryPool->GetAllocator<IASTNode>());
		ParseResult result = parser.ParseAll(*pTree);

		if (result != ParseResult::Success)
			return NULL;

		//	Compile the AST into LLVM IR
		MSIRCompiler compiler(pContext->GetContext(), id);

		for (int i = 0; i < nSymbols; ++i)
		{
			compiler.CreateImportDeclaration(&pSymbols[i]);
		}

		compiler.CompileAll(*pTree);

		MSScript* pScript = new MSScript(id);

		pScript->RegisterExportedFunctions(*pTree);

#ifdef _DEBUG
		compiler.GetModule()->dump();
#endif

		for (int i = 0; i < nSymbols; ++i)
			pScript->GetImportedSymbols().push_back(pSymbols[i]);

		pContext->Compile(pScript, std::move(compiler.GetModule()));
		return pScript;
	}
	catch (MSCompileException e)
	{
		errorCallback(id, 0, 0, e.what());
		return NULL;
	}
}

struct MSSymbolFindData
	: IMSBase
{
	std::vector<MSSymbol>::iterator it;
	std::vector<MSSymbol>::iterator end;
};
MSEXPORT HANDLE MSAPI MSGetFirstSymbol(HANDLE hScript, MSSymbol* pSymbol)
{
	MSScript* pScript = reinterpret_cast<MSScript*>(hScript);
	std::vector<MSSymbol>& symbols = pScript->GetExportedSymbols();

	if (symbols.size() > 0)
	{
		*pSymbol = symbols[0];

		MSSymbolFindData* pFindData = new MSSymbolFindData;
		pFindData->end = symbols.end();
		pFindData->it = ++symbols.begin();
		return pFindData;
	}

	return NULL;
}
MSEXPORT BOOL MSAPI MSGetNextSymbol(HANDLE hFind, MSSymbol* pSymbol)
{
	MSSymbolFindData* pFindData = reinterpret_cast<MSSymbolFindData*>(hFind);
	if (pFindData->it == pFindData->end)
	{
		return FALSE;
	}
	else
	{
		*pSymbol = *pFindData->it++;
		return TRUE;
	}
}

/*BOOL MSAPI MSAddSymbol(HANDLE hScript, MSSymbol* pSymbol)
{
	MSScript* pScript = reinterpret_cast<MSScript*>(hScript);
	pScript->GetImportedSymbols().push_back(*pSymbol);
	return TRUE;
}*/

MSEXPORT HANDLE MSAPI MSCreateContext()
{
	return new MSContext();
}
MSEXPORT VOID MSAPI MSExecute(HANDLE hContext, HANDLE hScript)
{
	MSContext* pContext = reinterpret_cast<MSContext*>(hContext);
	MSScript* pScript = reinterpret_cast<MSScript*>(hScript);
	pContext->Execute(pScript);
}

MSEXPORT VOID MSAPI MSCloseHandle(HANDLE handle)
{
	IMSBase* ptr = static_cast<IMSBase*>(handle);
	delete ptr;
}

MSEXPORT BOOL MSAPI MSAllocString(LPCWSTR str, MSString* pString)
{
	*pString = reinterpret_cast<MSString>(ms_rt_stralloc(str, wcslen(str)));
	return TRUE;
}
MSEXPORT VOID MSAPI MSFreeString(MSString s)
{
	ms_rt_hdldec(reinterpret_cast<MSHandleInternal*>(s));
}

MSEXPORT LPCWSTR MSAPI MSGetString(MSString s)
{
	if (s != nullptr)
	{
		MSHandleInternal* pHandle = reinterpret_cast<MSHandleInternal*>(s);
		return reinterpret_cast<MSStringInternal*>(pHandle->ptr)->data;
	}

	return nullptr;
}