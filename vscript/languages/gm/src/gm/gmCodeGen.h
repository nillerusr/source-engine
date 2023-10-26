/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMCODEGEN_H_
#define _GMCODEGEN_H_

#include "gmConfig.h"
#include "gmLog.h"
#include "gmCodeGenHooks.h"

// fwd decl
struct gmCodeTreeNode;

/// \class gmCodeGen
/// \brief gmCodeGen will create byte code for a given code tree.  after parsing script into a code tree using gmCodeTree,
///        turn it into byte code using this class.  After the code gen has been run, the gmCodeTree may be unlocked.
///        Note that the code tree is parsed into a set of functions authored using a gmCodeGenHooks implementation.
class gmCodeGen
{
public:

  /// \brief Get() will return the singleton code generator.
  static gmCodeGen& Get();

  /// \brief FreeMemory() will free all memory allocated by the code tree.  must be unlocked
  virtual void FreeMemory() = 0;

  /// \brief Lock() will create the byte code for the given gode tree.
  /// \param a_codeTree is the code tree.
  /// \param a_hooks is the byte code authoring object.
  /// \param a_debug is true if debug info is required.
  /// \param a_log is the compile log.
  /// \return the number of errors encounted
  virtual int Lock(const gmCodeTreeNode * a_codeTree, gmCodeGenHooks * a_hooks, bool a_debug, gmLog * a_log) = 0;
 
  /// \brief Unlock() will reset the code generator.
  virtual int Unlock() = 0;
};


#endif // _GMCODEGEN_H_
