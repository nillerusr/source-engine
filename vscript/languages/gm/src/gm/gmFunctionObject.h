/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMFUNCTIONOBJECT_H_
#define _GMFUNCTIONOBJECT_H_

#include "gmConfig.h"
#include "gmVariable.h"
#include "gmCodeGenHooks.h"
#include "gmMem.h"

// fwd decls
class gmThread;

enum gmCFunctionReturn
{
  GM_OK = 0,
  GM_EXCEPTION = -1,
  GM_SYS_YIELD = -2, // system only
  GM_SYS_BLOCK = -3, // system only
  GM_SYS_SLEEP = -4, // system only
  GM_SYS_KILL =  -5, // system only
  GM_SYS_STATE = -6, // system only
};

/*!
  \brief gmCFunction is the function type for binding c functions to gm.
  \return gmCFunctionReturn
*/
typedef int (GM_CDECL *gmCFunction)(gmThread *);

/*!
  \class gmFunctionObject
  \brief 
*/
class gmFunctionObject : public gmObject
{
public:

  virtual int GetType() const { return GM_FUNCTION; }

  virtual void Destruct(gmMachine * a_machine);
#if GM_USE_INCGC
  virtual bool Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone);
#else //GM_USE_INCGC
  virtual void Mark(gmMachine * a_machine, gmuint32 a_mark);
#endif //GM_USE_INCGC


  /*!
    \brief Init() will initialise a function object.
    \param a_debug is true if this is a debug build
    \param a_info is a function info struct as built by gmCodeGenHooks.
    \param a_sourceId is an unique id specifiying the source code of the function, is used for debugging.
    \return true on success.
  */
  bool Init(gmMachine * a_machine, bool a_debug, gmFunctionInfo &a_info, gmuint32 a_sourceId = 0);

  /*!
    \brief GetMaxStackSize
    \return the maximum stack growth not including parameters or locals
  */
  inline int GetMaxStackSize() const { return m_maxStackSize; }

   /// \brief GetNumLocals
  inline int GetNumLocals() const { return m_numLocals; }

  /// \brief GetNumParams
  inline int GetNumParams() const { return m_numParams; }

  /// \brief GetNumParamsLocals()
  inline int GetNumParamsLocals() const { return m_numParamsLocals; }

  /// \brief GetByteCode()
  inline const void * GetByteCode() const { return m_byteCode; }

  /// \brief GetDebugName()
  inline const char * GetDebugName() const;

  /// \brief GetLine() will return the source line for the given address
  int GetLine(int a_address) const;
  int GetLine(const void * a_instruction) const { return GetLine((const char * ) a_instruction - (char *) m_byteCode); }

  /// \brief GetInstructionAtLine() will return the instruction at the given line, or NULL of line was not within this function
  const void * GetInstructionAtLine(int a_line) const;

  /// \brief GetSourceId() will get the source code id when in debug mode, else 0
  gmuint32 GetSourceId() const;

  /// \brief GetSymbol() will return the symbol name at the given offset.
  inline const char * GetSymbol(int a_offset) const;

  // public data
  gmCFunction m_cFunction;
  const void* m_cUserData;

protected:

  /// \brief Non-public constructor.  Create via gmMachine.
  gmFunctionObject();
  friend class gmMachine;

private:

  /*!
    \brief gmFunctionObjectDebugInfo stores debugging info for a debug build
  */
  struct gmFunctionObjectDebugInfo
  {
    char * m_debugName;
    char ** m_symbols;
    int m_lineInfoCount;
    gmuint32 m_sourceId; // source code id.
    gmLineInfo * m_lineInfo;
  };

  gmFunctionObjectDebugInfo * m_debugInfo;
  void * m_byteCode;
  int m_byteCodeLength;
  int m_maxStackSize;
  int m_numLocals;
  int m_numParams;
  int m_numParamsLocals; //!< m_numLocals + m_numParams
  int m_numReferences; //!< number of references within the byte code.
  gmptr * m_references; //!< references from the byte code
};

//
//
// INLINE IMPLEMENTATION
//
//

inline const char * gmFunctionObject::GetDebugName() const
{
  if(m_debugInfo && m_debugInfo->m_debugName)
  {
    return m_debugInfo->m_debugName;
  }
  return "__unknown";
}



inline const char * gmFunctionObject::GetSymbol(int a_offset) const
{
  if(m_debugInfo && m_debugInfo->m_symbols && (a_offset >= 0) && (a_offset < m_numParamsLocals))
  {
    return m_debugInfo->m_symbols[a_offset];
  }
  return "__unknown";
}


#endif // _GMFUNCTIONOBJECT_H_
