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
  *   FILE     jstack.h
  *   VERSION  2.12
  */

#ifndef __JSTACH_H
#define __JSTACK_H

#ifndef JM_CFG_H
#include <jm/jm_cfg.h>
#endif

JM_NAMESPACE(__JM)

//
// class jstack
// simplified stack optimised for push/peek/pop
// operations, we could use std::stack<std::vector<T>> instead...
//
template <class T, class Allocator JM_DEF_ALLOC_PARAM(T) >
class jstack
{
private:
   typedef JM_MAYBE_TYPENAME REBIND_TYPE(unsigned char, Allocator) alloc_type;
   typedef typename REBIND_TYPE(T, Allocator)::size_type size_type;
   struct node
   {
      node* next;
      T* start;  // first item
      T* end;    // last item
      T* last;   // end of storage
   };
   
   //
   // empty base member optimisation:
   struct data : public alloc_type
   {
      unsigned char buf[sizeof(T)*16];
      data(const Allocator& a) : alloc_type(a){}
   };

   data alloc_inst;
   mutable node* stack;
   mutable node* unused;
   node base;
   size_type block_size;

   void RE_CALL pop_aux()const;
   void RE_CALL push_aux();

public:
   jstack(size_type n = 64, const Allocator& a = Allocator());

   ~jstack();

   node* RE_CALL get_node()
   {
      node* new_stack = (node*)alloc_inst.allocate(sizeof(node) + sizeof(T) * block_size);
      new_stack->last = (T*)(new_stack+1);
      new_stack->start = new_stack->end = new_stack->last + block_size;
      new_stack->next = 0;
      return new_stack;
   }

   bool RE_CALL empty()
   {
      return (stack->start == stack->end) && (stack->next == 0);
   }

   bool RE_CALL good()
   {
      return (stack->start != stack->end) || (stack->next != 0);
   }

   T& RE_CALL peek()
   {
      if(stack->start == stack->end)
         pop_aux();
      return *stack->end;
   }

   const T& RE_CALL peek()const
   {
      if(stack->start == stack->end)
         pop_aux();
      return *stack->end;
   }

   void RE_CALL pop()
   {
      if(stack->start == stack->end)
         pop_aux();
      jm_destroy(stack->end);
      ++(stack->end);
   }

   void RE_CALL pop(T& t)
   {
      if(stack->start == stack->end)
         pop_aux();
      t = *stack->end;
      jm_destroy(stack->end);
      ++(stack->end);
   }

   void RE_CALL push(const T& t)
   {
      if(stack->end == stack->last)
         push_aux();
      --(stack->end);
      jm_construct(stack->end, t);
   }

};

template <class T, class Allocator>
jstack<T, Allocator>::jstack(size_type n, const Allocator& a)
    : alloc_inst(a)
{
  unused = 0;
  block_size = n;
  stack = &base;
  base.last = (T*)alloc_inst.buf;
  base.end = base.start = base.last + 16;
  base.next = 0;
}

template <class T, class Allocator>
void RE_CALL jstack<T, Allocator>::push_aux()
{
   // make sure we have spare space on TOS:
   register node* new_node;
   if(unused)
   {
      new_node = unused;
      unused = new_node->next;
      new_node->next = stack;
      stack = new_node;
   }
   else
   {
      new_node = get_node();
      new_node->next = stack;
      stack = new_node;
   }
}

template <class T, class Allocator>
void RE_CALL jstack<T, Allocator>::pop_aux()const
{
   // make sure that we have a valid item
   // on TOS:
   jm_assert(stack->next);
   register node* p = stack;
   stack = p->next;
   p->next = unused;
   unused = p;
}

template <class T, class Allocator>
jstack<T, Allocator>::~jstack()
{
   node* condemned;
   while(good())
      pop();
   while(unused)
   {
      condemned = unused;
      unused = unused->next;
      alloc_inst.deallocate((unsigned char*)condemned, sizeof(node) + sizeof(T) * block_size);
   }
   while(stack != &base)
   {
      condemned = stack;
      stack = stack->next;
      alloc_inst.deallocate((unsigned char*)condemned, sizeof(node) + sizeof(T) * block_size);
   }
}

JM_END_NAMESPACE

#endif





