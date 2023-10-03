/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmIncGC.h"

// Must be last header
#include "memdbgon.h"

// NOTES: 

// o Q: What about when we turn GC off manually to prevent new objects from disappearing ?
//   A: This can't happen with allocate black AS LONG AS the collect is not called during processing, only at yield.
//   Still, would be nice to put this functionality in for non-game use.
//

// How to make new types compatible with GC.
//
// 1) Implement the following user type callbacks:
//    typedef void (GM_CDECL *gmGCDestructCallback)(gmMachine * a_machine, gmUserObject* a_object);
//    typedef bool (GM_CDECL *gmGCTraceCallback)(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone);
//
//    o The Destruct function merely destructs and frees the user object. (Same as it used to.)
//
//    o The Trace function calls gc->GetNextObject(obj) where obj is each gmObject* at that level.
//    You should increment a_workDone for each call and once extra (so min value should be +1).
//    The Trace has the potential to be re-called for multiple increments, but this functionality is not necessary and has not been tested.
//    Always return true when finished tracing.  Returning false signals that you are not finished and it should be re-called.
//
// 2) If you make a table or array type class that contains variables that could be gmObjects,
//    call gc->WriteBarrier(obj) where obj is the old gmObject about to be overwritten in a SetInd or SetDot call etc.
//
// Note that permanant strings are stored in a separate list so they are ignored by the GC.

//////////////////////////////////////////////////
// gmGCColorSet
//////////////////////////////////////////////////

gmGCColorSet::gmGCColorSet()
{
  Init(NULL);
}


gmGCColorSet::~gmGCColorSet()
{
}


void gmGCColorSet::Init(gmGarbageCollector* a_gc)
{
  m_gc = a_gc;

  // Note that only the Scan and Free markers actually move
  //
  //  | GRAY BLACK FREE | WHITE |
  //  ^      ^     ^    ^       ^
  //  G      S     F    W       T
  //
  m_gray = &m_headObject;
  m_white = &m_separatorObject;
  m_free = m_white;
  m_scan = m_free;
  m_tail = &m_tailObject;

  m_tailObject.SetPrev(&m_separatorObject);
  m_tailObject.SetNext(NULL);
  
  m_separatorObject.SetPrev(&m_headObject);
  m_separatorObject.SetNext(&m_tailObject);
  
  m_headObject.SetPrev(NULL);
  m_headObject.SetNext(&m_separatorObject);

  // Make persistList into a list node
  m_persistList.SetNext(&m_persistList);
  m_persistList.SetPrev(&m_persistList);

#if GM_GC_STATS
  m_numAllocated = 0;
#endif //GM_GC_STATS
}


#if GM_GC_DEBUG
bool gmGCColorSet::VerifyIntegrity()
{
  // Scan through list make sure all pointers are in valid positions and all objects are colored correctly
  gmGCObjBase* curObj = m_gray->GetNext();
  int curCol = GM_GC_DEBUG_COL_GRAY;
  while(curObj != m_tail)
  {
    if(curObj == m_scan)
    {
      curCol = GM_GC_DEBUG_COL_BLACK;
    }
    if(curObj == m_free)
    {
      curCol = GM_GC_DEBUG_COL_FREE;
    }
    if(curObj == m_white)
    {
      curCol = GM_GC_DEBUG_COL_WHITE;
    }
    if(curObj != &m_separatorObject)
    {
      GM_ASSERT(curObj->m_curPosColor == curCol);
    }

    curObj = curObj->GetNext();
  }
  
  return true;
}
#endif //GM_GC_DEBUG


void gmGCColorSet::DestructPersistantObjects()
{
  int count=0;

  gmGCObjBase* curObj = m_persistList.GetNext();
  while(curObj != &m_persistList)
  {
    gmGCObjBase* objToDestruct = curObj;
    curObj = curObj->GetNext();

    objToDestruct->Destruct(m_gc->GetVM());
    ++count;
  }

  // Reset persist list (Make persistList into a list node)
  m_persistList.SetNext(&m_persistList);
  m_persistList.SetPrev(&m_persistList);
}


int gmGCColorSet::FollowPointers(int a_maxBytesToTrace)
{
  int workDone = 0;

  if(m_gc->GetTraceState().m_object->Trace(m_gc->GetVM(), m_gc, a_maxBytesToTrace, workDone))
  {
    m_gc->GetTraceState().m_done = true;
  }

  return workDone;
}


bool gmGCColorSet::BlackenNextGray(int& a_workDone, int a_workLeftToGo)
{
  if(m_gc->GetTraceState().m_done == true)
  {
    if(m_scan->GetPrev() == m_gray) // No grays to blacken.
    { 
      a_workDone = 0;
      return false;
    } 
    else 
    {
      m_scan = m_scan->GetPrev();
      m_gc->GetTraceState().m_object = m_scan;
    
#if GM_GC_DEBUG
      GM_ASSERT(m_scan->m_curPosColor == GM_GC_DEBUG_COL_GRAY);
      m_scan->m_curPosColor = GM_GC_DEBUG_COL_BLACK;
#endif //GM_GC_DEBUG

      a_workDone = FollowPointers(a_workLeftToGo);

      return true;  // Gray was blackened
    }
  } 
  else 
  {
    // Resume previously interrupted scanning of an object
    a_workDone = FollowPointers(a_workLeftToGo);
    return true;
  }
} 


void gmGCColorSet::GrayThisObject(gmGCObjBase* a_obj)
{
  gmGCObjBase* objPrev = a_obj->GetPrev();
  gmGCObjBase* objNext = a_obj->GetNext();

  // This routine should never get called with a shaded object
  GM_ASSERT(!m_gc->IsShaded(a_obj));

#if GM_GC_DEBUG
  GM_ASSERT(a_obj->m_curPosColor == GM_GC_DEBUG_COL_WHITE);
  a_obj->m_curPosColor = GM_GC_DEBUG_COL_GRAY;
#endif //GM_GC_DEBUG

  // Set object`s color to shaded first.
  a_obj->SetColor(m_gc->GetCurShadeColor());

  // The object must be shaded
  GM_ASSERT(m_gc->IsShaded(a_obj));

  // Splice the object out of the white list
  // This can be done unconditionally as no set pointers can point to any object in this set.
  objPrev->SetNext(objNext);
  objNext->SetPrev(objPrev);

  // Put the object into the correct place in the gray list

#if DEPTH_FIRST
  // Put the gray object at the head of the gray list.
  a_obj->SetPrev(m_scan->GetPrev());
  a_obj->SetNext(m_scan);
  m_scan->GetPrev()->SetNext(a_obj);
  m_scan->SetPrev(a_obj);
#else // BREADTH_FIRST
  // Put the gray object at the tail of the gray list.
  a_obj->SetPrev(m_gray);
  a_obj->SetNext(m_gray->GetNext());
  m_gray->GetNext()->SetPrev(a_obj);
  m_gray->SetNext(a_obj);
#endif // BREADTH_FIRST

#if GM_GC_DEBUG
  // Slow, paranoid check
  VerifyIntegrity();
#endif //GM_GC_DEBUG
}

  
void gmGCColorSet::Revive(gmGCObjBase* a_obj)
{
  // NOTE: Once objects are in the free list, we can't trust the color mark, 
  //       it may have been set either side of the flip.
  //       We should only revive 'free', but we can't simply tell where in the list the object is.
  
  // Always mark black, this is only done for strings, and we are logically re-allocating the dead object

#if GM_GC_DEBUG 
  a_obj->m_curPosColor = GM_GC_DEBUG_COL_BLACK; // Blacken as if re-allocated
#endif //GM_GC_DEBUG

  // Set object`s color to shaded first.
  a_obj->SetColor(m_gc->GetCurShadeColor());

  // Fix scan (NOTE: If scan == obj, obj must already be black, so could skip rest of this function)
  if( m_scan == a_obj )
  {
    m_scan = m_scan->GetNext(); // Not Prev as scan should be the start of black inclusive
  }
  // We don't care if (m_gc->GetTraceState().m_object == a_obj ) as this is Done and write only, and can only be string, not potentially a resumable object.
#if GM_GC_DEBUG
  if( m_gc->GetTraceState().m_object == a_obj )
  {
    GM_ASSERT( m_gc->GetTraceState().m_done );
  }
#endif // GM_GC_DEBUG

  // Fix sentinels
  if( m_free == a_obj )
  {
    m_free = m_free->GetNext();
  }

  // Splice the object out of the free/white list (Or anywhere in this case)
  gmGCObjBase* objPrev = a_obj->GetPrev();
  gmGCObjBase* objNext = a_obj->GetNext();
  objPrev->SetNext(objNext);
  objNext->SetPrev(objPrev);

  // Insert at the end of black list
  a_obj->SetNext(m_free);             // Next is first Free
  a_obj->SetPrev(m_free->GetPrev());  // Prev is last Black
  m_free->GetPrev()->SetNext(a_obj);  // Last Black next is now this
  m_free->SetPrev(a_obj);             // Free prev is now this

  // If there were no blacks, move scan forward to prevent scanning this new black
  if( m_scan == m_free )
  {
    m_scan = a_obj;
  }
}


void gmGCColorSet::ReclaimGarbage()
{
  GM_ASSERT(m_scan->GetPrev() == m_gray);

#if GM_GC_DEBUG
  {
    // Traverse the newly found garbage objects just to make sure there
    // aren't any live objects in there.  Used for debugging only.
    for(gmGCObjBase* temp = m_white->GetNext();
        temp != m_tail;
        temp = temp->GetNext())
    {
      GM_ASSERT(!m_gc->IsShaded(temp));
    }
  }
#endif

  if (m_white->GetNext() != m_tail)       // There are garbage objects
  {
#if GM_GC_DEBUG
    //flag white->tail as white?
    for(gmGCObjBase* temp = m_white->GetNext();
        temp != m_tail;
        temp = temp->GetNext())
    {
      GM_ASSERT(temp->m_curPosColor == GM_GC_DEBUG_COL_WHITE);
      temp->m_curPosColor = GM_GC_DEBUG_COL_FREE;
    }
#endif //GM_GC_DEBUG

    bool fixScan = false;
    if( m_scan == m_free )  // There are no black objects
    {  
      fixScan = true;    
    }

    // Reclaim the garbage.
    // Insert old White->Tail at start of Free (Free may == White)
    gmGCObjBase* firstFree = m_white->GetNext();
    
    firstFree->SetPrev(m_free->GetPrev());
    m_free->GetPrev()->SetNext(firstFree);
    
    m_tail->GetPrev()->SetNext(m_free);
    m_free->SetPrev(m_tail->GetPrev());
    
    m_free = firstFree;
    
    if( fixScan )
    {
      m_scan = m_free;
    }

    m_white->SetNext(m_tail);
    m_tail->SetPrev(m_white);
  }

  // Whiten the live objects.
  if (m_scan != m_free)   // There are live (black) objects
  {

#if GM_GC_DEBUG
    //flag scan->free as white?
    for(gmGCObjBase* temp = m_scan;//m_scan->GetNext();
        temp != m_free;
        temp = temp->GetNext())
    {
      GM_ASSERT(temp->m_curPosColor == GM_GC_DEBUG_COL_BLACK);
      temp->m_curPosColor = GM_GC_DEBUG_COL_WHITE;
    }
#endif //GM_GC_DEBUG

    GM_ASSERT(m_white->GetNext() == m_tail);
    GM_ASSERT(m_tail->GetPrev() == m_white);

    m_scan->GetPrev()->SetNext(m_free);
    m_free->GetPrev()->SetNext(m_tail);
    m_tail->SetPrev(m_free->GetPrev());
    m_free->SetPrev(m_scan->GetPrev());
    m_scan->SetPrev(m_white);
    m_white->SetNext(m_scan);
    m_scan = m_free;
  }

  GM_ASSERT(m_gray->GetNext() == m_scan);

#if GM_GC_DEBUG
  {
    int count = 0;
    for(gmGCObjBase* temp = m_free; temp != m_white; temp = temp->GetNext())
    {
      ++count;
    }
  }
#endif
}


int gmGCColorSet::DestructSomeFreeObjects(int a_maxToDestruct)
{
  int numDestructed = 0;
  // Go through the free list (perhaps over multiple installments in future) and call Destruct() on them.
  if(m_free != m_white)
  {
    gmGCObjBase* beforeFree = m_free->GetPrev(); // Save previous node so we can relink after removing some
    bool fixScan = false;
    if(m_scan == m_free) // Will need to fix the scan ptr later if this is so.
    {
      fixScan = true;
    }

    while(m_free != m_white)
    {
      gmGCObjBase* objToRecycle = m_free;
      m_free = m_free->GetNext();

#if GM_GC_DEBUG
      //GM_ASSERT(objToRecycle->m_curPosColor == GM_GC_DEBUG_COL_WHITE);
      GM_ASSERT(objToRecycle->m_curPosColor == GM_GC_DEBUG_COL_FREE);
      objToRecycle->m_curPosColor = GM_GC_DEBUG_COL_INVALID;
#endif //GM_GC_DEBUG

#if GM_GC_KEEP_PERSISTANT_SEPARATE
      GM_ASSERT(!objToRecycle->GetPersist());
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE
#if GM_GC_STATS
      --m_numAllocated;
#endif //GM_GC_STATS

      objToRecycle->Destruct(m_gc->GetVM());
      ++numDestructed;
      --a_maxToDestruct;
      if(a_maxToDestruct <= 0)
      {
        // Relink since we removed elements
        beforeFree->SetNext(m_free);
        m_free->SetPrev(beforeFree);
        if(fixScan)
        {
          m_scan = m_free;
        }
        return numDestructed; // Work is done for now.
      }
    }
    // Relink since we removed elements
    beforeFree->SetNext(m_free);
    m_free->SetPrev(beforeFree);
    if(fixScan)
    {
      m_scan = m_free;
    }
  }
  return numDestructed;
}


void gmGCColorSet::Allocate(gmGCObjBase* a_obj)
{
#if GM_GC_STATS
  ++m_numAllocated;
#endif //GM_GC_STATS

  a_obj->SetPersist(false);

  a_obj->SetColor(m_gc->GetCurShadeColor());

#if GM_GC_DEBUG
  GM_ASSERT(a_obj->m_curPosColor == GM_GC_DEBUG_COL_INVALID);
  a_obj->m_curPosColor = GM_GC_DEBUG_COL_BLACK;
#endif //GM_GC_DEBUG
  
  //Insert at the end of black list
  a_obj->SetNext(m_free);             //Next is first Free
  a_obj->SetPrev(m_free->GetPrev());  //Prev is last Black
  m_free->GetPrev()->SetNext(a_obj);  //Last Black next is now this
  m_free->SetPrev(a_obj);             //Free prev is now this
 
  //If there were no blacks, move scan forward to prevent scanning this new black
  if(m_scan == m_free)
  {
    m_scan = a_obj;
  }
}


void gmGCColorSet::DestructAll()
{
  int count = 0;

  DestructPersistantObjects();

  // Black and Gray
  gmGCObjBase* curGrayOrBlack = m_gray->GetNext();
  while(curGrayOrBlack != m_free)
  {
    gmGCObjBase* objToRecycle = curGrayOrBlack;
    curGrayOrBlack = curGrayOrBlack->GetNext();
#if GM_GC_KEEP_PERSISTANT_SEPARATE
    GM_ASSERT(!objToRecycle->GetPersist());
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE
    objToRecycle->Destruct(m_gc->GetVM());
#if GM_GC_STATS
    --m_numAllocated;
#endif //GM_GC_STATS
    ++count;
  }

  // Whites
  gmGCObjBase* curWhite = m_white->GetNext();
  while(curWhite != m_tail)
  {
    gmGCObjBase* objToRecycle = curWhite;
    curWhite = curWhite->GetNext();

#if GM_GC_KEEP_PERSISTANT_SEPARATE
    GM_ASSERT(!objToRecycle->GetPersist());
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE
    objToRecycle->Destruct(m_gc->GetVM());
#if GM_GC_STATS
    --m_numAllocated;
#endif //GM_GC_STATS
    ++count;
  }

  // Free list
  gmGCObjBase* curFree = m_free;
  while(curFree != m_white)
  {
    gmGCObjBase* objToRecycle = curFree;
    curFree = curFree->GetNext();
    {
#if GM_GC_KEEP_PERSISTANT_SEPARATE
      GM_ASSERT(!objToRecycle->GetPersist());
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE
      objToRecycle->Destruct(m_gc->GetVM());
#if GM_GC_STATS
      --m_numAllocated;
#endif //GM_GC_STATS
      ++count;
    }
  }

  Init(m_gc);
}

//////////////////////////////////////////////////
// gmGarbageCollector
//////////////////////////////////////////////////

gmGarbageCollector::gmGarbageCollector()
{
  Init(NULL, NULL);
}


gmGarbageCollector::~gmGarbageCollector()
{
}


void gmGarbageCollector::Init(gmGCScanRootsCallBack a_scanRootsCallback, gmMachine* a_gmMachine)
{
  m_curShadeColor = 0; // Another color is !0
  m_workPerIncrement = GM_GC_DEFAULT_WORK_INCREMENT;
  m_maxObjsToDestructPerIncrement = GM_GC_DEFAULT_DESTRUCT_INCREMENT;
  m_workLeftToGo = 0;
  m_fullThrottle = false;
  m_gcTurnedOff = true; // Start in OFF state, machine will turn on when needed.
  m_firstCollectionIncrement = true;
  m_doneTracing = false;
  m_colorSet.Init(this);
  m_traceState.Reset();
  m_flipCallback = NULL;
  m_scanRootsCallback = a_scanRootsCallback;
  m_gmMachine = a_gmMachine;
}


bool gmGarbageCollector::BlackenGrays() 
{
  int workDone;

  while(m_colorSet.AnyGrays()) 
  {
    // gmGCColorSet::BlackenNextGray returns 1 if there was a gray to
    // blacken (even if it couldn't finish blackening it), and a 0 otherwise.

    workDone = m_workLeftToGo;

    while(m_colorSet.BlackenNextGray(workDone, m_workLeftToGo))
    {
      m_workLeftToGo -= workDone;
      if (m_workLeftToGo <= 0)
      {
        // Quit early
        return true;  // We have completed one increment of work
      }
    }
  };
  
  return false;
}


bool gmGarbageCollector::Collect()
{
  if(m_fullThrottle)
  {
    m_workLeftToGo = GM_MAX_INT32;
  }
  else
  {
    m_workLeftToGo = m_workPerIncrement;
  }

  m_doneTracing = false;

  if(m_firstCollectionIncrement)
  {
    // Scan each root object and gray it
    GM_ASSERT(m_scanRootsCallback);
    m_scanRootsCallback(m_gmMachine, this);

    m_firstCollectionIncrement = false;
    return false;
  }

  // If any grays exist, scan them first
  if(m_colorSet.AnyGrays())
  {
    if(BlackenGrays()) // Returns 0 if no more grays, and 1 if done with an increment of collection.
    {
      return false; // Out of time, so exit function
    }
  }

  m_doneTracing = true;

  // Let the collect continue until garbage memory has been reclaimed
  // This could be done as an external phase
  if(ReclaimSomeFreeObjects())
  {
    return false;
  }

#if GM_GC_TURN_OFF_ABLE  
  // Turn off gc until almost out of memory.
  // Can only do when allocating black.
  m_gcTurnedOff = true;
#else //GM_GC_TURN_OFF_ABLE  
  Flip();
#endif //GM_GC_TURN_OFF_ABLE  

  return true;
}


void gmGarbageCollector::Flip()
{
  m_firstCollectionIncrement = true;

  if(m_flipCallback)
  {
    m_flipCallback();
  }

#if GM_GC_TURN_OFF_ABLE  
  // The garbage collector can only be turned off if we are allocating black.
  m_gcTurnedOff = false; // Turn the garbage collector back on.
#endif //GM_GC_TURN_OFF_ABLE  
  
  m_colorSet.ReclaimGarbage();
  
  ToggleCurShadeColor();
}


// This function is called when there are no free objects in the colorset.
// If the gc is turned off, it calls flip to reclaim any garbage objects
// that have been found by the garbage collector.
void gmGarbageCollector::ReclaimObjectsAndRestartCollection()
{
#if GM_GC_TURN_OFF_ABLE
  // The garbage collector only gets turned off if we are allocating
  // black.  GC is turned off after finishing  tracing and before 
  // doing a gc flip. So if there are no free objects left, first 
  // flip the GC and turn white objects into free.  Hopefully, this 
  // will provide more free objects for allocation.

  if(m_gcTurnedOff) 
  {
    Flip();
  }
#endif //GM_GC_TURN_OFF_ABLE
}


/// \brief Destruct all objects.
void gmGarbageCollector::DestructAll()
{
  m_colorSet.DestructAll();

  //Reset some of our members
  m_curShadeColor = 0;
  m_workPerIncrement = 100;
  m_maxObjsToDestructPerIncrement = 100;
  m_workLeftToGo = 0;
  m_doneTracing = false;
  m_fullThrottle = false;
  m_firstCollectionIncrement = true;
  m_traceState.Reset();
}


void gmGarbageCollector::FullCollect()
{
  m_fullThrottle = true;

  if(IsOff()) // If GC is off
  {
    ReclaimObjectsAndRestartCollection(); // Do flip and turn it back on
  }

  while(!Collect())
  {
    // Do the collect phase
  }
  ReclaimObjectsAndRestartCollection(); // Do flip and turn it back on

  // Collect a second time to catch floating black objects
  while(!Collect())
  {
    // Do the collect phase
  }
  ReclaimObjectsAndRestartCollection(); // Do flip and turn it back on

  // NOTE: The GC is now restarted and in an 'On' state, meaning it will now collect again from the machine.
  //       This behavior may not be desirable, so this function really needs more analysis to determine the
  //       optimum sequence for a full collect with minimal redundancy.

  // Free memory of garbage objects
  while(ReclaimSomeFreeObjects())
  {
    // Reclaim all garbage
  }
  m_fullThrottle = false;
}



//////////////////////////////////////////////////
// Helper functions for VM and debugger
//////////////////////////////////////////////////
#include "gmVariable.h"
#include "gmFunctionObject.h"

const void* gmGCColorSet::GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line)
{
  gmGCObjBase* cur;
  
  // Search Gray to Free
  cur = m_gray->GetNext();
  while(cur != m_free)
  {
    gmObject* object = (gmObject*)cur;
    if(object->GetType() == GM_FUNCTION)
    {
      gmFunctionObject * function = (gmFunctionObject *) object;
      if(function->GetSourceId() == a_sourceId)
      {
        const void * instr = function->GetInstructionAtLine(a_line);
        if(instr)
        {
          return instr;
        }
      }
    }
    cur = cur->GetNext();
  }

  // Search White
  cur = m_white->GetNext();
  while(cur != m_tail)
  {
    gmObject* object = (gmObject*)cur;
    if(object->GetType() == GM_FUNCTION)
    {
      gmFunctionObject * function = (gmFunctionObject *) object;
      if(function->GetSourceId() == a_sourceId)
      {
        const void * instr = function->GetInstructionAtLine(a_line);
        if(instr)
        {
          return instr;
        }
      }
    }
    cur = cur->GetNext();
  }

  return NULL;
}


gmObject* gmGCColorSet::CheckReference(gmptr a_ref)
{
  gmGCObjBase* cur;

  // Search Gray to Free
  cur = m_gray->GetNext();
  while(cur != m_free)
  {
    gmObject* object = (gmObject*)cur;
    if((gmptr)object == a_ref)
    {
      return object;
    }
    cur = cur->GetNext();
  }

  // Search White
  cur = m_white->GetNext();
  while(cur != m_tail)
  {
    gmObject* object = (gmObject*)cur;
    if((gmptr)object == a_ref)
    {
      return object;
    }
    cur = cur->GetNext();
  }

  // Search Persistant list
  cur = m_persistList.GetNext();
  while(cur != &m_persistList)
  {
    gmObject* object = (gmObject*)cur;
    if((gmptr)object == a_ref)
    {
      return object;
    }
    cur = cur->GetNext();
  }

  return NULL;
}
