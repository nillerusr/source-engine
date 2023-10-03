/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMDEBUG_H_
#define _GMDEBUG_H_

#include "gmConfig.h"
#include "gmStreamBuffer.h"
#include "gmHash.h"

class gmMachine;
class gmDebugSession;

// bind debug lib
void gmBindDebugLib(gmMachine * a_machine);

// callbacks used to hook up comms
typedef void (GM_CDECL *gmSendDebuggerMessage)(gmDebugSession * a_session, const void * a_command, int a_len);
typedef const void * (GM_CDECL *gmPumpDebuggerMessage)(gmDebugSession * a_session, int &a_len);

#if GMDEBUG_SUPPORT

/// \class gmDebugSession
class gmDebugSession
{
public:

  gmDebugSession();
  ~gmDebugSession();

  /// \brief Update() must be called to pump messages
  void Update();

  /// \brief Open() will start debugging on a_machine
  bool Open(gmMachine * a_machine);
  
  /// \brief Close() will stop debugging
  bool Close();

  /// \brief GetMachine()
  inline gmMachine * GetMachine() const { return m_machine; }

  gmSendDebuggerMessage m_sendMessage;
  gmPumpDebuggerMessage m_pumpMessage;
  void * m_user;

  // send message helpers
  gmDebugSession &Pack(int a_val);
  gmDebugSession &Pack(const char * a_val);
  void Send();

  // rcv message helpers
  gmDebugSession &Unpack(int &a_val);
  gmDebugSession &Unpack(const char * &a_val);

  // helpers
  bool AddBreakPoint(const void * a_bp, int a_threadId);
  int * FindBreakPoint(const void * a_bp); // return thread id
  bool RemoveBreakPoint(const void * a_bp);

private:

  class BreakPoint : public gmHashNode<void *, BreakPoint>
  {
  public:
    inline const void * GetKey() const { return m_bp; }
    const void * m_bp;
    int m_threadId;
  };

  gmMachine * m_machine;
  
  gmHash<void *, BreakPoint> m_breaks;
  gmStreamBufferDynamic m_out;
  gmStreamBufferStatic m_in;
};

#endif

#endif // _GMDEBUG_H_
