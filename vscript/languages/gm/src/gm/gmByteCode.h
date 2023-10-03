/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMBYTECODE_H_
#define _GMBYTECODE_H_

#include "gmConfig.h"

/// \enum gmByteCode
/// \brief gmByteCode are the op codes for the game monkey scripting.  The first byte codes MUST match the gmOperator
///        enum.
enum gmByteCode
{
  // BC_GETDOT to BC_NOP MUST MATCH ENUM GMOPERATOR
  
  BC_GETDOT = 0,      // tos '.' opptr, push result
  BC_SETDOT,          // tos-1 '.' opptr = tos, tos -= 2
  BC_GETIND,          // tos-1 = tos-1 [tos], --tos
  BC_SETIND,          // tos-2 [tos-1] = tos, tos -= 3

  // math     
  BC_OP_ADD,
  BC_OP_SUB,
  BC_OP_MUL,
  BC_OP_DIV,
  BC_OP_REM,

  // bit
  BC_BIT_OR,
  BC_BIT_XOR,
  BC_BIT_AND,
  BC_BIT_SHL,
  BC_BIT_SHR,
  BC_BIT_INV,
              
  // compare   
  BC_OP_LT,
  BC_OP_GT,
  BC_OP_LTE,
  BC_OP_GTE,
  BC_OP_EQ,
  BC_OP_NEQ,

  // unary    
  BC_OP_NEG,
  BC_OP_POS,
  BC_OP_NOT,

  BC_NOP,
  BC_LINE,            // indicates instruction is on a new code line to the last executed instruction. used in debug mode

  // branch
  BC_BRA,             // branch always
  BC_BRZ,             // branch tos equal to zero, --tos
  BC_BRNZ,            // branch tos not equal to zero, --tos
  BC_BRZK,            // branch tos equal to zero keep value on stack
  BC_BRNZK,           // branch tos not equal to zero keep value on stack
  BC_CALL,            // call op16 num parameters
  BC_RET,             // return null, ++tos
  BC_RETV,            // return tos
  BC_FOREACH,         // op16 op16, table, iterator, leave loop complete bool on stack.
              
  // stack    
  BC_POP,             // --tos
  BC_POP2,            // tos -=2
  BC_DUP,             // tos + 1 = tos, ++tos
  BC_DUP2,            // tos + 1 = tos -1, tos + 2 = tos, tos += 2
  BC_SWAP,            // 
  BC_PUSHNULL,        // push null,
  BC_PUSHINT,         // push int opptr
  BC_PUSHINT0,        // push 0
  BC_PUSHINT1,        // push 1
  BC_PUSHFP,          // push floating point op32
  BC_PUSHSTR,         // push string opptr
  BC_PUSHTBL,         // push table
  BC_PUSHFN,          // push function opptr
  BC_PUSHTHIS,        // push this

  // get set
  BC_GETLOCAL,        // get local op16 (stack offset) ++tos
  BC_SETLOCAL,        // set local op16 (stack offset) --tos
  BC_GETGLOBAL,       // get global opptr (symbol id) ++tos
  BC_SETGLOBAL,       // set global opptr (symbol id) --tos
  BC_GETTHIS,         // get this opptr (symbol id) ++tos
  BC_SETTHIS,         // set this opptr (symbol id) --tos
};

#if GM_COMPILE_DEBUG

void gmByteCodePrint(FILE * a_fp, const void * a_byteCode, int a_byteCodeLength);

#endif // GM_COMPILE_DEBUG

#endif
