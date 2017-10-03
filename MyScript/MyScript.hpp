#pragma once

#include "MyScript.h"

#ifdef __cplusplus

//	Helper stuff for c++

#include <tuple>
#include <type_traits>
#include <memory>

/* Get MSType from C type */
template <typename T>
constexpr MSType MSCTypeToMSType()
{
	static_assert(0, "Type not supported");
}
template <>
constexpr MSType MSCTypeToMSType<int>()
{
	return MSType::MS_TYPE_INTEGER;
}
template <>
constexpr MSType MSCTypeToMSType<void>()
{
	return MSType::MS_TYPE_VOID;
}
template <>
constexpr MSType MSCTypeToMSType<float>()
{
	return MSType::MS_TYPE_FLOAT;
}
template <>
constexpr MSType MSCTypeToMSType<bool>()
{
	return MSType::MS_TYPE_BOOLEAN;
}
template <>
constexpr MSType MSCTypeToMSType<const wchar_t*>()
{
	return MSType::MS_TYPE_STRING;
}
template <>
constexpr MSType MSCTypeToMSType<wchar_t*>()
{
	return MSCTypeToMSType<const wchar_t*>();
}

template <typename... Args>
struct MSFillMSTypeArray;

template <typename T, typename... Args>
struct MSFillMSTypeArray<T, Args...>
{
	static void Fill(MSType* pArray)
	{
		*pArray = MSCTypeToMSType<T>();
		MSFillMSTypeArray<Args...>::Fill(++pArray);
	}
};

template <>
struct MSFillMSTypeArray<>
{
	static void Fill(MSType* pArray) {}
};

/* Create a symbol from a C function */
template <typename R, typename... Args>
MSSymbol MSSymbolFromCFunction(R(__cdecl *f)(Args...), const char* name)
{
	static_assert(sizeof...(Args) <= 10, "Argument count higher than 10 not supported");

	MSSymbol symbol;
	symbol.name = name;
	symbol.address = reinterpret_cast<void*>(f);
	symbol.type = MSSymbolType::MS_SYMBOL_FUNCTION;
	symbol.functionData.resultType = MSCTypeToMSType<R>();
	symbol.functionData.count = sizeof...(Args);
	symbol.functionData.callingConvention = MSCallingConvention::MS_CC_CDECL;
	MSFillMSTypeArray<Args...>::Fill(symbol.functionData.parameterTypes);

	return symbol;
}
template <typename R, typename... Args>
MSSymbol MSSymbolFromCFunction(R(__stdcall *f)(Args...), const char* name)
{
	static_assert(sizeof...(Args) <= 10, "Argument count higher than 10 not supported");

	MSSymbol symbol;
	symbol.name = name;
	symbol.address = reinterpret_cast<void*>(f);
	symbol.type = MSSymbolType::MS_SYMBOL_FUNCTION;
	symbol.functionData.resultType = MSCTypeToMSType<R>();
	symbol.functionData.count = sizeof...(Args);
	symbol.functionData.callingConvention = MSCallingConvention::MS_CC_STDCALL;
	MSFillMSTypeArray<Args...>::Fill(symbol.functionData.parameterTypes);

	return symbol;
}

/* Some marshalling stuff */
template<typename T>
struct MSMarshal;

template<>
struct MSMarshal<int>
{
	typedef int MarshaledType;

	static int Marshal(int i)
	{
		return i;
	}

	static void Free(int i)
	{

	}
};

template<>
struct MSMarshal<bool>
{
	typedef bool MarshaledType;

	static bool Marshal(bool b)
	{
		return b;
	}

	static void Free(bool b)
	{

	}
};

template<>
struct MSMarshal<float>
{
	typedef float MarshaledType;

	static float Marshal(float f)
	{
		return f;
	}

	static void Free(float f)
	{

	}
};

template<>
struct MSMarshal<const wchar_t*>
{
	typedef MSString MarshaledType;

	static MSString Marshal(const wchar_t* s)
	{
		MSString str = nullptr;
		if (!MSAllocString(s, &str))
			throw std::exception();

		return str;
	}

	static void Free(MSString str)
	{
		MSFreeString(str);
	}
};
template<>
struct MSMarshal<wchar_t*>
{
	typedef MSString MarshaledType;

	static MSString Marshal(wchar_t* s)
	{
		return MSMarshal<const wchar_t*>::Marshal(s);
	}

	static void Free(MSString str)
	{
		return MSMarshal<const wchar_t*>::Free(str);
	}
};
/*
Runtime check symbol arguments. throw std::exception on error
*/
template<typename... Args>
struct MSArgChecker;

template<typename T, typename... Args>
struct MSArgChecker<T, Args...>
{
	static void ArgCheck(MSType* pTypes)
	{
		if (*pTypes == MSCTypeToMSType<T>())
			throw std::exception();

		MSArgChecker<Args...>::ArgCheck(++pTypes);
	}
};

template<>
struct MSArgChecker<>
{
	static void ArgCheck(MSSymbol& symbol)
	{
	}
};

// /!\ Pretty ugly... but works for now
class MSSmartString
{
	std::shared_ptr<MSString> m_ptr;
public:
	MSSmartString()
	{

	}
	MSSmartString(MSString s)
		: m_ptr(new MSString(s), [](MSString* s) { MSFreeString(*s); })
	{
	}
	MSSmartString(const MSSmartString& s)
		: m_ptr(s.m_ptr)
	{
	}
	~MSSmartString()
	{
		m_ptr.reset();
	}

	MSSmartString& operator=(const MSSmartString& s)
	{
		m_ptr.reset();

		m_ptr = s.m_ptr;

		return *this;
	}

	void Reset()
	{
		m_ptr.reset();
	}

	MSString Get() const
	{
		return *m_ptr;
	}

	const wchar_t* Str() const
	{
		return MSGetString(*m_ptr);
	}

	operator const wchar_t*() const
	{
		return Str();
	}
};

/* some variadic stuff needed for tuple unpacking */
//	This is a fake type to allow variadic type deduction
template<int ...>
struct int_sequence { };

//	This generate a sequence of int, 0 to N, as variadic template parameters
template<int N, int ...S>
struct int_sequence_generator : int_sequence_generator<N - 1, N - 1, S...> { };

template<int ...S>
struct int_sequence_generator<0, S...> {
	typedef int_sequence<S...> type;
};

/* Some tuple unpacking stuff */
template<typename F, typename... Args, int... i>
auto unpack_call(F f, std::tuple<Args...>& tuple, int_sequence<i...>) -> typename std::result_of<F(Args...)>::type
{
	return f(std::get<i>(tuple)...);
}

//	Call f with all tuple items as variadic arguments
template<typename F, typename... Args>
auto unpack(F& f, std::tuple<Args...>& tuple) -> typename std::result_of<F(Args...)>::type
{
	return unpack_call(f, tuple, typename int_sequence_generator<sizeof...(Args)>::type());
}

template<typename F, typename... Args, int... i>
void unpack_each_call(F& f, std::tuple<Args...>& tuple, int_sequence<i...>)
{
	auto a = { (f(std::get<i>(tuple)), 0)... };
}

//	specialization for empty args. Required cause if 'a' initialization list is empty, auto cannot deduce the type and doesnt compile
template<typename F>
void unpack_each_call(F& f, std::tuple<>& tuple, int_sequence<>)
{
}


//	Call f one time for each object in the tuple.
//	/!\ F must be a class with () operator. Cannot pass a function like that, its impossible.
template<typename F, typename... Args>
void unpack_each(F f, std::tuple<Args...>& tuple)
{
	unpack_each_call(f, tuple, typename int_sequence_generator<sizeof...(Args)>::type());
}

/*
Allow simple function call by taking care of marshalling/destruction of parameters.
Does runtime check for argument types
*/
template<typename R>
struct MSSymbolFunctor
{
private:
	MSSymbol m_symbol;

	//	Need that to unpack correctly, cause we cannot unpack Args and args at the same time
	template<typename T>
	typename MSMarshal<T>::MarshaledType Marshal(T t)
	{
		return MSMarshal<T>::Marshal(t);
	}

	//	Argument destructor
	struct MSArgDestructor
	{
		template<typename T>
		void operator()(T t)
		{
			MSMarshal<T>::Free(t);
		}
	};
public:
	MSSymbolFunctor(const MSSymbol& symbol)
		: m_symbol(symbol)
	{
	}


	template<typename... Args>
	R operator()(Args... args)
	{
		R(*f)(typename MSMarshal<Args>::MarshaledType...) = reinterpret_cast<R(*)(typename MSMarshal<Args>::MarshaledType...)>(m_symbol.address);

		//	Store marshaled arguments
		std::tuple<typename MSMarshal<Args>::MarshaledType...> params(Marshal(args)...);

		//	Call function with arguements
		R result = unpack(f, params);

		//	Delete arguments
		unpack_each(MSArgDestructor(), params);

		return result;
	}
};
template<>
struct MSSymbolFunctor<void>
{
private:
	MSSymbol m_symbol;

	//	Need that to unpack correctly, cause we cannot unpack Args and args at the same time
	template<typename T>
	typename MSMarshal<T>::MarshaledType Marshal(T t)
	{
		return MSMarshal<T>::Marshal(t);
	}

	//	Argument destructor
	struct MSArgDestructor
	{
		template<typename T>
		void operator()(T t)
		{
			MSMarshal<T>::Free(t);
		}
	};
public:
	MSSymbolFunctor(const MSSymbol& symbol)
		: m_symbol(symbol)
	{
	}


	template<typename... Args>
	void operator()(Args... args)
	{
		void(*f)(typename MSMarshal<Args>::MarshaledType...) = reinterpret_cast<void(*)(typename MSMarshal<Args>::MarshaledType...)>(m_symbol.address);

		//	Store marshaled arguments
		std::tuple<typename MSMarshal<Args>::MarshaledType...> params(Marshal(args)...);

		//	Call function with arguements
		unpack(f, params);

		//	Delete arguments
		unpack_each(MSArgDestructor(), params);
	}
};

/*
 * Helper for enumerating symbols.
 */
class MSSymbolEnumerator
{
	HANDLE m_hFind = NULL;
	HANDLE m_hScript = NULL;
	MSSymbol m_symbol;
public:
	MSSymbolEnumerator(HANDLE hScript)
		: m_hScript(hScript)
	{

	}
	~MSSymbolEnumerator()
	{
		if (m_hFind != NULL)
		{
			MSCloseHandle(m_hFind);
			m_hFind = NULL;
		}
	}
	bool Next()
	{
		if (m_hFind == NULL)
		{
			m_hFind = MSGetFirstSymbol(m_hScript, &m_symbol);
			if (m_hFind != NULL)
				return true;
			else
				return false;
		}
		else
		{
			return MSGetNextSymbol(m_hFind, &m_symbol);
		}
	}

	const MSSymbol& Current() const
	{
		return m_symbol;
	}

};
#endif