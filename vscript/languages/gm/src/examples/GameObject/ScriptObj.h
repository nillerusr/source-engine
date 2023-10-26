#ifndef SCRIPTOBJ_H
#define SCRIPTOBJ_H

//
// ScriptObj.h
//
// Example script interface component for game object
//

#include "gmThread.h"
#include "StdStuff.h"

// Fwd decls
class GameObj;


// NOTE: In this implementation, the 'gmUserObject' only exists when the cpp object is used or needed by script.
//       This implementation also shares that single user object amongst all referencing variables in script.
//       Because of this, the cpp code does not need to handle the user object as if it were owned by cpp.
//       The cpp object does however always contain a gmTableObject, and this is owned by cpp as it may not 
//       exist (be referenced) within the script.  For this reason, the gmTableObject must be handled as a 
//       cpp owned object to allow correct GC handling.
//
//       An alternate method, would be to always have a gmUserObject and let cpp code own this.  This 
//       user object would be the root of its own child objects like the gmTableObject.  This method may
//       be simpler.
//

// Script interface for game objects
class ScriptObj
{
public:

  static gmType GMTYPE_GAMEOBJ;                   ///< The user type of a game object

  static void RegisterScriptBindings();           ///< Register game object script bindings

  ScriptObj(GameObj* a_gameObj);
  virtual ~ScriptObj();

  GameObj* GetGameObj()                           { return m_gameObj; }

  gmTableObject* GetTableObject()                 { return m_tableObject; }
  gmUserObject* GetUserObject();

  void AddThreadId(int a_threadId)                
  {
    m_threads.InsertLast(a_threadId);
  } 

  void RemoveThreadId(int a_threadId)
  {
    for(unsigned int tIndex=0; tIndex < m_threads.GetSize(); ++tIndex)
    {
      if(m_threads[tIndex] == a_threadId)
      {
        m_threads.RemoveSwapLast(tIndex);
      }
    }
  }

  /// Kill all threads running on this object
  void KillThreads();

  void ExecuteStringOnThis(const char* a_string);

  bool ExecuteGlobalFunctionOnThis(const char* a_functionName);
  
  void SetMemberInt(const char* a_memberName, int a_int);
  void SetMemberFloat(const char* a_memberName, float a_float);
  void SetMemberString(const char* a_memberName, const char* a_string, int a_strLength = -1);
  void SetMemberGameObj(const char* a_memberName, GameObj* a_gameObj);
  gmTableObject* SetMemberTable(const char* a_memberName);

  bool GetMemberInt(const char* a_memberName, int& a_int);
  bool GetMemberFloat(const char* a_memberName, float& a_float);
  bool GetMemberString(const char* a_memberName, String& a_string);
  bool GetMemberGameObj(const char* a_memberName, GameObj*& a_gameObj);
  bool GetMemberTable(const char* a_memberName, gmTableObject*& a_retTable);

protected:

  static void GM_CDECL GameObjCallback_AsString(gmUserObject * a_object, char* a_buffer, int a_bufferLen);
#if GM_USE_INCGC
  static bool GM_CDECL GameObjCallback_GCTrace(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workRemaining, int& a_workDone);
  static void GM_CDECL GameObjCallback_GCDestruct(gmMachine * a_machine, gmUserObject * a_object);
#else //GM_USE_INCGC
  static void GM_CDECL GameObjCallback_GCMark(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark);
  static void GM_CDECL GameObjCallback_GCCollect(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark);
#endif //GM_USE_INCGC
  static void GM_CDECL GameObj_GetDot(gmThread * a_thread, gmVariable * a_operands);
  static void GM_CDECL GameObj_SetDot(gmThread * a_thread, gmVariable * a_operands);

  GameObj* m_gameObj;                             ///< The game object owner of this interface
  gmUserObject* m_userObject;                     ///< The script object
  gmTableObject* m_tableObject;                   ///< Table functionality for script object members
  gmArraySimple<int> m_threads;                   ///< Threads associated with this game object

};

/*
/// \brief Get 'this' as GameObj of TYPE
/// Eg. Soldier* obj = GetThisGameObj<Soldier>(a_thread);
template<class TYPE>
TYPE* GetThisGameObj(gmThread* a_thread)
{
  GM_ASSERT(a_thread->GetThis()->m_type == ScriptObj::GMTYPE_GAMEOBJ); //Paranoid check for type function

  ScriptObj* scriptObj = (ScriptObj*)a_thread->ThisUser();

  CHECK(scriptObj); //Check for null GameObj ptr

  // You can check for valid derived type here

  return static_cast<TYPE*>(scriptObj->GetGameObj());
}


/// \brief Get param as GameObj of TYPE
/// Eg. Soldier* obj = GetGameObjParam<Soldier>(a_thread, 0);
template<class TYPE>
TYPE* GetGameObjParam(gmThread* a_thread, int a_paramIndex)
{
  ScriptObj* scriptObj = (ScriptObj*)a_thread->ParamUserCheckType(a_paramIndex, ScriptObj::GMTYPE_GAMEOBJ);

  CHECK(scriptObj); //Check for null GameObj ptr

  // You can check for valid derived type here

  return static_cast<TYPE*>(scriptObj->GetGameObj());
}
*/


/// \brief Get 'this' as GameObj of TYPE
inline GameObj* GetThisGameObj(gmThread* a_thread)
{
  GM_ASSERT(a_thread->GetThis()->m_type == ScriptObj::GMTYPE_GAMEOBJ); //Paranoid check for type function

  ScriptObj* scriptObj = (ScriptObj*)a_thread->ThisUser();

  if(!scriptObj)
  {
    return NULL;
  }

  return scriptObj->GetGameObj();
}


/// \brief Get param as GameObj of TYPE
inline GameObj* GetGameObjParam(gmThread* a_thread, int a_paramIndex)
{
  ScriptObj* scriptObj = (ScriptObj*)a_thread->ParamUserCheckType(a_paramIndex, ScriptObj::GMTYPE_GAMEOBJ);

  if(!scriptObj)
  {
    return NULL;
  }

  return scriptObj->GetGameObj();
}


#endif //SCRIPTOBJ_H