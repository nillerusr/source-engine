/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in this file

*/

#ifndef _GMMACHINE_H_
#define _GMMACHINE_H_

/**********************************************************
                  GameMonkey Script
                     created by
            Matthew Riek and Greg Douglas
**********************************************************/

/**********************************************************
GameMonkey Script License

Copyright (c) 2003  Auran Development Ltd.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated 
documentation files (the "Software"), to deal in the 
Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, 
distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice 
shall be included in all copies or substantial portions of
the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR 
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**********************************************************/

#include "gmConfig.h"
#include "gmMemFixed.h"
#include "gmMemFixedSet.h"
#include "gmLog.h"
#include "gmVariable.h"
#include "gmTableObject.h"
#include "gmOperators.h"
#include "gmFunctionObject.h"
#include "gmHash.h"
#include "gmArraySimple.h"
#include "gmIncGC.h"

#undef GetObject //Argh Windows defines this in WINGDI.H

#define GM_VERSION "1.24"

// fwd decls
class gmStringObject;
class gmUserObject;
class gmMachine;
class gmThread;
struct gmStackFrame;
struct gmSignal;
class gmSourceEntry;
class gmStream;
class gmBlockList;

enum gmMachineCommand
{
  MC_COLLECT_GARBAGE = 0, // called when a gc cycle is run, a_context is the gc mark

  MC_THREAD_EXCEPTION,    // called when a thread causes an exception.  a_context is the thread. (which is about to die) return false to kill thread, else it will hang around for debugging
  MC_THREAD_CREATE,       // called when a thread is created.  a_context is the thread.
  MC_THREAD_DESTROY,      // called when a thread is destroyed.  a_context is the thread. (which is about to die)
};

/// \brief gmMachineCallback is a callback used for debugger tie ins etc.  
/// \return depends on a_command
typedef bool (GM_CDECL *gmMachineCallback)(gmMachine * a_machine, gmMachineCommand a_command, const void * a_context);
#if GM_USE_INCGC
  typedef void (GM_CDECL *gmGCDestructCallback)(gmMachine * a_machine, gmUserObject* a_object);
  typedef bool (GM_CDECL *gmGCTraceCallback)(gmMachine * a_machine, gmUserObject* a_object, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone);
#else //GM_USE_INCGC
  typedef void (GM_CDECL *gmGarbageCollectCallback)(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark);
#endif //GM_USE_INCGC

/// \brief gmAsStringCallback is a callback registered with a gm type, which will convert the type to a string for print and debug purposes, the callback must
///        convert the type to a string < 256 characters long.
/// \param a_bufferSize at least 256 chars
typedef void (GM_CDECL *gmAsStringCallback)(gmUserObject * a_userObj, char * a_buffer, int a_bufferSize);
typedef void (GM_CDECL *gmPrintCallback)(gmMachine * a_machine, const char * a_string);
typedef bool (GM_CDECL *gmThreadIteratorCallback)(gmThread * a_thread, void * a_context);
typedef bool (GM_CDECL *gmUserBreakCallback)(gmThread * a_thread);

// the following callbacks return true if the thread is to yield after completion of the callback.
typedef bool (GM_CDECL *gmDebugLineCallback)(gmThread * a_thread);
typedef bool (GM_CDECL *gmDebugCallCallback)(gmThread * a_thread);
typedef bool (GM_CDECL *gmDebugRetCallback)(gmThread * a_thread);
typedef bool (GM_CDECL *gmDebugIsBrokenCallback)(gmThread * a_thread); // returns true if the thread is broken, or if the thread is pending delete after exception

/// \struct gmFunctionEntry
struct gmFunctionEntry
{
  const char * m_name;
  gmCFunction m_function;
  const void * m_userData;                        ///< Optional user ptr or value to assist binding
};

/// \struct gmThreadInfo
struct gmThreadInfo
{
  int m_threadId;
  int m_threadState;
  void * m_threadDebugUser;
};

/// \struct gmSignal
struct gmSignal
{
  int m_srcThreadId; ///< id of the signalling thread.
  int m_dstThreadId; ///< id of dst thread.
  gmVariable m_signal; ///< signal.
  gmSignal * m_nextSignal; ///< next thread in the threads signal list.
};

/// \class gmBlock
class gmBlock : public gmListDoubleNode<gmBlock>
{
public:
  bool m_signalled; ///< has this block been signalled?
  int m_srcThreadId; ///< id of the thread to cause the unblock
  gmVariable m_block; ///< the block object, this will be pushed on the stack if unblocked.
  gmThread * m_thread; ///< the thread this block belongs to
  gmBlock * m_nextBlock; ///< next block on this thread
  class gmBlockList * m_list; ///< parent block list.
};


/// \class gmMachine
/// \brief the gmMachine object represents a scripting instance.  many script threads may run on the one gmMachine.
///        gmMachine manages script source, script memory, garbage collection, function type overrides, operator overrides,
///        threads etc.
class gmMachine
{
public:

  gmMachine();
  virtual ~gmMachine();

  /// \brief ResetAndFreeMemory() will reset the gmMachine.
  void ResetAndFreeMemory();

  /// \brief Init() will recreate the machine... only need to call this if you have called ResetAndFreeMemory()
  /// \sa ResetAndFreeMemory();
  void Init();

  static gmMachineCallback s_machineCallback; ///< global machine callback
  static gmPrintCallback s_printCallback; ///< global print callback
  static gmUserBreakCallback s_userBreakCallback; ///< global execution break callback

  //
  //
  // Registry
  //
  //
  
  /// \brief RegisterLibrary() will register an array of functions to the machine as globals.
  /// \param a_asTable as non-null will create a table in global scope and place the functions in the table.
  /// \param a_newTable will create a new table for the functions; if false it will insert them into the existing table (if found) 
  void RegisterLibrary(gmFunctionEntry * a_functions, int a_numFunctions, const char * a_asTable = NULL, bool a_newTable = true);

  /// \brief RegisterTypeLibrary() will register an array of functions to the machine as type variables.
  void RegisterTypeLibrary(gmType a_type, gmFunctionEntry * a_functions, int a_numFunctions);

  /// \brief RegisterLibraryFunction() will register a single functions to the machine as global.
  /// \param a_name Script function name
  /// \param a_function C function
  /// \param a_asTable as non-null will use/create a table in global scope and place the functions in the table.
  void RegisterLibraryFunction(const char * a_name, gmCFunction a_function, const char * a_asTable = NULL, const void* a_userData = NULL)
  {
    gmFunctionEntry entry = {a_name, a_function, a_userData};
    RegisterLibrary(&entry, 1, a_asTable, false);
  }
 
  /// \brief RegisterTypeLibraryFunction() will register a single functions to the machine as type variable.
  /// \param a_type Valid/Registered type identifier
  /// \param a_name Script function name
  /// \param a_function C function
  void RegisterTypeLibraryFunction(gmType a_type, const char * a_name, gmCFunction a_function, const void* a_userData = NULL)
  {
    gmFunctionEntry entry = {a_name, a_function, a_userData};
    RegisterTypeLibrary(a_type, &entry, 1);
  }

  /// \brief CreateUserType() will create a new user type such that you may bind your own operator functions etc.
  /// \return the new user type.
  gmType CreateUserType(const char * a_name);

  /// \brief RegisterTypeVariable() will register a variable for a given gm type.  This allows a default library
  ///        to bind type specific functions and constants to that type. ie, binding a ToFloat() function to the
  ///        int type.
  void RegisterTypeVariable(gmType a_type, const char * a_variableName, const gmVariable &a_variable);

  /// \brief RegisterTypeOperator() will let you regiester operators for user types only.
  /// \param a_operator
  /// \param a_function
  /// \param a_nativeFunction
  bool RegisterTypeOperator(gmType a_type, gmOperator a_operator, gmFunctionObject * a_function, gmOperatorFunction a_nativeFunction = NULL);

  /// \brief GetTypeVariable() will lookup the type variables for the given variable key.
  inline gmVariable GetTypeVariable(gmType a_type, const gmVariable &a_key) const;

  /// \brief GetTypeNativeOperator() will lookup a type for a native operator
  inline gmOperatorFunction GetTypeNativeOperator(gmType a_type, gmOperator a_operator);

  /// \brief GetTypeOperator() will lookup a type for a GM function operator
  inline gmFunctionObject * GetTypeOperator(gmType a_type, gmOperator a_operator);

  /// \brief GetTypeName() will return the name of a_type
  const char * GetTypeName(gmType a_type);

  /// \brief Access the table for a 'type', or return NULL.
  gmTableObject * GetTypeTable(gmType a_type);

  //
  //
  // Thread Interface
  //
  //

  /// \brief CheckSyntax()
  /// \brief return the number of errors
  int CheckSyntax(const char * a_string);

  /// \brief ExecuteString() will compile a_string to byte code.  If no errors occured, the code will be executed
  ///        in a new thread.
  /// \param a_string is null terminated script string.
  /// \param a_threadId is set to the id of the thread and may be NULL.
  /// \param a_now is true, and execution will occur immediataly, and not at the next Execute() call.
  /// \param a_filename Name of the source file, used for debug purposes, or NULL.
  /// \param a_this The 'this' pointer passed to the un-named function.  Or NULL to pass a null gmVariable.
  /// \return the number of errors from compiling the script.
  /// \sa GetCompileLog()
  int ExecuteString(const char * a_string, int * a_threadId = NULL, bool a_now = true, const char * a_filename = NULL, gmVariable* a_this = NULL);

  /// \brief ExecuteLib() will execute a pre-compiled lib in a new thread
  /// \param a_stream is a stream to pull the lib from
  /// \param a_threadId is set to the id of the thread and may be NULL.
  /// \param a_now is true, and execution will occur immediataly, and not at the next Execute() call.
  /// \param a_filename is the filename the lib came from for debugging purposes.
  /// \return false on lib error
  bool ExecuteLib(gmStream &a_stream, int * a_threadId = NULL, bool a_now = true, const char * a_filename = NULL, gmVariable* a_this = NULL);

  /// \brief ExecuteFunction() will execute a thread on the passed function
  bool ExecuteFunction(gmFunctionObject * a_function, int * a_threadId = NULL, bool a_now = true, gmVariable* a_this = NULL);

  /// \brief CompileStringToLib() will compile a_string to byte code suitable for storage in a file.
  /// \param a_string is null terminated script string.
  /// \param a_stream is the file stream to compile the lib to
  /// \return the number of errors from compiling the script.
  /// \sa GetCompileLog()
  int CompileStringToLib(const char * a_string, gmStream &a_stream);

  /// \brief CompileStringToFunction()
  gmFunctionObject * CompileStringToFunction(const char * a_string, int *a_errorCount = NULL, const char * a_filename = NULL);

  /// \brief BindLibToFunction() Bind a precompiled library to a function.
  /// \param a_stream is a stream to pull the lib from
  /// \param a_filename is the filename the lib came from for debugging purposes. 
  /// \return Root function from Lib or NULL if failed
  gmFunctionObject * BindLibToFunction(gmStream &a_stream, const char * a_filename = NULL);
  
  /// \brief GetLog() will get the compile and runtime log of the last script compiled. log any runtime errors from
  ///        linked c functions to this log.
  inline gmLog &GetLog() { return m_log; }

  /// \brief CreateThread() will create a new thread.  This thread will be placed in the running thread list.
  /// \param a_this
  /// \param a_function
  /// \param a_threadId is set to the id of the created thread and may be NULL.
  /// \return a thread, or NULL if the thread finished executing or could not be created.
  gmThread * CreateThread(const gmVariable &a_this, const gmVariable &a_function, int * a_threadId = NULL);
  gmThread * CreateThread(int * a_threadId = NULL);

  /// \brief GetThread() will return the thread given a thread id.
  /// \return NULL on error.
  gmThread * GetThread(int a_threadId);

  /// \brief Signal() will signal a single thread with the given variable
  /// \param a_signal is the signal var
  /// \param a_dstThreadId is the thread id we wish to unblock. dstThread id of GM_INVALID_THREAD is all threads
  /// \param a_srcThreadId is the thread id of the signaling object
  /// \param a_persist is true and the signal may persist if it had the potential to unblock a SYS_PENDING thread.
  /// \return true if the signal fired, false otherwise
  bool Signal(const gmVariable &a_signal, int a_dstThreadId, int a_srcThreadId);

  /// \brief Sys_Block() will add a block to a thread.
  /// \return the index of the block that was signalled, or -1 if the blocks were added and the thread is to enter a blocked state.
  int Sys_Block(gmThread * a_thread, int m_numBlocks, const gmVariable * a_blocks);

  /// \brief KillThread()
  void KillThread(int a_threadId);

  /// \brief ForEachThread()
  void ForEachThread(gmThreadIteratorCallback a_callback, void * a_context);

  /// \brief Sys_SwitchState() will change the executing state of a thread.  if a_to is gmThread::KILLED, the thread
  ///        is reset.
  void Sys_SwitchState(gmThread * a_thread, int a_to);

  /// \brief KillExceptionThreads()
  void KillExceptionThreads();

  /// \brief Execute() will execute all running threads
  /// \param m_deltaTime is the time in milliseconds since the machine was last updated.
  /// \return number of running sleeping and blocked threads.
  int Execute(gmuint32 a_delta);

  /// \brief GetTime() will return the machine time in milliseconds.
  inline gmuint32 GetTime() const { return m_time; }

  /// \brief Presize() will presize the pools
  void Presize(int a_pool8,
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
               );

  //
  //
  // Garbage Collection Interface
  //
  //

  /// \brief EnableGC() allows you to turn garbage collection on and off.... useful for when you are allocating many
  ///        objects from c \ c++, but have not yet added them to a thread stack or referenced table \ object such that
  ///        they will be marked should the garbage collection fire.  make sure you turn gc back on again!!!
  inline void EnableGC(bool a_state = true) { m_gcEnabled = a_state; }
  inline bool IsGCEnabled() const { return m_gcEnabled; }

  /// \brief CollectGarbage() will perform a garbage collection sweep iff the desired byte memory usage is less than
  ///        the current memory usage.
  ///        NOTE: If incremental collector is used, this may perform a FULL collect immediately.
  /// \param a_forceFullCollect Force full collection immediately if garbage collection is not disabled
  /// \return true if the garbage collector was run.
  bool CollectGarbage(bool a_forceFullCollect = false);

  /// \brief GetCurrentMemoryUsage() will return the number of bytes currently allocated by the system.
  inline int GetCurrentMemoryUsage() const { return m_currentMemoryUsage + m_fixedSet.GetMemUsed(); }

  /// \brief Set the machine to run a gc sweep whenever memory exceeds this limit.
  inline void SetDesiredByteMemoryUsageHard(int a_desiredByteMemoryUsageHard);

  /// \brief Set the machine to run a gc sweep whenever memory exceeds this limit.
  inline void SetDesiredByteMemoryUsageSoft(int a_desiredByteMemoryUsageSoft);

  /// \brief Return the number of bytes currently allocated by the system.
  inline int GetDesiredByteMemoryUsageHard() const { return m_desiredByteMemoryUsageHard; }

  /// \brief Return the number of bytes currently allocated by the system.
  inline int GetDesiredByteMemoryUsageSoft() const { return m_desiredByteMemoryUsageSoft; }

  /// \brief SetAutoMemoryUsage() will enable auto adjustment of the memory limit occuring with subsequent garbage collects.
  inline void SetAutoMemoryUsage(bool a_enableAutoAdjust);
  
  /// \brief Is automatic memory limit calculation enabled?
  inline bool GetAutoMemoryUsage() const          { return m_autoMem; }

  /// \brief GetSystemMemUsed will return the number of bytes allocated by the system.  This is slow, call for debug only
  unsigned int GetSystemMemUsed() const;

  /// \brief Adjust the amount of memory known to be used by the machine.
  /// Use this function carefully with user types to allow the machine 
  /// to consider external memory allocated or freed in association 
  /// with a user type that will be garbage collected by the machine.  
  /// Make sure the function is called as positive on the alloc and negative on the free.
  inline void AdjustKnownMemoryUsed(int a_amountUsedOrFreed)  { m_currentMemoryUsage  += a_amountUsedOrFreed; }

  /// \brief RegisterUserCallbacks() will register user type garbage collect methods.
#if GM_USE_INCGC
  void RegisterUserCallbacks(gmType a_type, gmGCTraceCallback a_gcTrace, gmGCDestructCallback a_gcDestruct, gmAsStringCallback a_asString = NULL);

  /// \brief GetUserMarkCallback() will return the gc mark call back for a user type
  inline gmGCTraceCallback GetUserGCTraceCallback(gmType a_type) const { return m_types[a_type].m_gcTrace; }

  /// \brief GetUserGCCallback() will return the gc destruct call back for a user type
  inline gmGCDestructCallback GetUserGCDestructCallback(gmType a_type) const { return m_types[a_type].m_gcDestruct; }

#else //GM_USE_INCGC

  void RegisterUserCallbacks(gmType a_type, gmGarbageCollectCallback a_mark, gmGarbageCollectCallback a_gc, gmAsStringCallback a_asString = NULL);

  /// \brief GetUserMarkCallback() will return the gc mark call back for a user type
  inline gmGarbageCollectCallback GetUserMarkCallback(gmType a_type) const { return m_types[a_type].m_mark; }

  /// \brief GetUserGCCallback() will return the gc destruct call back for a user type
  inline gmGarbageCollectCallback GetUserGCCallback(gmType a_type) const { return m_types[a_type].m_gc; }

#endif //GM_USE_INCGC

  /// \brief Return the callback associated with a_type
  inline gmAsStringCallback GetUserAsStringCallback(gmType a_type) const { return m_types[a_type].m_asString; }

  //
  //
  // Object Interface
  //
  //
  
  /// \brief GetGlobals() will return the gmTableObject used to store global variables.  note that global variables 
  ///        are common to all threads.
  inline gmTableObject * GetGlobals() { return m_global; }

  inline gmTableObject * GetTrueGlobals() { return m_trueGlobal; }

  void SetGlobals( gmTableObject *pNewGlobals ) { m_global = pNewGlobals; }

  /// \brief GetObject() will convert a gmptr (machine pointer size int) into an object pointer.  use this whenever
  ///        converting from a gmVariable m_value.m_ref to an object.
  inline gmObject * GetObject(gmptr a_ref);

  /// \brief AllocStringObject() will create a constant string object from the unique string pool.
  /// \param a_length is the string length not including '\0' terminator, (-1) if unknown
  gmStringObject * AllocStringObject(const char * a_string, int a_length = -1);

  /// \brief AllocPermanantStringObject() will create a constant string object from the unique string pool.  this
  ///        string will not be garbage collected. (m_mark == GM_PERSIST)
  /// \param a_length is the string length not including '\0' terminator, (-1) if unknown
  gmStringObject * AllocPermanantStringObject(const char * a_string, int a_length = -1);

  /// \brief AllocTableObject() will create a new empty table.
  gmTableObject * AllocTableObject();

  /// \brief AllocFunctionObject() will create a new function.
  gmFunctionObject * AllocFunctionObject(gmCFunction a_function = NULL);

  /// \brief AllocUserObject() will create a new user object.  
  /// \param a_user is a hook to tie the user object to any system.
  /// \param a_userType is the user type user for operator binding etc. 
  /// \sa CreateUserType()
  gmUserObject * AllocUserObject(void * a_user, int a_userType);

  //
  //
  // Debug Interface
  //
  //

  void * m_debugUser; ///< user hook for debugger

  /// \brief SetDebugMode() will compile byte code with debug info.
  ///        This includes storing the source code in the gmMachine, and compiling line and symbol information
  ///        into the gmFunctionObjects.
  inline void SetDebugMode(bool a_debug) { m_debug = a_debug; }

  /// \brief GetDebugMode()
  inline bool GetDebugMode() const { return m_debug; }

  /// \brief AddSourceCode() will add source code to the machine, and return a unique id.
  ///        This is used when debug mode is set so the remote debugger can retrieve source as needed
  ///        for debugging.
  /// \return a unique id for the source code, or 0 on error.
  gmuint32 AddSourceCode(const char * a_source, const char * a_filename);

  /// \brief GetSourceCode() will get source code debugging purposes.
  /// \return true if the source was found, a_source and a_filename are set to the source and filname for the unique id.
  bool GetSourceCode(gmuint32 a_id, const char * &a_source, const char * &a_filename);

  /// \brief GetInstructionAtBreakPoint will return the insturction at the given break point, or NULL if the break
  ///        point could not be found.  only works in debug mode.  This is quite slow.
  const void * GetInstructionAtBreakPoint(gmuint32 a_sourceId, int a_line);

  // debugger hooks

  gmDebugLineCallback m_line;
  gmDebugCallCallback m_call;
  gmDebugRetCallback m_return;
  gmDebugIsBrokenCallback m_isBroken;

  //
  //
  // Implementation allocators
  //
  //

  inline gmStackFrame * Sys_AllocStackFrame() { return (gmStackFrame *) m_memStackFrames.Alloc(); }
  inline void Sys_FreeStackFrame(gmStackFrame * a_frame) { m_memStackFrames.Free(a_frame); }
  void Sys_FreeUniqueString(const char * a_string);
  inline void * Sys_Alloc(int a_size);
  inline void Sys_Free(void * a_mem) { m_fixedSet.Free(a_mem); }

  void Sys_RemoveBlocks(gmThread * a_thread);
  void Sys_RemoveSignals(gmThread * a_thread);

  void Sys_SignalCreateThread(gmThread * a_thread);

#if GM_USE_INCGC
  static void GM_CDECL ScanRootsCallBack(gmMachine* a_machine, gmGarbageCollector* a_gc);
  inline gmGarbageCollector* GetGC()                {GM_ASSERT(m_gc); return m_gc;}
  inline void DestructDeleteObject(gmObject* a_object) {FreeObject(a_object);}

  gmGarbageCollector* m_gc;
#endif //GM_USE_INCGC

  inline int GetStatsGCNumFullCollects()          { return m_statsGCFullCollect; }
  inline int GetStatsGCNumIncCollects()           { return m_statsGCIncCollect; }
  inline int GetStatsGCNumWarnings()              { return m_statsGCWarnings; }
  /// \brief Is GC actually running a cycle
  bool IsGCRunning();

  /// \brief Add a CPP owned gmObject so machine knows about it to handle GC.
  void AddCPPOwnedGMObject(gmObject * a_obj);
  /// \brief Remove a CPP owned gmObject.
  void RemoveCPPOwnedGMObject(gmObject * a_obj);
  /// \brief Check if gmObject is in CPP owned list
  bool IsCPPOwnedGMObject(gmObject * a_obj);
  /// \brief Remove all CPP owned gmObjects.
  void RemoveAllCPPOwnedGMObjects();

protected:

  // Threads
  int m_threadId;                                 ///< cycling thread number
  gmListDouble<gmThread> m_runningThreads;
  gmListDouble<gmThread> m_blockedThreads;
  gmListDouble<gmThread> m_sleepingThreads;       ///< sorted by timez
  gmListDouble<gmThread> m_killedThreads;
  gmListDouble<gmThread> m_exceptionThreads;      ///< dead threads, hanging around for debugging
  gmHash<int, gmThread> m_threads;
  int GetThreadId();
  gmuint32 m_time;                                ///< machine time in milliseconds. (gives us 50 days)
  gmThread * m_nextThread;                        ///< Set when cycling through threads, allows remove during iteration
  bool m_nextThreadValid;                         ///< Set to true when m_nextThread is in use, even if it is null, which occurs on last thread.

  // Objects
  void FreeObject(gmObject * a_obj);              ///< FreeObject() does not Destruct the object.
  gmObject * CheckReference(gmptr a_ref);
  gmTableObject * m_global;                       ///< global variables
  gmTableObject * m_trueGlobal;
  gmObject * m_objects;                           ///< list of all objects

  // Allocators
  gmMemFixed m_memStringObj;                      ///< memory for String objects
  gmMemFixed m_memTableObj;                       ///< memory for Table objects
  gmMemFixed m_memFunctionObj;                    ///< memory for Function objects
  gmMemFixed m_memUserObj;                        ///< memory for User objects
  gmMemFixed m_memStackFrames;                    ///< memory for stack frame structures
  gmMemFixedSet m_fixedSet;                       ///< string and small variable sized stuff allocator.

  // Garbage Collection
  int m_desiredByteMemoryUsageHard;               ///< The hard upper memory limit
  int m_desiredByteMemoryUsageSoft;               ///< The soft limit where incremental GC may start
  int m_currentMemoryUsage;                       ///< Current (known) used memory
  bool m_autoMem;                                 ///< Automatically adjust memory limit(s)
  gmuint32 m_mark;                                ///< The mark phase Id for atomic GC
  bool m_gcEnabled;                               ///< GC enabled/disabled
  int m_framesSinceLastIncCollect;                ///< number of frames or oportunities since last inc GC end
  int m_gcPhaseCount;                             ///< GC phase, 2 phases required for full GC
  int m_statsGCFullCollect;                       ///< How many times a full collect has occured
  int m_statsGCIncCollect;                        ///< How many times incremental collect has started
  int m_statsGCWarnings;                          ///< The incGC thinks it is being used inefficiently.  It this number is large and growing rapidly the hard and soft limits may need calibrating.

  // String Table
  gmHash<const char *, gmStringObject> m_strings;

  // Types
  class Type
  {
  public:
    gmStringObject * m_name;                      ///< type name
    gmOperatorFunction m_nativeOperators[O_MAXOPERATORS]; ///< stack operators
    gmptr m_operators[O_MAXOPERATORS];            ///< slow script call operators
    gmTableObject * m_variables;                  ///< user type variables
#if GM_USE_INCGC
    gmGCDestructCallback m_gcDestruct;            ///< user type gc destruct callback
    gmGCTraceCallback m_gcTrace;                  ///< user type gc trace callback
#else //GM_USE_INCGC
    gmGarbageCollectCallback m_mark;              ///< user type gc mark callback
    gmGarbageCollectCallback m_gc;                ///< user type gc callback
#endif //GM_USE_INCGC
    gmAsStringCallback m_asString;                ///< user type AsString callback

    void Init();
  };

  gmArraySimple<Type> m_types;                   ///< Variable types
#if GM_USE_INCGC
  #if !GC_KEEP_PERSISTANT_SEPARATE
  gmArraySimple<gmObject*> m_permanantStrings;
  #endif //!GC_KEEP_PERSISTANT_SEPARATE
#endif //GM_USE_INCGC

  void ResetDefaultTypes();

  // Blocking
  gmHash<gmVariable, gmBlockList, gmVariable> m_blocks; // current registered blocks.

  // C++ owned gmObjects
  class ObjHashNode : public gmHashNode<gmObject *, ObjHashNode>
  {
  public:
    gmObject * m_obj;
    const gmObject * GetKey() const { return m_obj; }
  };
  gmHash<gmObject*, ObjHashNode> m_cppOwnedGMObjs; ///< cpp owned gmObjects

  // Debugging
  bool m_debug;
  gmListDouble<gmSourceEntry> m_source;
  gmLog m_log;
};

//
//
// INLINE IMPLEMENTATION
//
//


inline void gmMachine::SetDesiredByteMemoryUsageHard(int a_desiredByteMemoryUsageHard)
{
  m_desiredByteMemoryUsageHard = a_desiredByteMemoryUsageHard;
}


inline void gmMachine::SetDesiredByteMemoryUsageSoft(int a_desiredByteMemoryUsageSoft)
{
  m_desiredByteMemoryUsageSoft = a_desiredByteMemoryUsageSoft;
}


inline void gmMachine::SetAutoMemoryUsage(bool a_enableAutoAdjust)
{
  m_autoMem = a_enableAutoAdjust;
}


inline gmObject * gmMachine::GetObject(gmptr a_ref)
{
#if GMMACHINE_SUPERPARANOIDGC
  return CheckReference(a_ref);
#else // GMMACHINE_SUPERPARANOIDGC
  return (gmObject *) a_ref;
#endif // GMMACHINE_SUPERPARANOIDGC
}



inline gmVariable gmMachine::GetTypeVariable(gmType a_type, const gmVariable &a_key) const
{
  return m_types[a_type].m_variables->Get(a_key);
}



inline gmOperatorFunction gmMachine::GetTypeNativeOperator(gmType a_type, gmOperator a_operator)
{
  return m_types[a_type].m_nativeOperators[a_operator];
}



inline gmFunctionObject * gmMachine::GetTypeOperator(gmType a_type, gmOperator a_operator)
{
  return (gmFunctionObject *) GetObject(m_types[a_type].m_operators[a_operator]);
}



inline void * gmMachine::Sys_Alloc(int a_size)
{
  return m_fixedSet.Alloc(a_size); 
}

#endif // _GMMACHINE_H_
