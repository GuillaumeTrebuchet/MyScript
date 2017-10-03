#pragma once
#include "stdafx.h"


class MSCacheCompiler
	: llvm::orc::SimpleCompiler
{
public:
	MSCacheCompiler(llvm::TargetMachine &TM)
		: llvm::orc::SimpleCompiler(TM)
	{}

	llvm::object::OwningBinary<llvm::object::ObjectFile> operator()(llvm::Module &M) const
	{
		llvm::object::OwningBinary<llvm::object::ObjectFile> obj = llvm::orc::SimpleCompiler::operator()(M);

		std::ofstream file = std::ofstream(M.getName().str() + ".precompiled", std::fstream::binary | std::fstream::trunc);
		file.seekp(0, std::fstream::beg);

		llvm::MemoryBufferRef buffer = obj.getBinary()->getMemoryBufferRef();
		file.write(buffer.getBufferStart(), buffer.getBufferSize());

		return obj;
	}
};