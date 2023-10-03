//
// gmGCRoot.cpp
//

#include "gmGCRoot.h"

// Must be last header
#include "memdbgon.h"


// Init statics and constants
gmGCRootManager* gmGCRootManager::s_staticInstance;


gmGCRootManager::gmGCRootManager()
{
  m_machineHolderSet.Init(0, 4);
}


gmGCRootManager::~gmGCRootManager()
{
  DestroyAllMachines();
}

 
void gmGCRootManager::Init()
{
  GM_ASSERT( !s_staticInstance );
  
  if( !s_staticInstance )
  {
    s_staticInstance = new gmGCRootManager;
  }
}


void gmGCRootManager::Destroy()
{
  if( s_staticInstance )
  {
    delete s_staticInstance;
    s_staticInstance = NULL;
  }
}


gmgcrHolder* gmGCRootManager::FindOrAdd(gmObject* a_object, gmMachine* a_machine)
{
  GM_ASSERT(a_object);
  GM_ASSERT(a_machine);
  
  if( !a_object || !a_machine )
  {
    return NULL;
  }
  
  MachineHolders* machineSet = FindOrAddMachine(a_machine);
  
  gmgcrHolder* holder = NULL;
  if( !machineSet->m_mapPtrToHolder.GetAt(a_object, holder) )
  {
    // Add new
    holder = m_memHolder.Alloc();
    holder->Init(a_object, a_machine);
    machineSet->m_mapPtrToHolder.SetAt(a_object, holder);
    
    a_machine->AddCPPOwnedGMObject(a_object); // Add to GC roots
  }
  return holder;
}


void gmGCRootManager::RemoveObject(gmObject* a_object, gmMachine* a_machine)
{
  if( a_object && a_machine )
  {
    a_machine->RemoveCPPOwnedGMObject(a_object); // Remove from GC roots

    gmGCRootManager::MachineHolders* machineSet = FindOrAddMachine(a_machine);

    machineSet->m_mapPtrToHolder.RemoveAt(a_object);
  }
}


void gmGCRootManager::FreeHolder(const gmgcrHolder* a_holder)
{
  if( a_holder )
  {
    m_memHolder.Free( const_cast<gmgcrHolder*>(a_holder) );
  }
}


void gmGCRootManager::DestroyMachine(gmMachine* a_machine)
{
  int foundIndex = -1;

  // Find existing set
  for(int mIndex=0; mIndex < m_machineHolderSet.GetSize(); ++mIndex)
  {
    if( m_machineHolderSet[mIndex].m_machine == a_machine )
    {
      foundIndex = mIndex;
      break;
    }
  }
  
  if( foundIndex >= 0 )
  {
    MachineHolders* destroySet = &m_machineHolderSet[foundIndex];
    
    // Cleanup all internal pointers
    gmgcrMap<gmObject*, gmgcrHolder*>::Iterator it;
    
    // Iterate carefully as we are deleting while iterating
    destroySet->m_mapPtrToHolder.GetFirst(it);
    while( !destroySet->m_mapPtrToHolder.IsNull(it) )
    {
      gmgcrHolder* toDelete = NULL;
      if( destroySet->m_mapPtrToHolder.GetValue(it, toDelete) )
      {
        // NOTE: Delete this found value after incrementing iterator
      }
      destroySet->m_mapPtrToHolder.GetNext(it);
      if( toDelete )
      {
        toDelete->Destroy();
      }
    }    

    // Remove machine set
    m_machineHolderSet.RemoveAt(foundIndex);
  }
}


gmGCRootManager::MachineHolders* gmGCRootManager::FindOrAddMachine(gmMachine* a_machine)
{
  // NOTE: Expect very small number of machines.  If this is not so, accelerate search.
  
  GM_ASSERT( a_machine );

  // Find existing set
  for(int mIndex=0; mIndex < m_machineHolderSet.GetSize(); ++mIndex)
  {
    if( m_machineHolderSet[mIndex].m_machine == a_machine )
    {
      return &m_machineHolderSet[mIndex];
    }
  }
  
  // Create new set
  int newIndex = m_machineHolderSet.AddEmpty();
  MachineHolders* newSet = &m_machineHolderSet[newIndex];
  newSet->m_machine = a_machine;
  return newSet;
}


void gmGCRootManager::DestroyAllMachines()
{
  while( m_machineHolderSet.GetSize() )
  {
    DestroyMachine( m_machineHolderSet[m_machineHolderSet.GetLastIndex()].m_machine );
  }
}


#if 0
// Test the gmGCRoot system
void TestGMGCRoot()
{ 
  gmGCRootManager::Init(); // Init system
  {
    gmMachine machine1;

    gmGCRoot<gmStringObject> ptr1;
    
    gmStringObject* stringObj1 = machine1.AllocStringObject("hello");
    
    ptr1.Set(stringObj1, &machine1); // Initialize via func
    
    gmStringObject* nat1 = ptr1; // Assign pointer
    
    gmGCRoot<gmStringObject> ptr2;
    ptr2 = ptr1;
    
    ptr1 = NULL; // Assign to null
    ptr2 = NULL;
    //ptr2 = nat1; // ERROR

    gmStringObject* stringObj2 = machine1.AllocStringObject("apple");
    gmGCRoot<gmStringObject> ptr3(stringObj2, &machine1); // Initialize via constructor
    gmGCRoot<gmStringObject> ptr4(stringObj2, &machine1); // Duplicate without copy

    gmGCRoot<gmStringObject> ptr5;
    {
      gmMachine machine2;

      gmStringObject* stringObj3 = machine2.AllocStringObject("bannana");
      ptr5.Set(stringObj3, &machine2);
     
      gmGCRootManager::Get()->DestroyMachine(&machine2); // Null associated pointers
    }
    if( ptr5 )
    {
      int i=1; // Won't get here
    }
    
  }
  gmGCRootManager::Destroy();
}
#endif
