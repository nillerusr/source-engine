/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMHASH_H_
#define _GMHASH_H_

#include "gmConfig.h"
#include "gmIterator.h"

#define TMPL template<class KEY, class T, class HASHER>
#define QUAL gmHash<KEY, T, HASHER>
#define NQUAL gmHashNode<KEY, T, HASHER>

TMPL class gmHash;
class gmDefaultHasher;

/// \class gmHashNode
/// \brief inherit gmHashNode
template<class KEY, class T, class HASHER = gmDefaultHasher>
class gmHashNode
{
public:
  inline gmHashNode() {}

  /// \brief Return whatever is the 'key' for this type.  Used by gmHash
  //const KEY& GetKey() const = 0;

private:

  T * m_next;
  friend class QUAL;
};

/// \class gmHash
/// \brief templated intrusive hash class
///        HASHER must provide static gmuint ::Hash(const KEY &a_key) and int ::Compare(const KEY &a_key, const KEY &a_key)
template<class KEY, class T, class HASHER = gmDefaultHasher>
class gmHash
{
public:

  /// \class Iterator
  class Iterator
  {
  public:

    GM_INCLUDE_ITERATOR_KERNEL(T)

    inline Iterator() 
    {
      m_hash = NULL;
      m_elem = NULL;
      m_slot = 0;
    }

    inline Iterator(const gmHash * a_hash)
    {
      m_hash = a_hash;
      m_slot = 0;
      m_elem = NULL;
      Inc();
    }

    inline void Inc(void)
    {
      GM_ASSERT(m_hash);
      if(m_elem)
      {
        m_elem = m_hash->GetNext(m_elem);
      }
      while(m_elem == NULL && m_slot < m_hash->m_size)
      {
        m_elem = m_hash->m_table[m_slot++];
      }
    }
  
    inline void Dec(void) { GM_ASSERT(false); }
    inline T * Resolve(void) { return m_elem; }
    inline const T * Resolve(void) const { return m_elem; }
    inline bool IsValid() const { return (m_elem != NULL); }
 
  private:

    const gmHash * m_hash;
    T * m_elem;
    unsigned int m_slot;
  };

  // members

  gmHash(gmuint a_size);
  ~gmHash();

  void RemoveAll();
  void RemoveAndDeleteAll();

  /// \brief Insert() will insert an item into the hash table. 
  /// \return non-null on failure, in which case the returned item is the duplicate existing in the hash
  T * Insert(T * a_node);

  /// \brief Remove() will remove an item from the hash table
  /// \return the removed item
  T * Remove(T * a_node);

  /// \brief Remove() will remove an item from the hash table via an iterator
  /// \return the removed item
  T * Remove(Iterator & a_it);

  /// \brief RemoveKey() will remove an item by key
  /// \return the removed item
  T * RemoveKey(const KEY &a_key);

  /// \brief Find()
  T * Find(const KEY &a_key);

  inline gmuint Count() const { return m_count; }
  inline Iterator First() const { return Iterator(this); }

private:

  T * GetNext(T * a_elem) const { return a_elem->NQUAL::m_next; }
  T ** m_table;
  gmuint m_count;
  gmuint m_size;

  friend class Iterator;
};


/// \class gmDefaultHasher
/// \brief use the gmDefaultHasher as the HASHER template arg for the common hashing keys
class gmDefaultHasher
{
public:

  static inline gmuint Hash(const char * a_key)
  {
    gmuint key = 0;
    const char * cp = (const char *) a_key;

    while(*cp != '\0') 
    {
      key = (key + ((key << 5) + *cp));
      ++cp;
    }
    return key;
  }

  static inline int Compare(const char * a_keyA, const char * a_keyB)
  {
    return strcmp(a_keyA, a_keyB);
  }

  static inline gmuint Hash(int a_key) 
  {
    return (gmuint) a_key;
  }

  static inline int Compare(int a_keyA, int a_keyB)
  {
    return (a_keyA - a_keyB);
  }
  
  static inline gmuint Hash(const void * a_key) 
  {
    return (gmuint) (((gmuint) a_key) / sizeof(double));
  }

  static inline int Compare(const void * a_keyA, const void * a_keyB)
  {
    return (int) ((char *) a_keyA - (char *) a_keyB);
  }
};



TMPL
QUAL::gmHash(gmuint a_size)
{
  // make sure size is power of 2
  GM_ASSERT((a_size & (a_size - 1)) == 0);
  m_size = a_size;
  m_table = GM_NEW(T * [a_size]);
  int i = m_size;
  while(i--)
  {
    m_table[i] = NULL;
  }
  m_count = 0;
}

TMPL
QUAL::~gmHash()
{
  delete [] m_table;
}


TMPL
void QUAL::RemoveAll()
{
  int i = m_size;
  while(i--)
  {
    m_table[i] = NULL;
  }
  m_count = 0;
}


TMPL
void QUAL::RemoveAndDeleteAll()
{
  // iterate over table and delete all
  int i = m_size;
  T * node, * next;
  while(i--)
  {
    node = m_table[i];
    while(node)
    {
      next = node->NQUAL::m_next;
      delete node;
      node = next;
    }
    m_table[i] = NULL;
  }
  m_count = 0;
}


TMPL
T * QUAL::Insert(T * a_node)
{
  gmuint slot = HASHER::Hash(a_node->GetKey()) & (m_size - 1);
  T ** node = &m_table[slot];

  while(*node)
  {
    int compare = HASHER::Compare(a_node->GetKey(), (*node)->GetKey());
    if(compare == 0) return (*node);
    else if(compare < 0) break;
    node = &((*node)->NQUAL::m_next);
  }

  a_node->NQUAL::m_next = *node;
  *node = a_node;
  ++m_count;
  return NULL;
}


TMPL
T * QUAL::Remove(T * a_node)
{
  gmuint slot = HASHER::Hash(a_node->GetKey()) & (m_size - 1);
  T ** node = &m_table[slot];

  while(*node)
  {
    if(a_node == *node)
    {
      *node = a_node->NQUAL::m_next;
      --m_count;
      return a_node;
    }
    node = &((*node)->NQUAL::m_next);
  }
  return NULL;
}


TMPL
T * QUAL::Remove(Iterator & a_it)
{
  T * node = a_it.Resolve();
  if(node)
  {
    a_it.Inc();
    return Remove(node);
  }
  return NULL;
}



TMPL
T * QUAL::RemoveKey(const KEY &a_key)
{
  gmuint slot = HASHER::Hash(a_key) & (m_size - 1);
  T ** node = &m_table[slot];
  T * found;

  while(*node)
  {
    int compare = HASHER::Compare((*node)->GetKey(), a_key);
    if(compare == 0)
    {
      --m_count;
      found = *node;
      *node = found->NQUAL::m_next;
      return (found);
    }
    else if(compare > 0)
    {
      return NULL;
    }
    node = &((*node)->NQUAL::m_next);
  }
  return NULL;
}


TMPL
T * QUAL::Find(const KEY &a_key)
{
  gmuint slot = HASHER::Hash(a_key) & (m_size - 1);
  T * node = m_table[slot];

  while(node)
  {
    int compare = HASHER::Compare(static_cast<T*>(node)->GetKey(), a_key);
    if(compare == 0)
    {
      return node;
    }
    else if(compare > 0)
    {
      return NULL;
    }
    node = node->NQUAL::m_next;
  }
  return NULL;
}

#undef TMPL
#undef QUAL
#undef NQUAL

#endif // _GMHASH_H_
