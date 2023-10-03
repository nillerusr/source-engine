/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMOPERATORS_H_
#define _GMOPERATORS_H_

#include "gmConfig.h"
#include "gmVariable.h"
#include "gmByteCode.h"

struct gmVariable;
class gmThread;


/// \enum gmOperator
enum gmOperator
{
  // O_GETDOT to O_NOT MUST MATCH gmByteCode enum

  O_GETDOT = 0,       // object, "member"          (tos is a_operands + 2)
  O_SETDOT,           // object, value, "member"   (tos is a_operands + 3)
  O_GETIND,           // object, index 
  O_SETIND,           // object, index, value

  O_ADD,              // op1, op2                  (tos is a_operands + 2)
  O_SUB,              // op1, op2
  O_MUL,              // op1, op2
  O_DIV,              // op1, op2
  O_REM,              // op1, op2

  O_BIT_OR,           // op1, op2
  O_BIT_XOR,          // op1, op2
  O_BIT_AND,          // op1, op2
  O_BIT_SHIFTLEFT,    // op1, op2 (shift)
  O_BIT_SHIFTRIGHT,   // op1, op2 (shift)
  O_BIT_INV,          // op1

  O_LT,               // op1, op2
  O_GT,               // op1, op2
  O_LTE,              // op1, op2
  O_GTE,              // op1, op2
  O_EQ,               // op1, op2
  O_NEQ,              // op1, op2

  O_NEG,              // op1
  O_POS,              // op1
  O_NOT,              // op1

#if GM_BOOL_OP
  O_BOOL,             // Special case for use in branch tests. Unary
#endif // GM_BOOL_OP

  O_MAXOPERATORS,
};

const char * gmGetOperatorName(gmOperator a_operator);
gmOperator gmGetOperator(const char * a_operatorName); // return O_MAXOPERATORS on error

//
// gmOperatorFunction is an operator function that may be used instead of binding a c funciton.
// This style of operator is much quicker, as it does not require a full stack frame push and pop.
// Note though, that if you are creating gmObjects from within one of these operators, you must call
// a_thread->SetTop() with the known top of stack before you allocate your gmObject such that the garbage
// collector can mark all objects properly.
// Be careful using this style of operator and GC.
//
typedef void (GM_CDECL *gmOperatorFunction)(gmThread * a_thread, gmVariable * a_operands);

void gmInitBasicType(gmType a_type, gmOperatorFunction * a_operators);

#endif // _GMOPERATORS_H_
