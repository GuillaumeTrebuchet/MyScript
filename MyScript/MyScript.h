#pragma once

#include <Windows.h>

#define MSAPI __stdcall

#ifdef MS_EXPORT_DLL
#define MSEXPORT __declspec(dllexport)
#else
#define MSEXPORT
#endif

//	Do not use namespace for C compatibility
/*
struct MSHandle
{
	int		refcount;
	void*	ptr;
};

struct MSString
{
	int size;
	wchar_t data[0];
};
*/

//	Opaque type. Just for type safety
typedef struct {} *MSString;

//	Types available in MyScript
enum MSType
{
	MS_TYPE_INTEGER = 0,	//	int
	MS_TYPE_FLOAT = 1,		//	float
	MS_TYPE_BOOLEAN = 2,	//	bool
	MS_TYPE_STRING = 3,		//	void*
	MS_TYPE_VOID = 4,		//	void
	MS_TYPE_OBJECT = 5,
};

enum MSOperator
{
	MS_OPERATOR_ADD,			//	+
	MS_OPERATOR_SUBTRACT,		//	-
	MS_OPERATOR_MULTIPLY,		//	*
	MS_OPERATOR_DIVIDE,			//	/
	MS_OPERATOR_MODULO,			//	%
	MS_OPERATOR_AND,			//	and
	MS_OPERATOR_OR,				//	or
	MS_OPERATOR_EQUALITY,		//	==
	MS_OPERATOR_INEQUALITY,		//	!=
	MS_OPERATOR_GREATER,		//	>
	MS_OPERATOR_LESSER,			//	<
	MS_OPERATOR_GREATEREQUAL,	//	>=
	MS_OPERATOR_LESSEREQUAL,	//	<=

};

enum MSSymbolType
{
	MS_SYMBOL_VARIABLE,
	MS_SYMBOL_FUNCTION,
};
enum MSCallingConvention
{
	MS_CC_CDECL,
	MS_CC_STDCALL,
};
//	Actually a function
struct MSSymbol
{
	const char*		name;
	void*			address;
	MSSymbolType	type;
	union
	{
		struct
		{
			MSType				resultType;
			MSType				parameterTypes[10];
			MSCallingConvention	callingConvention;
			int					count;
		} functionData;
		struct
		{
			MSType		type;
		} variableData;
	};
};

typedef VOID(MSAPI* MSSyntaxErrorCallback)(LPCSTR id, DWORD line, DWORD col, LPCSTR msg);
typedef VOID(MSAPI* MSCacheCallback)(LPCSTR id, BYTE buffer, DWORD length);


MSEXPORT HANDLE MSAPI MSGetFirstSymbol(HANDLE hScript, MSSymbol* pSymbol);
MSEXPORT BOOL MSAPI MSGetNextSymbol(HANDLE hFind, MSSymbol* pSymbol);

//BOOL MSAPI MSAddSymbol(HANDLE hScript, MSSymbol* pSymbol);

MSEXPORT HANDLE MSAPI MSCreateContext();
//	Compile a script from source
MSEXPORT HANDLE MSAPI MSCompile(
	HANDLE hContext,
	LPCSTR id,
	LPCSTR source,
	DWORD sourceLength,
	MSSymbol* pSymbols,
	DWORD nSymbols,
	//MSCacheCallback cacheCallback,
	MSSyntaxErrorCallback errorCallback);
//	Load a script from precompiled source
/*MSEXPORT HANDLE MSAPI MSLink(
	HANDLE hContext,
	LPCSTR id,
	BYTE buffer,
	DWORD bufferLength,
	MSSymbol* pSymbols,
	DWORD nSymbols,
	MSSyntaxErrorCallback errorCallback);*/

MSEXPORT BOOL MSAPI MSAllocString(
	LPCWSTR str,
	MSString* pString);

MSEXPORT VOID MSAPI MSFreeString(MSString s);
MSEXPORT LPCWSTR MSAPI MSGetString(MSString s);

MSEXPORT VOID MSAPI MSExecute(HANDLE hContext, HANDLE hScript);
MSEXPORT VOID MSAPI MSCloseHandle(HANDLE handle);
