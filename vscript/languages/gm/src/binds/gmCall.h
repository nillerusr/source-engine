/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMCALL_H_
#define _GMCALL_H_

#include "gmConfig.h"
#include "gmThread.h"
#include "gmMachine.h"

#undef GetObject //Fix for Win32 where GetObject is #defined

/// \class gmCall
/// \brief A helper class to call script functions from C
/// Warning: Do not store any of the reference type return variables (eg. GM_SRING).  
/// As the object may be garbage collected.  Instead, copy immediately as necessary.
class gmCall
{
public:

  /// \brief Constructor
  gmCall()
  {
#ifdef GM_DEBUG_BUILD
    m_locked = false;
#endif //GM_DEBUG_BUILD
  }

  /// \brief Begin the call of a global function
  /// \param a_machine Virtual machine instance 
  /// \param a_funcName Name of function
  /// \param a_this The 'this' used by the function.
  /// \param a_delayExecuteFlag Set true if you want function thread to not execute now.
  /// \return true on sucess, false if function was not found.
  GM_FORCEINLINE bool BeginGlobalFunction(gmMachine * a_machine, const char * a_funcName, 
                                          const gmVariable& a_this = gmVariable::s_null, 
                                          bool a_delayExecuteFlag = false)
  {
    GM_ASSERT(a_machine);

    gmStringObject * funcNameStringObj = a_machine->AllocPermanantStringObject(a_funcName); // Slow
    
    return BeginGlobalFunction(a_machine, funcNameStringObj, a_this, a_delayExecuteFlag);
  }

  /// \brief Begin the call of a global function
  /// \param a_machine Virtual machine instance 
  /// \param a_funcNameStringObj A string object that was found or created earlier, much faster than creating from c string.
  /// \param a_this The 'this' used by the function.
  /// \param a_delayExecuteFlag Set true if you want function thread to not execute now.
  /// \return true on sucess, false if function was not found.
  GM_FORCEINLINE bool BeginGlobalFunction(gmMachine * a_machine, gmStringObject * a_funcNameStringObj, 
                                          const gmVariable& a_this = gmVariable::s_null, 
                                          bool a_delayExecuteFlag = false)
  {
    GM_ASSERT(a_machine);
    GM_ASSERT(a_funcNameStringObj);
    
    gmVariable lookUpVar;
    gmVariable foundFunc;

    lookUpVar.SetString(a_funcNameStringObj);
    foundFunc = a_machine->GetGlobals()->Get(lookUpVar);

    if( GM_FUNCTION == foundFunc.m_type )         // Check found variable is a function
    {
      gmFunctionObject * functionObj = (gmFunctionObject *)foundFunc.m_value.m_ref; //Func Obj from variable
  
      return BeginFunction(a_machine, functionObj, a_this, a_delayExecuteFlag);
    }
    return false;
  }

  /// \brief Begin the call of a object function
  /// \param a_machine Virtual machine instance 
  /// \param a_funcName Name of function.
  /// \param a_tableObj The table on the object to look up the function.
  /// \param a_this The 'this' used by the function.
  /// \param a_delayExecuteFlag Set true if you want function thread to not execute now.
  /// \return true on sucess, false if function was not found.
  GM_FORCEINLINE bool BeginTableFunction(gmMachine * a_machine, const char * a_funcName, 
                                  gmTableObject * a_tableObj, const gmVariable& a_this = gmVariable::s_null, 
                                  bool a_delayExecuteFlag = false)
  {
    GM_ASSERT(a_machine);
    GM_ASSERT(a_funcName);

    gmStringObject * funcNameStringObj = a_machine->AllocPermanantStringObject(a_funcName); // Slow
    
    return BeginTableFunction(a_machine, funcNameStringObj, a_tableObj, a_this, a_delayExecuteFlag);
  }

  /// \brief Begin the call of a object function
  /// \param a_machine Virtual machine instance 
  /// \param a_funcNameStringObj A string object that was found or created earlier, much faster than creating from c string.
  /// \param a_tableObj The table on the object to look up the function.
  /// \param a_this The 'this' used by the function.
  /// \param a_delayExecuteFlag Set true if you want function thread to not execute now.
  /// \return true on sucess, false if function was not found.
  GM_FORCEINLINE bool BeginTableFunction(gmMachine * a_machine, gmStringObject * a_funcNameStringObj, 
                                  gmTableObject * a_tableObj, const gmVariable& a_this = gmVariable::s_null, 
                                  bool a_delayExecuteFlag = false)
  {
    GM_ASSERT(a_machine);   
    //GM_ASSERT(a_tableObj);
    GM_ASSERT(a_funcNameStringObj);

    gmVariable lookUpVar;
    gmVariable foundFunc;

	if ( !a_tableObj )
	{
		a_tableObj = a_machine->GetGlobals();
	}

    lookUpVar.SetString(a_funcNameStringObj);
    foundFunc = a_tableObj->Get(lookUpVar);

    if( GM_FUNCTION == foundFunc.m_type )         // Check found variable is a function
    {
      gmFunctionObject * functionObj = (gmFunctionObject *)foundFunc.m_value.m_ref; //Func Obj from variable
      return BeginFunction(a_machine, functionObj, a_this, a_delayExecuteFlag);
    }
    return false;
  }

  /// \brief Begin the call of a object function
  /// \param a_funcObj A function object that was found or created earlier.
  /// \param a_tableObj The table on the object to look up the function.
  /// \param a_this The 'this' used by the function.
  /// \param a_delayExecuteFlag Set true if you want function thread to not execute now.
  /// \return true on sucess, false if function was not found.
  GM_FORCEINLINE bool BeginFunction(gmMachine * a_machine, gmFunctionObject * a_funcObj, 
                                    const gmVariable &a_thisVar = gmVariable::s_null, 
                                    bool a_delayExecuteFlag = false)
  {
    GM_ASSERT(a_machine);   
    GM_ASSERT(a_funcObj);

#ifdef GM_DEBUG_BUILD
    // YOU CANNOT NEST gmCall::Begin
    GM_ASSERT(m_locked == false);
    m_locked = true;
#endif //GM_DEBUG_BUILD

    Reset(a_machine);

    if( GM_FUNCTION == a_funcObj->GetType() )         // Check found variable is a function
    {
      m_thread = m_machine->CreateThread();     // Create thread for func to run on      
      m_thread->Push(a_thisVar);                // this
      m_thread->PushFunction(a_funcObj);        // function
      m_delayExecuteFlag = a_delayExecuteFlag;
      return true;
    }

#ifdef GM_DEBUG_BUILD
    GM_ASSERT(m_locked == true);
    m_locked = false;
#endif //GM_DEBUG_BUILD

    return false;
  }
 
  /// \brief Add a parameter variable
  GM_FORCEINLINE void AddParam(const gmVariable& a_var)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->Push(a_var);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is null
  GM_FORCEINLINE void AddParamNull()
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->PushNull();
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a integer
  GM_FORCEINLINE void AddParamInt(const int a_value)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->PushInt(a_value);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a float
  GM_FORCEINLINE void AddParamFloat(const float a_value)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->PushFloat(a_value);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a string
  GM_FORCEINLINE void AddParamString(const char * a_value, int a_len = -1)
  {
    GM_ASSERT(m_machine);    
    GM_ASSERT(m_thread);

    m_thread->PushNewString(a_value, a_len);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a string (faster version since c string does not need lookup)
  GM_FORCEINLINE void AddParamString(gmStringObject * a_value)
  {
    GM_ASSERT(m_machine);    
    GM_ASSERT(m_thread);

    m_thread->PushString(a_value);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a user object.  Creates a new user object.
  /// \param a_value Pointer to user object data
  /// \param a_userType Type of user object beyond GM_USER
  GM_FORCEINLINE void AddParamUser(void * a_value, int a_userType)
  {
    GM_ASSERT(m_machine);    
    GM_ASSERT(m_thread);

    m_thread->PushNewUser(a_value, a_userType);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a user object.
  /// \param a_userObj Pushes an existing user object without creating a new one.
  GM_FORCEINLINE void AddParamUser(gmUserObject * a_userObj)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->PushUser(a_userObj);
    ++m_paramCount;
  }

  /// \brief Add a parameter that is a table object.
  /// \param a_tableObj Pushes an existing table object without creating a new one.
  GM_FORCEINLINE void AddParamTable(gmTableObject * a_tableObj)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);

    m_thread->PushTable(a_tableObj);
    ++m_paramCount;
  }

  /// \brief Make the call.  If a return value was expected, it will be set in here.
  /// \param a_threadId Optional 
  GM_FORCEINLINE void End(int * a_threadId = NULL)
  {
    GM_ASSERT(m_machine);
    GM_ASSERT(m_thread);
    
#ifdef GM_DEBUG_BUILD
    // CAN ONLY CALL ::End() after a successful ::Begin
    GM_ASSERT(m_locked == true);
    m_locked = false;
#endif //GM_DEBUG_BUILD
        
    int state = m_thread->PushStackFrame(m_paramCount);
    if(state != gmThread::KILLED) // Can be killed immedialy if it was a C function
    {
      if(m_delayExecuteFlag)
      {
        state = m_thread->GetState();
      }
      else
      {
        state = m_thread->Sys_Execute(&m_returnVar);
      }
    }
    else
    {
      // Was a C function call, grab return var off top of stack
      m_returnVar = *(m_thread->GetTop() - 1);
      m_machine->Sys_SwitchState(m_thread, gmThread::KILLED);
    }

    // If we requested a thread Id
    if(a_threadId)
    {
      if(state != gmThread::KILLED)
      {
        *a_threadId = m_thread->GetId();
      }
      else
      {
        *a_threadId = GM_INVALID_THREAD;
      }
    }

    if(state == gmThread::KILLED)
    {
      m_returnFlag = true; // Function always returns something, null if not explicit.
      m_thread = NULL; // Thread has exited, no need to remember it.
    }
  }

  /// \brief Accesss thread created for function call.
  GM_FORCEINLINE gmThread * GetThread() { return m_thread; }

  /// \brief Returns reference to 'return' variable.  Never fails, but variable may be a 'null' varaible if none was returned.
  const gmVariable& GetReturnedVariable() { return m_returnVar; }
  
  /// \brief Returns true if function exited and returned a variable.
  bool DidReturnVariable() { return m_returnFlag; }

  /// \brief Did function return a null?
  /// \return true if function returned null. 
  bool GetReturnedNull()
  {
    if(m_returnFlag && (m_returnVar.m_type == GM_NULL))
    {
      return true;
    }
    return false;
  }

  /// \brief Get returned int
  /// \return true if function returned an int. 
  bool GetReturnedInt(int& a_value)
  {
    if(m_returnFlag && (m_returnVar.m_type == GM_INT))
    {
      a_value = m_returnVar.m_value.m_int;
      return true;
    }
    return false;
  }

  /// \brief Get returned float
  /// \return true if function returned an float. 
  bool GetReturnedFloat(float& a_value)
  {
    if(m_returnFlag && (m_returnVar.m_type == GM_FLOAT))
    {
      a_value = m_returnVar.m_value.m_float;
      return true;
    }
    return false;
  }

  /// \brief Get returned string
  /// \return true if function returned an string. 
  bool GetReturnedString(const char *& a_value)
  {
    if(m_returnFlag && (m_returnVar.m_type == GM_STRING))
    {
      a_value = ((gmStringObject *)m_machine->GetObject(m_returnVar.m_value.m_ref))->GetString();
      return true;
    }
    return false;
  }

  /// \brief Get returned user
  /// \return true if function returned an user. 
  bool GetReturnedUser(void *& a_value, int a_userType)
  {
    if(m_returnFlag && (m_returnVar.m_type == a_userType))
    {
      a_value = (void *)m_returnVar.m_value.m_ref;
      return true;
    }
    return false;
  }

  /// \brief Get returned user or null
  /// \return true if function returned an user or null. 
  bool GetReturnedUserOrNull(void *& a_value, int a_userType)
  {
    if(m_returnFlag)
    {
      if(m_returnVar.m_type == a_userType)
      {
        a_value = (void *)m_returnVar.m_value.m_ref;
        return true;
      }
      else if(m_returnVar.m_type == GM_NULL)
      {
        a_value = (void *)NULL;
        return true;
      }
    }
    return false;
  }

protected:

  gmMachine * m_machine;
  gmThread * m_thread;
  gmVariable m_returnVar;
  int m_paramCount;
  bool m_returnFlag;
  bool m_delayExecuteFlag;
#ifdef GM_DEBUG_BUILD
  bool m_locked;
#endif //GM_DEBUG_BUILD

  /// \brief Used internally to clear call information.
  GM_FORCEINLINE void Reset(gmMachine * a_machine)
  {
    GM_ASSERT(a_machine);
    m_machine = a_machine;
    m_thread = NULL;
    m_returnVar.Nullify();
    m_returnFlag = false;
    m_paramCount = 0;
    m_delayExecuteFlag = false;
  };
 
};


#endif // _GMCALL_H_
