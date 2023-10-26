/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMMEMFIXED_H_
#define _GMMEMFIXED_H_

#include "gmMemChain.h"

/// \class gmMemFixed
/// \brief Fixed memory allocator, wrapper on chain memory allocator to provide memory reuse. 
///        Performance note: use can cause more random memory access
class gmMemFixed
{
public:

  inline gmMemFixed(unsigned int a_elementSize, unsigned int a_growSize = 64);
  inline ~gmMemFixed();

  /// \brief Alloc() an element
  inline void* Alloc();
  
  /// \brief Free() an element
  inline void Free(void* a_ptr);
  
  /// \brief Reset()
  inline void Reset();
  
  /// \brief ResetAndFreeMemory()
  inline void ResetAndFreeMemory();

  /// \brief Presize
  inline void Presize(int a_kbytes)               { m_memChain.Presize(a_kbytes); }

  inline unsigned int GetElementSize();

  /// \brief GetSystemMemUsed will return the number of bytes allocated by the system.
  inline unsigned int GetSystemMemUsed() const { return m_memChain.GetSystemMemUsed(); }

#ifdef GM_DEBUG_BUILD
  /// \brief GetMemUsed()
  inline unsigned int GetMemUsed() const { return m_memUsed; }
#endif // GM_DEBUG_BUILD

protected:

  // Free list node structure to simplify coding of recycle list.
  struct FreeListNode
  {
    FreeListNode * m_next;
  };

  FreeListNode* m_freeList;                  //!< List of memory block we can reuse
  gmMemChain m_memChain;                     //!< The chain memory used to actually allocate chunks

#ifdef GM_DEBUG_BUILD
  int m_memUsed;
#endif // GM_DEBUG_BUILD
};



gmMemFixed::gmMemFixed(unsigned int a_elementSize, unsigned int a_growSize)
  : m_memChain(a_elementSize, a_growSize)
{
  GM_ASSERT(a_elementSize >= sizeof(FreeListNode));
  m_freeList = NULL;
#ifdef GM_DEBUG_BUILD
  m_memUsed = 0;
#endif // GM_DEBUG_BUILD
}



gmMemFixed::~gmMemFixed()
{
  ResetAndFreeMemory();
}



void* gmMemFixed::Alloc()
{
  void* newMemPtr; 
  
  //Is one available on the free list?
  newMemPtr = m_freeList;
  if(m_freeList)
  {
    m_freeList = m_freeList->m_next;
  }
  else
  {
    //No, so get chain to alloc a new one
    newMemPtr = m_memChain.Alloc();
  }

#ifdef GM_DEBUG_BUILD
  m_memUsed += m_memChain.GetElementSize();
#endif // GM_DEBUG_BUILD

#if 0
  // clear new mem pointer to 0xB00BFEED
  int * n = (int *) newMemPtr;
  int c = m_memChain.GetElementSize() / sizeof(int);
  while(c--) *(n++) = 0xB00BFEED;
#endif

  return newMemPtr;
}



void gmMemFixed::Free(void* a_ptr)
{
#if 0
  // make sure a_ptr is not already in list (freeing something twice)
  FreeListNode * node = m_freeList;
  while(node)
  {
    GM_ASSERT(a_ptr != node);
    node = node->m_next;
  }
#endif

  if(a_ptr)
  {
#if 0
    // clear new mem pointer to 0xFEEDFACE
    int * n = (int *) a_ptr;
    int c = m_memChain.GetElementSize() / sizeof(int);
    while(c--) *(n++) = 0xFEEDFACE;
#endif

    //Add pointer to free list so we can reuse it
    ((FreeListNode*)a_ptr)->m_next = m_freeList;
    m_freeList = (FreeListNode*)a_ptr;
#ifdef GM_DEBUG_BUILD
    m_memUsed -= m_memChain.GetElementSize();
    GM_ASSERT(m_memUsed >= 0);
#endif // GM_DEBUG_BUILD
  }
}



void gmMemFixed::ResetAndFreeMemory()
{
  m_freeList = NULL;
#ifdef GM_DEBUG_BUILD
  m_memUsed = 0;
#endif // GM_DEBUG_BUILD
  m_memChain.ResetAndFreeMemory();
}



void gmMemFixed::Reset()
{
  m_freeList = NULL;
#ifdef GM_DEBUG_BUILD
  m_memUsed = 0;
#endif // GM_DEBUG_BUILD
  m_memChain.Reset();
}



unsigned int gmMemFixed::GetElementSize()
{
  return (unsigned int)m_memChain.GetElementSize();
}


#endif // _GMMEMFIXED_H_
