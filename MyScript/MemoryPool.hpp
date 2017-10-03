#pragma once
#include "stdafx.h"

/*
Convenient way to construct objects allocated by the memory pool
*/
namespace MyScript
{
	template<typename T>
	class BlockConstructor
	{
		T* m_ptr = nullptr;
		int m_arraySize = 1;
	public:
		BlockConstructor(T* ptr, int arraySize = 1)
			: m_ptr(ptr),
			m_arraySize(arraySize)
		{

		}

		template <typename... Args>
		T* operator()(Args... args)
		{
			for (int i = 0; i < m_arraySize; ++i)
				new(&m_ptr[i]) T(args...);

			return m_ptr;
		}

		operator T*()
		{
			return m_ptr;
		}
	};

	template <class T>
	struct PoolAllocator;

	class MemoryPool
		: mystd::NonCopyable
	{
		struct MemoryBlock
		{
			unsigned char*	ptr;
			int				usedSize;
			int				size;
		};

		const int m_blockSize = 4096;
		std::vector<MemoryBlock> m_blocks;

		unsigned char*	m_lastAllocPtr = nullptr;
		int				m_lastAllocSize = 0;
	public:
		MemoryPool(int blockSize = 4096)
			: m_blockSize(blockSize)
		{

		}
		~MemoryPool()
		{
			for (auto& block : m_blocks)
				delete[] block.ptr;
		}
		template <typename T>
		BlockConstructor<T> Alloc(int size = 1)
		{
			T* ptr = nullptr;

			int totalSize = size * sizeof(T);
			if (m_blocks.empty() || (m_blocks.back().size - m_blocks.back().usedSize < totalSize))
			{
				m_blocks.push_back(MemoryBlock());
				m_blocks.back().ptr = new unsigned char[m_blockSize];
				m_blocks.back().size = m_blockSize;
				m_blocks.back().usedSize = totalSize;
				ptr = reinterpret_cast<T*>(m_blocks.back().ptr);
			}
			else
			{
				ptr = reinterpret_cast<T*>(m_blocks.back().ptr + m_blocks.back().usedSize);
				m_blocks.back().usedSize += totalSize;
			}

			m_lastAllocPtr = reinterpret_cast<unsigned char*>(ptr);
			m_lastAllocSize = totalSize;

			return BlockConstructor<T>(ptr, size);
		}

		template <typename T>
		void Free(T* ptr)
		{
			if (reinterpret_cast<unsigned char*>(ptr) == m_lastAllocPtr)
			{
				//	No need to check for overlapping blocks cause it cannot happen
				m_blocks.back().usedSize -= m_lastAllocSize;
				m_lastAllocPtr = nullptr;
				m_lastAllocSize = 0;
			}
		}

		template <typename T>
		PoolAllocator<T> GetAllocator()
		{
			return PoolAllocator<T>(this);
		}
	};

	/*
	Pool allocator for STL classes
	*/
	template <class T>
	struct PoolAllocator
	{
		MemoryPool* m_pMemoryPool = nullptr;
	public:
		typedef T value_type;

		PoolAllocator()
		{

		}
		PoolAllocator(MemoryPool* pMemoryPool)
			: m_pMemoryPool(pMemoryPool)
		{

		}

		template <class U>
		PoolAllocator(const PoolAllocator<U>& a)
			: m_pMemoryPool(a.m_pMemoryPool)
		{}

		T* allocate(std::size_t n)
		{
			return m_pMemoryPool->Alloc<T>(n);
		}
		void deallocate(T* p, std::size_t)
		{
			m_pMemoryPool->Free(p);
		}
	};
	template <class T, class U>
	bool operator==(const PoolAllocator<T>&, const PoolAllocator<U>&) { return true; }
	template <class T, class U>
	bool operator!=(const PoolAllocator<T>&, const PoolAllocator<U>&) { return false; }

}