/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMLOG_H_
#define _GMLOG_H_

#include "gmConfig.h"
#include "gmMemChain.h"

/// \class gmLog is a log class for the compiler.  The log is passed between the parser, analyser, and code generator.
///        the log class will batch the errors into a list of error strings.
class gmLog
{
public:
  gmLog();
  ~gmLog();

  /// \brief Reset() will remove log entries, but not free the chain mem.
  void Reset();

  /// \brief ResetAndFreeMemory()
  void ResetAndFreeMemory();

  /// \brief LogEntry() will log an entry to the log.  the length of log must not exceed GMLOG_CHAINSIZE
  /// \param a_format
  void GM_CDECL LogEntry(const char * a_format, ...);

  /// \brief GetEntry() will return a log entry, or NULL if there are no more entries.
  /// \param a_first is true for the first call to get entry, this will be set to false for successive calls.
  /// \return a log entry, or NULL if no more entries exist
  const char * GetEntry(bool &a_first);

  /// \brief Set approximate memory limit for error logs.
  /// \param a_limit The approximate memory limit in bytes or <= 0 for no limit.
  void SetMemLimit(int a_limit);

private:

  struct Entry
  {
    const char * m_text;
    Entry * m_next;
  };

  Entry * m_first;                                ///< Message list first ptr
  Entry * m_last;                                 ///< Message list last ptr
  Entry * m_curr;                                 ///< Current message 
  int m_memApproxLimit;                           ///< Approximate memory limit

  gmMemChain m_mem;
};

#endif // _GMLOG_H_
