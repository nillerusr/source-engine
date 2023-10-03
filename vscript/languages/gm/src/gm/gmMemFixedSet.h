/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMMEMFIXEDSET_H_
#define _GMMEMFIXEDSET_H_

#include "gmMemFixed.h"
#include "gmListDouble.h"


/// \class gmMemFixedSet
/// \brief Fixed memory allocator, Allows varying size allocations using a set of fixed allocators.
class gmMemFixedSet
{
public:

  inline gmMemFixedSet();
  inline ~gmMemFixedSet();

  /// \brief Alloc() an element
  /// \param a_size size of allocation
  inline void* Alloc(int a_size);
  
  /// \brief Free() an element
  /// \param a_size size of allocation
  inline void Free(void* a_ptr);
  
  /// \brief Reset()
  void Reset();
  
  /// \brief ResetAndFreeMemory()
  void ResetAndFreeMemory();

  /// \brief GetMemUsed() will return the number of bytes allocated and returned, not
  ///        the total number of bytes allocated by the memFixedSet object.
  inline unsigned int GetMemUsed() const { return m_memUsed; }

  /// \brief GetSystemMemUsed will return the number of bytes allocated by the system.
  unsigned int GetSystemMemUsed() const;

  /// \brief Presize() will presize the memfixed pools
  void Presize(int a_pool8,
               int a_pool16,
               int a_pool24,
               int a_pool32,
               int a_pool64,
               int a_pool128,
               int a_pool256,
               int a_pool512
              );

  /// \brief PrintStats() will print out the stats for gm memory
  void PrintStats() const;

protected:

  /// \brief Internal data structure for small allocations
  struct SmallMemNode
  {
    int m_size;                                   ///< Allocation size
    char* Data() {return (char*)(this + 1);}      ///< Get ptr after this structure
  };

  /// \brief Internal data structure for large allocations
  struct BigMemNode: public gmListDoubleNode<BigMemNode>
  { 
    int m_size;                                   ///< Allocation size
    char* Data() {return (char*)(this + 1);}      ///< Get ptr after this structure
  };

  void FreeBigAllocs();                           ///< Free the big allocations
  inline SmallMemNode* GetSmallNodeData(void* a_ptr)  { return ((SmallMemNode*)a_ptr)-1; }
  inline BigMemNode* GetBigNodeData(void* a_ptr)  { return ((BigMemNode*)a_ptr)-1; }

  gmMemFixed m_mem8;                              ///< Memory for 8 bytes and less
  gmMemFixed m_mem16;                             ///< Memory for 16 bytes and less
  gmMemFixed m_mem24;                             ///< Memory for 24 bytes and less
  gmMemFixed m_mem32;                             ///< Memory for 32 bytes and less
  gmMemFixed m_mem64;                             ///< Memory for 64 bytes and less
  gmMemFixed m_mem128;                            ///< Memory for 128 bytes and less
  gmMemFixed m_mem256;                            ///< Memory for 256 bytes and less
  gmMemFixed m_mem512;                            ///< Memory for 512 bytes and less
  gmListDouble<BigMemNode> m_bigAllocs;           ///< List holding memory for more than 512 bytes
  int m_memUsed;

};


gmMemFixedSet::gmMemFixedSet()
  : m_mem8(8 + sizeof(SmallMemNode),     64),
    m_mem16(16 + sizeof(SmallMemNode),   64),
    m_mem24(24 + sizeof(SmallMemNode),   64),
    m_mem32(32 + sizeof(SmallMemNode),   64),
    m_mem64(64 + sizeof(SmallMemNode),   32),
    m_mem128(128 + sizeof(SmallMemNode), 32),
    m_mem256(256 + sizeof(SmallMemNode), 16),
    m_mem512(512 + sizeof(SmallMemNode), 16)
{
  m_memUsed = 0;
}


gmMemFixedSet::~gmMemFixedSet()
{
  FreeBigAllocs();
}


void* gmMemFixedSet::Alloc(int a_size)
{
  SmallMemNode* node;

  if (a_size <= 32)
  {
    if (a_size <= 8)
    {
      node = (SmallMemNode*)m_mem8.Alloc();
      node->m_size = 8;
      m_memUsed += 8;
    }
    else if (a_size <= 16)
    {
      node = (SmallMemNode*)m_mem16.Alloc();
      node->m_size = 16;
      m_memUsed += 16;
    }
    else if (a_size <= 24)
    {
      node = (SmallMemNode*)m_mem24.Alloc();
      node->m_size = 24;
      m_memUsed += 24;
    }
    else // if (a_size <= 32)
    {
      GM_ASSERT(a_size <= 32);

      node = (SmallMemNode*)m_mem32.Alloc();
      node->m_size = 32;
      m_memUsed += 32;
    }
  }
  else
  {
    if (a_size <= 64)
    {
      node = (SmallMemNode*)m_mem64.Alloc();
      node->m_size = 64;
      m_memUsed += 64;
    }
    else if (a_size <= 128)
    {
      node = (SmallMemNode*)m_mem128.Alloc();
      node->m_size = 128;
      m_memUsed += 128;
    }
    else if (a_size <= 256)
    {
      node = (SmallMemNode*)m_mem256.Alloc();
      node->m_size = 256;
      m_memUsed += 256;
    }
    else if (a_size <= 512)
    {
      node = (SmallMemNode*)m_mem512.Alloc();
      node->m_size = 512;
      m_memUsed += 512;
    }    
    else
    {
      BigMemNode* bigNode;

      bigNode = (BigMemNode*)GM_NEW( char[a_size + sizeof(*bigNode)] ); // This will be aligned as it calls sys new

      m_bigAllocs.InsertFirst(bigNode);
      bigNode->m_size = a_size;
      m_memUsed += a_size;

      return bigNode->Data();
    }
  }

  return node->Data();
}


void gmMemFixedSet::Free(void* a_ptr)
{
  SmallMemNode* node = GetSmallNodeData(a_ptr);
  int size = node->m_size;

  if (size <= 32)
  {
    if (size == 8)
    {
      m_mem8.Free(node);
      m_memUsed -= 8;
    }
    else if (size == 16)
    {
      m_mem16.Free(node);
      m_memUsed -= 16;
    }
    else if (size == 24)
    {
      m_mem24.Free(node);
      m_memUsed -= 24;
    }
    else // if (size == 32)
    {
      GM_ASSERT(size == 32);

      m_mem32.Free(node);
      m_memUsed -= 32;
    }
  }
  else
  {
    if (size == 64)
    {
      m_mem64.Free(node);
      m_memUsed -= 64;
    }
    else if (size == 128)
    {
      m_mem128.Free(node);
      m_memUsed -= 128;
    }
    else if (size == 256)
    {
      m_mem256.Free(node);
      m_memUsed -= 256;
    }
    else if (size == 512)
    {
      m_mem512.Free(node);
      m_memUsed -= 512;
    }
    else
    {
      BigMemNode* bigNode = GetBigNodeData(a_ptr);
      m_memUsed -= bigNode->m_size;
      m_bigAllocs.Remove(bigNode);
      delete [] (char*) bigNode;
    }
  }
    
  GM_ASSERT(m_memUsed >= 0);
}

#endif // _GMMEMFIXEDSET_H_
