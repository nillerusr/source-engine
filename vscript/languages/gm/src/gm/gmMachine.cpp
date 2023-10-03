/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmMachine.h"
#include "gmThread.h"
#include "gmTableObject.h"
#include "gmStringObject.h"
#include "gmUserObject.h"
#include "gmFunctionObject.h"
#if !GMMACHINE_REMOVECOMPILER
  #include "gmCodeTree.h"
  #include "gmCodeGen.h"
#endif //GMMACHINE_REMOVECOMPILER
#include "gmOperators.h"
#include "gmMachineLib.h"
#include "gmCrc.h"
#include "gmStream.h"
#include "gmLibHooks.h"


#if GM_USE_INCGC
  #include "gmIncGC.h"
#endif //GM_USE_INCGC

#if !GM_USE_INCGC
  #define GM_ADDOBJECT(A) { (A)->m_sysNext = m_objects; m_objects = (A); }
#endif //!GM_USE_INCGC

// Must be last header
#include "memdbgon.h"

//
//
// gmHooks is an implementation of gmCodeGenHooks and is used by gmMachine to compile gm scripts.
//
//

class gmHooks : public gmCodeGenHooks
{
public:
  gmHooks(gmMachine * a_machine, const char * a_source, const char * a_filename); 
  virtual ~gmHooks();

  virtual bool Begin(bool a_debug);
  virtual bool AddFunction(gmFunctionInfo &a_info);
  virtual bool End(int a_errors);
  virtual gmptr GetFunctionId();
  virtual gmptr GetSymbolId(const char * a_symbol);
  virtual gmptr GetStringId(const char * a_string);

  gmFunctionObject * GetRootFunction() { return m_rootFunction; }

private:

  gmFunctionObject * m_rootFunction;
  int m_errors;
  bool m_debug;
  bool m_gcEnabled;
  gmMachine * m_machine;
  const char * m_source;
  const char * m_filename;
  gmuint32 m_sourceId;
};

//
//
// Implementation of gmSourceEntry, used for storing source code in debug mode
//
//

class gmSourceEntry : public gmListDoubleNode<gmSourceEntry>
{
public:

  gmSourceEntry(const char * a_source, const char * a_filename)
  {
    int slen = strlen(a_source);
    int flen = strlen(a_filename);
    m_id = gmCrc32String(a_source);

    m_source = GM_NEW( char[slen + flen + 2] );
    memcpy(m_source, a_source, slen + 1);
    m_filename = m_source + slen + 1;
    memcpy(m_filename, a_filename, flen + 1);
  }

  ~gmSourceEntry()
  {
    if(m_source) 
      delete[] m_source;
  }

  gmuint32 m_id;
  char * m_source;
  char * m_filename;
};

//
//
// Implementation of gmBlock, gmSignal, gmBlocklist for thread blocking 
//
//

class gmBlockList : public gmHashNode<gmVariable, gmBlockList, gmVariable>
{
public:

  gmBlockList() {}
  virtual ~gmBlockList() {}

  virtual const gmVariable &GetKey() const
  {
    return m_block;
  }

  gmVariable m_block;
  gmListDouble<gmBlock> m_blocks;
};

//
//
// Default Print Callback
//
//
void gmDefaultPrintCallback(gmMachine * a_machine, const char * a_string)
{
  GM_PRINTF("%s"GM_NL, a_string);
}

//
//
// Implementation of gmMachine
//
//

gmMachineCallback gmMachine::s_machineCallback = NULL;
gmPrintCallback gmMachine::s_printCallback = gmDefaultPrintCallback;
gmUserBreakCallback gmMachine::s_userBreakCallback = NULL;


#if GM_USE_INCGC
void GM_CDECL gmMachine::ScanRootsCallBack(gmMachine* a_machine, gmGarbageCollector* a_gc)
{
  gmThread * tit;

  // call the gc callback
  if(s_machineCallback) s_machineCallback(a_machine, MC_COLLECT_GARBAGE, NULL);

  // iterate cpp owned gmObjects
  gmHash<gmObject*, ObjHashNode>::Iterator cgmoIt;
  for(cgmoIt = a_machine->m_cppOwnedGMObjs.First(); cgmoIt; ++cgmoIt)
  {
    gmObject* curObj = cgmoIt->m_obj;
    a_gc->GetNextObject(curObj);
  }

  // iterate over all threads and mark the stacks.
  for(tit = a_machine->m_runningThreads.GetFirst(); a_machine->m_runningThreads.IsValid(tit); tit = a_machine->m_runningThreads.GetNext(tit)) tit->GCScanRoots(a_machine, a_gc);
  for(tit = a_machine->m_blockedThreads.GetFirst(); a_machine->m_blockedThreads.IsValid(tit); tit = a_machine->m_blockedThreads.GetNext(tit)) tit->GCScanRoots(a_machine, a_gc);
  for(tit = a_machine->m_sleepingThreads.GetFirst(); a_machine->m_sleepingThreads.IsValid(tit); tit = a_machine->m_sleepingThreads.GetNext(tit)) tit->GCScanRoots(a_machine, a_gc);
  for(tit = a_machine->m_exceptionThreads.GetFirst(); a_machine->m_exceptionThreads.IsValid(tit); tit = a_machine->m_sleepingThreads.GetNext(tit)) tit->GCScanRoots(a_machine, a_gc);

  // iterate over global variables and mark
  if(a_machine->m_global)
  {
    a_gc->GetNextObject(a_machine->m_global);
  }
  // iterate over type variables and mark
  gmuint i;
  for(i = 0; i < a_machine->m_types.Count(); ++i)
  {
    a_gc->GetNextObject(a_machine->m_types[i].m_variables);
  }

#if !GM_GC_KEEP_PERSISTANT_SEPARATE
  //NOTE This needs to be spread over time perhaps.
  for(i=0; i<(gmuint)a_machine->m_numPermanantStrings; ++i)
  {
    a_gc->GetNextObject(a_machine->m_permanantStrings[i]);
  }
#endif //!GM_GC_KEEP_PERSISTANT_SEPARATE
}

#endif //GM_USE_INCGC

gmMachine::gmMachine()
  :
    m_threads(128),
    m_memStringObj(sizeof(gmStringObject), GMMACHINE_STRINGCHUNKSIZE),
    m_memTableObj(sizeof(gmTableObject), GMMACHINE_TBLCHUNKSIZE),
    m_memFunctionObj(sizeof(gmFunctionObject), GMMACHINE_OBJECTCHUNKSIZE),
    m_memUserObj(sizeof(gmUserObject), GMMACHINE_OBJECTCHUNKSIZE),
    m_memStackFrames(sizeof(gmStackFrame), GMMACHINE_STACKFCHUNKSIZE),

    m_strings(GMMACHINE_STRINGHASHSIZE),
    m_blocks(64),
    m_cppOwnedGMObjs(GMMACHINE_CPPOWNEDGMOBJHASHSIZE)

{
  m_line = NULL;
  m_call = NULL;
  m_return = NULL;
  m_isBroken = NULL;

#if GM_USE_INCGC
  m_gc = GM_NEW( gmGarbageCollector );
  m_gc->Init(ScanRootsCallBack, this);
  #if !GM_GC_KEEP_PERSISTANT_SEPARATE
  m_permanantStrings.SetCount(0);
  #endif //!GM_GC_KEEP_PERSISTANT_SEPARATE
#endif //GM_USE_INCGC
  m_trueGlobal = m_global = NULL;

  m_objects = NULL;
  m_threadId = 0;
  m_nextThread = NULL;
  m_nextThreadValid = false;
  m_autoMem = GMMACHINE_AUTOMEM;
  m_currentMemoryUsage = 0;
  m_desiredByteMemoryUsageHard = GMMACHINE_INITIALGCHARDLIMIT;
  m_desiredByteMemoryUsageSoft = GMMACHINE_INITIALGCSOFTLIMIT;
  m_mark = GM_MARK_START;

  m_framesSinceLastIncCollect = 0;
  m_gcPhaseCount = 0;
  m_statsGCFullCollect = 0;
  m_statsGCIncCollect = 0;
  m_statsGCWarnings = 0;

  m_debug = false;
  m_debugUser = NULL;

  m_gcEnabled = true;

  m_trueGlobal = m_global = AllocTableObject(); // Alloc global table

  m_types.SetCount(0);

  ResetDefaultTypes();
  m_time = 0;
  
  gmMachineLib(this);
}



gmMachine::~gmMachine()
{
  ResetAndFreeMemory();
#if GM_USE_INCGC
  delete m_gc;
#endif //GM_USE_INCGC
}



void gmMachine::ResetAndFreeMemory()
{

#if GM_USE_INCGC

  m_gc->DestructAll();

  #if !GM_GC_KEEP_PERSISTANT_SEPARATE
  m_permanantStrings.SetCount(0);
  #endif //!GM_GC_KEEP_PERSISTANT_SEPARATE

  m_trueGlobal = m_global = NULL;
  gmuint i;
  for(i = 0; i < m_types.Count(); ++i)
  {
    m_types[i].m_variables = NULL;
    m_types[i].m_name = NULL;
  }

  //FindMissingCountObj();

#else //GM_USE_INCGC
  // destruct all objects
  for(gmObject* it = m_objects; it; )
  {
    gmObject * nit = it->m_sysNext;
    it->Destruct(this);
    FreeObject(it);
    it = nit;
  }
  m_trueGlobal = m_global = NULL; //Global was freed with the rest
  // operators\types
  gmuint i;
  for(i = 0; i < m_types.Count(); ++i)
  {
    m_types[i].m_variables = NULL;
    m_types[i].m_name = NULL;
  }
#endif //GM_USE_INCGC
  m_objects = NULL;

  // string table
  GM_ASSERT(m_strings.Count() == 0);
  m_strings.RemoveAll();
  
  // threads
  m_runningThreads.RemoveAll();
  m_blockedThreads.RemoveAll();
  m_sleepingThreads.RemoveAll();
  m_exceptionThreads.RemoveAll();
  m_killedThreads.RemoveAndDeleteAll();
  m_threads.RemoveAndDeleteAll();
  m_threadId = 0;
  m_time = 0;
  m_nextThread = NULL;
  m_nextThreadValid = false;
  GM_ASSERT(m_blocks.Count() == 0);

  // CPP owned gmObjects
  m_cppOwnedGMObjs.RemoveAll();

  // memory allocators (make sure we arent leaking memory)

  GM_ASSERT(m_memStringObj.GetMemUsed() == 0);
  m_memStringObj.ResetAndFreeMemory();

  GM_ASSERT(m_memTableObj.GetMemUsed() == 0);
  m_memTableObj.ResetAndFreeMemory();

  GM_ASSERT(m_memFunctionObj.GetMemUsed() == 0);
  m_memFunctionObj.ResetAndFreeMemory();

  GM_ASSERT(m_memUserObj.GetMemUsed() == 0);
  m_memUserObj.ResetAndFreeMemory();

  GM_ASSERT(m_memStackFrames.GetMemUsed() == 0);
  m_memStackFrames.ResetAndFreeMemory();

  GM_ASSERT(m_fixedSet.GetMemUsed() == 0);
  m_fixedSet.ResetAndFreeMemory();

  m_debug = false;
  m_debugUser = NULL;
  m_source.RemoveAndDeleteAll();

  // types
  m_types.ResetAndFreeMemory();

  // compiler
  m_log.ResetAndFreeMemory();
#if !GMMACHINE_REMOVECOMPILER
  gmCodeTree::Get().FreeMemory();
  gmCodeGen::Get().FreeMemory();
#endif //GMMACHINE_REMOVECOMPILER

  // garbage collection
  m_autoMem = GMMACHINE_AUTOMEM;
  m_currentMemoryUsage = 0;
  m_desiredByteMemoryUsageHard = GMMACHINE_INITIALGCHARDLIMIT;
  m_desiredByteMemoryUsageSoft = GMMACHINE_INITIALGCSOFTLIMIT;
  m_mark = GM_MARK_START;
  m_gcEnabled = true;
}


void gmMachine::Init()
{
  m_gcEnabled = true;
  m_threadId = 0;
  m_autoMem = GMMACHINE_AUTOMEM;
  m_currentMemoryUsage = 0;
  m_desiredByteMemoryUsageHard = GMMACHINE_INITIALGCHARDLIMIT;
  m_desiredByteMemoryUsageSoft = GMMACHINE_INITIALGCSOFTLIMIT;
  m_mark = GM_MARK_START;
  m_trueGlobal = m_global = AllocTableObject(); // Alloc global table
  m_types.SetCount(0);
  ResetDefaultTypes();
  m_blocks.RemoveAll();
  m_cppOwnedGMObjs.RemoveAll();
  m_time = 0;

  m_debug = false;

  gmMachineLib(this);
}


void gmMachine::RegisterLibrary(gmFunctionEntry * a_functions, int a_numFunctions, const char * a_asTable, bool a_newTable)
{
  gmTableObject * table = m_global; 

  if( a_asTable ) 
  { 
#if GM_CASE_INSENSITIVE 

    char * asTable = (char *) gmNewStackArray(char, strlen(a_asTable) + 1); 
    strcpy(asTable, a_asTable); 
    _strlwr(asTable); 
    a_asTable = asTable; 

#endif // GM_CASE_INSENSITIVE 

    if( a_newTable == true ) 
    { 
      table = AllocTableObject(); 
    } 
    else 
    { 
      gmVariable tabVar = m_global->Get(this, a_asTable); 
      if( tabVar.m_type == GM_TABLE ) 
      { 
        // Existing table found, use this 
        table = (gmTableObject *)tabVar.m_value.m_ref; 
      } 
      else 
      { 
        // Otherwise create a new table afterall 
        table = AllocTableObject(); 
      } 
    } 
    m_global->Set(this, a_asTable, gmVariable(GM_TABLE, (gmptr) table)); 
  } 

  GM_ASSERT(table); 

  for(int index = 0; index < a_numFunctions; ++index )
  {
    gmFunctionObject* funcObj = AllocFunctionObject(a_functions[index].m_function);
    funcObj->m_cUserData = a_functions[index].m_userData;

#if GM_CASE_INSENSITIVE

    char * funcNameLower = (char *) gmNewStackArray(char, strlen(a_functions[index].m_name) + 1);
    strcpy(funcNameLower, a_functions[index].m_name);
    _strlwr(funcNameLower);

    m_cFuncUserData
    table->Set(this, funcNameLower, gmVariable(GM_FUNCTION, (gmptr)funcObj));

#else // !GM_CASE_INSENSITIVE

    table->Set(this, a_functions[index].m_name, gmVariable(GM_FUNCTION, (gmptr)funcObj));

#endif // !GM_CASE_INSENSITIVE
  }
}



void gmMachine::RegisterTypeLibrary(gmType a_type, gmFunctionEntry * a_functions, int a_numFunctions)
{
  int i;
  for(i = 0; i < a_numFunctions; ++i)
  {
    gmFunctionObject* funcObj = AllocFunctionObject(a_functions[i].m_function);
    funcObj->m_cUserData = a_functions[i].m_userData;

    RegisterTypeVariable(a_type, a_functions[i].m_name, gmVariable(GM_FUNCTION, (gmptr)funcObj));
  }
}



gmType gmMachine::CreateUserType(const char * a_name)
{
  bool enabled = IsGCEnabled();
  EnableGC(false); //Disable GC as user types are roots and we are in the middle of creating a new one

  gmType type = (gmType) m_types.Count();
  m_types.InsertLast();
  m_types[type].Init();
  m_types[type].m_name = AllocPermanantStringObject(a_name);
  gmInitBasicType(GM_USER, m_types[type].m_nativeOperators);

  // Alloc user type table
  m_types[type].m_variables = AllocTableObject();

  EnableGC(enabled); //Restore GC

  return type;
}


#if GM_USE_INCGC
void gmMachine::RegisterUserCallbacks(gmType a_type, gmGCTraceCallback a_gcTrace, gmGCDestructCallback a_gcDestruct, gmAsStringCallback a_asString)
{
  m_types[a_type].m_gcTrace = a_gcTrace;
  m_types[a_type].m_gcDestruct = a_gcDestruct;
  m_types[a_type].m_asString = a_asString;
}

#else //GM_USE_INCGC
void gmMachine::RegisterUserCallbacks(gmType a_type, gmGarbageCollectCallback a_mark, gmGarbageCollectCallback a_gc, gmAsStringCallback a_asString)
{
  m_types[a_type].m_mark = a_mark;
  m_types[a_type].m_gc = a_gc;
  m_types[a_type].m_asString = a_asString;
}
#endif //GM_USE_INCGC


void gmMachine::RegisterTypeVariable(gmType a_type, const char * a_variableName, const gmVariable &a_variable)
{
#if GM_CASE_INSENSITIVE

  char * varNameLower = (char *) gmNewStackArray(char, strlen(a_variableName) + 1);
  strcpy(varNameLower, a_variableName);
  _strlwr(varNameLower);
  m_types[a_type].m_variables->Set(this, varNameLower, a_variable);

#else // !GM_CASE_INSENSITIVE
  m_types[a_type].m_variables->Set(this, a_variableName, a_variable);
#endif // !GM_CASE_INSENSITIVE
}



bool gmMachine::RegisterTypeOperator(gmType a_type, gmOperator a_operator, gmFunctionObject * a_function, gmOperatorFunction a_nativeFunction)
{
//  if(a_type < GM_USER) return false; //Prevent overriding default operators

  if(a_function)
  {
    m_types[a_type].m_operators[a_operator] = a_function->GetRef();
  }
  else if(a_nativeFunction)
  {
    m_types[a_type].m_nativeOperators[a_operator] = a_nativeFunction;
  }
  return true;
}



const char * gmMachine::GetTypeName(gmType a_type)
{
  return (const char *) *m_types[a_type].m_name;
}



int gmMachine::CheckSyntax(const char * a_string)
{
#if GMMACHINE_REMOVECOMPILER
  GetLog().LogEntry("No compiler in build");
  return 1;
#else // GMMACHINE_REMOVECOMPILER
  gmCodeGenHooksNull nullHooks;

  // parse
  int errors = gmCodeTree::Get().Lock(a_string, &m_log);
  if(errors > 0) 
  {
    gmCodeTree::Get().Unlock();
    return errors;
  }

  // compile
  errors = gmCodeGen::Get().Lock(gmCodeTree::Get().GetCodeTree(), &nullHooks, true, &m_log);
  if(errors > 0)
  {
    gmCodeTree::Get().Unlock();
    gmCodeGen::Get().Unlock();
    return errors;
  }

  gmCodeTree::Get().Unlock();
  gmCodeGen::Get().Unlock();

  return errors;
#endif //GMMACHINE_REMOVECOMPILER
}



int gmMachine::ExecuteString(const char * a_string, int * a_threadId, bool a_now, const char * a_filename, gmVariable* a_this)
{
#if GMMACHINE_REMOVECOMPILER
  GetLog().LogEntry("No compiler in build");
  return 1;
#else // GMMACHINE_REMOVECOMPILER
  if(a_threadId) { *a_threadId = GM_INVALID_THREAD; }

  // parse
  int errors = gmCodeTree::Get().Lock(a_string, &m_log);
  if(errors > 0) 
  {
    gmCodeTree::Get().Unlock();
    return errors;
  }

  // compile
  gmHooks hooks(this, a_string, a_filename);
  errors = gmCodeGen::Get().Lock(gmCodeTree::Get().GetCodeTree(), &hooks, m_debug, &m_log);
  if(errors > 0)
  {
    gmCodeTree::Get().Unlock();
    gmCodeGen::Get().Unlock();
    return errors;
  }

  gmCodeTree::Get().Unlock();
  gmCodeGen::Get().Unlock();

  // null or this
  gmVariable thisVar;
  if(!a_this)
  {
    thisVar.Nullify();
  }
  else
  {
    thisVar = *a_this;
  }

  // execute
  gmThread * thread = CreateThread(thisVar, gmVariable(GM_FUNCTION, (gmptr) hooks.GetRootFunction()), a_threadId);

  if(a_now)
  {
    thread->Sys_Execute();
  }

  return 0;
#endif // GMMACHINE_REMOVECOMPILER
}


bool gmMachine::ExecuteLib(gmStream &a_stream, int * a_threadId, bool a_now, const char * a_filename, gmVariable* a_this)
{
  gmFunctionObject * rootFunction = gmLibHooks::BindLib(*this, a_stream, a_filename);
  if(rootFunction)
  {
    // null or this
    gmVariable thisVar;
    if(!a_this)
    {
      thisVar.Nullify();
    }
    else
    {
      thisVar = *a_this;
    }

    gmThread * thread = CreateThread(thisVar, gmVariable(GM_FUNCTION, rootFunction->GetRef()), a_threadId);
    if(a_now)
    {
      thread->Sys_Execute();
    }
    return true;
  }
  return false;
}


bool gmMachine::ExecuteFunction(gmFunctionObject * a_function, int * a_threadId, bool a_now, gmVariable* a_this)
{
  gmVariable thisVar;
  if(!a_this)
  {
    thisVar.Nullify();
  }
  else
  {
    thisVar = *a_this;
  }

  gmThread * thread = CreateThread(thisVar, gmVariable(GM_FUNCTION, a_function->GetRef()), a_threadId);
  if(a_now)
  {
    thread->Sys_Execute();
  }
  return true;
}


int gmMachine::CompileStringToLib(const char * a_string, gmStream &a_stream)
{
#if GMMACHINE_REMOVECOMPILER
  GetLog().LogEntry("No compiler in build");
  return 1;
#else // GMMACHINE_REMOVECOMPILER
  // parse
  int errors = gmCodeTree::Get().Lock(a_string, &m_log);
  if(errors > 0) 
  {
    gmCodeTree::Get().Unlock();
    return errors;
  }
  
  #if 0 // Dump code tree to file for debugging
  FILE * fp = fopen("c:/codetree.txt", "wb");
  gmCodeTree::Get().Print(fp);
  fclose(fp);
  #endif
  
  // compile
  gmLibHooks hooks(a_stream, a_string);
  errors = gmCodeGen::Get().Lock(gmCodeTree::Get().GetCodeTree(), &hooks, m_debug, &m_log);

  gmCodeTree::Get().Unlock();
  gmCodeGen::Get().Unlock();

  return errors;
#endif // GMMACHINE_REMOVECOMPILER
}


gmFunctionObject * gmMachine::CompileStringToFunction(const char * a_string, int *a_errorCount, const char * a_filename)
{
#if GMMACHINE_REMOVECOMPILER
  GetLog().LogEntry("No compiler in build");
  if(a_errorCount)
  { 
    *a_errorCount++;
  }
  return NULL;
#else // GMMACHINE_REMOVECOMPILER
  // parse
  int errors = gmCodeTree::Get().Lock(a_string, &m_log);
  if(errors > 0) 
  {
    gmCodeTree::Get().Unlock();
    if(a_errorCount) 
      *a_errorCount = errors;
    return NULL;
  }

  // compile
  gmHooks hooks(this, a_string, a_filename);
  errors = gmCodeGen::Get().Lock(gmCodeTree::Get().GetCodeTree(), &hooks, m_debug, &m_log);
  if(errors > 0)
  {
    gmCodeTree::Get().Unlock();
    gmCodeGen::Get().Unlock();
    if(a_errorCount) 
      *a_errorCount = errors;
    return NULL;
  }

  gmCodeTree::Get().Unlock();
  gmCodeGen::Get().Unlock();

  if(a_errorCount) 
    *a_errorCount = errors;

  return hooks.GetRootFunction();
#endif //GMMACHINE_REMOVECOMPILER
}


gmFunctionObject * gmMachine::BindLibToFunction(gmStream &a_stream, const char * a_filename)
{
  gmFunctionObject * rootFunction = gmLibHooks::BindLib(*this, a_stream, a_filename);
  return rootFunction;
}
 

gmThread * gmMachine::CreateThread(const gmVariable &a_this, const gmVariable &a_function, int * a_threadId)
{
  gmThread * thread = CreateThread(a_threadId);
  thread->Push(a_this);
  thread->Push(a_function);
  if(thread->PushStackFrame(0, 0) == gmThread::RUNNING) return thread;
  return NULL;
}


void gmMachine::Sys_SignalCreateThread(gmThread * a_thread)
{
  // Send create thread message
  if(s_machineCallback)
  {
    s_machineCallback(this, MC_THREAD_CREATE, a_thread);
  }
}


gmThread * gmMachine::CreateThread(int * a_threadId)
{
  gmThread * thread = m_killedThreads.RemoveFirst();
  if(thread == NULL)
  {
    thread = GM_NEW( gmThread(this) );
  }
  thread->Sys_Reset(GetThreadId());
  if(a_threadId) *a_threadId = thread->GetId();
  m_threads.Insert(thread);
  thread->Sys_SetState(gmThread::RUNNING);
  thread->Sys_SetStartTime(m_time);
  m_runningThreads.InsertLast(thread); // insert last to maintain propper execution order.

  if(   m_nextThreadValid 
     && (!m_runningThreads.IsValid(m_nextThread)) )
  {
    m_nextThread = thread; // Inserted last, but iterator was at last pos, if we don't do this, we'll miss skip this thread until next cycle.
  }  

  return thread;
}



gmThread * gmMachine::GetThread(int a_threadId)
{
  return m_threads.Find(a_threadId);
}



bool gmMachine::Signal(const gmVariable &a_signal, int a_dstThreadId, int a_srcThreadId)
{
  gmBlockList * blockList = m_blocks.Find(a_signal);
  bool used = false;

  if(blockList)
  {
    // iterate over all threads in the block list, and add the signal to the appropriate threads.
    gmBlock * block = blockList->m_blocks.GetFirst();
    while(blockList->m_blocks.IsValid(block))
    {
      gmThread * thread = block->m_thread;
      if(a_dstThreadId == GM_INVALID_THREAD || a_dstThreadId == thread->GetId())
      {
        gmSignal * signal = NULL;
        used = true;

        // allocate a signal
        if(thread->GetState() == gmThread::SYS_PENDING)
        {
          signal = (gmSignal *) Sys_Alloc(sizeof(gmSignal));
          signal->m_signal = a_signal;
          signal->m_srcThreadId = a_srcThreadId;
          signal->m_dstThreadId = a_dstThreadId;
          signal->m_nextSignal = thread->Sys_GetSignals();
          thread->Sys_SetSignals(signal);
        }
        else
        {
          block->m_signalled = true;
          block->m_srcThreadId = a_srcThreadId;
          thread->Sys_SetState(gmThread::SYS_PENDING);
        }

        if(a_dstThreadId != GM_INVALID_THREAD) break;
      }
      block = blockList->m_blocks.GetNext(block);
    }
  }
  return used;
}



int gmMachine::Sys_Block(gmThread * a_thread, int m_numBlocks, const gmVariable * a_blocks)
{
  // use up our signals.
  gmSignal * signal = a_thread->Sys_GetSignals(), * next;

  while(signal)
  {
    int i;
    for(i = 0; i < m_numBlocks; ++i)
    {
      if(a_blocks[i].m_type == signal->m_signal.m_type && 
         a_blocks[i].m_value.m_ref == signal->m_signal.m_value.m_ref)
      {
        // remove the signal
        a_thread->Sys_SetSignals(signal->m_nextSignal);
        Sys_Free(signal);

        // return the block
        return i;
      }
    }

    // we didnt fire on the signal, so remove it.
    next = signal->m_nextSignal;
    a_thread->Sys_SetSignals(next);
    Sys_Free(signal);
    signal = next;
  }

  // add the blocks.
  int i;
  for(i = 0; i < m_numBlocks; ++i)
  {
    gmBlockList * blockList = m_blocks.Find(a_blocks[i]);
    if(blockList == NULL)
    {
      blockList = (gmBlockList *) Sys_Alloc(sizeof(gmBlockList));
      blockList = gmConstructElement<gmBlockList>(blockList);
      blockList->m_block = a_blocks[i];
      m_blocks.Insert(blockList);
    }

    gmBlock * block = (gmBlock *) Sys_Alloc(sizeof(gmBlock));
    block->m_list = blockList;
    block->m_block = a_blocks[i];
    block->m_signalled = false;
    block->m_thread = a_thread;
    block->m_nextBlock = a_thread->Sys_GetBlocks();
    a_thread->Sys_SetBlocks(block);
    blockList->m_blocks.InsertFirst(block);
  }
  return -1;
}



void gmMachine::KillThread(int a_threadId)
{
  gmThread * thread = GetThread(a_threadId);
  if(thread)
  {
    Sys_SwitchState(thread, gmThread::KILLED);
  }
}


void gmMachine::ForEachThread(gmThreadIteratorCallback a_callback, void * a_context)
{
  gmListDouble<gmThread>::Iterator it;

  for(it = m_runningThreads.First(); it;)
  {
    gmThread * thread = it.Resolve();
    ++it;
    if(!a_callback(thread, a_context)) return;
  }

  for(it = m_blockedThreads.First(); it;)
  {
    gmThread * thread = it.Resolve();
    ++it;
    if(!a_callback(thread, a_context)) return;
  }

  for(it = m_sleepingThreads.First(); it;)
  {
    gmThread * thread = it.Resolve();
    ++it;
    if(!a_callback(thread, a_context)) return;
  }

  for(it = m_exceptionThreads.First(); it;)
  {
    gmThread * thread = it.Resolve();
    ++it;
    if(!a_callback(thread, a_context)) return;
  }
}



void gmMachine::Sys_SwitchState(gmThread * a_thread, int a_to)
{
  if(a_thread->GetState() == (gmThread::State) a_to) return;
  switch(a_thread->GetState())
  {
    case gmThread::RUNNING :
    {
      Sys_RemoveSignals(a_thread);
      if(a_thread == m_nextThread)
      {
        m_nextThread = m_runningThreads.GetNext(a_thread);
      }
      m_runningThreads.Remove(a_thread); 
      break;
    }
    case gmThread::BLOCKED :
    case gmThread::SYS_PENDING :
    {
      // remove and clean up the blocks.
      Sys_RemoveBlocks(a_thread);
      m_blockedThreads.Remove(a_thread);
      break;
    } 
    case gmThread::SLEEPING : m_sleepingThreads.Remove(a_thread); break;
    case gmThread::KILLED : m_killedThreads.Remove(a_thread); break;
    case gmThread::EXCEPTION : m_exceptionThreads.Remove(a_thread); break;
    default : GM_ASSERT(0); break;
  }
  switch(a_to)
  {
    case gmThread::RUNNING : m_runningThreads.InsertLast(a_thread); break;
    case gmThread::BLOCKED : m_blockedThreads.InsertFirst(a_thread); break;
    case gmThread::EXCEPTION : m_exceptionThreads.InsertFirst(a_thread); break;
    case gmThread::SLEEPING :
    {
      // insert ordered by thread time stamp (GetTimeStamp)
      gmListDouble<gmThread>::Iterator it = m_sleepingThreads.First();
      bool inserted = false;
      while(it)
      {
        if(it->GetTimeStamp() > a_thread->GetTimeStamp())
        {
          m_sleepingThreads.InsertBefore(it.Resolve(), a_thread);
          inserted = true;
          break;
        }
        ++it;
      }
      if(!inserted) m_sleepingThreads.InsertLast(a_thread);
      break;
    }
    case gmThread::KILLED :
    {
      // Change the thread state before resetting the thread for consistency.
      // The reset will clear the state.
      a_thread->Sys_SetState(gmThread::KILLED);

      // send destroy thread message
      if(s_machineCallback)
      {
        s_machineCallback(this, MC_THREAD_DESTROY, a_thread);
      }

      m_threads.Remove(a_thread);
      a_thread->Sys_Reset(0);

      if(m_killedThreads.Count() < GMMACHINE_MAXKILLEDTHREADS)
      {
        m_killedThreads.InsertFirst(a_thread);
        // Thread is dead and we don't want to set it's state (already set).
        // Besides it's ID is now invalid.
        return;
      }
      // NOTE: Might be good to always delay thread deletion so killed threads are valid in nested call stacks.
      //       Would we need to take care not to reuse a recent thread if it could still be read/written to?
      delete a_thread;
      return;
    }
    default : GM_ASSERT(0); break;
  }
  a_thread->Sys_SetState((gmThread::State) a_to);
}


void gmMachine::KillExceptionThreads()
{
  gmThread * thread = m_exceptionThreads.GetLast();
  while(m_exceptionThreads.IsValid(thread))
  {
    Sys_SwitchState(thread, gmThread::KILLED);
    thread = m_exceptionThreads.GetLast();
  }
}



int gmMachine::Execute(gmuint32 a_delta)
{
  m_time += a_delta;

  //
  // Wake up any sleeping threads at their timestamp
  //
  for(;;)
  {
    gmThread * thread = m_sleepingThreads.GetFirst();
    if(m_sleepingThreads.IsValid(thread) && (thread->GetTimeStamp() <= m_time))
    {
      Sys_SwitchState(thread, gmThread::RUNNING);
      continue;
    }
    break;
  }

  //
  // Move all SYS_PENDING threads from the blocked list to the front of the RUNNING list.
  //
  gmThread * it, * nit;
  for(it = m_blockedThreads.GetFirst(); m_blockedThreads.IsValid(it);)
  {
    nit = m_blockedThreads.GetNext(it);
    if(it->GetState() == gmThread::SYS_PENDING)
    {
      // get the unblocking signal
      gmBlock * block = it->Sys_GetBlocks();
      while(block)
      {
        if(block->m_signalled) break;
        block = block->m_nextBlock;
      }
      GM_ASSERT(block);
      it->Pop();
      it->Push(block->m_block);

      // move the thread to the running state
      Sys_SwitchState(it, gmThread::RUNNING);
    }
    it = nit;
  }

  //
  // Execute running threads
  //
  m_nextThreadValid = true;  
  for(it = m_runningThreads.GetFirst(); m_runningThreads.IsValid(it);)
  {
    m_nextThread = m_runningThreads.GetNext(it);
    it->Sys_Execute();
    it = m_nextThread;
  }
  m_nextThreadValid = false;

  CollectGarbage();

  return m_threads.Count();
}


void gmMachine::Presize(int a_pool8,
                        int a_pool16,
                        int a_pool24,
                        int a_pool32,
                        int a_pool64,
                        int a_pool128,
                        int a_pool256,
                        int a_pool512,
                        int a_gmStringObj,
                        int a_gmTableObj,
                        int a_gmFunctionObj,
                        int a_gmUserObj,
                        int a_gmStackFrame
                        )
{
  m_fixedSet.Presize(a_pool8, a_pool16, a_pool24, a_pool32, a_pool64, a_pool128, a_pool256, a_pool512);
  if( a_gmStringObj ) { m_memStringObj.Presize(a_gmStringObj); }
  if( a_gmTableObj) { m_memTableObj.Presize(a_gmTableObj); }
  if( a_gmFunctionObj ) { m_memFunctionObj.Presize(a_gmFunctionObj); }
  if( a_gmUserObj ) { m_memUserObj.Presize(a_gmUserObj); }
  if( a_gmStackFrame ) { m_memStackFrames.Presize(a_gmStackFrame); }
}

  
#if GM_USE_INCGC

bool gmMachine::CollectGarbage(bool a_forceFullCollect)
{
  // NOTES: 
  //
  // 1) If the desired hard memory limit is set high and the 
  //    desired soft memory limit collects before ever reaching the hard limit
  //    the actual amount of memory used is unknown as there is always some
  //    slack memory in use or being classified.  This is normal, but may not
  //    help calibrate memory limits.
  //
  // 2) You may want to calibrate memory usage by setting the hard limit too low
  //    then letting it grow as it needs to.  After that, adjust the soft limit
  //    until the full collects never occur.
  // 
  // 3) The hard and soft limits should be set well above the actual used memory limit
  //    for efficient operation (wheather manually or automatically).  If this is not
  //    the case, the GC may occur unnecessarily often.
  //

  bool result = false;
  if(m_gcEnabled)
  {
    GM_ASSERT(GetDesiredByteMemoryUsageSoft() <= GetDesiredByteMemoryUsageHard());
    GM_ASSERT(GetDesiredByteMemoryUsageHard() > 0);
    
    // Even with assert, to prevent undefined behaviour, don't let soft limit be set above hard limit here.
    if(GetDesiredByteMemoryUsageSoft() > GetDesiredByteMemoryUsageHard())
    {
      SetDesiredByteMemoryUsageSoft(GetDesiredByteMemoryUsageHard());
    }

    ++m_framesSinceLastIncCollect;

    // Have we exceeded the hard limit?
    if(a_forceFullCollect || (GetCurrentMemoryUsage() > GetDesiredByteMemoryUsageHard()))
    {
      //int beforeMemUsage = GetCurrentMemoryUsage();
      ++m_statsGCFullCollect;
      result = true;

      // Perform full collection & reclaimation now
      m_gc->FullCollect(); 
      
      if(m_autoMem)
      {
        int afterMemUsage = GetCurrentMemoryUsage();
        if(afterMemUsage < GetDesiredByteMemoryUsageSoft()) // Used must be below both soft and hard
        {
          float fracMemUsed = ((float)afterMemUsage / (float)GetDesiredByteMemoryUsageHard());
                  
          // If we hardly used any of our memory, let the memory limit shrink
          if(fracMemUsed < GMMACHINE_GC_HARD_MEM_SHRINK_THRESH)
          {
            if( GMMACHINE_AUTOMEMALLOWSHRINK )
            {
              SetDesiredByteMemoryUsageHard( (int)(GMMACHINE_GC_HARD_MEM_DEC_FRAC_OF_USED * (float)afterMemUsage) );
              SetDesiredByteMemoryUsageSoft( (int)(GMMACHINE_GC_SOFT_MEM_DEFAULT_FRAC_OF_HARD * (float)GetDesiredByteMemoryUsageHard()) );
            }
          }
          else
          {
            float softFrac = (float)GetDesiredByteMemoryUsageSoft() / (float)GetDesiredByteMemoryUsageHard();
            softFrac -= GMMACHINE_GC_SOFT_MEM_DEC_FRAC;
            if(softFrac > GMMACHINE_GC_SOFT_MEM_MIN_FRAC)
            {
              // We ran out of memory, but the machine didn't need to, we should have started inc GC earlier
              int desired =  (int)(softFrac * (float)GetDesiredByteMemoryUsageHard());
              if(desired > afterMemUsage)
              {
                SetDesiredByteMemoryUsageSoft(desired);
                // NOTE: Not using GMMACHINE_AUTOMEMALLOWSHRINK here because hard limit is the critical 
                //       one and soft limit was badly set, so allow soft to be shrunk in this case only.
              }
            }
            else
            {
              // This should never occur.
              // The GC needs to be configured to collect more per cycle so it finishes faster
              // Check out the values of m_gc.GetWorkPerIncrement() and  m_gc.GetDestructPerIncrement()
              ++m_statsGCWarnings;
            }
          }
        }
        else
        {
          // We ran out of memory because we needed more than our limit, so increase limit
          int newHard = (int)(GMMACHINE_GC_HARD_MEM_INC_FRAC_OF_USED * (float)afterMemUsage);
          int newSoft = (int)(GMMACHINE_GC_SOFT_MEM_DEFAULT_FRAC_OF_HARD * (float)GetDesiredByteMemoryUsageHard());

          if( !GMMACHINE_AUTOMEMALLOWSHRINK )
          {
            // Don't allow shrink
            newSoft = gmMax(newSoft, GetDesiredByteMemoryUsageSoft());
            newHard = gmMax(newHard, GetDesiredByteMemoryUsageHard());
            // We can only size up, so make sure hard limit is at least this fraction above soft
            float softFracHard = (float)newSoft / (float)newHard;
            if( softFracHard < GMMACHINE_GC_SOFT_MEM_DEFAULT_FRAC_OF_HARD )
            {
              newHard = (int)(newSoft * (1.0f / GMMACHINE_GC_SOFT_MEM_DEFAULT_FRAC_OF_HARD) );
            }
          }

          SetDesiredByteMemoryUsageHard( newHard );
          SetDesiredByteMemoryUsageSoft( newSoft );
        }
      }
    }
    else 
    {
      // If we are not collecting, see if we need to start
      if(m_gc->IsOff())
      {
        // Have esceeded the soft limit?
        if(GetCurrentMemoryUsage() > GetDesiredByteMemoryUsageSoft())
        {
          // Reclaim memory from known garbage if we can
          if(!m_gc->ReclaimSomeFreeObjects())
          {
            ++m_gcPhaseCount;

            // Turn GC back on
            m_gc->ReclaimObjectsAndRestartCollection(); 
          }
        }
      }
      // If we are collecting, then collect some more this opportunity
      if(!m_gc->IsOff())
      {
        if(m_gc->Collect())
        {
          // We have finished collecting.  The to-be-freed memory will still be waiting for reclaimation.
          if( m_gcPhaseCount == 2 )
          {
            m_gcPhaseCount = 0;
            if(m_framesSinceLastIncCollect < GMMACHINE_GC_MIN_FRAMES_SINCE_RESTART)
            {
              // Since we got here and we have been collecting very regularly,
              // the soft limit may be set too close to the actual memory usage
              // causing oscilation.  Both the hard and soft limits should be set higher.
              ++m_statsGCWarnings;
            }
            m_framesSinceLastIncCollect = 0;
          }
          ++m_statsGCIncCollect;

          // Note that this point is not the low memory after a GC cycle.
          // It may be half of the two part process after restarting due to the alloc black method.
          // If GC took a while, lots of new allocs may have built up also.
          // This is the reason auto-calibrating memory limits in the soft range is difficult.
        }
      }
    }
  }

  return result;
}


bool gmMachine::IsGCRunning()
{
  return !m_gc->IsOff();
}


#else //GM_USE_INCGC

bool gmMachine::CollectGarbage(bool a_forceFullCollect)
{
  gmThread * tit;
  gmObject * obj;

  // do we need to garbage collect?
  if(m_gcEnabled  && (a_forceFullCollect || (GetCurrentMemoryUsage() > m_desiredByteMemoryUsageHard)) )
  {
    //printf("************* COLLECTING GARBAGE ****************"GM_NL);

    if(++m_mark == GM_MARK_PERSIST) m_mark = GM_MARK_START + 1;

    // call the gc callback
    if(s_machineCallback) s_machineCallback(this, MC_COLLECT_GARBAGE, (void *) &m_mark);

    // iterate cpp owned gmObjects
    gmHash<gmObject*, ObjHashNode>::Iterator cgmoIt;
    for(cgmoIt = a_machine->m_cppOwnedGMObjs.First(); cgmoIt; ++cgmoIt)
    {
      gmObject* curObj = cgmoIt->m_obj;
      curObj->Mark(a_machine, mark);
    }

    // iterate over all threads and mark the stacks.
    for(tit = m_runningThreads.GetFirst(); m_runningThreads.IsValid(tit); tit = m_runningThreads.GetNext(tit)) tit->Mark(m_mark);
    for(tit = m_blockedThreads.GetFirst(); m_blockedThreads.IsValid(tit); tit = m_blockedThreads.GetNext(tit)) tit->Mark(m_mark);
    for(tit = m_sleepingThreads.GetFirst(); m_sleepingThreads.IsValid(tit); tit = m_sleepingThreads.GetNext(tit)) tit->Mark(m_mark);
    for(tit = m_exceptionThreads.GetFirst(); m_exceptionThreads.IsValid(tit); tit = m_exceptionThreads.GetNext(tit)) tit->Mark(m_mark);

    // iterate over global variables and mark
    m_global->Mark(this, m_mark);
    gmuint i;
    for(i = 0; i < m_types.Count(); ++i)
    {
      m_types[i].m_variables->Mark(this, m_mark);
    }

#if GMMACHINE_THREEPASSGC
    // mark any persisting objects such that they may mark their children
    obj = m_objects;
    while(obj)
    {
      if(obj->m_mark == GM_MARK_PERSIST)
      {
        obj->Mark(this, m_mark);
      }
      obj = obj->m_sysNext;
    }
#endif // GMMACHINE_THREEPASSGC

    // iterate over all objects, destruct any old object and free it.
    gmObject ** objnext = &m_objects;
    while(*objnext)
    {
      obj = *objnext;
      if(obj->m_mark != m_mark && obj->m_mark != GM_MARK_PERSIST)
      {
        *objnext = obj->m_sysNext;
        obj->Destruct(this);
        FreeObject(obj);
      }
      else 
      {
        objnext = &obj->m_sysNext;
      }
    }

    if(m_autoMem)
    {
      m_desiredByteMemoryUsageHard = (int) (GMMACHINE_AUTOMEMMULTIPY * (float) GetCurrentMemoryUsage());
    }

    return true;
  }
  
  return false;
}


bool gmMachine::IsGCRunning()
{
  return false;
}


#endif //GM_USE_INCGC


unsigned int gmMachine::GetSystemMemUsed() const
{
  unsigned int total = 0;
  total += m_memStringObj.GetSystemMemUsed();
  total += m_memTableObj.GetSystemMemUsed();
  total += m_memFunctionObj.GetSystemMemUsed();
  total += m_memUserObj.GetSystemMemUsed();
  total += m_memStackFrames.GetSystemMemUsed();
  total += m_fixedSet.GetSystemMemUsed();

  // threads
  gmThread * tit;
  for(tit = m_runningThreads.GetFirst(); m_runningThreads.IsValid(tit); tit = m_runningThreads.GetNext(tit)) total += tit->GetSystemMemUsed();
  for(tit = m_blockedThreads.GetFirst(); m_blockedThreads.IsValid(tit); tit = m_blockedThreads.GetNext(tit)) total += tit->GetSystemMemUsed();
  for(tit = m_sleepingThreads.GetFirst(); m_sleepingThreads.IsValid(tit); tit = m_sleepingThreads.GetNext(tit)) total += tit->GetSystemMemUsed();
  for(tit = m_killedThreads.GetFirst(); m_killedThreads.IsValid(tit); tit = m_killedThreads.GetNext(tit)) total += tit->GetSystemMemUsed();
  for(tit = m_exceptionThreads.GetFirst(); m_exceptionThreads.IsValid(tit); tit = m_exceptionThreads.GetNext(tit)) total += tit->GetSystemMemUsed();

  return total;
}

#include "memdbgoff.h"

gmStringObject * gmMachine::AllocStringObject(const char * a_string, int a_length)
{
  gmStringObject * newStringObj = m_strings.Find(a_string);
  if(newStringObj)
  {
    m_gc->Revive(newStringObj); // If string was in free list waiting to be finalized, revive it.
    return newStringObj;
  }
  
  if(a_length < 0)
  {
    a_length = strlen(a_string);
  }
  char * string = (char *) Sys_Alloc(a_length + 1);
  memcpy(string, a_string, a_length + 1);

#if GMMACHINE_GCEVERYALLOC
  CollectGarbage();
#endif
  newStringObj = (gmStringObject *) m_memStringObj.Alloc();

  GM_PLACEMENT_NEW( gmStringObject(string, a_length), newStringObj );

#if GM_USE_INCGC
  m_gc->AllocateObject(newStringObj);
  //AddCountObj(newStringObj);
#else //GM_USE_INCGC
  GM_ADDOBJECT(newStringObj);
#endif //GM_USE_INCGC

  // insert into hash
  m_strings.Insert(newStringObj);

  m_currentMemoryUsage += sizeof(gmStringObject);
  return newStringObj;
}



gmStringObject * gmMachine::AllocPermanantStringObject(const char * a_string, int a_length)
{
  gmStringObject * newStringObj = AllocStringObject(a_string, a_length);
#if GM_USE_INCGC 
  newStringObj->SetPersist(true);
  #if GM_GC_KEEP_PERSISTANT_SEPARATE
    m_gc->MakeObjectPersistant(newStringObj);
  #else //GM_GC_KEEP_PERSISTANT_SEPARATE
    m_permanantStrings.InsertLast(newStringObj);
  #endif //GM_GC_KEEP_PERSISTANT_SEPARATE
#else //GM_USE_INCGC
  newStringObj->m_mark = GM_MARK_PERSIST;
#endif //GM_USE_INCGC

  return newStringObj;
}



gmTableObject * gmMachine::AllocTableObject()
{
#if GMMACHINE_GCEVERYALLOC
  CollectGarbage();
#endif
  gmTableObject * newTableObj = (gmTableObject *) m_memTableObj.Alloc();
  GM_PLACEMENT_NEW(gmTableObject, newTableObj);

#if GM_USE_INCGC
  m_gc->AllocateObject(newTableObj);
  //AddCountObj(newTableObj);
#else //GM_USE_INCGC
  GM_ADDOBJECT(newTableObj);
#endif //GM_USE_INCGC

  m_currentMemoryUsage += sizeof(gmTableObject);
  return newTableObj;
}

gmFunctionObject * gmMachine::AllocFunctionObject(gmCFunction a_function)
{
#if GMMACHINE_GCEVERYALLOC
  CollectGarbage();
#endif
  gmFunctionObject * newFunctionObj = (gmFunctionObject *) m_memFunctionObj.Alloc();
   GM_PLACEMENT_NEW(gmFunctionObject, newFunctionObj);
  
#if GM_USE_INCGC
  m_gc->AllocateObject(newFunctionObj);
  //AddCountObj(newFunctionObj);
#else //GM_USE_INCGC
  GM_ADDOBJECT(newFunctionObj);
#endif //GM_USE_INCGC

  newFunctionObj->m_cFunction = a_function;

  m_currentMemoryUsage += sizeof(gmFunctionObject);
  return newFunctionObj;
}

#include "memdbgon.h"



gmUserObject * gmMachine::AllocUserObject(void * a_user, int a_userType)
{
#if GMMACHINE_GCEVERYALLOC
  CollectGarbage();
#endif
  gmUserObject * newUserObj = (gmUserObject *) m_memUserObj.Alloc();
  gmConstructElement<gmUserObject>(newUserObj);

#if GM_USE_INCGC
  m_gc->AllocateObject(newUserObj);
  //AddCountObj(newUserObj);
#else //GM_USE_INCGC
  GM_ADDOBJECT(newUserObj);
#endif //GM_USE_INCGC

  newUserObj->m_userType = a_userType;
  newUserObj->m_user = a_user;
  m_currentMemoryUsage += sizeof(gmUserObject);
  return newUserObj;
}



void gmMachine::Sys_FreeUniqueString(const char * a_string)
{
  if(m_strings.RemoveKey(a_string))
  {
    Sys_Free(const_cast<char *>(a_string));
  }
}



int gmMachine::GetThreadId()
{
  while(GetThread(++m_threadId)) {}
  return m_threadId;
}



void gmMachine::FreeObject(gmObject * a_obj)
{
  //RemoveCountObj(a_obj);

  switch(a_obj->GetType())
  {
    case GM_STRING:
    {
      m_memStringObj.Free(a_obj);
      m_currentMemoryUsage -= sizeof(gmStringObject);
      break;
    }
    case GM_TABLE:
    {
      m_memTableObj.Free(a_obj);
      m_currentMemoryUsage -= sizeof(gmTableObject);
      break;
    }
    case GM_FUNCTION:
    {
      m_memFunctionObj.Free(a_obj);
      m_currentMemoryUsage -= sizeof(gmFunctionObject);
      break;
    }
    default: // >= GM_USER types
    {
      m_memUserObj.Free(a_obj);
      m_currentMemoryUsage -= sizeof(gmUserObject);
      break;
    }
  }
}


#if GM_USE_INCGC
gmObject * gmMachine::CheckReference(gmptr a_ref)
{
  return m_gc->CheckReference(a_ref);
}
#else //GM_USE_INCGC
gmObject * gmMachine::CheckReference(gmptr a_ref)
{
  for(gmObject* it = m_objects; it; it = it->m_sysNext)
  {
    if((gmptr)it == a_ref)
    {
      return it;
    }
  }

  // check global refs
  if(a_ref == (gmptr) m_global) return m_global;
  
  GM_ASSERT(0);
  return NULL;
}
#endif //GM_USE_INCGC


void gmMachine::Type::Init()
{
  m_variables = NULL;
  m_name = NULL;
  memset(m_nativeOperators, 0, sizeof(gmOperatorFunction) * O_MAXOPERATORS);
  memset(m_operators, 0, sizeof(gmptr) * O_MAXOPERATORS);
  m_asString = NULL;
#if GM_USE_INCGC
  m_gcDestruct = NULL;
  m_gcTrace = NULL;
#else
  m_mark = NULL;
  m_gc = NULL;
#endif
  m_asString = NULL;
}



void gmMachine::ResetDefaultTypes()
{
  // clean up old types
  gmuint i;
  for(i = 0; i < m_types.Count(); ++i)
  {
#if GM_USE_INCGC
    //Note, old objects must be null or already destructed by GC
    m_types[i].m_variables = NULL;
    m_types[i].m_name = NULL;
#else //GM_USE_INCGC
    m_types[i].m_variables->Destruct(this);
    FreeObject(m_types[i].m_variables);
    m_types[i].m_name = NULL;
#endif //GM_USE_INCGC
  }

  // init the basic types
  m_types.SetCount(GM_USER);
  // Alloc type table
  for(i=0; i<m_types.Count(); ++i)
  {
    m_types[i].Init();
    m_types[i].m_variables = AllocTableObject();
  }

  // init names
  m_types[GM_NULL].m_name = AllocPermanantStringObject("null");
  m_types[GM_INT].m_name = AllocPermanantStringObject("int");
  m_types[GM_FLOAT].m_name = AllocPermanantStringObject("float");
  m_types[GM_STRING].m_name = AllocPermanantStringObject("string");
  m_types[GM_TABLE].m_name = AllocPermanantStringObject("table");
  m_types[GM_FUNCTION].m_name = AllocPermanantStringObject("function");

  // init basic operators
  gmInitBasicType(GM_NULL, m_types[GM_NULL].m_nativeOperators);
  gmInitBasicType(GM_INT, m_types[GM_INT].m_nativeOperators);
  gmInitBasicType(GM_FLOAT, m_types[GM_FLOAT].m_nativeOperators);
  gmInitBasicType(GM_STRING, m_types[GM_STRING].m_nativeOperators);
  gmInitBasicType(GM_TABLE, m_types[GM_TABLE].m_nativeOperators);
  gmInitBasicType(GM_FUNCTION, m_types[GM_FUNCTION].m_nativeOperators);
}



void gmMachine::Sys_RemoveBlocks(gmThread * a_thread)
{
  gmBlock * block = a_thread->Sys_GetBlocks(), * next;
  while(block)
  {
    next = block->m_nextBlock;
    gmBlockList * list = block->m_list;
    block->Remove();
    if(list->m_blocks.IsEmpty())
    {
      list = (gmBlockList *) m_blocks.Remove(list);
      Sys_Free(list);
    }
    Sys_Free(block);
    block = next;
  }
  a_thread->Sys_SetBlocks(NULL);
}



void gmMachine::Sys_RemoveSignals(gmThread * a_thread)
{
  gmSignal * signal = a_thread->Sys_GetSignals(), * next;
  while(signal)
  {
    next = signal->m_nextSignal;
    Sys_Free(signal);
    signal = next;
  }
  a_thread->Sys_SetSignals(NULL);
}



gmuint32 gmMachine::AddSourceCode(const char * a_source, const char * a_filename)
{
  gmuint32 id = 0;

  if(a_filename == NULL) { a_filename = "unknown"; }

  if(m_debug)
  {
    // calculate the id.
    id = gmCrc32String(a_source);

    // see if we already have the source.
    gmSourceEntry * entry = m_source.GetFirst();
    while(m_source.IsValid(entry))
    {
      if(entry->m_id == id)
      {
        return id;
      }
      entry = m_source.GetNext(entry);
    }

    // we dont have it, add it
    m_source.InsertFirst( GM_NEW( gmSourceEntry(a_source, a_filename) ) );
  }
  return id;
}



bool gmMachine::GetSourceCode(gmuint32 a_id, const char * &a_source, const char * &a_filename)
{
  if(m_debug)
  {
    gmSourceEntry * entry = m_source.GetFirst();
    while(m_source.IsValid(entry))
    {
      if(entry->m_id == a_id)
      {
        a_source = entry->m_source;
        a_filename = entry->m_filename;
        return true;
      }
      entry = m_source.GetNext(entry);
    }
  }
  return false;
}



#if GM_USE_INCGC
const void * gmMachine::GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line)
{
  return m_gc->GetInstructionAtBreakPoint(a_sourceId, a_line);
}
#else //GM_INC_GM
const void * gmMachine::GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line)
{
  gmObject * object = m_objects;
  while(object)
  {
    if(object->GetType() == GM_FUNCTION)
    {
      gmFunctionObject * function = (gmFunctionObject *) object;
      if(function->GetSourceId() == a_sourceId)
      {
        const void * i = function->GetInstructionAtLine(a_line);
        if(i) return i;
      }
    }
    object = object->m_sysNext;
  }
  return NULL;
}
#endif //GM_INC_GM


bool gmMachine::IsCPPOwnedGMObject(gmObject * a_obj)
{
  ObjHashNode * foundNode = m_cppOwnedGMObjs.Find(a_obj);
  return (foundNode != NULL);
}


void gmMachine::AddCPPOwnedGMObject(gmObject * a_obj)
{
  if( !a_obj )
  {
    return; // Ignore NULL
  }
  
#ifdef GM_DEBUG_BUILD
  // Should not already exist
  ObjHashNode * foundNode = m_cppOwnedGMObjs.Find(a_obj);
  GM_ASSERT( !foundNode );
#endif //GM_DEBUG_BUILD

  ObjHashNode * newNode = (ObjHashNode *)Sys_Alloc( sizeof(ObjHashNode) );
  newNode->m_obj = a_obj;
  m_cppOwnedGMObjs.Insert(newNode);
}


void gmMachine::RemoveCPPOwnedGMObject(gmObject * a_obj)
{
  if( !a_obj )
  {
    return; // Ignore NULL
  }

  ObjHashNode * foundNode = m_cppOwnedGMObjs.Find(a_obj);
  if( foundNode )
  {
    m_cppOwnedGMObjs.Remove(foundNode);
    Sys_Free(foundNode);

    // Apply write barrier for this logical LHS
    m_gc->WriteBarrier(a_obj);
  }
}


void gmMachine::RemoveAllCPPOwnedGMObjects()
{
	gmHash<gmObject*, ObjHashNode>::Iterator iter( &m_cppOwnedGMObjs );
	while ( iter.IsValid() )
	{
		ObjHashNode *pNode = iter.Resolve();
		iter.Inc();
		Sys_Free(pNode);
	}
	m_cppOwnedGMObjs.RemoveAll();
}


gmTableObject * gmMachine::GetTypeTable(gmType a_type) 
{    
   if( (a_type >= 0) && (a_type < (int)m_types.Count()) )
   {
     return m_types[a_type].m_variables;
   }
   return NULL; 
}

//
//
// Implementation of gmHooks
//
//



gmHooks::gmHooks(gmMachine * a_machine, const char * a_source, const char * a_filename) :
  m_machine(a_machine),
  m_source(a_source),
  m_filename(a_filename)
{
  m_gcEnabled = a_machine->IsGCEnabled();
  m_debug = false;
  m_errors = 0;
  m_rootFunction = NULL;
  m_sourceId = 0;
}



gmHooks::~gmHooks()
{
}



bool gmHooks::Begin(bool a_debug)
{
  m_gcEnabled = m_machine->IsGCEnabled();
  m_machine->EnableGC(false);

  // add the source code. if we are debugging
  if(a_debug)
  {
    m_sourceId = m_machine->AddSourceCode(m_source, m_filename);
  }
  
  m_debug = a_debug;
  return true;
}



bool gmHooks::AddFunction(gmFunctionInfo &a_info)
{
  gmObject * object = GM_MOBJECT(m_machine, a_info.m_id);
  if(object->GetType() == GM_FUNCTION)
  {
    gmFunctionObject * function = (gmFunctionObject *) object;
    if(a_info.m_root) { m_rootFunction = function; }
    return function->Init(m_machine, m_debug, a_info, m_sourceId);
  }
  return false;
}



bool gmHooks::End(int a_errors)
{
  m_machine->EnableGC(m_gcEnabled);  
  m_errors = a_errors;
  return true;
}



gmptr gmHooks::GetFunctionId()
{
  return (gmptr) m_machine->AllocFunctionObject(NULL);
}



gmptr gmHooks::GetSymbolId(const char * a_symbol)
{
  return (gmptr) m_machine->AllocPermanantStringObject(a_symbol);
}



gmptr gmHooks::GetStringId(const char * a_string)
{
  return (gmptr) m_machine->AllocStringObject(a_string);
}
