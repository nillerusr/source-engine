/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmTableObject.h"
#include "gmMachine.h"
#include "gmThread.h"

// Must be last header
#include "memdbgon.h"



gmTableObject::gmTableObject()
{
  m_nodes = NULL;
  m_firstFree = NULL;
  m_tableSize = 0;
  m_slotsUsed = 0;
}


#if GM_USE_INCGC


bool gmTableObject::Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  gmTableNode * curNode;
  int index;
  for(index = 0; index < m_tableSize; ++index)
  {
    if(m_nodes[index].m_key.m_type != GM_NULL)
    {
      curNode = &m_nodes[index];
      if(curNode->m_key.IsReference())
      {
        gmObject* object = GM_MOBJECT(a_machine, curNode->m_key.m_value.m_ref);
        a_gc->GetNextObject(object);
        ++a_workDone;
      }
      if(curNode->m_value.IsReference())
      {
        gmObject* object = GM_MOBJECT(a_machine, curNode->m_value.m_value.m_ref);
        a_gc->GetNextObject(object);
        ++a_workDone;
      }
    }
  }
  
  ++a_workDone;
  return true;
}

#else //GM_USE_INCGC

void gmTableObject::Mark(gmMachine * a_machine, gmuint32 a_mark)
{
  if(m_mark != GM_MARK_PERSIST) m_mark = a_mark;

  gmTableNode * curNode;
  int index;
  for(index = 0; index < m_tableSize; ++index)
  {
    if(m_nodes[index].m_key.m_type != GM_NULL)
    {
      curNode = &m_nodes[index];
      if(curNode->m_key.IsReference())
      {
        gmObject* object = GM_MOBJECT(a_machine, curNode->m_key.m_value.m_ref);
        if(object->NeedsMark(a_mark)) object->Mark(a_machine, a_mark);
      }
      if(curNode->m_value.IsReference())
      {
        gmObject* object = GM_MOBJECT(a_machine, curNode->m_value.m_value.m_ref);
        if(object->NeedsMark(a_mark)) object->Mark(a_machine, a_mark);
      }
    }
  }
}
#endif //GM_USE_INCGC


void gmTableObject::Destruct(gmMachine * a_machine)
{
  if(m_nodes)
  {
    a_machine->Sys_Free(m_nodes);
    m_nodes = NULL;
  }

  m_firstFree = NULL;
  m_tableSize = 0;
  m_slotsUsed = 0;

#if GM_USE_INCGC
  a_machine->DestructDeleteObject(this);
#endif //GM_USE_INCGC
}



gmVariable gmTableObject::Get(const gmVariable &a_key) const
{
  gmTableNode* foundNode = NULL;

  if(m_nodes && a_key.m_type != GM_NULL)
  {
    foundNode = GetAtHashPos(&a_key);

    do
    {
      if(a_key.m_value.m_ref == foundNode->m_key.m_value.m_ref &&
         a_key.m_type == foundNode->m_key.m_type)
      {
        return foundNode->m_value;
      }
      foundNode = foundNode->m_nextInHashTable;
    } while (foundNode);
  }  

  return gmVariable::s_null;
}


gmVariable gmTableObject::Get(gmMachine * a_machine, const char * a_key) const
{
  return Get(gmVariable(GM_STRING, a_machine->AllocStringObject(a_key)->GetRef()));
}


#if GM_USE_INCGC  
void gmTableObject::Set(gmMachine * a_machine, const gmVariable &a_key, const gmVariable &a_value, bool a_disableWriteBarrier)
#else //GM_USE_INCGC  
void gmTableObject::Set(gmMachine * a_machine, const gmVariable &a_key, const gmVariable &a_value)
#endif //GM_USE_INCGC
{
  GM_ASSERT(m_firstFree >= &m_nodes[0] && m_firstFree <= &m_nodes[m_tableSize-1]);

  if(!m_tableSize)
  {
    Construct(a_machine);
  }

  if(a_key.m_type == GM_NULL)
  {
    return;
  }

  gmTableNode* origHashNode = GetAtHashPos(&a_key);
  gmTableNode* foundNode = origHashNode;
  gmTableNode* lastNode = NULL;

  GM_ASSERT(foundNode);

  // find key, if it exists
  do
  {
    if( (a_key.m_value.m_ref == foundNode->m_key.m_value.m_ref) &&
        (a_key.m_type == foundNode->m_key.m_type))
    {
      //If found and value is null, remove it
      if(GM_NULL == a_value.m_type)
      {
#if GM_USE_INCGC
        if( !a_disableWriteBarrier )
        {
          // Both key and value are going, write barrier them both
          if(foundNode->m_key.IsReference())
          {
            a_machine->GetGC()->WriteBarrier((gmObject*)foundNode->m_key.m_value.m_ref);
          }
          if(foundNode->m_value.IsReference())
          {
            a_machine->GetGC()->WriteBarrier((gmObject*)foundNode->m_value.m_value.m_ref);
          }
        }
#endif //GM_USE_INCGC

        if(lastNode)
        {
          lastNode->m_nextInHashTable = foundNode->m_nextInHashTable;

          foundNode->m_key.m_type = GM_NULL;
          foundNode->m_nextInHashTable = NULL;
        }
        else if(foundNode->m_nextInHashTable)
        {
          gmTableNode* nextSlot = foundNode->m_nextInHashTable;
          
          *foundNode = *nextSlot;

          nextSlot->m_key.m_type = GM_NULL;
          nextSlot->m_nextInHashTable = NULL;
        }
        else
        {
          foundNode->m_key.m_type = GM_NULL;
        }
        --m_slotsUsed;
        return;
      }
#if GM_USE_INCGC
      if( !a_disableWriteBarrier )
      {
        // Value is going, write barrier value only
        if(foundNode->m_value.IsReference())
        {
          a_machine->GetGC()->WriteBarrier((gmObject*)foundNode->m_value.m_value.m_ref);
        }
      }
#endif //GM_USE_INCGC
      foundNode->m_value = a_value;
      return;
    }
    lastNode = foundNode;
    foundNode = foundNode->m_nextInHashTable;
  } while (foundNode);

  //If not found, but value is null, don't add it
  if(GM_NULL == a_value.m_type)
  {
    return;
  }

  // key was not found, insert it
  if(origHashNode->m_key.m_type != GM_NULL) //Main pos is not free
  {
    gmTableNode * other;

    if( (other = GetAtHashPos(&origHashNode->m_key)) != origHashNode ) //Colliding node is out of its desired position.
    {
      while(other->m_nextInHashTable != origHashNode)
      {
        other = other->m_nextInHashTable;
      }
      other->m_nextInHashTable = m_firstFree;
      *m_firstFree = *origHashNode; //Copy colliding node into free pos
      origHashNode->m_nextInHashTable = NULL; //original is now completely free
    }
    else
    {
      //Colliding node is in its desired pos, new node will go into free pos
      m_firstFree->m_nextInHashTable = origHashNode->m_nextInHashTable;
      origHashNode->m_nextInHashTable = m_firstFree;
      origHashNode = m_firstFree; 
    }
  }

  // Fill new value
  origHashNode->m_key = a_key;
  origHashNode->m_value = a_value;

  ++m_slotsUsed;

  // Update m_firstFree
  for(;;)
  {
    if(m_firstFree->m_key.m_type == GM_NULL)
    {
      //Free place still exists
      return;
    }
    else if(m_firstFree == &m_nodes[0])
    {
      break; //We hit the start of the table, resize/rehash it
    }
    else
    {
      --m_firstFree; //Try moving toward table start
    }
  }

  Resize(a_machine);
}



void gmTableObject::Set(gmMachine * a_machine, const char * a_key, const gmVariable &a_value)
{
  bool enabled = a_machine->IsGCEnabled();
  a_machine->EnableGC(false);
  Set(a_machine, gmVariable(GM_STRING, a_machine->AllocStringObject(a_key)->GetRef()), a_value);
  a_machine->EnableGC(enabled);
}



gmTableObject * gmTableObject::Duplicate(gmMachine * a_machine)
{
  gmTableObject * object = a_machine->AllocTableObject();

  if(m_tableSize)
  {
    object->AllocSize(a_machine, m_tableSize);

    int index;
    for(index = 0; index < m_tableSize; ++index)
    {
      if(m_nodes[index].m_key.m_type != GM_NULL)
      {
        object->Set(a_machine, m_nodes[index].m_key, m_nodes[index].m_value);
      }
    }
  }

  return object;
}



void gmTableObject::Construct(gmMachine * a_machine)
{
  AllocSize(a_machine, MIN_TABLE_SIZE);
}



gmTableNode* gmTableObject::GetNext(gmTableIterator& a_it) const
{
  int index = a_it;
  if(index == IT_NULL)
  {
    return NULL;
  }
  if(index == IT_FIRST)
  {
    index = 0;
  }
  while(index<m_tableSize)
  {
    if(m_nodes[index].m_key.m_type != GM_NULL)
    {
      a_it = index + 1;
      return &m_nodes[index];
    }
    ++index;
  }
  a_it = IT_NULL;
  return NULL;
}



void gmTableObject::Resize(gmMachine * a_machine)
{
  int newSize = m_tableSize;

  if(m_slotsUsed >= m_tableSize - ( m_tableSize / 4 ))
  {
    newSize = m_tableSize * 2;
  }
  else if((m_slotsUsed <= ( m_tableSize / 4 )) && (m_tableSize > 4))
  {
    newSize = m_tableSize / 2;
  }
  else
  {
    //No need to resize, but need to reset m_firstFree
    int index;
    for(index = m_tableSize-1; index>=0; --index)
    {
      if(m_nodes[index].m_key.m_type == GM_NULL)
      {
        m_firstFree = &m_nodes[index];
        return;
      }
    }
    GM_ASSERT(0); //Shouldn't ever get here
  }
  
  gmTableNode* oldNodes = m_nodes;
  int oldTableSize = m_tableSize;

  AllocSize(a_machine, newSize);

  int index;
  for(index = 0; index < oldTableSize; ++index)
  {
    if(oldNodes[index].m_key.m_type != GM_NULL)
    {
#if GM_USE_INCGC
      Set(a_machine, oldNodes[index].m_key, oldNodes[index].m_value, true);
#else //GM_USE_INCGC
      Set(a_machine, oldNodes[index].m_key, oldNodes[index].m_value);
#endif //GM_USE_INCGC
    }
  }

  a_machine->Sys_Free(oldNodes);
}



void gmTableObject::AllocSize(gmMachine * a_machine, int a_size)
{
  GM_ASSERT((a_size & (a_size-1)) == 0 ); //Check for power of 2 size

  int memSize = sizeof(m_nodes[0]) * a_size;
  //WARNING: Sys_Alloc may call Mark and access this class before returning a new pointer!
  m_nodes = (gmTableNode*)a_machine->Sys_Alloc(memSize);
  m_tableSize = a_size;
  m_slotsUsed = 0;

  memset(m_nodes, 0, memSize);
  m_firstFree = &m_nodes[m_tableSize-1];
}



gmVariable gmTableObject::GetLinearSearch(const char * a_key) const
{
  gmTableIterator it;
  for( gmTableNode * node = GetFirst(it);
       !IsNull(it);
       node = GetNext(it) )
  {
    if( GM_STRING == node->m_key.m_type )
    {
      if( strcmp(((gmStringObject*)node->m_key.m_value.m_ref)->GetString(), a_key) == 0 )
      {
        return node->m_value;
      }
    }
  }
  return gmVariable::s_null;
}
