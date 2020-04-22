//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*
 *
 * Copyright (c) 1998-9
 * Dr John Maddock
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Dr John Maddock makes no representations
 * about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 */

 /*
  *   FILE     re_raw.h
  *   VERSION  2.12
  */
  
#ifndef RE_RAW_H
#define RE_RAW_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

JM_NAMESPACE(__JM)

union padding
{
   void* p;
   unsigned int i;
};


//
// class raw_storage
// basically this is a simplified vector<unsigned char>
// this is used by reg_expression for expression storage
//

template <class Allocator>
class raw_storage
{
public:
   typedef Allocator alloc_type;
   typedef typename REBIND_TYPE(unsigned char, alloc_type)::size_type size_type;
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(unsigned char, alloc_type) alloc_inst_type;
   typedef typename REBIND_TYPE(unsigned char, alloc_type)::pointer pointer;
private:
   //
   // empty member optimisation:
   struct alloc_data : public alloc_inst_type
   {
      pointer last;
      alloc_data(const Allocator& a) : alloc_inst_type(a){}
   } alloc_inst;
   pointer start, end;
public:

   raw_storage(const Allocator& a = Allocator());
   raw_storage(size_type n, const Allocator& a = Allocator());

   ~raw_storage()
   {
      alloc_inst.deallocate(start, (alloc_inst.last - start));
   }

   void RE_CALL resize(size_type n);
   
   void* RE_CALL extend(size_type n)
   {
      if(size_type(alloc_inst.last - end) < n)
         resize(n + (end - start));
      register void* result = end;
      end += n;
      return result;
   }

   void* RE_CALL insert(size_type pos, size_type n);

   size_type RE_CALL size()
   {
      return end - start;
   }

   size_type RE_CALL capacity()
   {
      return alloc_inst.last - start;
   }

   void* RE_CALL data()const
   {
      return start;
   }

   size_type RE_CALL index(void* ptr)
   {
      return (unsigned char*)ptr - start;
   }

   void RE_CALL clear()
   {
      end = start;
   }

   void RE_CALL align()
   {
      // move end up to a boundary:
      end = (unsigned char*)((long)(end + sizeof(padding) - 1) & ~((long)sizeof(padding) - 1));
   }

   Allocator RE_CALL allocator()const;
};

template <class Allocator>
CONSTRUCTOR_INLINE raw_storage<Allocator>::raw_storage(const Allocator& a)
  : alloc_inst(a)
{
  start = end = alloc_inst.allocate(1024);
  alloc_inst.last = start + 1024;
}

template <class Allocator>
CONSTRUCTOR_INLINE raw_storage<Allocator>::raw_storage(size_type n, const Allocator& a)
  : alloc_inst(a)
{
  start = end = alloc_inst.allocate(n);
  alloc_inst.last = start + n;
}

template <class Allocator>
Allocator RE_CALL raw_storage<Allocator>::allocator()const
{
  return alloc_inst;
}

template <class Allocator>
void RE_CALL raw_storage<Allocator>::resize(size_type n)
{
   register size_type newsize = (alloc_inst.last - start) * 2;
   register size_type datasize = end - start;
   if(newsize < n)
      newsize = n;
   // extend newsize to WORD/DWORD boundary:
   newsize = (newsize + (sizeof(padding) - 1)) & ~(sizeof(padding) - 1);

   // allocate and copy data:
   register unsigned char* ptr = alloc_inst.allocate(newsize);
   memcpy(ptr, start, datasize);

   // get rid of old buffer:
   alloc_inst.deallocate(start, (alloc_inst.last - start));

   // and set up pointers:
   start = ptr;
   end = ptr + datasize;
   alloc_inst.last = ptr + newsize;
}

template <class Allocator>
void* RE_CALL raw_storage<Allocator>::insert(size_type pos, size_type n)
{
   jm_assert(pos <= size_type(end - start));
   if(size_type(alloc_inst.last - end) < n)
      resize(n + (end - start));
   register void* result = start + pos;
   memmove(start + pos + n, start + pos, (end - start) - pos);
   end += n;
   return result;
}

JM_END_NAMESPACE

#endif


