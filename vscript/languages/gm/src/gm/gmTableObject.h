/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMTABLEOBJECT_H_
#define _GMTABLEOBJECT_H_

#include "gmConfig.h"
#include "gmVariable.h"
//#include "gmMem.h"

typedef int gmTableIterator; ///< Table iterator, is actually the array index, or a reserved value


/// \class gmTableNode
/// \brief Values stored in the table are wrapped in these nodes.
struct gmTableNode
{
  gmTableNode* m_nextInHashTable;                 ///< The next node in the hash table

  gmVariable m_key;                               ///< The key used to find a value.
  gmVariable m_value;                             ///< The value associated with the key
};


/// \class gmTable
/// \brief
class gmTableObject : public gmObject
{
public:

  //
  // object
  //

#if GM_USE_INCGC
  virtual bool Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone);
#else //GM_USE_INCGC
  virtual void Mark(gmMachine * a_machine, gmuint32 a_mark);
#endif //GM_USE_INCGC
  virtual void Destruct(gmMachine * a_machine);
  virtual int GetType() const { return GM_TABLE; }

  //
  // table set, get
  //
  
  // Get by variable
  gmVariable Get(const gmVariable &a_key) const;
  // Get by c string (uses table search)
  gmVariable Get(gmMachine * a_machine, const char * a_key) const;
  // Get by array index (uses table search)
  gmVariable Get(int a_indexKey) const { return Get(gmVariable(a_indexKey)); }
  // Get by c string (uses linear search)
  gmVariable GetLinearSearch(const char * a_key) const;

#if GM_USE_INCGC  
  void Set(gmMachine * a_machine, const gmVariable &a_key, const gmVariable &a_value, bool a_disableWriteBarrier = false);  
#else //GM_USE_INCGC
  void Set(gmMachine * a_machine, const gmVariable &a_key, const gmVariable &a_value);
#endif //GM_USE_INCGC
  void Set(gmMachine * a_machine, const char * a_key, const gmVariable &a_value);
  inline void Set(gmMachine * a_machine, int a_index, const gmVariable &a_value)
  {
    Set(a_machine, gmVariable(GM_INT, (gmptr)a_index), a_value);
  }

  inline int Count() const { return m_slotsUsed; }
  gmTableObject * Duplicate(gmMachine * a_machine);


  //
  // iterator
  //

  inline gmTableNode * GetFirst(gmTableIterator& a_it) const
  {
    a_it = IT_FIRST;

    return GetNext(a_it);
  }

  inline bool IsNull(gmTableIterator a_it) const
  {
    if(a_it == IT_NULL)
    {
      return true;
    }
    return false;
  }

  gmTableNode * GetNext(gmTableIterator& a_it) const;

protected:

  /// \brief Non-public constructor.  Create via gmMachine.
  gmTableObject();
  friend class gmMachine;

private:

  enum
  {
    IT_NULL = -1,
    IT_FIRST = -2,
    MIN_TABLE_SIZE = 4,
  };

  void Construct(gmMachine * a_machine);
 
  void RemoveAndDeleteAll(gmMachine * a_machine);
  inline gmTableNode * GetAtHashPos(const gmVariable* a_key) const
  {
    unsigned int hash = a_key->m_value.m_ref;

    if(a_key->IsReference())
    {
      hash >>= 2; // Remove 4 byte pointer alignment (may not be optimal)
    }

    hash &= (unsigned int)(m_tableSize - 1); //mod/and to table size

    return &m_nodes[hash];
  }


  void Resize(gmMachine * a_machine);
  void AllocSize(gmMachine * a_machine, int a_size);

  gmTableNode * m_nodes;
  gmTableNode * m_firstFree;
  int m_tableSize;
  int m_slotsUsed;
};

#endif // _GMTABLEOBJECT_H_
