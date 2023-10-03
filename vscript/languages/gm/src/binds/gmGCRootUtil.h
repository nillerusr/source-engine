#ifndef GMGCROOTUTIL_H
#define GMGCROOTUTIL_H

//
// gmGCRootUtil.h
//

#include "gmThread.h"

//
// This file has containers and utilities for implementing gmGCRoot
// You may replace these implementations with your own containers with relative ease.
//

// Fwd decls
class gmGCRootManager;
class gmgcrHolder;


// STL version
#include <map>
#include <vector>

template<typename KEY, typename VALUE>
class gmgcrMap 
{
  typename std::map<KEY, VALUE> m_map;

public:
  
  // Iterator, derive or contain native container iterator
  class Iterator 
  {
  public:
    typename std::map<KEY, VALUE>::iterator m_it;
  };
  
  // Get Value at unique Key, return true if found
  int GetAt(KEY a_key, VALUE& a_value)
  {
    typename std::map<KEY, VALUE>::iterator it = m_map.find(a_key);
    if( it != m_map.end() )
    {
      a_value = (*it).second;
      return true;
    }
    return false;
  };

  // Set Value and unique Key
  void SetAt(KEY a_key, VALUE& a_value)
  {
    m_map[a_key] = a_value;
  }
  
  // Remove pair at unique Key
  void RemoveAt(KEY a_key)
  {
    m_map.erase(a_key);
  }
  
  // Iteration Get first pair
  void GetFirst(Iterator& a_it)
  {
    a_it.m_it = m_map.begin();
  }
  
  // Iteration Get next pair
  void GetNext(Iterator& a_it)
  {
    if( a_it.m_it != m_map.end() )
    {
      ++a_it.m_it;
    }
  }
  
  // Iteration Is iterator valid.  Return false if not.
  int IsNull(Iterator& a_it)
  {
    if( a_it.m_it == m_map.end() )
    {
      return true;
    }
    return false;
  }
  
  // Iteration Get Value from iterator, return false if failed
  int GetValue(Iterator& a_it, VALUE& a_value)
  {
    if( a_it.m_it != m_map.end() )
    {
      a_value = (*a_it.m_it).second;
      return true;
    }
    return false;
  }

};


// Array, derive or contain array style container
template<typename TYPE>
class gmgcrArray
{
  typename std::vector<TYPE> m_array;

public:

  // Set initial array size and any grow / presize for efficiency
  void Init(int a_initialSize, int a_growSize)
  {
    SetSize(a_initialSize);
  }
  
  // Set array size, and grow size
  void SetSize(int a_size)
  {
    m_array.resize(a_size);
  }

  // Return number of elements in array
  int GetSize()
  {
    return m_array.size();
  }

  // Return last index eg. GetSize()-1
  int GetLastIndex()
  {
    return GetSize() - 1;
  }

  // Remove element at index
  void RemoveAt(int a_index)
  {
    m_array.erase(m_array.begin() + a_index);
  }

  // Access element at index (by reference)
  TYPE& operator[](int a_index)
  {
    return m_array[a_index];
  }

  // Add (default) element to array, returning index of new element
  int AddEmpty()
  {
    int oldCount = GetSize(); // old count becomes new index
    SetSize(oldCount+1);
    return oldCount;
  }

};


// Allocator, derive or contain frame style allocator
class gmgcrMemFixed : public gmMemFixed 
{
public:
  
  gmgcrMemFixed();
  
  // Allocate and construct
  gmgcrHolder* Alloc();
  
  // Destruct and free
  void Free(gmgcrHolder* a_ptr);
  
};

#endif  //GMGCROOTUTIL_H