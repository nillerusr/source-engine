/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmMemFixedSet.h"

// Must be last header
#include "memdbgon.h"


void gmMemFixedSet::Reset()
{
  m_mem8.Reset();
  m_mem16.Reset();
  m_mem24.Reset();
  m_mem32.Reset();
  m_mem64.Reset();
  m_mem128.Reset();
  m_mem256.Reset();
  m_mem512.Reset();
  FreeBigAllocs();  //This actually frees the big fellas
}



void gmMemFixedSet::ResetAndFreeMemory()
{
  m_mem8.ResetAndFreeMemory();
  m_mem16.ResetAndFreeMemory();
  m_mem24.ResetAndFreeMemory();
  m_mem32.ResetAndFreeMemory();
  m_mem64.ResetAndFreeMemory();
  m_mem128.ResetAndFreeMemory();
  m_mem256.ResetAndFreeMemory();
  m_mem512.ResetAndFreeMemory();
  FreeBigAllocs();
}



unsigned int gmMemFixedSet::GetSystemMemUsed() const
{
  unsigned int total = 0;
  total += m_mem8.GetSystemMemUsed();
  total += m_mem16.GetSystemMemUsed();
  total += m_mem24.GetSystemMemUsed();
  total += m_mem32.GetSystemMemUsed();
  total += m_mem64.GetSystemMemUsed();
  total += m_mem128.GetSystemMemUsed();
  total += m_mem256.GetSystemMemUsed();
  total += m_mem512.GetSystemMemUsed();

  
  BigMemNode* curNode = m_bigAllocs.GetFirst();
  while(m_bigAllocs.IsValid(curNode))
  {
    total += curNode->m_size;
    curNode = m_bigAllocs.GetNext(curNode);
  }
  return total;
}


void gmMemFixedSet::PrintStats() const
{
#ifdef GM_DEBUG_BUILD

  int used;
  const char * msg = "%d btye pool uses %dk of memory, thats %d allocations\n";

  used = m_mem8.GetSystemMemUsed();
  GM_PRINTF(msg, 8, used / 1024, used / 8);

  used = m_mem16.GetSystemMemUsed();
  GM_PRINTF(msg, 16, used / 1024, used / 16);

  used = m_mem24.GetSystemMemUsed();
  GM_PRINTF(msg, 24, used / 1024, used / 24);

  used = m_mem32.GetSystemMemUsed();
  GM_PRINTF(msg, 32, used / 1024, used / 32);

  used = m_mem64.GetSystemMemUsed();
  GM_PRINTF(msg, 64, used / 1024, used / 64);

  used = m_mem128.GetSystemMemUsed();
  GM_PRINTF(msg, 128, used / 1024, used / 128);

  used = m_mem256.GetSystemMemUsed();
  GM_PRINTF(msg, 256, used / 1024, used / 256);

  used = m_mem512.GetSystemMemUsed();
  GM_PRINTF(msg, 512, used / 1024, used / 512);

#endif //GM_DEBUG_BUILD
}


void gmMemFixedSet::Presize(int a_pool8,
                            int a_pool16,
                            int a_pool24,
                            int a_pool32,
                            int a_pool64,
                            int a_pool128,
                            int a_pool256,
                            int a_pool512
                            )
{
  if( a_pool8 ) { m_mem8.Presize(a_pool8); }
  if( a_pool16 ) { m_mem16.Presize(a_pool16); }
  if( a_pool24 ) { m_mem24.Presize(a_pool24); }
  if( a_pool32 ) { m_mem32.Presize(a_pool32); }
  if( a_pool64 ) { m_mem64.Presize(a_pool64); }
  if( a_pool128 ) { m_mem128.Presize(a_pool128); }
  if( a_pool256 ) { m_mem256.Presize(a_pool256); }
  if( a_pool512 ) { m_mem512.Presize(a_pool512); }
}


void gmMemFixedSet::FreeBigAllocs()
{
  BigMemNode* curNode = m_bigAllocs.GetFirst();
  while(m_bigAllocs.IsValid(curNode))
  {
    BigMemNode* nodeToDelete = curNode;
    curNode = m_bigAllocs.GetNext(curNode);

    delete [] (char*)nodeToDelete;
  }
  m_bigAllocs.RemoveAll();
}

