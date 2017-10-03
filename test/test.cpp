// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <array>
#include <memory>

#include "../MyScript/MyScript.hpp"

class Timer
{
	LARGE_INTEGER m_frequency;
	LARGE_INTEGER m_start;
	LARGE_INTEGER m_stop;
public:
	Timer()
	{
		if (!QueryPerformanceFrequency(&m_frequency))
			throw std::exception();
	}

	void Start()
	{
		if (!QueryPerformanceCounter(&m_start))
			throw std::exception();
	}

	void Stop()
	{
		if (!QueryPerformanceCounter(&m_stop))
			throw std::exception();
	}

	__int64 GetElapsedCount()
	{
		return m_stop.QuadPart - m_start.QuadPart;
	}

	double GetElapsedMs()
	{
		return (double)((m_stop.QuadPart - m_start.QuadPart) * 1000) / (double)m_frequency.QuadPart;
	}
};
void MSAPI error_callback(LPCSTR id, DWORD line, DWORD col, LPCSTR msg)
{
	std::cout << id << " : (line " << line << ", col " << col << ") : " << msg << std::endl;
}

void print(const wchar_t* s)
{
	std::wcout << s;

}

std::vector<char> LoadFile(const std::string& filename)
{
	std::ifstream file = std::ifstream(filename, std::ios::binary);
	file.seekg(0, std::ios::end);
	int size = file.tellg();
	file.seekg(0);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);

	return std::move(buffer);
}
int main()
{
	std::vector<char> buffer = LoadFile("test.ms");

	Timer timer;

	std::vector<MSSymbol> symbols =
	{
		MSSymbolFromCFunction(print, "print"),
	};

	//	Compile script
	HANDLE hContext = MSCreateContext();

	timer.Start();

	HANDLE hScript = MSCompile(hContext, "test.ms", buffer.data(), buffer.size(), symbols.data(), symbols.size(), error_callback);

	timer.Stop();
	std::wcout << "compile time : " << timer.GetElapsedMs() << std::endl;

	if (!hScript)
	{
		MSCloseHandle(hContext);
		return 1;
	}

	//	Get exported symbol list
	std::map<std::string, MSSymbol> exportedSymbols;

	MSSymbolEnumerator enumerator(hScript);
	while (enumerator.Next())
		exportedSymbols[enumerator.Current().name] = enumerator.Current();

	//	Call script functions
	try
	{
		MSSymbolFunctor<MSString> getAuthorName(exportedSymbols.at("GetAuthorName"));
		MSSmartString s = getAuthorName();

		const wchar_t* author_name = s;
	}
	catch (std::out_of_range)
	{
		std::wcout << L"GetAuthorName function not found" << std::endl;
	}

	MSCloseHandle(hScript);
	MSCloseHandle(hContext);
	return 0;
}

