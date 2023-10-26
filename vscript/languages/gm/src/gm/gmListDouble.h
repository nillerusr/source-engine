/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMLISTDOUBLEI_H_
#define _GMLISTDOUBLEI_H_

#include "gmConfig.h"
#include "gmIterator.h"

template <class T>
class gmListDouble;

/// \class gmListDoubleNode
/// \brief intrusive node, must inherit this class templated to your class
template <class T>
class gmListDoubleNode
{
public:

  inline gmListDoubleNode() {}

  /// \brief Nullify()
  inline void Nullify()
  {
    m_next = m_prev = NULL;
  }

  /// \brief Remove from list.  Allows node to unlink from unknown list.
  inline void RemoveAndNullify()
  {
    m_next->m_prev = m_prev;
    m_prev->m_next = m_next;
    this->Nullify();
  }

  /// \brief Remove from list, must be in list.
  inline void Remove()
  {
    m_next->m_prev = m_prev;
    m_prev->m_next = m_next;
  }

  /// \brief Returns true if node is linked to list, assumes the list is being managed correctly with nullify()
  inline bool IsLinked() const
  {
    return (m_next != NULL);
  }
    
private:

  gmListDoubleNode * m_next;
  gmListDoubleNode * m_prev;
  
  friend class gmListDouble<T>;
};


/// \class gmListDoubleNodeObj
/// \brief List node that stores object pointer.
/// Allows node to be contained rather than be inherited.
template<class T>
class gmListDoubleNodeObj : public gmListDoubleNode< gmListDoubleNodeObj<T> >
{
public:

  /// \brief Construct, Nullifies link and object
  inline gmListDoubleNodeObj()
  {
    this->Nullify();
    SetObject(NULL);
  }

  /// \brief Construct, Nullifies link and set object
  inline gmListDoubleNodeObj(T* a_obj)
  {
    this->Nullify();
    SetObject(a_obj);
  }

  inline T* GetObject() { return m_object; }
  inline void SetObject(T* a_obj) { m_object = a_obj; }

private:

  T* m_object;
};

/// \class gmListDouble
/// \brief Templated intrusive doubly linked list using a sentinal node
template <class T>
class gmListDouble
{
public:

  /// \class Iterator
  class Iterator
  {
  public:

    GM_INCLUDE_ITERATOR_KERNEL(T)

    inline Iterator() { m_node = NULL; m_list = NULL; }
    inline Iterator(T * a_node, const gmListDouble * a_list) { m_node = a_node; m_list = a_list; }

    inline void Inc() { m_node = m_list->GetNext(m_node); }
    inline void Dec() { m_node = m_list->GetPrev(m_node); }
    inline T* Resolve() { return m_node; }
    inline const T* Resolve() const { return m_node; }
    inline bool IsValid() const { return (m_list && m_list->IsValid(m_node)); }

  private:

    T * m_node;
    const gmListDouble * m_list;
  };

  // methods

  inline gmListDouble();
  inline ~gmListDouble();

  inline void InsertAfter(T * a_cursor, T * a_elem);
  inline void InsertBefore(T * a_cursor, T * a_elem);
  inline void InsertLast(T * a_elem);
  inline void InsertFirst(T * a_elem);
  inline void Remove(T * a_elem);
  inline void RemoveAndNullify(T * a_elem);
  inline T * Remove(Iterator &a_it); // iterator is incremented
  inline T * RemoveAndNullify(Iterator &a_it); // iterator is incremented
  inline T * RemoveGetNext(T * a_elem);
  inline T * RemoveLast(); // returns NULL if no elements left in list
  inline T * RemoveLastAndNullify();
  inline T * RemoveFirst(); // returns NULL if no elements left in list
  inline T * RemoveFirstAndNullify();
  inline void RemoveAll();
  inline void RemoveAndNullifyAll();
  inline void RemoveAndDeleteAll();
  inline int Count() const;
  inline bool IsValid(const T* a_elem) const { return (a_elem != &m_sentinel); }
  inline T* GetFirst() const { return (T*) m_sentinel.m_next; }
  inline T* GetLast() const { return (T*) m_sentinel.m_prev; }
  inline T* GetNext(const T* a_elem) const { return (T*) a_elem->gmListDoubleNode<T>::m_next; }
  inline T* GetPrev(const T* a_elem) const { return (T*) a_elem->gmListDoubleNode<T>::m_prev; }
  inline bool IsEmpty() const { return (m_sentinel.m_next == &m_sentinel); }
  inline Iterator First(void) const { return Iterator(static_cast<T*>(m_sentinel.m_next), this); }
  inline Iterator Last(void) const { return Iterator(static_cast<T*>(m_sentinel.m_prev), this); }
  
private:

  gmListDoubleNode<T> m_sentinel;
};

//
// implementation of gmListDouble
//

template <class T>
inline gmListDouble<T>::gmListDouble()
{
  m_sentinel.m_next = &m_sentinel;
  m_sentinel.m_prev = &m_sentinel;
}


template <class T>
inline gmListDouble<T>::~gmListDouble()
{
}


template <class T>
inline void gmListDouble<T>::InsertAfter(T * a_cursor, T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_next = a_cursor->gmListDoubleNode<T>::m_next;
  a_elem->gmListDoubleNode<T>::m_prev = a_cursor;
  a_elem->gmListDoubleNode<T>::m_prev->m_next = a_elem;
  a_elem->gmListDoubleNode<T>::m_next->m_prev = a_elem;
}


template <class T>
inline void gmListDouble<T>::InsertBefore(T * a_cursor, T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_next = a_cursor;
  a_elem->gmListDoubleNode<T>::m_prev = a_cursor->gmListDoubleNode<T>::m_prev;
  a_elem->gmListDoubleNode<T>::m_prev->m_next = a_elem;
  a_elem->gmListDoubleNode<T>::m_next->m_prev = a_elem;
}


template <class T>
inline void gmListDouble<T>::InsertLast(T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_prev = m_sentinel.m_prev;
  a_elem->gmListDoubleNode<T>::m_next = &m_sentinel;
  m_sentinel.m_prev->m_next = a_elem;
  m_sentinel.m_prev = a_elem;
}


template <class T>
inline void gmListDouble<T>::InsertFirst(T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_prev = &m_sentinel;
  a_elem->gmListDoubleNode<T>::m_next = m_sentinel.m_next;
  m_sentinel.m_next->m_prev = a_elem;
  m_sentinel.m_next = a_elem;
}


template <class T>
inline void gmListDouble<T>::Remove(T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_next->m_prev = a_elem->gmListDoubleNode<T>::m_prev;
  a_elem->gmListDoubleNode<T>::m_prev->m_next = a_elem->gmListDoubleNode<T>::m_next;
}


template <class T>
inline void gmListDouble<T>::RemoveAndNullify(T * a_elem)
{
  a_elem->gmListDoubleNode<T>::m_next->m_prev = a_elem->gmListDoubleNode<T>::m_prev;
  a_elem->gmListDoubleNode<T>::m_prev->m_next = a_elem->gmListDoubleNode<T>::m_next;
  a_elem->gmListDoubleNode<T>::m_next = a_elem->gmListDoubleNode<T>::m_prev = NULL;
}


template <class T>
inline T * gmListDouble<T>::Remove(Iterator &a_it)
{
  T* node = a_it.Resolve();
  a_it.Inc();
  Remove(node);
  return node;
}


template <class T>
inline T * gmListDouble<T>::RemoveAndNullify(Iterator &a_it)
{
  T* node = a_it.Resolve();
  a_it.Inc();
  RemoveAndNullify(node);
  return node;
}


template <class T>
inline T * gmListDouble<T>::RemoveGetNext(T * a_elem)
{
  gmListDoubleNode<T> * next = a_elem->gmListDoubleNode<T>::m_next;

  a_elem->gmListDoubleNode<T>::m_next->m_prev = a_elem->gmListDoubleNode<T>::m_prev;
  a_elem->gmListDoubleNode<T>::m_prev->m_next = a_elem->gmListDoubleNode<T>::m_next;
  a_elem->gmListDoubleNode<T>::m_next = a_elem->gmListDoubleNode<T>::m_prev = NULL;

  return static_cast<T *>(next);
}


template <class T>
inline T * gmListDouble<T>::RemoveLast()
{
  if(m_sentinel.m_prev == &m_sentinel) return NULL;
  
  gmListDoubleNode<T> * temp;
  temp = m_sentinel.m_prev;
  temp->m_next->m_prev = temp->m_prev;
  temp->m_prev->m_next = temp->m_next;
  return static_cast<T*>(temp);
}


template <class T>
inline T * gmListDouble<T>::RemoveLastAndNullify()
{
  if(m_sentinel.m_prev == &m_sentinel) return NULL;

  gmListDoubleNode<T> * temp;
  temp = m_sentinel.m_prev;

  temp->m_next->m_prev = temp->m_prev;
  temp->m_prev->m_next = temp->m_next;
  temp->m_next = temp->m_prev = NULL;

  return static_cast<T*>(temp);
}


template <class T>
inline T * gmListDouble<T>::RemoveFirst()
{
  if(m_sentinel.m_next == &m_sentinel) return NULL;

  gmListDoubleNode<T> * temp;
  temp = m_sentinel.m_next;
  temp->m_next->m_prev = temp->m_prev;
  temp->m_prev->m_next = temp->m_next;
  return static_cast<T*>(temp);
}


template <class T>
inline T * gmListDouble<T>::RemoveFirstAndNullify()
{
  if(m_sentinel.m_next == &m_sentinel) return NULL;

  gmListDoubleNode<T> * temp;
  temp = m_sentinel.m_next;
  temp->m_next->m_prev = temp->m_prev;
  temp->m_prev->m_next = temp->m_next;
  temp->m_next = temp->m_prev = NULL;
  return static_cast<T*>(temp);
}


template <class T>
inline void gmListDouble<T>::RemoveAll()
{
  m_sentinel.m_next = &m_sentinel;
  m_sentinel.m_prev = &m_sentinel;
}


template <class T>
inline void gmListDouble<T>::RemoveAndNullifyAll()
{
  gmListDoubleNode<T> * node = m_sentinel.m_next, * temp;
  while(node != &m_sentinel)
  {
    temp = node;
    node = node->m_next;
    temp->m_next = temp->m_prev = NULL;
  }
  m_sentinel.m_next = &m_sentinel;
  m_sentinel.m_prev = &m_sentinel;
}


template <class T>
inline void gmListDouble<T>::RemoveAndDeleteAll()
{
  gmListDoubleNode<T> * node = m_sentinel.m_next, * temp;
  while(node != &m_sentinel)
  {
    temp = node;
    node = node->m_next;
    delete ((T *) temp);
  }
  m_sentinel.m_next = &m_sentinel;
  m_sentinel.m_prev = &m_sentinel;
}


template <class T>
inline int gmListDouble<T>::Count() const
{
  int count = 0;
  gmListDoubleNode<T> * node = m_sentinel.m_next;
  while(node != &m_sentinel)
  {
    node = node->m_next;
    ++count;
  }
  return count;
}


#endif // _GMLISTDOUBLEI_H_
