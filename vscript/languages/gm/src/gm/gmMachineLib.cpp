/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmMachineLib.h"
#include "gmThread.h"
#include "gmMachine.h"
#include "gmUtil.h"

// Must be last header
#include "memdbgon.h"

//
// machine
//

static int GM_CDECL gmVersion(gmThread * a_thread)
{
  a_thread->PushNewString(GM_VERSION);
  return GM_OK;
}


static int GM_CDECL gmTypeId(gmThread * a_thread) // return int, or null
{
  if(a_thread->GetNumParams() > 0)
  {
    a_thread->PushInt((gmptr) a_thread->Param(0).m_type);
  }
  return GM_OK;
}



static int GM_CDECL gmTypeName(gmThread * a_thread) // return string, or null
{
  if(a_thread->GetNumParams() > 0)
  {
    const char * name = a_thread->GetMachine()->GetTypeName(a_thread->Param(0).m_type);
    a_thread->PushNewString(name);
  }
  return GM_OK;
}



static int GM_CDECL gmRegisterTypeOperator(gmThread * a_thread) // typeid, operatorname, function, returns true on success
{
  GM_CHECK_NUM_PARAMS(3);
  GM_CHECK_INT_PARAM(typeId, 0);
  GM_CHECK_STRING_PARAM(operatorName, 1);
  GM_CHECK_FUNCTION_PARAM(function, 2);

  gmOperator op = gmGetOperator(operatorName);
  if(op != O_MAXOPERATORS)
  {
    a_thread->PushInt(a_thread->GetMachine()->RegisterTypeOperator((gmType) typeId, op, function) ? 1 : 0);
    return GM_OK;
  }
  a_thread->PushInt(0);
  return GM_OK;
}



static int GM_CDECL gmRegisterTypeVariable(gmThread * a_thread) // typeid, key (string), value
{
  GM_CHECK_NUM_PARAMS(3);
  GM_CHECK_INT_PARAM(typeId, 0);
  GM_CHECK_STRING_PARAM(variable, 1);

  a_thread->GetMachine()->RegisterTypeVariable((gmType) typeId, variable, a_thread->Param(2));
  
  return GM_OK;
}



static int GM_CDECL gmCollectGarbage(gmThread * a_thread) // returns true if gc was run
{
  GM_INT_PARAM(forceFullCollect, 0, false);

  a_thread->PushInt(a_thread->GetMachine()->CollectGarbage(forceFullCollect != 0) ? 1 : 0);
  return GM_OK;
}



static int GM_CDECL gmGetCurrentMemoryUsage(gmThread * a_thread) // returns current memory usage in bytes
{
  a_thread->PushInt(a_thread->GetMachine()->GetCurrentMemoryUsage());
  return GM_OK;
}


static int GM_CDECL gmSetDesiredMemoryUsageHard(gmThread * a_thread) // mem usage in bytes
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(mem, 0);

  a_thread->GetMachine()->SetDesiredByteMemoryUsageHard(mem);
  return GM_OK;
}


static int GM_CDECL gmSetDesiredMemoryUsageSoft(gmThread * a_thread) // mem usage in bytes
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(mem, 0);

  a_thread->GetMachine()->SetDesiredByteMemoryUsageSoft(mem);
  return GM_OK;
}


static int GM_CDECL gmGetDesiredMemoryUsageHard(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->GetDesiredByteMemoryUsageHard());
  return GM_OK;
}


static int GM_CDECL gmGetDesiredMemoryUsageSoft(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->GetDesiredByteMemoryUsageSoft());
  return GM_OK;
}


static int GM_CDECL gmSetDesiredMemoryUsageAuto(gmThread * a_thread) // mem usage in bytes
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(autoEnable, 0);

  a_thread->GetMachine()->SetAutoMemoryUsage(autoEnable != 0);
  return GM_OK;
}


static int GM_CDECL gmSysGetStatsGCNumFullCollects(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->GetStatsGCNumFullCollects());
  return GM_OK;
}


static int GM_CDECL gmSysGetStatsGCNumIncCollects(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->GetStatsGCNumIncCollects());
  return GM_OK;
}


static int GM_CDECL gmSysGetStatsGCNumWarnings(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->GetStatsGCNumWarnings());
  return GM_OK;
}


static int GM_CDECL gmSysIsGCRunning(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetMachine()->IsGCRunning());
  return GM_OK;
}


static int GM_CDECL gmDoString(gmThread * a_thread) // string, now(int), returns thread id, null on error, exception on compile error
{
  GM_CHECK_NUM_PARAMS(1); // Need at least 1 parameter
  GM_CHECK_STRING_PARAM(script, 0); // 1st param is script string
  GM_INT_PARAM(now, 1, 1); // 2nd param is execute now flag
  gmVariable paramThis = a_thread->Param(2, gmVariable::s_null); // 3rd param is 'this'

  int id = GM_INVALID_THREAD;
  if( script )
  {
    int errors = a_thread->GetMachine()->ExecuteString(script, &id, (now) ? true : false, NULL, &paramThis);
    if( errors )
    {
      return GM_EXCEPTION;
    }
    a_thread->PushInt(id);
  }
  return GM_OK;
}



static int GM_CDECL gmGlobals(gmThread * a_thread) // return table
{
  a_thread->PushTable(a_thread->GetMachine()->GetGlobals());
  return GM_OK;
}



static int GM_CDECL gmMachineTime(gmThread * a_thread) // return machine time
{
  a_thread->PushInt(a_thread->GetMachine()->GetTime());
  return GM_OK;
}



//
// thread
//



static int GM_CDECL gmSleep(gmThread * a_thread) // float\int param time in seconds
{
  GM_CHECK_NUM_PARAMS(1);
  gmType type = a_thread->ParamType(0);
  gmuint32 ms = 0;
  
  if(type == GM_INT) ms = a_thread->Param(0).m_value.m_int * 1000;
  else if(type == GM_FLOAT) ms = (gmuint32) floorf(a_thread->Param(0).m_value.m_float * 1000.0f);

  a_thread->Sys_SetTimeStamp(a_thread->GetMachine()->GetTime() + ms);
  return GM_SYS_SLEEP;
}



static int GM_CDECL gmYield(gmThread * a_thread)
{
  return GM_SYS_YIELD;
}



static int GM_CDECL gmThreadTime(gmThread * a_thread)
{
  a_thread->PushInt(a_thread->GetThreadTime());
  return GM_OK;
}



static int GM_CDECL gmThreadId(gmThread * a_thread) // return thread id
{
  a_thread->PushInt(a_thread->GetId());
  return GM_OK;  
}


// Callback iteration function for gmThreadAllIds
static bool gmThreadIdIter(gmThread * a_thread, void * a_context)
{
  gmTableObject* table = (gmTableObject*)a_context;
  gmVariable threadId;
  threadId.SetInt(a_thread->GetId());
  table->Set(a_thread->GetMachine(), table->Count(), threadId);
  return true;
}
 


static int GM_CDECL gmThreadAllIds(gmThread * a_thread) // thread id
{
  gmTableObject * threadIds = a_thread->PushNewTable();
  a_thread->GetMachine()->ForEachThread(gmThreadIdIter, threadIds);
  return GM_OK;
}



static int GM_CDECL gmExit(gmThread * a_thread)
{
  return GM_SYS_KILL;  
}



static int GM_CDECL gmKillThread(gmThread * a_thread) // thread id
{
  GM_INT_PARAM(id, 0, GM_INVALID_THREAD); // 1 optional param, default is this thread
  
  // Kill this thread
  if( (id == GM_INVALID_THREAD) || (id == a_thread->GetId()) )
  {
    return GM_SYS_KILL;  // Kill this thread
  }

  // Attempt to kill other thread by Id
  gmThread * thread = a_thread->GetMachine()->GetThread(id);
  if( thread )
  {
    thread->GetMachine()->Sys_SwitchState(thread, gmThread::KILLED); // Kill other thread
  }

  return GM_OK;
}


// Callback iteration function for gmKillAllThreads() 
// Kills all threads except current, current thread is passed in as context.
static bool threadIterKill(gmThread * a_thread, void * a_context)
{
  gmThread* caller = (gmThread*)a_context;
  
  if(a_thread != caller) // Ignore calling thread
  {
    switch(a_thread->GetState())
    {
      case gmThread::RUNNING:
      case gmThread::SLEEPING:
      case gmThread::BLOCKED:
      {
        // Kill the thread
        a_thread->GetMachine()->Sys_SwitchState(a_thread, gmThread::KILLED);
        break;
      }
      case gmThread::EXCEPTION:
      case gmThread::KILLED:
      default:
      {
        // Ignore threads of these states
        break;
      }
    }
  }

  return true;
}


static int GM_CDECL gmKillAllThreads(gmThread * a_thread) // thread id
{
  GM_INT_PARAM(killCurrent, 0, 0);

  a_thread->GetMachine()->ForEachThread(threadIterKill, a_thread);

  if(killCurrent)
  {
    return GM_SYS_KILL;
  }

  return GM_OK;
}



static int GM_CDECL gmfThread(gmThread * a_thread) // fn, params, returns thread id (0) on error, else returns new thread id
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_FUNCTION_PARAM(function, 0);

  int id, i;
  gmThread * thread = a_thread->GetMachine()->CreateThread(&id);
  if(thread)
  {
    thread->Push(*a_thread->GetThis());
    thread->PushFunction(function);
    int numParameters = a_thread->GetNumParams() - 1;
    for(i = 0; i < numParameters; ++i)
      thread->Push(a_thread->Param(i + 1));
    thread->PushStackFrame(numParameters, 0);
  }
  a_thread->PushInt(id);
  return GM_OK;
}



static int GM_CDECL gmAssert(gmThread * a_thread)
{
  if(a_thread->GetNumParams() > 0)
  {
    if(a_thread->Param(0).m_value.m_int)
    {
      return GM_OK;
    }
  }
  GM_STRING_PARAM(message, 1, "assert failed");
  a_thread->GetMachine()->GetLog().LogEntry("%s", message);
  return GM_EXCEPTION;
}



static gmType s_gmStateUserType = GM_NULL;

struct gmStateUserType
{
  gmFunctionObject * m_lastState; // last state
  gmFunctionObject * m_currentState; // current state
  gmFunctionObject * m_setExitState; // leave hanlder
};


static int GM_CDECL gmSetState(gmThread * a_thread) // fp, params
{
  GM_CHECK_NUM_PARAMS(GM_STATE_NUM_PARAMS);
  GM_CHECK_FUNCTION_PARAM(function, 0);

  // make sure we have our state type.
  GM_ASSERT(s_gmStateUserType != GM_NULL); 

  // save off the parameters to the new state
  gmVariable thisVar = *a_thread->GetThis();
  int i, numParameters = a_thread->GetNumParams() - GM_STATE_NUM_PARAMS;
  gmVariable * params = (gmVariable *) alloca(sizeof(gmVariable) * numParameters);
  for(i = 0; i < numParameters; ++i)
  {
    params[i] = a_thread->Param(i + GM_STATE_NUM_PARAMS);
  }

  // get the current state
  gmVariable newStateVariable;
  gmVariable * currentStateVariable = a_thread->GetBottom();
  if(currentStateVariable->m_type == s_gmStateUserType)
  {
    gmUserObject * userObj = (gmUserObject *) GM_OBJECT(currentStateVariable->m_value.m_ref);
    gmStateUserType * currentState = (gmStateUserType *) userObj->m_user;

    // call the on state leave if one exists.
    if(currentState->m_setExitState)
    {
      gmThread * thread = a_thread->GetMachine()->CreateThread(thisVar, gmVariable(GM_FUNCTION, currentState->m_setExitState->GetRef()));
      if(thread)
      {
        thread->Sys_Execute();
      }
    }

    currentState->m_setExitState = NULL;
    currentState->m_lastState = currentState->m_currentState;
    currentState->m_currentState = function;
    newStateVariable = *currentStateVariable;
  }
  else
  {
    gmStateUserType * state = (gmStateUserType *) a_thread->GetMachine()->Sys_Alloc(sizeof(gmStateUserType));
    state->m_setExitState = NULL;
    state->m_currentState = function;
    state->m_lastState = NULL;

    // create a new state variable
    newStateVariable.SetUser(a_thread->GetMachine()->AllocUserObject(state, s_gmStateUserType));
  }

  // reset the stack. and push new state
  int user = a_thread->m_user;
  a_thread->Sys_Reset(a_thread->GetId());
  a_thread->m_user = user;
  a_thread->Sys_SetStartTime(a_thread->GetMachine()->GetTime());
  a_thread->Touch(4 + numParameters);
  a_thread->Push(newStateVariable);
  a_thread->Push(thisVar);
  a_thread->PushFunction(function);
  for(i = 0; i < numParameters; ++i)
  {
    a_thread->Push(params[i]);
  }

  return GM_SYS_STATE;
}


static int GM_CDECL gmSetStateOnThread(gmThread * a_thread) // (threadid, fp, params...) returns true or false.
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(threadId, 0);
  GM_CHECK_FUNCTION_PARAM(function, 1);

  // make sure we have our state type.
  GM_ASSERT(s_gmStateUserType != GM_NULL); 

  // get the target thread
  gmThread * thread = a_thread->GetMachine()->GetThread(threadId);
  if(thread == a_thread)
  {
    a_thread->GetMachine()->GetLog().LogEntry("use setstate() on own thread");
    return GM_EXCEPTION;
  }

  if(thread == NULL)
  {
    return GM_OK;
  }

  // get the current state of the thread
  gmVariable newStateVariable;
  gmVariable thisVar = *thread->GetThis();
  gmVariable * currentStateVariable = thread->GetBottom();
  if(currentStateVariable->m_type == s_gmStateUserType)
  {
    gmUserObject * userObj = (gmUserObject *) GM_OBJECT(currentStateVariable->m_value.m_ref);
    gmStateUserType * currentState = (gmStateUserType *) userObj->m_user;

    // call the on state leave if one exists.
    if(currentState->m_setExitState)
    {
      gmThread * thread = a_thread->GetMachine()->CreateThread(thisVar, gmVariable(GM_FUNCTION, currentState->m_setExitState->GetRef()));
      if(thread)
      {
        thread->Sys_Execute();
      }
    }

    currentState->m_setExitState = NULL;
    currentState->m_lastState = currentState->m_currentState;
    currentState->m_currentState = function;
    newStateVariable = *currentStateVariable;
  }
  else
  {
    gmStateUserType * state = (gmStateUserType *) a_thread->GetMachine()->Sys_Alloc(sizeof(gmStateUserType));
    state->m_setExitState = NULL;
    state->m_currentState = function;
    state->m_lastState = NULL;

    // create a new state variable
    newStateVariable.SetUser(a_thread->GetMachine()->AllocUserObject(state, s_gmStateUserType));
  }

  // reset the stack. and push new state
  int numParameters = a_thread->GetNumParams() - 2;
  
  int user = thread->m_user;
  thread->Sys_Reset(thread->GetId());
  thread->m_user = user;
  thread->Sys_SetStartTime(thread->GetMachine()->GetTime());
  thread->Touch(4 + numParameters);
  thread->Push(newStateVariable);
  thread->Push(thisVar);
  thread->PushFunction(function);

  int i;
  for(i = 0; i < numParameters; ++i)
  {
    thread->Push(a_thread->Param(i+2));
  }

  thread->PushStackFrame(numParameters);
  a_thread->GetMachine()->Sys_SwitchState(thread, gmThread::RUNNING);

  return GM_OK;
}



static int GM_CDECL gmGetState(gmThread * a_thread) // return var
{
  GM_ASSERT(s_gmStateUserType != GM_NULL); 

  gmThread * testThread = a_thread;
  
  //Optional parameter, threadId
  if(a_thread->GetNumParams() >= 1)
  {
    GM_CHECK_INT_PARAM(testThreadId, 0);
    testThread = a_thread->GetMachine()->GetThread(testThreadId);
    if(!testThread)
    {
      a_thread->PushNull();
      return GM_OK;
    }
  }
  
  gmVariable * currentStateVariable = testThread->GetBottom();
  if(currentStateVariable->m_type == s_gmStateUserType)
  {
    gmUserObject * userObj = (gmUserObject *) GM_OBJECT(currentStateVariable->m_value.m_ref);
    gmStateUserType * currentState = (gmStateUserType *) userObj->m_user;
    a_thread->PushFunction(currentState->m_currentState);
  }
  return GM_OK;
}


static int GM_CDECL gmGetLastState(gmThread * a_thread) // return var
{
  GM_ASSERT(s_gmStateUserType != GM_NULL); 
  
  gmThread * testThread = a_thread;
  
  //Optional parameter, threadId
  if(a_thread->GetNumParams() >= 1)
  {
    GM_CHECK_INT_PARAM(testThreadId, 0);
    testThread = a_thread->GetMachine()->GetThread(testThreadId);
    if(!testThread)
    {
      a_thread->PushNull();
      return GM_OK;
    }
  }

  gmVariable * currentStateVariable = testThread->GetBottom();
  if(currentStateVariable->m_type == s_gmStateUserType)
  {
    gmUserObject * userObj = (gmUserObject *) GM_OBJECT(currentStateVariable->m_value.m_ref);
    gmStateUserType * currentState = (gmStateUserType *) userObj->m_user;
    if(currentState->m_lastState)
    {
      a_thread->PushFunction(currentState->m_lastState);
    }    
  }
  return GM_OK;
}



static int GM_CDECL gmSetExitState(gmThread * a_thread) // function
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_FUNCTION_PARAM(function, 0);

  GM_ASSERT(s_gmStateUserType != GM_NULL); 

  gmVariable * currentStateVariable = a_thread->GetBottom();
  if(currentStateVariable->m_type == s_gmStateUserType)
  {
    gmUserObject * userObj = (gmUserObject *) GM_OBJECT(currentStateVariable->m_value.m_ref);
    gmStateUserType * currentState = (gmStateUserType *) userObj->m_user;
    currentState->m_setExitState = function;
  }
  return GM_OK;
}



static int GM_CDECL gmSignal(gmThread * a_thread) // var, dest thread id
{
  GM_CHECK_NUM_PARAMS(1);
  GM_INT_PARAM(dstThreadId, 1, GM_INVALID_THREAD);
  a_thread->GetMachine()->Signal(a_thread->Param(0), dstThreadId, a_thread->GetId());
  return GM_OK;  
}



static int GM_CDECL gmBlock(gmThread * a_thread) // var, ...
{
  GM_CHECK_NUM_PARAMS(1);

  int res = a_thread->GetMachine()->Sys_Block(a_thread, a_thread->GetNumParams(), a_thread->GetBase());
  if(res == -1)
  {
    return GM_SYS_BLOCK;
  }
  else if(res == -2)
  {
    return GM_SYS_YIELD;
  }
  a_thread->Push(a_thread->Param(res));
  return GM_OK;
}


#if GM_USE_INCGC

static void GM_CDECL gmGCDestructStateUserType(gmMachine * a_machine, gmUserObject* a_object)
{
  gmStateUserType * state = (gmStateUserType *) a_object->m_user;
  a_machine->Sys_Free(state);
}

static bool GM_CDECL gmGCTraceStateUserType(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  gmStateUserType * state = (gmStateUserType *) a_object->m_user;
  if(state->m_currentState) a_gc->GetNextObject(state->m_currentState);
  if(state->m_lastState) a_gc->GetNextObject(state->m_lastState);
  if(state->m_setExitState) a_gc->GetNextObject(state->m_setExitState);

  a_workDone += 4; //contents + this

  return true;
}

#else //GM_USE_INCGC

static void GM_CDECL gmGCStateUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  gmStateUserType * state = (gmStateUserType *) a_object->m_user;
  a_machine->Sys_Free(state);
}

static void GM_CDECL gmMarkStateUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  gmStateUserType * state = (gmStateUserType *) a_object->m_user;
  if(state->m_currentState && state->m_currentState->NeedsMark(a_mark)) state->m_currentState->Mark(a_machine, a_mark);
  if(state->m_lastState && state->m_lastState->NeedsMark(a_mark)) state->m_lastState->Mark(a_machine, a_mark);
  if(state->m_setExitState && state->m_setExitState->NeedsMark(a_mark)) state->m_setExitState->Mark(a_machine, a_mark);
}

#endif //GM_USE_INCGC

//
// table
//

static int GM_CDECL gmTableCount(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_TABLE_PARAM(table, 0);
  a_thread->PushInt(table->Count());
  return GM_OK;
}

static int GM_CDECL gmTableDuplicate(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_TABLE_PARAM(table, 0);
  a_thread->PushTable(table->Duplicate(a_thread->GetMachine()));
  return GM_OK;
}

//
// std
//

void gmConcat(gmMachine * a_machine, char * &a_dst, int &a_len, int &a_size, const char * a_src, int a_growBy = 32)
{
  int len = strlen(a_src);

  if((a_len + len + 1) >= a_size)
  {
    a_size = a_len + len + a_growBy + 1;
    char * str = (char *) a_machine->Sys_Alloc(a_size);
    if(a_dst != NULL)
    {
      memcpy(str, a_dst, a_len);
      a_machine->Sys_Free(a_dst);
    }
    a_dst = str;
    a_dst[a_len] = '\0';
  }
  memcpy(a_dst + a_len, a_src, len);
  a_len += len;
  a_dst[a_len] = '\0';
}



static int GM_CDECL gmPrint(gmThread * a_thread)
{
  const int bufferSize = 256;
  int len = 0, size = 0, i;
  char * str = NULL, buffer[bufferSize];

  // build the string
  for(i = 0; i < a_thread->GetNumParams(); ++i)
  {
    gmConcat(a_thread->GetMachine(), str, len, size, a_thread->Param(i).AsString(a_thread->GetMachine(), buffer, bufferSize), 64);

    if(str)
    {
      GM_ASSERT(len < size);
      str[len++] = ' ';
      str[len] = '\0';
    }
  }

  // print the string
  if(str)
  {
    if(gmMachine::s_printCallback)
    {
      gmMachine::s_printCallback(a_thread->GetMachine(), str);
    }
    a_thread->GetMachine()->Sys_Free(str);
  }

  return GM_OK;
}



static int GM_CDECL gmfFormat(gmThread * a_thread) // string, params ...
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(format, 0);
  int param = 1;
  int len = 0, size = 0;
  const int bufferSize = 128;
  char * str = NULL, buffer[bufferSize];

  while(*format)
  {
    if(*format == '%')
    {
      switch(format[1])
      {
        case 'S' : 
        case 's' : 
        {
          GM_STRING_PARAM(pstr, param, "");
          ++param;
          gmConcat(a_thread->GetMachine(), str, len, size, pstr, 64);
          break;
        }
        case 'C' : 
        case 'c' : 
        {
          GM_INT_PARAM(ival, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%c", ival);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'D' :
        case 'd' :
        {
          GM_INT_PARAM(ival, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%d", ival);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'U' :
        case 'u' :
        {
          GM_INT_PARAM(ival, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%u", ival);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'B' :
        case 'b' :
        {
          GM_INT_PARAM(ival, param, 0);
          ++param;
          gmItoa(ival, buffer, 2);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'X' :
        case 'x' :
        {
          GM_INT_PARAM(ival, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%x", ival);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'F' :
        case 'f' :
        {
          GM_FLOAT_PARAM(fval, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%f", fval);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case 'e' :
        case 'E' :
        {
          GM_FLOAT_PARAM(fval, param, 0);
          ++param;
          V_snprintf(buffer, 64, "%e", fval);
          gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
          break;
        }
        case '%' :
        {
          if(len + 2 < size)
            str[len++] = '%';
          else
            gmConcat(a_thread->GetMachine(), str, len, size, "%", 64);
          break;
        }
        default :
          break;
      }
      format += 2;
    }
    else
    {
      if(len + 2 < size)
        str[len++] = *(format++);
      else
      {
        buffer[0] = *(format++);
        buffer[1] = '\0';
        gmConcat(a_thread->GetMachine(), str, len, size, buffer, 64);
      }
    }
  }

  if(str)
  {
    str[len] = '\0';
    a_thread->PushNewString(str);
    a_thread->GetMachine()->Sys_Free(str);
  }
  return GM_OK;
}


//
// lib
//

static gmFunctionEntry s_binding[] = 
{ 
  /*gm
    \lib gm
    \brief functions in the gm lib are all global scope
  */

  /*gm
    \function gmVersion
    \brief gmVersion will return the gmMachine version string.  version string is major type . minor type as a string
           and was added at version 1.1
    \return string
  */
  {"gmVersion", gmVersion},
  /*gm
    \function typeId
    \brief typeId will return the type id of the passed var
    \param var
    \return integer type
  */
  {"typeId", gmTypeId},
  /*gm
    \function typeName
    \brief typeName will return the type name of the passed var
    \param var
    \return string
  */
  {"typeName", gmTypeName},
  /*gm
    \function typeRegisterOperator
    \brief typeRegisterOperator will register an operator for a type
    \param int typeid
    \param string operator name is one of

          "getdot", "setdot", "getind", "setind", "add", "sub", "mul", "div", "mod",
          "inc", "dec", "bitor", "bitxor", "bitand", "shiftleft", "shiftright", "bitinv",
          "lt", "gt", "lte", "gte", "eq", "neq", "neg", "pos", "not"

    \param function
    \return 1 on success, otherwise 0
  */
  {"typeRegisterOperator", gmRegisterTypeOperator},
  /*gm
    \function typeRegisterVariable
    \brief typeRegisterVariable will register a variable with a type such that (type).varname will return the variable
    \param int typeid
    \param string var name
    \param var
    \return 1 on success, otherwise 0
  */
  {"typeRegisterVariable", gmRegisterTypeVariable},

  /*gm
    \function sysCollectGarbage
    \brief sysCollectGarbage will run the garbage collector iff the current mem used is over the desired mem used
    \param forceFullCollect (false) Optionally perform full garbage collection immediately if garbage collection is not disabled.
    \return 1 if the gc was run, 0 otherwise
  */
  {"sysCollectGarbage", gmCollectGarbage},
  /*gm
    \function sysGetMemoryUsage
    \brief sysGetMemoryUsage will return the current memory used in bytes
    \return int memory usage
  */
  {"sysGetMemoryUsage", gmGetCurrentMemoryUsage},

  /*gm
    \function sysSetDesiredMemoryUsageHard
    \brief sysSetDesiredMemoryUsageHard will set the desired memory useage in bytes.  when this is exceeded the garbage collector will be run.
    \param int desired mem usage in bytes
  */
  {"sysSetDesiredMemoryUsageHard", gmSetDesiredMemoryUsageHard},

  /*gm
    \function sysSetDesiredMemoryUsageSoft
    \brief sysSetDesiredMemoryUsageSoft will set the desired memory useage in bytes.  when this is exceeded the garbage collector will be run.
    \param int desired mem usage in bytes
  */
  {"sysSetDesiredMemoryUsageSoft", gmSetDesiredMemoryUsageSoft},

  /*gm
    \function sysGetDesiredMemoryUsageHard
    \brief sysGetDesiredMemoryUsageHard will get the desired memory useage in bytes.
           Note that this value is used to start garbage collection, it is not a strict limit.
    \return int Desired memory usage in bytes.
  */
  {"sysGetDesiredMemoryUsageHard", gmGetDesiredMemoryUsageHard},

  /*gm
    \function sysGetDesiredMemoryUsageSoft
    \brief sysGetDesiredMemoryUsageSoft will get the desired memory useage in bytes.
           Note that this value is used to start garbage collection, it is not a strict limit.
    \return int Desired memory usage in bytes.
  */
  {"sysGetDesiredMemoryUsageSoft", gmGetDesiredMemoryUsageSoft},


  /*gm
    \function sysSetDesiredMemoryUsageAuto
    \brief sysSetDesiredMemoryUsageAuto will enable auto adjustment of the memory limit(s) for subsequent garbage collections.
    \param int enable or disable flag
  */
  {"sysSetDesiredMemoryUsageAuto", gmSetDesiredMemoryUsageAuto},


  /*gm
    \function sysGetStatsGCNumFullCollects
    \brief sysGetStatsGCNumFullCollects Return the number of times full garbage collection has occured.
    \return int Number of times full collect has occured.
  */
  {"sysGetStatsGCNumFullCollects", gmSysGetStatsGCNumFullCollects},

  /*gm
    \function sysGetStatsGCNumIncCollects
    \brief sysGetStatsGCNumIncCollects Return the number of times incremental garbage collection has occured.
    This number may increase in twos as the GC has multiple phases which appear as restarts.
    \return int Number of times incremental collect has occured.
  */
  {"sysGetStatsGCNumIncCollects", gmSysGetStatsGCNumIncCollects},

  /*gm
    \function sysGetStatsGCNumWarnings
    \brief sysGetStatsGCNumWarnings Return the number of warnings because the GC or VM thought the GC was poorly configured.
    If this number is large and growing rapidly, the GC soft and hard limits need to be configured better.
    Do not be concerned if this number grows slowly.
    \return int Number of warnings garbage collect has generated.
  */
  {"sysGetStatsGCNumWarnings", gmSysGetStatsGCNumWarnings},

  /*gm
    \function sysIsGCRunning
    \brief Returns true if GC is running a cycle.
  */
  {"sysIsGCRunning", gmSysIsGCRunning},

  /*gm
    \function sysTime
    \brief sysTime will return the machine time in milli seconds
    \return int
  */
  {"sysTime", gmMachineTime},
    
  /*gm
    \function doString
    \brief doString will execute the passed gm script
    \param string script
    \param int optional (1) set as true and the string will execute before returning to this thread
    \param ref optional (null) set 'this'
    \return int thread id of thread created for string execution
  */
  {"doString", gmDoString},
  /*gm
    \function globals
    \brief globals will return the globals table
    \return table containing all global variables
  */
  {"globals", gmGlobals},

  /*gm
    \function threadTime
    \brief threadTime will return the thread execution time in milliseconds
    \return int 
  */
  {"threadTime", gmThreadTime},
  /*gm
    \function threadId
    \brief threadId will return the thread id of the current executing script
    \return int 
  */
  {"threadId", gmThreadId},
  /*gm
    \function threadAllIds
    \brief threadIds returns a table of thread Ids
    \return table of thread Ids
  */
  {"threadAllIds", gmThreadAllIds},
  /*gm
    \function threadKill
    \brief threadKill will kill the thread with the given id
    \param int threadId optional (0) will kill this thread
  */
  {"threadKill", gmKillThread},
  /*gm
    \function threadKillAll
    \brief threadKillAll will kill all the threads except the current one
    \param bool optional (false) will kill this thread if true
  */
  {"threadKillAll", gmKillAllThreads},
  /*gm
    \function thread
    \brief thread will start a new thread
    \param function entry point of the thread
    \param ... parameters to pass to the entry function
    \return int threadid
  */
  {"thread", gmfThread},
  /*gm
    \function yield
    \brief yield will hand execution control to the next thread
  */
  {"yield", gmYield},
  /*gm
    \function exit
    \brief exit will kill this thread
  */
  {"exit", gmExit},
  /*gm
    \function assert
    \brief assert 
    \param int expression if true, will do nothing, if false, will cause an exception
  */
  {"assert", gmAssert},
  /*gm
    \function sleep
    \brief sleep will sleep this thread for the given number of seconds 
    \param int\float seconds
  */
  {"sleep", gmSleep},
  /*gm
    \function signal
    \brief signal will signal the given variable, this will unblock dest threads that are blocked on the same variable.
    \param var
    \param int destThreadId optional (0) 0 will signal all threads
  */
  {"signal", gmSignal},
  /*gm
    \function block
    \brief block will block on all passed vars, execution will halt until another thread signals one of the block variables.  Will yield on null and return null.
    \param ... vars
    \return the unblocking var
  */
  {"block", gmBlock},

  /*gm
    \function stateSet
    \brief stateSet will collapse the stack to nothing, and push the passed functions.
    \param function new state function to execute
    \param ... params for new state function
  */
  {"stateSet", gmSetState},
  /*gm
    \function stateSetOnThread
    \brief stateSetOnThread will collapse the stack of the given thread id to nothing, and push the passed functions.
    \param int thread id
    \param function new state function to execute
    \param ... params for new state function
  */
  {"stateSetOnThread", gmSetStateOnThread},
  /*gm
    \function stateGet
    \brief stateGet will return the function on the bottom of this threads execution stack iff it was pushed using stateSet
    \param a_threadId Optional Id of thread to get state on.
    \reutrn function \ null
  */
  {"stateGet", gmGetState},
  /*gm
    \function stateGetLast
    \brief stateGetLast will return the last state function of this thread
    \param a_threadId Optional Id of thread to get last state on.    
    \reutrn function \ null
  */
  {"stateGetLast", gmGetLastState},
  /*gm
    \function stateSetExitFunction
    \brief stateSetExitFunction will set an exit function for this state, that will be called with no parameters if this thread
           switches state
    \param function
  */
  {"stateSetExitFunction", gmSetExitState},

  /*gm
    \function tableCount
    \brief tableCount will return the number of elements in a table object
    \param table
    \return int
  */
  {"tableCount", gmTableCount},
  /*gm
    \function tableDuplicate
    \brief tableDuplicate will duplicate the passed table object
    \param table
    \return table
  */
  {"tableDuplicate", gmTableDuplicate},
  
  /*gm
    \function print
    \brief print will print the given vars to the print handler.  passed strings are concatinated together with a seperating space.
    \param ... strings
  */
  {"print", gmPrint},
  /*gm
    \function format
    \brief format (like sprintf, but returns a string) %d, %s, %f, %c, %b, %x, %e
    \param string
  */
  {"format", gmfFormat},
};



void gmMachineLib(gmMachine * a_machine)
{
  // create the state type
  s_gmStateUserType = a_machine->CreateUserType("gmState");

#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmStateUserType, gmGCTraceStateUserType, gmGCDestructStateUserType);
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmStateUserType, gmMarkStateUserType, gmGCStateUserType);
#endif //GM_USE_INCGC

  // default lib
  a_machine->RegisterLibrary(s_binding, sizeof(s_binding) / sizeof(gmFunctionEntry), NULL);
}

