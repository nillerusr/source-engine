//
// ScriptObj.cpp
//

#include "gmCall.h"
#include "ScriptObj.h"
#include "ScriptSys.h"
#include "GameObj.h"


// Init statics and constants
gmType ScriptObj::GMTYPE_GAMEOBJ = -1;


ScriptObj::ScriptObj(GameObj* a_gameObj)
{
  GM_ASSERT(ScriptSys::Get());

  m_userObject = NULL; // A user object will be created when it is first used, and shared by all script variables.
  m_gameObj = a_gameObj;
  m_tableObject = ScriptSys::Get()->GetMachine()->AllocTableObject();

  ScriptSys::Get()->GetMachine()->AddCPPOwnedGMObject(m_tableObject);
}


ScriptObj::~ScriptObj()
{
  // Stop related threads
  KillThreads();

  if(m_userObject)
  {
    // Nullify script link to C object
    m_userObject->m_user = NULL;
  }

  // Destruct the gmObjects
#if GM_USE_INCGC
  // Do nothing, it will be collected later, just nullify all reference to it
#else
  if(m_userObject)
  {
    m_userObject->Destruct(ScriptSys::Get()->GetMachine());
  }
  m_tableObject->Destruct(ScriptSys::Get()->GetMachine());
#endif

  // Remove object from the list of all objects
  ScriptSys::Get()->GetMachine()->RemoveCPPOwnedGMObject(m_tableObject);
  if( m_userObject )
  {
    ScriptSys::Get()->GetMachine()->RemoveCPPOwnedGMObject(m_userObject);
  }
}


gmUserObject* ScriptObj::GetUserObject()
{
  if(!m_userObject)
  {
    m_userObject = ScriptSys::Get()->GetMachine()->AllocUserObject(this, GMTYPE_GAMEOBJ);
    ScriptSys::Get()->GetMachine()->AddCPPOwnedGMObject(m_userObject);
  }

  return m_userObject;
}


void ScriptObj::KillThreads()
{
  for(unsigned int tIndex=0; tIndex<m_threads.GetSize(); ++tIndex)
  {
    ScriptSys::Get()->RemoveThreadIdButDontTouchGameObj(m_threads[tIndex]);
    ScriptSys::Get()->GetMachine()->KillThread(m_threads[tIndex]);
  }
  m_threads.Reset();
}


void ScriptObj::ExecuteStringOnThis(const char* a_string)
{
  gmMachine* machine = ScriptSys::Get()->GetMachine();
  gmVariable thisVar;
  thisVar.SetUser(GetUserObject());
  int threadId = GM_INVALID_THREAD;

  int errors = machine->ExecuteString(a_string, &threadId, true, NULL, &thisVar);
  if(errors)
  {
    bool first = true;
    const char * message;
    while((message = machine->GetLog().GetEntry(first)))
    {
      ScriptSys::Get()->LogError("%s\n", message);
    }
    machine->GetLog().Reset();
  }
  else
  {
    ScriptSys::Get()->AssociateThreadIdWithGameObj(threadId, *GetGameObj());
  }
}


bool ScriptObj::ExecuteGlobalFunctionOnThis(const char* a_functionName)
{
  gmVariable thisVar;
  thisVar.SetUser(GetUserObject());

  gmCall call;
  if(call.BeginGlobalFunction(ScriptSys::Get()->GetMachine(), a_functionName, thisVar, false))
  {
    call.End();
    return true;
  }

  return false;
}


void ScriptObj::SetMemberInt(const char* a_memberName, int a_int)
{
  ScriptSys::Get()->SetTableInt(a_memberName, a_int, m_tableObject);
}


void ScriptObj::SetMemberFloat(const char* a_memberName, float a_float)
{
  ScriptSys::Get()->SetTableFloat(a_memberName, a_float, m_tableObject);
}


void ScriptObj::SetMemberString(const char* a_memberName, const char* a_string, int a_strLength)
{
  ScriptSys::Get()->SetTableString(a_memberName, a_string, a_strLength, m_tableObject);
}


void ScriptObj::SetMemberGameObj(const char* a_memberName, GameObj* a_gameObj)
{
  ScriptSys::Get()->SetTableGameObj(a_memberName, a_gameObj, m_tableObject);
}


gmTableObject* ScriptObj::SetMemberTable(const char* a_memberName)
{
  return ScriptSys::Get()->SetTableTable(a_memberName, m_tableObject);
}


bool ScriptObj::GetMemberInt(const char* a_memberName, int& a_int)
{
  return ScriptSys::Get()->GetTableInt(a_memberName, a_int, m_tableObject);
}


bool ScriptObj::GetMemberFloat(const char* a_memberName, float& a_float)
{
  return ScriptSys::Get()->GetTableFloat(a_memberName, a_float, m_tableObject);
}


bool ScriptObj::GetMemberString(const char* a_memberName, String& a_string)
{
  return ScriptSys::Get()->GetTableString(a_memberName, a_string, m_tableObject);
}


bool ScriptObj::GetMemberGameObj(const char* a_memberName, GameObj*& a_gameObj)
{
  return ScriptSys::Get()->GetTableGameObj(a_memberName, a_gameObj, m_tableObject);
}


bool ScriptObj::GetMemberTable(const char* a_memberName, gmTableObject*& a_retTable)
{
  return ScriptSys::Get()->GetTableTable(a_memberName, a_retTable, m_tableObject);
}


void GM_CDECL ScriptObj::GameObjCallback_AsString(gmUserObject * a_object, char* a_buffer, int a_bufferLen)
{
  char mixBuffer[128];

  ScriptObj* scriptObj = (ScriptObj*)a_object->m_user;
  GameObj* gameObjPtr = NULL;

  if(scriptObj)
  {
    gameObjPtr = scriptObj->GetGameObj();
  }
  
  sprintf(mixBuffer,"CPtr: %x", gameObjPtr);

  int mixLength = strlen(mixBuffer);
  int useLength = gmMin(mixLength, a_bufferLen-1);
  GM_ASSERT(useLength > 0);
  strncpy(a_buffer, mixBuffer, useLength);
  a_buffer[useLength] = 0;
}


#if GM_USE_INCGC

bool GM_CDECL ScriptObj::GameObjCallback_GCTrace(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workRemaining, int& a_workDone)
{
  GM_ASSERT(a_object->m_userType == GMTYPE_GAMEOBJ);
  ScriptObj* scriptObj = (ScriptObj*)a_object->m_user;

  if(scriptObj)
  {
    a_gc->GetNextObject(scriptObj->GetTableObject());
  }
  a_workDone +=2;
  return true;
}


void GM_CDECL ScriptObj::GameObjCallback_GCDestruct(gmMachine * a_machine, gmUserObject * a_object)
{
  GM_ASSERT(a_object->m_userType == GMTYPE_GAMEOBJ);
  ScriptObj* scriptObj = (ScriptObj*)a_object->m_user;

  if(scriptObj)
  {
    scriptObj->m_userObject = NULL;
  }
}

#else //GM_USE_INCGC

void GM_CDECL ScriptObj::GameObjCallback_GCMark(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  GM_ASSERT(a_object->m_userType == GMTYPE_GAMEOBJ);
  ScriptObj* scriptObj = (ScriptObj*)a_object->m_user;

  if(scriptObj)
  {
    if(scriptObj->GetTableObject()->NeedsMark(a_mark))
    {
      scriptObj->GetTableObject()->Mark(a_machine, a_mark);
    }
  }
}


void GM_CDECL ScriptObj::GameObjCallback_GCCollect(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  GM_ASSERT(a_object->m_userType == GMTYPE_GAMEOBJ);
  ScriptObj* scriptObj = (ScriptObj*)a_object->m_user;

  if(scriptObj)
  {
    scriptObj->m_userObject = NULL;
  }
}

#endif //GM_USE_INCGC


// NOTE: If you wanted to enable other dot operator behavior
// here is the place to do it, in the GetDot and SetDot operators.
// This example merely uses the gmTable embedded in the GameObj
// to allow script functions and data to be members of this object type.
// GameObj also registers 'type' functions that are accessed via the
// dot operator.


void GM_CDECL ScriptObj::GameObj_GetDot(gmThread * a_thread, gmVariable * a_operands)
{
  //O_GETDOT = 0,       // object, "member"          (tos is a_operands + 2)
  GM_ASSERT(a_operands[0].m_type == GMTYPE_GAMEOBJ);

  gmUserObject* userObj = (gmUserObject*) GM_OBJECT(a_operands[0].m_value.m_ref);
  ScriptObj* scriptObj = (ScriptObj*)userObj->m_user;

  if(!scriptObj)
  {
    a_operands[0].Nullify();

    return;
  }

  a_operands[0] = scriptObj->GetTableObject()->Get(a_operands[1]);
}


void GM_CDECL ScriptObj::GameObj_SetDot(gmThread * a_thread, gmVariable * a_operands)
{
  //O_SETDOT,           // object, value, "member"   (tos is a_operands + 3)
  GM_ASSERT(a_operands[0].m_type == GMTYPE_GAMEOBJ);

  gmUserObject* userObj = (gmUserObject*) GM_OBJECT(a_operands[0].m_value.m_ref);
  ScriptObj* scriptObj = (ScriptObj*)userObj->m_user;

  if(scriptObj)
  {
    scriptObj->GetTableObject()->Set(a_thread->GetMachine(), a_operands[2], a_operands[1]);
  }
}


void ScriptObj::RegisterScriptBindings()
{
  gmMachine* machine = ScriptSys::Get()->GetMachine();
  
  GM_ASSERT(machine);

  // Register new user type
  GMTYPE_GAMEOBJ = machine->CreateUserType("GameObj");
  // Register garbage collection for our new type
#if GM_USE_INCGC
  machine->RegisterUserCallbacks(GMTYPE_GAMEOBJ, GameObjCallback_GCTrace, GameObjCallback_GCDestruct, GameObjCallback_AsString); 
#else //GM_USE_INCGC
  machine->RegisterUserCallbacks(GMTYPE_GAMEOBJ, GameObjCallback_GCMark, GameObjCallback_GCCollect, GameObjCallback_AsString); 
#endif //GM_USE_INCGC
  // Bind Get dot operator for our type
  machine->RegisterTypeOperator(GMTYPE_GAMEOBJ, O_GETDOT, NULL, GameObj_GetDot); 
  // Bind Set dot operator for our type
  machine->RegisterTypeOperator(GMTYPE_GAMEOBJ, O_SETDOT, NULL, GameObj_SetDot); 
  // Bind functions
//  machine->RegisterLibrary(regFuncList, sizeof(regFuncList) / sizeof(regFuncList[0])); 
  // Bind type functions
//  machine->RegisterTypeLibrary(GM_GOB, regTypeFuncList, sizeof(regTypeFuncList) / sizeof(regTypeFuncList[0]));
}