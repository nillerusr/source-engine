#ifndef GMGCROOT_H
#define GMGCROOT_H

//
// gmGCRoot.h
//

#include "gmThread.h"
#include "gmGCRootUtil.h"


///////////////////////////////////////////////////////////////////////////////
//
// gmGCRoot 
//
// Templated smart pointer style wrapper for gmObject*
// Automatically adds and removed CPP owned gmObject* from gmMachine roots
//
// It works by storing the gmObject* inside a shared reference counted holder 
// object. When a gmObject* is first wrapped, a holder object is created to 
// store it. Subsequent initializations will share the same holder object.  It 
// is much more efficient to assign the pointer from one gmGCRoot to another 
// than create a new one from the raw gmObject*.
//
//
// To use, you must initialize and destroy the system with:
//   gmGCRootManager::Init();
//   gmGCRootManager::Destroy();
// When you destruct a gmMachine that was used by the system, call this first:
//   gmGCRootManager::Get()->DestroyMachine(myMachine);
// Initialize a pointer by either:
//   gmGCRoot<gmStringObject> ptr1(myObject, myMachine);
//   gmGCRoot<gmStringObject> ptr2;  ptr2.Set(myObject, myMachine);
// Copy and test pointers as you would a C style pointer eg:
//   if( ptr1 ) { int type = ptr1->GetType(); }
//   ptr2 = ptr1;
//   ptr1 = NULL;
//
//
// Example usage:
//   gmGCRootManager::Init();                            // Call once first eg. App initialization
//   gmGCRoot<gmStringObject> ptr1;                      // A null pointer
//   gmGCRoot<gmStringObject> ptr2(myString, myMachine); // A pointer
//   gmStringObject* nat1 = ptr2;                        // Cast to native
//   gmGCRoot<gmStringObject> ptr3 = ptr2;               // Assign from other (Fast copy of shared pointer)
//   ptr2 = NULL;                                        // Assign to null
//   ptr2 = nat1;                                        // COMPILE ERROR! can't assign without gmMachine
//   gmGCRootManager::Get()->DestroyMachine(myMachine);  // Call before destructing a gmMachine
//   gmGCRootManager::Destroy();                         // Call once before App exit
//
///////////////////////////////////////////////////////////////////////////////

// Fwd decls
class gmGCRootManager;
class gmgcrHolder;


/// Bass class for objects reference counted types
class gmgcrRefCount
{
public:

  /// Construct, starts reference count at 0 (Must be wrapped in smart ptr to increment and later release)
  gmgcrRefCount()                                 { m_referenceCount = 0; }
  /// Destructor
  virtual ~gmgcrRefCount()                        { }

  /// Get reference count
  int GetReferenceCount() const                   { return m_referenceCount; }

  /// Increment reference count
  void AddReference() const                       { ++m_referenceCount; }

  /// Decrement reference count
  int ReleaseReference() const
  { 
    GM_ASSERT( m_referenceCount > 0 );
    if( --m_referenceCount == 0 )
    {
      DeleteThis();
      return true;
    }
    return false;
  }

  /// Delete this object.  May override if this is not desired behavior.
  /// NOTE: This function is 'const' so override with const also!
  virtual void DeleteThis() const                 { delete this; }
  
private:

  mutable int m_referenceCount;                   ///< Reference counter
  
};


/// Manager for gm gc roots
class gmGCRootManager
{
public:

  /// Access singleton
  static gmGCRootManager* Get()                   { return s_staticInstance; }
  
  /// Initialize gm gcroot manager
  static void Init();
  
  /// Destroy gm gcroot manager
  static void Destroy();
  
  ~gmGCRootManager();

  // Find or Add shard holder for gmObject
  gmgcrHolder* FindOrAdd(gmObject* a_object, gmMachine* a_machine);
  
  // Free holder memory only
  void FreeHolder(const gmgcrHolder* a_holder);
  
  // Dissassociate holder contents
  void RemoveObject(gmObject* a_object, gmMachine* a_machine);
  
  /// Call before destroying a gmMachine so that any associated objects can be disassociated and have pointers nullified.
  void DestroyMachine(gmMachine* a_machine);
  
protected:

  // Store gmObjects per machine
  class MachineHolders
  {
  public:
    gmMachine* m_machine;
    //gmgcrMap m_mapPtrToHolder;
    gmgcrMap<gmObject*, gmgcrHolder*> m_mapPtrToHolder;
  };

  static gmGCRootManager* s_staticInstance;         // Singleton
  
  gmgcrArray< MachineHolders > m_machineHolderSet;  // Holders per machine
  gmgcrMemFixed m_memHolder;                        // Holder memory
  
  // Non public constructor to prevent multiple instances
  gmGCRootManager();
  
  // Find or add machine
  MachineHolders* FindOrAddMachine(gmMachine* a_machine);

  // Destroy all machine associations
  void DestroyAllMachines();
  
};


/// Holder that is always valid and may contain the real pointer
class gmgcrHolder : public gmgcrRefCount
{
public:

  /// Default contsructor for simple allocation, Init() should be called immediately.
  gmgcrHolder()
  {
    m_object = NULL;
    m_machine = NULL;
  }
  
  /// Initialize with real object
  void Init(gmObject* a_object,
            gmMachine* a_machine)
  {
    m_object = a_object;
    m_machine = a_machine;
  }

  /// Destroy object pointer (May occur before this holder dies)
  void Destroy() const
  {
    if( m_object )
    {
      GM_ASSERT( gmGCRootManager::Get() );
      gmGCRootManager::Get()->RemoveObject(m_object, m_machine);
    }
    
    m_object = NULL;
    m_machine = NULL;
  }

  /// Get the real pointer
  void* GetPtr()                              { return m_object; }

private:

  /// Called when reference count reaches zero
  void DeleteThis() const
  {
    Destroy();
    
    GM_ASSERT( gmGCRootManager::Get() );
    
    gmGCRootManager::Get()->FreeHolder(this);
  }

  mutable gmObject* m_object;                     ///< The object
  mutable gmMachine* m_machine;                   ///< The owning machine
  
};


/// This is the smart pointer to a gmObject
/// It adds the gmObject to the gmMachine GC roots so that the object is not collected while held
/// Initialize via constructor or Set(). eg. gmGCRoot<gmStringObject> ptr(myString, myMachine);
/// Fast cast to native type and fast copy to compatible pointer.
/// Slower FIRST construct and FINAL destruct as shared holder object performs map insert/removal.
/// If gmMachine is destructed, contents will be nullified. (As long as manager was called correctly.)
template< typename TYPE >
class gmGCRoot
{
public:

  /// Empty Constructor.  Equals NULL.
  gmGCRoot()                                      { m_ptrToHolder = NULL; }
  
  // NOTE: Currently not very safe since holder and controller are not typed.
  /// Construct from holder
  explicit gmGCRoot(gmgcrHolder* a_holder)
  {
    m_ptrToHolder = a_holder;
    if( m_ptrToHolder )
    {
      m_ptrToHolder->AddReference();
    }
  }

  /// Construct from pointers
  gmGCRoot(TYPE* a_object, gmMachine* a_machine)
  {
    m_ptrToHolder = NULL;
    Set(a_object, a_machine);
  }

  /// Set pointer
  void Set(TYPE* a_object, gmMachine* a_machine)
  {
    if( m_ptrToHolder )
    {
      m_ptrToHolder->ReleaseReference();
    }

    if( !a_object ) // Handle null assignment
    {
      m_ptrToHolder = NULL;
      return;
    }
   
    GM_ASSERT( gmGCRootManager::Get() );
   
    m_ptrToHolder = gmGCRootManager::Get()->FindOrAdd( (gmObject*)a_object, (gmMachine*) a_machine );
    m_ptrToHolder->AddReference();
  }

  /// Copy Constructor
  gmGCRoot(const gmGCRoot<TYPE>& a_ref)
  {
    m_ptrToHolder = a_ref.m_ptrToHolder;
    if (m_ptrToHolder != NULL)
    {
      m_ptrToHolder->AddReference();
    }
  }
  
  /// Destructor
  ~gmGCRoot()
  {
    if( m_ptrToHolder )
    {
      m_ptrToHolder->ReleaseReference();
    }
  }
  
  /// Access the referenced type
  operator TYPE* ()                               { return (TYPE*)m_ptrToHolder->GetPtr(); }
  operator const TYPE* () const                   { return (TYPE*)m_ptrToHolder->GetPtr(); }
  
  /// Access the object as pointer
  TYPE* operator -> ()                            { return (TYPE*)m_ptrToHolder->GetPtr(); }
  const TYPE* operator -> () const                { return (TYPE*)m_ptrToHolder->GetPtr(); }
  
  /// Access the object as reference
  TYPE& operator * ()                             { return *((TYPE*)m_ptrToHolder->GetPtr()); }
  const TYPE& operator * () const                 { return *((TYPE*)m_ptrToHolder->GetPtr()); }

  /// Access object with standard function instead of operator.  Useful when compiler finds conversion ambigous.
  TYPE* Resolve()                                 { return (TYPE*)m_ptrToHolder->GetPtr(); }
  const TYPE* Resolve() const                     { return (TYPE*)m_ptrToHolder->GetPtr(); }
  
  /// Assign from other reference
  gmGCRoot<TYPE>& operator = (const gmGCRoot<TYPE>& a_ref)
  {
    if( m_ptrToHolder != a_ref.GetHolder() )
    {
      if( m_ptrToHolder != NULL )
      {
        m_ptrToHolder->ReleaseReference();
      }
      m_ptrToHolder = a_ref.GetHolder();
      if(m_ptrToHolder)
      {
        m_ptrToHolder->AddReference();
      }
    }
    return *this;
  }

  /// Assign to NULL.  Make shorter code path for assignment with zero (null in C++)
  gmGCRoot<TYPE>& operator = (int a_null)
  {
    GM_ASSERT(a_null == 0);
    if( m_ptrToHolder != NULL )
    {
      m_ptrToHolder->ReleaseReference();
    }
    m_ptrToHolder = NULL;
    return *this;
  }
  
  /// Equality test
  int operator == (const gmGCRoot<TYPE>& a_ref) const
  {
    if( m_ptrToHolder == a_ref.GetHolder() )
    {
      return true;
    }
    // Both holders cannot be NULL at this point, but one maybe.
    if( (a_ref.GetHolder() == NULL) && (m_ptrToHolder->GetPtr() == NULL) )
    {
        return true;
    }
    if( m_ptrToHolder == NULL )
    {
      if( a_ref.GetHolder()->GetPtr() == NULL )
      {
        return true;
      }
    }
    else
    {
      if( a_ref.GetHolder() != NULL )
      {
        if( a_ref.GetHolder()->GetPtr()  == m_ptrToHolder->GetPtr() )
        {
          return true;
        }
      }
    }
    return false;
  }

  /// Non equality test
  int operator != (const gmGCRoot<TYPE>& a_ref) const { return !operator==(a_ref); }
  
  /// Less than comparison for use in Containers
  int operator < (const gmGCRoot<TYPE>& a_ref) const
  {
    if( a_ref.GetHolder() == NULL )
    {
      return false;
    }
    if( m_ptrToHolder == NULL )
    {
      return true;
    }
    return ( m_ptrToHolder->GetPtr() < a_ref.GetHolder()->GetPtr() );
  }
  
  /// Greater than comparison for use in Containers
  int operator > (const gmGCRoot<TYPE>& a_ref) const
  {
    if( m_ptrToHolder == NULL )
    {
      return false;
    }
    if( a_ref.GetHolder() == NULL )
    {
      return true;
    }
    return ( m_ptrToHolder->GetPtr() > a_ref.GetHolder()->GetPtr() );
  }

  /// Access the holder.  Necessary since this is a templated class.
  gmgcrHolder* GetHolder() const                  { return m_ptrToHolder; }

private:

  mutable gmgcrHolder* m_ptrToHolder;             ///< Pointer to holder of the real pointer
  
};




#endif //GMGCROOT_H
