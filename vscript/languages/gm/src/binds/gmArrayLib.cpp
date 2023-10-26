/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmArrayLib.h"
#include "gmThread.h"
#include "gmMachine.h"
#include "gmHelpers.h"

// Must be last header
#include "memdbgon.h"

//
//
// Implementation of array binding
//
//

#if GM_ARRAY_LIB

// Statics and globals
gmType GM_ARRAY = GM_NULL;
gmVariable gmUserArray::m_null;


bool gmUserArray::Construct(gmMachine * a_machine, int a_size)
{
  m_null.Nullify();
  m_array = NULL;
  m_size = 0;
  return Resize(a_machine, a_size);
}


void gmUserArray::Destruct(gmMachine * a_machine)
{
  if(m_array)
  {
    a_machine->Sys_Free(m_array);
    m_array = NULL;
  }
  m_size = 0;
}


bool gmUserArray::Resize(gmMachine * a_machine, int a_size)
{
  if(a_size < 0) a_size = 0;
  int copysize = (a_size > m_size) ? m_size : a_size;
  gmVariable * array = (gmVariable *) a_machine->Sys_Alloc(sizeof(gmVariable) * a_size);
  // copy contents.
  if(m_array)
  {
    memcpy(array, m_array, sizeof(gmVariable) * copysize);
    if(a_size > copysize)
    {
      memset(array + copysize, 0, sizeof(gmVariable) * (a_size - copysize));
    }
    a_machine->Sys_Free(m_array);
  }
  else
  {
    memset(array, 0, sizeof(gmVariable) * a_size);
  }
  m_array = array;
  m_size = a_size;
  return true;
}

#if GM_USE_INCGC
bool gmUserArray::Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  int i;
  for(i = 0; i < m_size; ++i)
  {
    if(m_array[i].IsReference())
    {
      gmObject * object = GM_MOBJECT(a_machine, m_array[i].m_value.m_ref);
      a_gc->GetNextObject(object);
      ++a_workDone;
    }
  }
  
  ++a_workDone;
  return true;
}
#else //GM_USE_INCGC
void gmUserArray::Mark(gmMachine * a_machine, gmuint32 a_mark)
{
  int i;
  for(i = 0; i < m_size; ++i)
  {
    if(m_array[i].IsReference())
    {
      gmObject * object = GM_MOBJECT(a_machine, m_array[i].m_value.m_ref);
      if(object->NeedsMark(a_mark)) object->Mark(a_machine, a_mark);
    }
  }
}
#endif //GM_USE_INCGC


bool gmUserArray::Shift(int a_shift)
{
  if(a_shift < 0) // shift left
  {
    a_shift = -a_shift;
    if(a_shift >= m_size)
    {
      memset(m_array, 0, m_size);
    }
    else
    {
      int size = m_size - a_shift;
      memmove(m_array, m_array + a_shift, sizeof(gmVariable) * size);
      memset(m_array + size, 0, sizeof(gmVariable) * a_shift);
    }
  }
  else if(a_shift > 0) // shift right
  {
    if(a_shift >= m_size)
    {
      memset(m_array, 0, m_size);
    }
    else
    {
      int size = m_size - a_shift;
      memmove(m_array + a_shift, m_array, sizeof(gmVariable) * size);
      memset(m_array, 0, sizeof(gmVariable) * a_shift);
    }
  }
  return true;
}


int gmUserArray::Move(int a_dest, int a_src, int a_size)
{
  int start = a_src;
  int dest = a_dest;
  int size = a_size;

  if(start < 0)
  {
    size += start;
    dest -= start;
    start = 0;
  }

  if(dest < 0)
  {
    size += dest;
    start -= dest;
    dest = 0;
  }

  if(size <= 0) return 0;
  if(start >= m_size) return 0;
  if(dest >= m_size) return 0;
  if((dest + size) < 0) return 0;

  if(start + size > m_size)
  {
    size = m_size - start;
  }
  if(dest + size > m_size)
  {
    size = m_size - dest;
  }
  if(size <= 0) return 0;

  GM_ASSERT(dest >= 0);
  GM_ASSERT(start >= 0);
  GM_ASSERT(start + size <= m_size);
  GM_ASSERT(dest + size <= m_size);

  memmove(m_array + dest, m_array + start, sizeof(gmVariable) * size);

  return size;
}

gmUserArray* gmUserArray_Create(gmMachine* a_machine, int a_size)
{
  gmUserArray * newArray = (gmUserArray *) a_machine->Sys_Alloc(sizeof(gmUserArray));
  newArray->Construct(a_machine, a_size);
  return newArray;
}


// functions

static int GM_CDECL gmfArray(gmThread * a_thread) // size
{
  GM_INT_PARAM(size, 0, 0);
  gmUserArray * array = (gmUserArray *) a_thread->GetMachine()->Sys_Alloc(sizeof(gmUserArray));
  array->Construct(a_thread->GetMachine(), size);
  a_thread->PushNewUser(array, GM_ARRAY);
  return GM_OK;
}

static int GM_CDECL gmfArraySize(gmThread * a_thread) // return size
{
  gmUserObject * arrayObject = a_thread->ThisUserObject();
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  if(arrayObject->m_user)
  {
    gmUserArray * array = (gmUserArray *) arrayObject->m_user;
    a_thread->PushInt(array->Size());
  }
  return GM_OK;
}

static int GM_CDECL gmfArrayResize(gmThread * a_thread) // size
{
  GM_INT_PARAM(size, 0, 0);
  gmUserObject * arrayObject = a_thread->ThisUserObject();
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  if(arrayObject->m_user)
  {
    gmUserArray * array = (gmUserArray *) arrayObject->m_user;
    array->Resize(a_thread->GetMachine(), size);
  }
  return GM_OK;
}

static int GM_CDECL gmfArrayShift(gmThread * a_thread) // shift
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(shift, 0);

  gmUserObject * arrayObject = a_thread->ThisUserObject();
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  if(arrayObject->m_user)
  {
    gmUserArray * array = (gmUserArray *) arrayObject->m_user;
    array->Shift(shift);
  }
  return GM_OK;
}

static int GM_CDECL gmfArrayMove(gmThread * a_thread) // dst, src, size
{
  GM_CHECK_NUM_PARAMS(3);
  GM_CHECK_INT_PARAM(dst, 0);
  GM_CHECK_INT_PARAM(src, 1);
  GM_CHECK_INT_PARAM(size, 2);

  gmUserObject * arrayObject = a_thread->ThisUserObject();
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  if(arrayObject->m_user)
  {
    gmUserArray * array = (gmUserArray *) arrayObject->m_user;
    array->Move(dst, src, size);
  }
  return GM_OK;
}

static void GM_CDECL gmArrayGetInd(gmThread * a_thread, gmVariable * a_operands)
{
  gmUserObject * arrayObject = (gmUserObject *) GM_OBJECT(a_operands->m_value.m_ref);
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  gmUserArray * array = (gmUserArray *) arrayObject->m_user;
  if(a_operands[1].m_type == GM_INT)
  {
    int index = a_operands[1].m_value.m_int;
    *a_operands = array->GetAt(index);
    return;
  }
  a_operands->Nullify();
}

static void GM_CDECL gmArraySetInd(gmThread * a_thread, gmVariable * a_operands)
{
  gmUserObject * arrayObject = (gmUserObject *) GM_OBJECT(a_operands->m_value.m_ref);
  GM_ASSERT(arrayObject->m_userType == GM_ARRAY);
  gmUserArray * array = (gmUserArray *) arrayObject->m_user;
  if(a_operands[1].m_type == GM_INT)
  {
    int index = a_operands[1].m_value.m_int;

#if GM_USE_INCGC
    //Apply write barrier
    gmVariable oldVar = array->GetAt(index);
    if(oldVar.IsReference())
    {
      a_thread->GetMachine()->GetGC()->WriteBarrier((gmObject*)oldVar.m_value.m_ref);
    }
#endif //GM_USE_INCGC

    array->SetAt(index, a_operands[2]);
  }
}

#if GM_USE_INCGC
static void GM_CDECL gmGCDestructArrayUserType(gmMachine * a_machine, gmUserObject* a_object)
{
  if(a_object->m_user) 
  {
    gmUserArray * array = (gmUserArray *) a_object->m_user;
    array->Destruct(a_machine);
    a_machine->Sys_Free(array);
  }
  a_object->m_user = NULL;
}

static bool GM_CDECL gmGCTraceArrayUserType(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  if(a_object->m_user) 
  {
    gmUserArray * array = (gmUserArray *) a_object->m_user;
    return array->Trace(a_machine, a_gc, a_workLeftToGo, a_workDone);
  }
  return true;
}

#else //GM_USE_INCGC
static void GM_CDECL gmMarkArrayUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  if(a_object->m_user) 
  {
    gmUserArray * array = (gmUserArray *) a_object->m_user;
    array->Mark(a_machine, a_mark);
  }
}

static void GM_CDECL gmGCArrayUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  if(a_object->m_user) 
  {
    gmUserArray * array = (gmUserArray *) a_object->m_user;
    array->Destruct(a_machine);
    a_machine->Sys_Free(array);
  }
  a_object->m_user = NULL;
}
#endif //GM_USE_INCGC

// libs

static gmFunctionEntry s_arrayLib[] = 
{ 
  /*gm
    \lib gm
  */
  /*gm
    \function array
    \brief array will create a fixed size array object
    \param int size optional (0)
    \return array
  */
  {"array", gmfArray},
};

static gmFunctionEntry s_arrayTypeLib[] = 
{ 
  /*gm
    \lib array
  */
  /*gm
    \function Size
    \brief Size will return the current size of the fixed array
    \return int array size
  */
  {"Size", gmfArraySize},
  /*gm
    \function Resize
    \brief Resize will resize the array to a new size
    \param int size optional (0)
    \return null
  */
  {"Resize", gmfArrayResize},
  /*gm
    \function Shift
    \brief Shift will shift slide the array elements by a delta, nulls are shifted in
    \param int delta
    \return null
  */
  {"Shift", gmfArrayShift},
  /*gm
    \function Move
    \brief Move will perform a non destructive move on the array
    \param int dst
    \param int src
    \param int size
    \return null
  */
  {"Move", gmfArrayMove},
};

void gmBindArrayLib(gmMachine * a_machine)
{
  gmUserArray::m_null.Nullify(); //Init static null

  a_machine->RegisterLibrary(s_arrayLib, sizeof(s_arrayLib) / sizeof(s_arrayLib[0]));
  GM_ARRAY = a_machine->CreateUserType("array");
  a_machine->RegisterTypeLibrary(GM_ARRAY, s_arrayTypeLib, sizeof(s_arrayTypeLib) / sizeof(s_arrayTypeLib[0]));
#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(GM_ARRAY, gmGCTraceArrayUserType, gmGCDestructArrayUserType);
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(GM_ARRAY, gmMarkArrayUserType, gmGCArrayUserType);
#endif //GM_USE_INCGC
  a_machine->RegisterTypeOperator(GM_ARRAY, O_GETIND, NULL, gmArrayGetInd);
  a_machine->RegisterTypeOperator(GM_ARRAY, O_SETIND, NULL, gmArraySetInd);
}

#endif // GM_ARRAY_LIB
