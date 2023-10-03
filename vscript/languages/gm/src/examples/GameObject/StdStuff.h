#ifndef STDSTUFF_H
#define STDSTUFF_H

//
// StdStuff.h
//
// Merely some containers and standard things you would
// find in MFC, STL, or your favourite library/engine.
// These were implemented as quickly and minimally as possible
// rather than introduce more code or external libraries.
//

#include "gmHash.h"

// Quick n dirty Map using the available hash table
template<class KEY, class VALUE>
class Map
{
public:

  Map()
   : m_hashTable(1024) // Just an arbitrary number at present
  {
  }

  virtual ~Map()
  {
    m_hashTable.RemoveAndDeleteAll();
  }

  /// \brief Insert a element associated with a key.
  /// \param a_key Key to identify data.
  /// \param a_value Data associated with Key.
  void SetAt(const KEY& a_key, const VALUE& a_value)
  {
    HashNode* node = m_hashTable.Find(a_key);
    if(node)
    {
      node->m_value = a_value;
    }
    else
    {
      node = new HashNode;
      node->m_key = a_key;
      node->m_value = a_value;
      m_hashTable.Insert(node);
    }
  };

  /// \brief Find a node in the map.
  /// \param a_key Key to identiy element.
  /// \param a_value Found element returned here.
  /// \return TRUE if found, FALSE if not in map.
  bool GetAt(const KEY& a_key, VALUE& a_value)
  {
    HashNode* node = m_hashTable.Find(a_key);
    if(node)
    {
      a_value = node->m_value;
      return true;
    }
    return false;
  }

  /// \brief Remove a node from the map.
  /// \param a_key Key to identiy element.
  /// \param a_removedData Found element returned here.
  /// \return TRUE if found, FALSE if not in map.
  bool RemoveAt(const KEY& a_key, VALUE& a_removedData)
  {
    HashNode* node = m_hashTable.Find(a_key);
    if(node)
    {
      a_removedData = node->m_value;

      m_hashTable.Remove(node);
      delete node;

      return true;
    }
    return false;
  }

  /// \brief Remove a node from the map.
  /// \param a_key Key to identiy element.
  /// \return TRUE if found, FALSE if not in map.
  bool RemoveAt(const KEY& a_key)
  {
    HashNode* node = m_hashTable.Find(a_key);
    if(node)
    {
      m_hashTable.Remove(node);
      delete node;

      return true;
    }
    return false;
  }

protected:

  struct HashNode : public gmHashNode<KEY, HashNode, HashNode>
  {
    VALUE m_value;
    KEY m_key;

    virtual const KEY& GetKey() const { return m_key; }

    static inline gmuint Hash(const KEY& a_key)
    {
      return (unsigned int)a_key;
    }

    static inline int Compare(const KEY& a_keyA, const KEY& a_keyB)
    {
      if(a_keyA < a_keyB) 
        { return -1; }
      if(a_keyA > a_keyB) 
        { return 1; }
      return 0;
    }
  };

  // Blocking
  gmHash<KEY, HashNode, HashNode> m_hashTable;

};


// The most crap string implementation ever
class String
{
public:

  String()
  {
    m_buffer = NULL;
    SetBuffer("");
  }

  String(const char* a_newString)
  {
    m_buffer = NULL;
    SetBuffer(a_newString);
  }

  String(const char* a_newString, const int a_newStringLength)
  {
    m_buffer = NULL;
    SetBuffer(a_newString, a_newStringLength);
  }

  ~String()
  {
    delete [] m_buffer;
  }

  operator const char* () const
  {
    return m_buffer;
  }

  const char* operator = (const char* a_newString)
  {
    SetBuffer(a_newString);
    return m_buffer;
  }

  const char* operator = (const String& a_newString)
  {
    SetBuffer(a_newString);
    return m_buffer;
  }

private:

  void SetBuffer(const char* a_newString, const int a_newStringLength)
  {
    delete [] m_buffer;
    m_buffer = new char [a_newStringLength + 1];
    memcpy(m_buffer, a_newString, a_newStringLength);
    m_buffer[a_newStringLength] = 0;
  }

  void SetBuffer(const char* a_newString)
  {
    int newStringLength = strlen(a_newString);
    SetBuffer(a_newString, newStringLength);
  }

  char * m_buffer;
};

#endif //STDSTUFF_H