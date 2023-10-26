/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmMemChain.h"
#include "gmMem.h"

// Must be last header
#include "memdbgon.h"


gmMemChain::gmMemChain(unsigned int a_elementSize, unsigned int a_numElementsInChunk)
{
  m_chunkSize = a_numElementsInChunk * a_elementSize;
  m_elementSize = a_elementSize;
  m_rootChunk = NULL;
  m_currentChunk = NULL;
}



gmMemChain::~gmMemChain()
{
  FreeChunks();
}



void gmMemChain::FreeChunks()
{
  if(m_rootChunk)
  {
    MemChunk *curChunk;

    curChunk = m_rootChunk;
    while(curChunk)
    {
      MemChunk *chunkToFree;
      
      chunkToFree = curChunk;
      curChunk = curChunk->m_nextChunk;
      
      delete [] (char*)chunkToFree;
    }
  }
}



void gmMemChain::ResetAndFreeMemory()
{
  //Actually free the memory
  FreeChunks();

  //Clear pointers to chunk chain
  m_rootChunk = NULL;
  m_currentChunk = NULL;
}



void gmMemChain::Presize(int a_kbytes)
{
  int numChunks = ((a_kbytes + 1) * 1024) / m_chunkSize;
  MemChunk *curChunk;
  curChunk = m_rootChunk;
  while(curChunk)
  {
    --numChunks;
    curChunk = curChunk->m_nextChunk;
  }
  MemChunk * currentChunk = m_currentChunk;
  while(numChunks > 0)
  {
    NewChunk();
    --numChunks;
  }
  if(currentChunk)
  {
    m_currentChunk = currentChunk;
  }
}


gmMemChain::MemChunk* gmMemChain::NewChunk()
{
  MemChunk* newChunk;

  //Is an unused chunk already allocated?
  if(m_currentChunk && m_currentChunk->m_nextChunk)
  {
    newChunk = m_currentChunk->m_nextChunk;
    newChunk->m_curAddress = newChunk->m_minAddress;    
    m_currentChunk = newChunk;
  }
  else //No, allocate a new one
  {
    char * mem = (char *) GM_NEW( char[sizeof(MemChunk) + m_chunkSize] );
    newChunk = (MemChunk *) mem;
  
    //Allocate memory and set address space
    newChunk->m_minAddress = mem + sizeof(MemChunk);
    newChunk->m_lastAddress = (void*)((unsigned int)newChunk->m_minAddress + m_chunkSize);
    newChunk->m_curAddress = newChunk->m_minAddress;

    //Link new chunk to chain
    newChunk->m_lastChunk = m_currentChunk;
    newChunk->m_nextChunk = NULL;
    if(m_currentChunk)
    {
      m_currentChunk->m_nextChunk = newChunk;
    }
    m_currentChunk = newChunk;
    if(!m_rootChunk)
    {
      m_rootChunk = m_currentChunk;
    }
  }
  return newChunk;
}



void* gmMemChain::Alloc()
{
  void *retPtr;
  
  //Allocate first chunk if none exist
  if(!m_rootChunk)
  {
    NewChunk();
  }

  //Advance ptr, allocate new chunk if necessary
  if(((gmuptr)m_currentChunk->m_curAddress + m_elementSize) > (gmuptr)m_currentChunk->m_lastAddress)
  {
    NewChunk();    
  }
  retPtr = m_currentChunk->m_curAddress;
  m_currentChunk->m_curAddress = (void*)((gmuptr)m_currentChunk->m_curAddress + m_elementSize);

  return retPtr;
}


void* gmMemChain::AllocBytes(unsigned int a_numBytes, unsigned int a_alignNumBytes)
{
  GM_ASSERT((a_numBytes + a_alignNumBytes) <= m_chunkSize); // Chunk size is too small to alloc that many elements at once and should be increased

  void *retPtr;
  
  //Allocate first chunk if none exist
  if(!m_rootChunk)
  {
    NewChunk();
  }

  //Align and record the pointer.
  retPtr = _gmAlignMem(m_currentChunk->m_curAddress, a_alignNumBytes);

  //Create new chunk if not enough memory in this one.
  if(((gmuptr)retPtr + a_numBytes) > (gmuptr)m_currentChunk->m_lastAddress)
  {
    NewChunk();    
    
    //Align and record the pointer.
    retPtr = _gmAlignMem(m_currentChunk->m_curAddress, a_alignNumBytes);
  }
  
  //Advance memory ptr for next allocation.
  m_currentChunk->m_curAddress = (void*)((gmuptr)retPtr + a_numBytes);

  return retPtr;
}



void* gmMemChain::Alloc(unsigned int a_numElementsToAlloc)
{
  unsigned int allocSize;
  void *retPtr;
  
  //Allocate first chunk if none exist
  if(!m_rootChunk)
  {
    NewChunk();
  }

  //Advance ptr, allocate new chunk if necessary
  allocSize = a_numElementsToAlloc * m_elementSize;

  GM_ASSERT(allocSize <= m_chunkSize); // Chunk size is too small to alloc that many elements at once and should be increased

  if(((unsigned int)m_currentChunk->m_curAddress + allocSize) > (unsigned int)m_currentChunk->m_lastAddress)
  {
    NewChunk();    
  }
  retPtr = m_currentChunk->m_curAddress;
  m_currentChunk->m_curAddress = (void*)((unsigned int)m_currentChunk->m_curAddress + allocSize);

  return retPtr;
}



void gmMemChain::Reset()
{
  m_currentChunk = m_rootChunk;
  if(m_currentChunk)
  {
    m_currentChunk->m_curAddress = m_currentChunk->m_minAddress;
  }
}



unsigned int gmMemChain::GetSystemMemUsed() const
{
  MemChunk * chunk = m_rootChunk;
  unsigned int total = 0;

  while(chunk)
  {
    total += m_chunkSize;
    chunk = chunk->m_nextChunk;
  }

  return total;
}

