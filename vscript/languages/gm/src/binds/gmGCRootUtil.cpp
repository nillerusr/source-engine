//
// gmGCRootUtil.cpp
//

#include "gmGCRoot.h"
#include "gmGCRootUtil.h"

// Must be last header
#include "memdbgon.h"



gmgcrMemFixed::gmgcrMemFixed()
   : gmMemFixed( sizeof(gmgcrHolder) )
{
 
}
  
gmgcrHolder* gmgcrMemFixed::Alloc()
{
  gmgcrHolder* ptr = (gmgcrHolder*)gmMemFixed::Alloc();
  gmConstructElement<gmgcrHolder>(ptr);
  return ptr;
}

void gmgcrMemFixed::Free(gmgcrHolder* a_ptr)
{
  if( a_ptr )
  {
    gmDestructElement(a_ptr);
    gmMemFixed::Free(a_ptr);
  }
}
