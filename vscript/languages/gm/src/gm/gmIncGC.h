/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMINCGC_H_
#define _GMINCGC_H_

#include "gmConfig.h"

// Configuration options
#define GM_GC_TURN_OFF_ABLE 1                     // Let GC turn off after completion, can be turned back on when memory low
#define GM_GC_KEEP_PERSISTANT_SEPARATE 1          // Keep persistant object in separate list for efficiency
#define DEPTH_FIRST 1                             // Depth first or bredth first tracing
#ifdef GM_DEBUG_BUILD
  #define GM_GC_STATS 1                           // Some stats
#else //GM_DEBUG_BUILD
  #define GM_GC_STATS 0
#endif //GM_DEBUG_BUILD

#define GM_GC_DEBUG 0                             // GC Debugging paranoid check code.  Only set to 1 when debugging the GC routines.

// Fwd decls
class gmgmGCObjBase;
class gmGarbageCollector;
class gmMachine;
class gmObject;

typedef void (GM_CDECL *GCFlipCallBack)();
typedef void (GM_CDECL *gmGCScanRootsCallBack)(gmMachine* a_machine, gmGarbageCollector* a_gc);


// Incremental garbage collection method:
//
// A tricolor marking scheme is used with new objects allocated Black.
// A write barrier is used to maintain integrity of the list.
// The list looks like:
//
//  | GRAY BLACK FREE | WHITE |
//  ^      ^     ^    ^       ^
//  G      S     F    W       T
//
//  Legend:
//  G Gray (head) pointer
//  S Scan pointer
//  F Free pointer
//  W White pointer
//  T Tail pointer
//  | List sentinels
//
// Note that only the Scan and Free markers actually move
// The objects are classified between the pointer pairs as follows:
//
// GRAY:  Gray  (exclusive)  to Scan  (exclusive)
// BLACK: Scan  (inclusive)  to Free  (exclusive)
// FREE:  Free  (inclusive)  to White (exclusive)
// WHITE: White (exclusive)  to Tail  (exclusive)
//

//////////////////////////////////////////////////
// gmgmGCObjBase
//////////////////////////////////////////////////

#if GM_GC_DEBUG
enum
{
  GM_GC_DEBUG_COL_INVALID, //0
  GM_GC_DEBUG_COL_GRAY,    //1
  GM_GC_DEBUG_COL_BLACK,   //2
  GM_GC_DEBUG_COL_WHITE,   //3
  GM_GC_DEBUG_COL_FREE,    //4
};
#endif //GM_GC_DEBUG

/// \brief All GC objects are dervied from this class
class gmGCObjBase
{
public:

#if GM_GC_DEBUG
  gmGCObjBase()
  {
    m_curPosColor = GM_GC_DEBUG_COL_INVALID;
  }
  int m_curPosColor;
#endif //GM_GC_DEBUG

  inline void SetColor(int a_color)               {m_color = (char)a_color;}
  inline int GetColor()                           {return (int)m_color;}
  inline gmGCObjBase* GetPrev() const             {return m_prev;}
  inline void SetPrev(gmGCObjBase* a_prev)        {m_prev = a_prev;}
  inline gmGCObjBase* GetNext() const             {return m_next;}
  inline void SetNext(gmGCObjBase* a_next)        {m_next = a_next;}

  inline char GetPersist()                        {return m_persist;}
  inline void SetPersist(bool a_flag)             {m_persist = a_flag;}

  /// \brief Called when GC wants to free this memory
  virtual void Destruct(gmMachine * a_machine)    {}

  /// \brief Trace pointers this object contains to other objects.
  /// It must call gmGarbageCollector::GetNextObject() on each pointer,
  /// until it has traced all pointers, or used up the a_workLeftToGo count.
  /// \param a_workLeftToGo Number of pointers to trace.
  /// \param a_workDone, The number of pointers traced.
  /// \return true if finished tracing this object, false if not finished.
  virtual bool Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone) 
  {
    //NOTE: Use a_gc->GetTraceState().m_context to help resume incremental tracing.
    a_workDone = 1;
    return true;
  }

private:

  gmGCObjBase* m_prev;                            ///< Point to previous object in color set
  gmGCObjBase* m_next;                            ///< Point to next object in color set
  char m_color;                                   ///< Is gray or black flag, really only need by 1 bit
  char m_persist;                                 ///< This object is persistant
  char m_pad[2];                                  ///< Pad to dword
};

//////////////////////////////////////////////////
// gmGCColorSet
//////////////////////////////////////////////////

/// \brief Tri color managing class
class gmGCColorSet
{
public:

  /// \brief Constructor
  gmGCColorSet();
  /// \brief Destructor
  virtual ~gmGCColorSet();

  /// \brief Initialize members
  void Init(gmGarbageCollector* a_gc);

  /// \brief Returns true if there are any gray objects in this size class, false otherwise.
  inline  bool AnyGrays(void)
  {
    return (m_gray->GetNext() != m_scan);
  }

  int FollowPointers(int a_maxBytesToTrace);

  /// \brief Blacken the next object (at the scan pointer) in the gray set.
  /// Object is blackened and its children are grayed.
  /// Returns 0 when no grays are left.
  bool BlackenNextGray(int& a_workDone, int a_workLeftToGo);

  /// \brief Called by GCGetNextObject() that is called by user code while tracing over a object.
  inline void GrayAWhite(gmGCObjBase* a_obj);

  /// \brief Gray this object.
  void GrayThisObject(gmGCObjBase* a_obj);
  
  /// \brief Called on a new object being allocated.
  void Allocate(gmGCObjBase* a_obj);

  /// \brief This routine reclaims the garbage memory for the system.
  void ReclaimGarbage();

  /// \brief Destruct some free objects
  /// \return The number of objects destructed
  int DestructSomeFreeObjects(int a_maxToDestruct);

  /// \brief Destruct all objects.
  void DestructAll();

  /// \brief Make an object persistant by moving it into the persistant list.
  void MakePersistant(gmGCObjBase* a_obj)
  {
    // Fix scan (If scan == obj, obj must already be black, as we have just allocated or revived this string)
    if( m_scan == a_obj )
    {
      m_scan = m_scan->GetNext(); // Not Prev as scan should be the start of black inclusive
    }
    // Fix sentinels
    if( m_free == a_obj )
    {
      m_free = m_free->GetNext();
    }
 
    // Unlink from current list
    a_obj->GetNext()->SetPrev(a_obj->GetPrev());
    a_obj->GetPrev()->SetNext(a_obj->GetNext());
 
    // Insert at start of persist list
    a_obj->SetNext(m_persistList.GetNext());
    a_obj->SetPrev(&m_persistList);
   
    m_persistList.GetNext()->SetPrev(a_obj);
    m_persistList.SetNext(a_obj);
  }

  /// \brief Destruct all objects in persistant list
  void DestructPersistantObjects();

  /// \brief Revive an object before it is finalized
  void Revive(gmGCObjBase* a_obj);

  /// \brief Check if reference is valid for VM
  gmObject* CheckReference(gmptr a_ref);
  
  /// \brief Get instruction at point for VM Debugger.
  const void * GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line);

#if GM_GC_DEBUG
  bool VerifyIntegrity();
#endif //GM_GC_DEBUG

protected:

  gmGCObjBase* m_gray;
  gmGCObjBase* m_scan;
  gmGCObjBase* m_free;
  gmGCObjBase* m_white;
  gmGCObjBase* m_tail;

  gmGCObjBase m_tailObject;
  gmGCObjBase m_headObject;
  gmGCObjBase m_separatorObject;

  gmGCObjBase m_persistList;

  gmGarbageCollector* m_gc;
#if GM_GC_STATS
  int m_numAllocated;
#endif //GM_GC_STATS
};


//////////////////////////////////////////////////
// gmGarbageCollector
//////////////////////////////////////////////////

struct gmGCTraceState
{
  gmGCTraceState()
  {
    Reset();
  }

  void Reset()
  {
    m_done = true;
    m_object = NULL;
    m_context = NULL;
  }

  bool m_done;
  gmGCObjBase* m_object;
  void* m_context;
};


/// \brief Incremental garbage collection
/// Method: Tri-Color, Non-copying, Write barrier, Root Snapshot.
class gmGarbageCollector
{
public:

  /// \brief Constructor
  gmGarbageCollector();
  
  /// \brief Destructor
  virtual ~gmGarbageCollector();

  /// \brief Initialize the garbage collection system
  void Init(gmGCScanRootsCallBack a_scanRootsCallback, gmMachine* a_gmMachine);

  /// \brief Perform write barrier operation on Left and/or Right side objects.
  inline void WriteBarrier(gmGCObjBase* a_lObj /*, gmGCObjBase* a_rObj*/);

  /// \brief Call to start collection
  /// \return true if collection completed, false if more work to do.
  bool Collect();

  /// \brief Do a full collect and don't return until done.
  void FullCollect();

  /// \brief Called during trace by client code, and by scan roots callback.  
  /// This grays a white object.
  inline void GetNextObject(gmGCObjBase* a_obj)   {m_colorSet.GrayAWhite(a_obj);}

  /// \brief Called on a new object being allocated
  inline void AllocateObject(gmGCObjBase* a_obj)  {m_colorSet.Allocate(a_obj);}

  /// \brief Get the current shade color since it is flipped each cycle.
  inline int GetCurShadeColor()                   {return (m_curShadeColor);}

  /// \brief Is the object colored gray or black?
  inline bool IsShaded(gmGCObjBase* a_obj)        {return (a_obj->GetColor() == m_curShadeColor);}

  /// \brief if GC is turned off, reclaim garbage memory
  void ReclaimObjectsAndRestartCollection();

  /// \brief Get the trace state to resume incremental collecting
  inline gmGCTraceState& GetTraceState()          {return m_traceState;}

  /// \brief Set the amount of work to do per increment of collecting
  inline void SetWorkPerIncrement(int a_workPerIncrement)     {m_workPerIncrement = a_workPerIncrement;}
  /// \brief Set the amount of objects to destruct per increment of collecting
  inline void SetDestructPerIncrement(int a_destructPerIncrement)     {m_maxObjsToDestructPerIncrement = a_destructPerIncrement;}

  /// \brief Get the amount of work to do per increment of collecting
  inline int GetWorkPerIncrement()                {return m_workPerIncrement;}
  /// \brief Get the amount of objects to destruct per increment of collecting
  inline int GetDestructPerIncrement()            {return m_maxObjsToDestructPerIncrement;}

  /// \brief Set function to be called before flip when dead objects are reclaimed. 
  /// Optional, so pass NULL to disable.
  inline void SetFlipCallback(GCFlipCallBack a_flipCallback)  {m_flipCallback = a_flipCallback;}

  /// \brief Is collector currently turned off?
  inline bool IsOff()                             {return m_gcTurnedOff;}

  /// \brief Destruct all objects.
  void DestructAll();

  /// \brief Reclaim some free objects.
  int ReclaimSomeFreeObjects()                    {return m_colorSet.DestructSomeFreeObjects(m_maxObjsToDestructPerIncrement);}

  /// \brief Make an object persistant by moving it into the persistant list.
  void MakeObjectPersistant(gmGCObjBase* a_obj)   {m_colorSet.MakePersistant(a_obj);}

  /// \brief Get the virtual machine for language
  inline gmMachine* GetVM()                       {return m_gmMachine;}

  /// \brief Check if reference is valid for VM
  gmObject* CheckReference(gmptr a_ref)           {return m_colorSet.CheckReference(a_ref);}
  
  /// \brief Get instruction at point for VM Debugger.
  const void * GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line) {return m_colorSet.GetInstructionAtBreakPoint(a_sourceId, a_line);}

  /// \brief Revive a dead object (only used to re-live a shared string before it is finalized)
  void Revive(gmGCObjBase* a_obj)                 
  { 
    if( !a_obj->GetPersist() ) 
    {
      m_colorSet.Revive(a_obj); 
    } 
  }

protected:

  /// \brief Has Collect() completed yet?
  inline bool IsDoneTracing()                     {return m_doneTracing;}

  /// \brief Called by Collect()
  bool BlackenGrays();

  /// \brief Flip system and reclaim garbage.
  void Flip();

  /// \brief Toggle bit used to represent 'colored'
  inline void ToggleCurShadeColor()               {m_curShadeColor = !m_curShadeColor;}

  gmGCColorSet m_colorSet;                        ///< Tri color helper class
  int m_curShadeColor;                            ///< Cur color used to shade this generation
  int m_workPerIncrement;                         ///< How much work to do per increment
  int m_workLeftToGo;                             ///< How much work left in this increment
  int m_maxObjsToDestructPerIncrement;            ///< How much destructing work to do this frame?
  bool m_gcTurnedOff;                             ///< Is the GC currently turned off?
  bool m_firstCollectionIncrement;                ///< Using snapshot method, scan roots atomically first
  bool m_fullThrottle;                            ///< Set to true when forcing a full collection
  bool m_doneTracing;                             ///< Has Collect() completed yet?
  gmGCTraceState m_traceState;                    ///< Allows trace to resume where it left off

  GCFlipCallBack m_flipCallback;                  ///< Called before flip, when dead objects are reclaimed. Default is NULL.
  gmGCScanRootsCallBack m_scanRootsCallback;      ///< Called at start of Collect() to add gray all roots. MUST be implemented.
  gmMachine* m_gmMachine;                         ///< Virtual machine to pass around
};

//////////////////////////////////////////////////
// inline functions
//////////////////////////////////////////////////

void gmGCColorSet::GrayAWhite(gmGCObjBase* a_obj)
{
#if  GM_GC_KEEP_PERSISTANT_SEPARATE
  if(a_obj->GetPersist()) // Don't do anything with persistant objects
  {
    return;
  }
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE

  // If right object is not shaded, shade it
  if(!m_gc->IsShaded(a_obj))
  {
    GrayThisObject(a_obj);
  }
}


void gmGarbageCollector::WriteBarrier(gmGCObjBase* a_lObj/*, gmGCObjBase* a_rObj*/)
{
  // If we are allocating black and the collector is off, do nothing
  if(m_gcTurnedOff) 
  { 
    return;
  }

  // We don't need to use write barrier on root objects, so check for it if we can.
  // if(IsRoot(a_lObj)) { return; }

  // Note: Left side is logically the old right side

  // This is a snapshot write barrier.  There is not old pointer to overwrite, so do nothing.
  if(!a_lObj) 
  { 
    return; 
  }

#if GM_GC_KEEP_PERSISTANT_SEPARATE
  if(a_lObj->GetPersist()) // Don't do anything with persistant objects
  {
    return;
  }
#endif //GM_GC_KEEP_PERSISTANT_SEPARATE

  if(!IsShaded(a_lObj)) 
  { 
    m_colorSet.GrayThisObject(a_lObj);
  }
}

#endif //_GMINCGC_H_
