/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMDEBUGGER_H_
#define _GMDEBUGGER_H_

//
// Please note that gmDebugger.c/.h are for implementing
// a debugger application and should not be included
// in an normal GM application build.
//


class gmDebuggerSession;

// callbacks used to hook up comms
typedef void (*gmSendMachineMessage)(gmDebuggerSession * a_session, const void * a_command, int a_len);
typedef const void * (*gmPumpMachineMessage)(gmDebuggerSession * a_session, int &a_len);

/// \class gmDebuggerSession
class gmDebuggerSession
{
public:

  gmDebuggerSession();
  ~gmDebuggerSession();

  /// \brief Update() must be called to pump messages
  void Update();

  /// \brief Open() will start debugging
  bool Open();
  
  /// \brief Close() will stop debugging
  bool Close();

  gmSendMachineMessage m_sendMessage;
  gmPumpMachineMessage m_pumpMessage;
  void * m_user; // hook to your debugger

  // send message helpers
  gmDebuggerSession &Pack(int a_val);
  gmDebuggerSession &Pack(const char * a_val);
  void Send();

  // rcv message helpers
  gmDebuggerSession &Unpack(int &a_val);
  gmDebuggerSession &Unpack(const char * &a_val);

private:

  void * m_out;
  int m_outCursor, m_outSize;
  void Need(int a_bytes);

  const void * m_in;
  int m_inCursor, m_inSize;
};

//
// the debugger must implement the following functions
//

extern void gmDebuggerBreak(gmDebuggerSession * a_session, int a_threadId, int a_sourceId, int a_lineNumber);
extern void gmDebuggerRun(gmDebuggerSession * a_session, int a_threadId);
extern void gmDebuggerStop(gmDebuggerSession * a_session, int a_threadId);
extern void gmDebuggerSource(gmDebuggerSession * a_session, int a_sourceId, const char * a_sourceName, const char * a_source);
extern void gmDebuggerException(gmDebuggerSession * a_session, int a_threadId);

extern void gmDebuggerBeginContext(gmDebuggerSession * a_session, int a_threadId, int a_callFrame);
extern void gmDebuggerContextCallFrame(gmDebuggerSession * a_session, int a_callFrame, const char * a_functionName, int a_sourceId, int a_lineNumber, const char * a_thisSymbol, const char * a_thisValue, int a_thisId);
extern void gmDebuggerContextVariable(gmDebuggerSession * a_session, const char * a_varSymbol, const char * a_varValue, int a_varId);
extern void gmDebuggerEndContext(gmDebuggerSession * a_session);

extern void gmDebuggerBeginSourceInfo(gmDebuggerSession * a_session);
extern void gmDebuggerSourceInfo(gmDebuggerSession * a_session, int a_sourceId, const char * a_sourceName);
extern void gmDebuggerEndSourceInfo(gmDebuggerSession * a_session);

extern void gmDebuggerBeginThreadInfo(gmDebuggerSession * a_session);
extern void gmDebuggerThreadInfo(gmDebuggerSession * a_session, int a_threadId, int a_threadState);
extern void gmDebuggerEndThreadInfo(gmDebuggerSession * a_session);

extern void gmDebuggerError(gmDebuggerSession * a_session, const char * a_error);
extern void gmDebuggerMessage(gmDebuggerSession * a_session, const char * a_message);
extern void gmDebuggerAck(gmDebuggerSession * a_session, int a_response, int a_posNeg);
extern void gmDebuggerQuit(gmDebuggerSession * a_session);

//
// the debugger can use the following functions to send messages to the machine
//

void gmMachineRun(gmDebuggerSession * a_session, int a_threadId);
void gmMachineStepInto(gmDebuggerSession * a_session, int a_threadId);
void gmMachineStepOver(gmDebuggerSession * a_session, int a_threadId);
void gmMachineStepOut(gmDebuggerSession * a_session, int a_threadId);
void gmMachineGetContext(gmDebuggerSession * a_session, int a_threadId, int a_callframe);
void gmMachineGetSource(gmDebuggerSession * a_session, int a_sourceId);
void gmMachineGetSourceInfo(gmDebuggerSession * a_session);
void gmMachineGetThreadInfo(gmDebuggerSession * a_session);
void gmMachineGetVariableInfo(gmDebuggerSession * a_session, int a_variableId);
void gmMachineSetBreakPoint(gmDebuggerSession * a_session, int a_responseId, int a_sourceId, int a_lineNumber, int a_threadId, int a_enabled);
void gmMachineBreak(gmDebuggerSession * a_session, int a_threadId);
void gmMachineQuit(gmDebuggerSession * a_session);

#endif

