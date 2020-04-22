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
  *   FILE     re_lst.h
  *   VERSION  2.12
  *   This is an internal header file, do not include directly.
  *   re_list support class, for regular
  *   expression library.
  */

#ifndef RE_LST_H
#define RE_LST_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

#include <new.h>

JM_NAMESPACE(__JM)

template <class T, class Allocator>
class re_list
{
public:
   struct node
   {
      node* next;
      T t;
      node(const T& o) : t(o) {}
   };
public:
   class iterator
   {
      node* pos;
   public:
      iterator() { pos = 0; }
      ~iterator() {}
      iterator(const iterator& i) { pos = i.pos; }
      iterator(node* n) { pos = n; }
      iterator& operator=(const iterator& i)
      {
         pos = i.pos;
         return *this;
      }
      bool operator==(iterator& i)
      {
         return pos == i.pos;
      }
      bool operator!=(iterator& i)
      {
         return pos != i.pos;
      }
      T& operator*() { return pos->t; }
      iterator& operator++()
      {
         pos = pos->next;
         return *this;
      }
      iterator operator++(int)
      {
         iterator t(*this);
         pos = pos->next;
         return t;
      }
      const node* tell()const
      {
         return pos;
      }
   };

   class const_iterator
   {
      const node* pos;
   public:
      const_iterator() { pos = 0; }
      ~const_iterator() {}
      const_iterator(const const_iterator& i) { pos = i.pos; }
      const_iterator(const iterator& i) { pos = i.tell(); }
      const_iterator(const node* n) { pos = n; }
      const_iterator& operator=(const iterator& i)
      {
         pos = i.tell();
         return *this;
      }
      const_iterator& operator=(const const_iterator& i)
      {
         pos = i.pos;
         return *this;
      }
      bool operator==(const_iterator& i)
      {
         return pos == i.pos;
      }
      bool operator!=(const_iterator& i)
      {
         return pos != i.pos;
      }
      const T& operator*() { return pos->t; }
      const_iterator& operator++()
      {
         pos = pos->next;
         return *this;
      }
      const_iterator operator++(int)
      {
         const_iterator t(*this);
         pos = pos->next;
         return t;
      }
   };
private:
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(node, Allocator) node_alloc;

   struct data : public node_alloc
   {
      node* first;
      data(const Allocator& a) : node_alloc(a), first(0) {}
   };
   data alloc_inst;

public:
   re_list(const Allocator& a = Allocator()) : alloc_inst(a) {}
   ~re_list() { clear(); }
   iterator RE_CALL begin() { return iterator(alloc_inst.first); }
   iterator RE_CALL end() { return iterator(0); }
   const_iterator RE_CALL begin()const { return const_iterator(alloc_inst.first); }
   const_iterator RE_CALL end()const { return const_iterator(0); }
   void RE_CALL add(const T& t)
   {
      node* temp;
      temp = alloc_inst.allocate(1);
#ifndef JM_NO_EXCEPTIONS
      try{
#endif
      alloc_inst.construct(temp, t);
#ifndef JM_NO_EXCEPTIONS
      }catch(...){ alloc_inst.deallocate(temp, 1); throw; }
#endif
      temp->next = alloc_inst.first;
      alloc_inst.first = temp;
   }
   void RE_CALL clear();
};

template <class T, class Allocator>
void RE_CALL re_list<T, Allocator>::clear()
{
   node* temp;
   while(alloc_inst.first)
   {
      temp = alloc_inst.first;
      alloc_inst.first = alloc_inst.first->next;
      alloc_inst.destroy(temp);
      alloc_inst.deallocate(temp, 1);
   }
}


JM_END_NAMESPACE

#endif


