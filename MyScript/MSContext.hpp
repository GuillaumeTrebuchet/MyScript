#pragma once
#include "stdafx.h"

#include "MemoryPool.hpp"

#include "Parser.hpp"

#include "MSScript.hpp"

#include "IMSBase.h"

#include "MSRuntime.h"

#include "MSCachingCompiler.hpp"

/*
This class holds LLVM context, and compiler/optimizer. It is responsible of
actual compiling and execution of scripts.
*/
namespace MyScript
{
	class MSSymbolResolver
		: public llvm::JITSymbolResolver
	{
		llvm::DataLayout* m_pLayout = nullptr;
		MSScript* m_pScript = nullptr;

		std::string GetMangledName(const std::string& name)
		{
			std::string MangledName;
			llvm::raw_string_ostream MangledNameStream(MangledName);
			llvm::Mangler::getNameWithPrefix(MangledNameStream, name, *m_pLayout);
			return MangledNameStream.str();
		}

		MSSymbolResolver() = delete;
	public:
		MSSymbolResolver(MSScript* pScript, llvm::DataLayout* pLayout)
			: m_pScript(pScript),
			m_pLayout(pLayout)
		{
			
		}
		llvm::JITSymbol findSymbolInLogicalDylib(const std::string &Name)
		{
			if (Name == GetMangledName("hdlinc"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_hdlinc), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("hdldec"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_hdldec), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("strlen"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_strlen), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("strcat"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_strcat), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("strcmp"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_strcmp), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("substr"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_substr), llvm::JITSymbolFlags::None);
			if (Name == GetMangledName("strgetptr"))
				return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(ms_rt_strgetptr), llvm::JITSymbolFlags::None);

			for (auto& symbol : m_pScript->GetImportedSymbols())
			{
				if (GetMangledName(m_pScript->GetName() + "::" + symbol.name) == Name)
					return llvm::JITSymbol(reinterpret_cast<llvm::JITTargetAddress>(symbol.address), llvm::JITSymbolFlags::None);
			}

			throw MSCompileException(("symbol not found " + Name).c_str());
		}
		llvm::JITSymbol findSymbol(const std::string &Name)
		{
			throw MSCompileException(("symbol not found " + Name).c_str());
		}
	};
	class MSContext
		: public IMSBase
	{
		MSContext(const MSContext&) = delete;
		MSContext& operator=(const MSContext&) = delete;
	private:
		typedef std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)> OptimizeFunctionT;

		typedef llvm::orc::ObjectLinkingLayer<> ObjectLinkingLayerT;
		typedef llvm::orc::IRCompileLayer<ObjectLinkingLayerT> IRCompileLayerT;
		typedef llvm::orc::IRTransformLayer<IRCompileLayerT, OptimizeFunctionT> IRTransformLayerT;
		typedef llvm::orc::CompileOnDemandLayer<IRTransformLayerT> CompileOnDemandLayerT;

		llvm::LLVMContext m_context;

		std::unique_ptr<ObjectLinkingLayerT> m_pObjectLayer;
		std::unique_ptr<IRCompileLayerT> m_pCompileLayer;
		std::unique_ptr<IRTransformLayerT> m_pOptimizeLayer;
		//std::unique_ptr<llvm::orc::JITCompileCallbackManager> m_pCompileCallbackManager;
		//std::unique_ptr<CompileOnDemandLayerT> m_pCompileOnDemandLayer;
		std::unique_ptr<llvm::DataLayout> m_pLayout;
		std::unique_ptr<MemoryPool> m_pMemoryPool;

		std::string GetMangledName(const std::string& name)
		{
			std::string MangledName;
			llvm::raw_string_ostream MangledNameStream(MangledName);
			llvm::Mangler::getNameWithPrefix(MangledNameStream, name, *m_pLayout);
			return MangledNameStream.str();
		}

		std::unique_ptr<llvm::Module> OptimizeModule(std::unique_ptr<llvm::Module> M)
		{
			// Create a function pass manager.
			auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(M.get());

			// Add some optimizations.
			FPM->add(llvm::createInstructionCombiningPass());
			FPM->add(llvm::createReassociatePass());
			FPM->add(llvm::createGVNPass());
			FPM->add(llvm::createCFGSimplificationPass());
			FPM->doInitialization();

			// Run the optimizations over all functions in the module being added to
			// the JIT.
			for (auto &F : *M)
				FPM->run(F);

			return std::move(M);
		}
	public:
		MSContext()
		{
			llvm::InitializeNativeTarget();
			llvm::InitializeNativeTargetAsmPrinter();
			llvm::InitializeNativeTargetAsmParser();

			auto targetMachine = llvm::EngineBuilder().selectTarget();

			m_pLayout = llvm::make_unique<llvm::DataLayout>(targetMachine->createDataLayout());

			m_pObjectLayer = llvm::make_unique<llvm::orc::ObjectLinkingLayer<>>();
			m_pCompileLayer = llvm::make_unique<llvm::orc::IRCompileLayer<llvm::orc::ObjectLinkingLayer<>>>(*m_pObjectLayer, llvm::orc::SimpleCompiler(*targetMachine));
			//m_pCompileLayer = llvm::make_unique<llvm::orc::IRCompileLayer<llvm::orc::ObjectLinkingLayer<>>>(*m_pObjectLayer, MSCachingCompiler(*targetMachine));

			m_pMemoryPool = std::make_unique<MemoryPool>(1024);
			m_pOptimizeLayer = llvm::make_unique<IRTransformLayerT>(
				*m_pCompileLayer,
				[this](std::unique_ptr<llvm::Module> M)
			{
				return OptimizeModule(std::move(M));
			}
			);
		}

		void UpdateSymbols(MSScript* pScript)
		{
			std::string name = pScript->GetName();

			for (auto& s : pScript->GetExportedSymbols())
			{
				std::string mangledName = GetMangledName(name + "::" + s.name);
				s.address = reinterpret_cast<void*>(m_pOptimizeLayer->findSymbol(mangledName, false).getAddress());
			}
		}
		/*void Link(std::unique_ptr<llvm::MemoryBuffer> ObjBuffer)
		{
			std::vector<std::unique_ptr<llvm::object::OwningBinary<llvm::object::ObjectFile>>> Objects;

			auto Obj = llvm::object::ObjectFile::createObjectFile(ObjBuffer->getMemBufferRef());

			auto Object = llvm::make_unique<llvm::object::OwningBinary<llvm::object::ObjectFile>>(std::move(*Obj), std::move(ObjBuffer));

			//	Add object to object layer
			Objects.push_back(std::move(Object));

			std::unique_ptr<MSSymbolResolver> pResolver = std::make_unique<MSSymbolResolver>(m_pLayout.get());
			m_pObjectLayer->addObjectSet(
				Objects,
				llvm::make_unique<llvm::SectionMemoryManager>(),
				std::move(pResolver));

		}*/
		void Compile(MSScript* pScript, std::unique_ptr<llvm::Module> pModule)
		{
			//	Add the script module as a module set

			std::vector<std::unique_ptr<llvm::Module>> moduleSet;
			
			pModule->setDataLayout(*m_pLayout);
			moduleSet.emplace_back(std::move(pModule));

			
			// This is used to resolve symbols used INSIDE the script (like function calls and such)
			std::unique_ptr<MSSymbolResolver> pResolver = std::make_unique<MSSymbolResolver>(pScript, m_pLayout.get());

			m_pOptimizeLayer->addModuleSet(std::move(moduleSet),
				llvm::make_unique<llvm::SectionMemoryManager>(),
				std::move(pResolver));

			//	Now that the script is actually compiled, update all the exported symbols so it can be used
			UpdateSymbols(pScript);
		}
		void Execute(MSScript* pScript)
		{
			//	Just get the main function of the script and call it.
			std::string mangledName = GetMangledName(pScript->GetName() + "::$");
			void(*entryPoint)() = reinterpret_cast<void(*)()>(m_pOptimizeLayer->findSymbol(mangledName, false).getAddress());

			entryPoint();
		}
		llvm::LLVMContext* GetContext()
		{
			return &m_context;
		}
	};
}