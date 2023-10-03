/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMCODEGENHOOKS_H_
#define _GMCODEGENHOOKS_H_

#include "gmConfig.h"

/// \struct gmLineInfo
/// \brief gmLineInfo describes the debug info required for line number debugging
struct gmLineInfo
{
  int m_address; //!< byte code address
  int m_lineNumber; //!< code line number
};

/// \struct gmFunctionInfo
/// \brief gmFunctionInfo
struct gmFunctionInfo
{
  gmptr m_id;                     //!< unique id of the function (as used in BC_PUSHFN)
  bool m_root;                    //!< is this function the root function '__main'

  const void * m_byteCode;        //!< byte code
  int m_byteCodeLength;           //!< byte code length in bytes
  int m_numParams;                //!< parameter count
  int m_numLocals;                //!< local variable count (includes registers)
  int m_maxStackSize;             //!< required temporary storage

  const char * m_debugName;       //!< name of variable function was assigned to... may be NULL
  const char ** m_symbols;        //!< param and local variable names, sizeof m_numParams + m_numLocals; (indexed by stack offset)
  int m_lineInfoCount;            //!< number of entries in the line info array
  const gmLineInfo * m_lineInfo;  //!< line - instruction address mapping for debugging purposes.
};

/// \class gmCodeGenHooks
/// \brief gmCodeGenHooks is an interface that is fed to the compiler.  basically the code gen hooks class allows you
///        to compile script directly into the runtime vm, or into a libary.
class gmCodeGenHooks
{
public:
  gmCodeGenHooks() {}
  virtual ~gmCodeGenHooks() {}

  /// \brief Begin() will be called by gmCodeGen at the start of compilation.
  /// \param a_debug is true if this is a debug build.
  virtual bool Begin(bool a_debug) = 0;

  /// \brief AddFunction() is called each time the byte code for a function has been created.  The memory passed in
  ///        the info structure is not valid after AddFunction returns.
  /// \return true on success
  virtual bool AddFunction(gmFunctionInfo &a_functionInfo) = 0;

  /// \brief End() is called by gmCodeGen at the end of a compilation.
  /// \param a_errors is the number of compilation errors.
  virtual bool End(int a_errors) = 0;

  /// \brief GetFunctionId() is called for the creation of unique function ids.
  /// \return a unique id.
  virtual gmptr GetFunctionId() = 0;

  /// \brief GetSymbolId() is called by the compiler to get a unique symbol id.  this sybol id is a machine size int
  ///        id written into the byte code.
  /// \return a unique id for each unique a_symbol.
  virtual gmptr GetSymbolId(const char * a_symbol) = 0;

  /// \brief GetStringId() is called by the compiler to get a constant string id.  the returned value is written into 
  ///        the byte code for string lookups.
  /// \return a unique id for each unique string.
  virtual gmptr GetStringId(const char * a_string) = 0;

  /// \brief SwapEndian() returns true if the byte code is being compiled for a machine of differing endian
  virtual bool SwapEndian() const { return false; }
};


/// \class gmCodeGenHooksNull 
/// \brief used for syntax checking etc.
class gmCodeGenHooksNull : public gmCodeGenHooks
{
public:
  gmCodeGenHooksNull() {}
  virtual ~gmCodeGenHooksNull() {}

  virtual bool Begin(bool a_debug) { return true; }
  virtual bool AddFunction(gmFunctionInfo &a_functionInfo) { return true; }
  virtual bool End(int a_errors) { return true; }
  virtual gmptr GetFunctionId() { return 0; }
  virtual gmptr GetSymbolId(const char * a_symbol) { return 0; }
  virtual gmptr GetStringId(const char * a_string) { return 0; }
};

#endif // _GMCODEGENHOOKS_H_
