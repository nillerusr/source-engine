#ifndef SCRIPTSYS_H
#define SCRIPTSYS_H

//
// ScriptSys.h
//
// Example script system to support scriptable game objects
//

#include "gmThread.h"
#include "gmDebug.h"
#include "gmArraySimple.h"
#include "StdStuff.h"
#include "NetClient.h"

// Fwd decls
class GameObj;
class ScriptObj;

// Script system to support and control the virtual machine
class ScriptSys
{
public:

  /// Access this system from anywhere once it has been initialized for convenience
  static ScriptSys* Get()                         { return s_instance; }

  ScriptSys();
  virtual ~ScriptSys();

  /// Get the GM machine
  gmMachine* GetMachine()                         { return m_machine; }

  /// Set bindings and Init constant strings
  static void Init();
  /// Clean out this structure
  static void Destroy();


  /// Log an error message
  void __cdecl LogError(const char *a_str, ...);

  /// Log any machine error messages that may be waiting
  void LogAnyMachineErrorMessages();

  /// Get GameObj that was associated with a thread Id.
  GameObj* GetGameObjFromThreadId(int a_threadId);
  
  /// Associate a threadId with a GameObj, logically as a primary thread.
  void AssociateThreadIdWithGameObj(int a_threadId, GameObj& a_gameObj);
  /// Disassociate a threadId with a GameObj.
  void DisassociateThreadIdWithGameObj(int a_threadId);
  /// Remove the thread Id association, but don't modify the GameObj.
  /// This can be used internally by GameObj to perform iteration and removal.
  void RemoveThreadIdButDontTouchGameObj(int a_threadId);

  /// Run a script file
  bool ExecuteFile(const char* a_fileName);
  /// Executes a string.
  bool ExecuteString(const char* a_str);

  /// Update the virtual machine.
  int Execute(unsigned int a_deltaTimeMS);

  void SetTableNull(const char* a_memberName, gmTableObject* a_table);
  void SetTableInt(const char* a_memberName, int a_int, gmTableObject* a_table);
  void SetTableFloat(const char* a_memberName, float a_float, gmTableObject* a_table);
  void SetTableString(const char* a_memberName, const char* a_string, int a_strLength, gmTableObject* a_table);
  void SetTableGameObj(const char* a_memberName, GameObj* a_gameObj, gmTableObject* a_table);
  gmTableObject* SetTableTable(const char* a_memberName, gmTableObject* a_table);

  bool GetTableInt(const char* a_memberName, int& a_int, gmTableObject* a_table);
  bool GetTableFloat(const char* a_memberName, float& a_float, gmTableObject* a_table);
  bool GetTableString(const char* a_memberName, String& a_string, gmTableObject* a_table);
  bool GetTableGameObj(const char* a_memberName, GameObj*& a_gameObj, gmTableObject* a_table);
  bool GetTableTable(const char* a_memberName, gmTableObject*& a_retTable, gmTableObject* a_table);

protected:

  /// Machine 'print' binding callback
  static void GM_CDECL ScriptSysCallback_Print(gmMachine* a_machine, const char* a_string);
  /// Machine general and exception callback
  static bool GM_CDECL ScriptSysCallback_Machine(gmMachine* a_machine, gmMachineCommand a_command, const void* a_context);

  /// Debugging support Send a message
  static void DebuggerSendMessage(gmDebugSession * a_session, const void * a_command, int a_len);
  /// Debugging support Pump a message
  static const void* DebuggerPumpMessage(gmDebugSession * a_session, int &a_len);

  gmMachine* m_machine;                           ///< GM machine instance
  Map<int, ScriptObj*> m_mapThreadGameObjs;       ///< Map script threadId to game object
/* // Old code
  gmArraySimple<ScriptObj*> m_allScriptObjs;      ///< All the script objects, for garbage collection handling
*/

  nClient m_debugClient;                          ///< Debugger network client
  gmDebugSession m_debugSession;                  ///< Debugger session
  const char* m_debuggerIP;                       ///< Debugger IP
  int m_debuggerPort;                             ///< Debugger port

  static const int DEBUGGER_DEFAULT_PORT;         ///< Debugger port number
  static const char* DEBUGGER_DEFAULT_IP;         ///< Debugger port number
  static ScriptSys* s_instance;                   ///< Static instance for convenience
};


#endif //SCRIPTSYS_H