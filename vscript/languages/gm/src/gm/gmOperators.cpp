/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmOperators.h"
#include "gmThread.h"
#include "gmStringObject.h"
//#include <math.h>

// Must be last header
#include "memdbgon.h"


const char * gmGetOperatorName(gmOperator a_operator)
{
  switch(a_operator)
  {
    case O_GETDOT : return "getdot";
    case O_SETDOT : return "setdot";
    case O_GETIND : return "getind";
    case O_SETIND : return "setind";
    case O_ADD : return "add";
    case O_SUB : return "sub";
    case O_MUL : return "mul";
    case O_DIV : return "div";
    case O_REM : return "mod";
    case O_BIT_OR : return "bitor";
    case O_BIT_XOR : return "bitxor";
    case O_BIT_AND : return "bitand";
    case O_BIT_SHIFTLEFT : return "shiftleft";
    case O_BIT_SHIFTRIGHT : return "shiftright";
    case O_BIT_INV : return "bitinv";
    case O_LT : return "lt";
    case O_GT : return "gt";
    case O_LTE : return "lte";
    case O_GTE : return "gte";
    case O_EQ : return "eq";
    case O_NEQ : return "neq";
    case O_NEG : return "neg";
    case O_POS : return "pos";
    case O_NOT : return "not";
#if GM_BOOL_OP
    case O_BOOL : return "bool";
#endif // GM_BOOL_OP
    default :;
  }
  return "undefined";
}

gmOperator gmGetOperator(const char * a_operatorName)
{
  if(_gmstricmp(a_operatorName, "getdot") == 0) return O_GETDOT;
  if(_gmstricmp(a_operatorName, "setdot") == 0) return O_SETDOT;
  if(_gmstricmp(a_operatorName, "getind") == 0) return O_GETIND;
  if(_gmstricmp(a_operatorName, "setind") == 0) return O_SETIND;
  if(_gmstricmp(a_operatorName, "add") == 0) return O_ADD;
  if(_gmstricmp(a_operatorName, "sub") == 0) return O_SUB;
  if(_gmstricmp(a_operatorName, "mul") == 0) return O_MUL;
  if(_gmstricmp(a_operatorName, "div") == 0) return O_DIV;
  if(_gmstricmp(a_operatorName, "mod") == 0) return O_REM;
  if(_gmstricmp(a_operatorName, "bitor") == 0) return O_BIT_OR;
  if(_gmstricmp(a_operatorName, "bitxor") == 0) return O_BIT_XOR;
  if(_gmstricmp(a_operatorName, "bitand") == 0) return O_BIT_AND;
  if(_gmstricmp(a_operatorName, "shiftleft") == 0) return O_BIT_SHIFTLEFT;
  if(_gmstricmp(a_operatorName, "shiftright") == 0) return O_BIT_SHIFTRIGHT;
  if(_gmstricmp(a_operatorName, "bitinv") == 0) return O_BIT_INV;
  if(_gmstricmp(a_operatorName, "lt") == 0) return O_LT;
  if(_gmstricmp(a_operatorName, "gt") == 0) return O_GT;
  if(_gmstricmp(a_operatorName, "lte") == 0) return O_LTE;
  if(_gmstricmp(a_operatorName, "gte") == 0) return O_GTE;
  if(_gmstricmp(a_operatorName, "eq") == 0) return O_EQ;
  if(_gmstricmp(a_operatorName, "neq") == 0) return O_NEQ;
  if(_gmstricmp(a_operatorName, "neg") == 0) return O_NEG;
  if(_gmstricmp(a_operatorName, "pos") == 0) return O_POS;
  if(_gmstricmp(a_operatorName, "not") == 0) return O_NOT;
#if GM_BOOL_OP
  if(_gmstricmp(a_operatorName, "bool") == 0) return O_BOOL;
#endif // GM_BOOL_OP
  return O_MAXOPERATORS;
}


//
// GM_NULL
//

//
// GM_INT
//

void GM_CDECL gmIntOpAdd(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int += a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpSub(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int -= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpMul(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int *= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpDiv(gmThread * a_thread, gmVariable * a_operands)
{
#if GMMACHINE_GMCHECKDIVBYZERO
  if(a_operands[1].m_value.m_int != 0)
  {
    a_operands[0].m_value.m_int /= a_operands[1].m_value.m_int;
  }
  else
  {
    a_thread->GetMachine()->GetLog().LogEntry("Divide by zero.");
    a_operands[0].Nullify();
    // NOTE: No proper way to signal exception from here at present
  }
#else // GMMACHINE_GMCHECKDIVBYZERO
  a_operands[0].m_value.m_int /= a_operands[1].m_value.m_int;
#endif // GMMACHINE_GMCHECKDIVBYZERO
}
void GM_CDECL gmIntOpRem(gmThread * a_thread, gmVariable * a_operands)
{
#if GMMACHINE_GMCHECKDIVBYZERO
  if(a_operands[1].m_value.m_int != 0)
  {
    a_operands[0].m_value.m_int %= a_operands[1].m_value.m_int;
  }
  else
  {
    a_thread->GetMachine()->GetLog().LogEntry("Divide by zero.");
    a_operands[0].Nullify();
    // NOTE: No proper way to signal exception from here at present
  }
#else // GMMACHINE_GMCHECKDIVBYZERO
  a_operands[0].m_value.m_int %= a_operands[1].m_value.m_int;
#endif // GMMACHINE_GMCHECKDIVBYZERO
}
void GM_CDECL gmIntOpBitOr(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int |= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpBitXor(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int ^= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpBitAnd(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int &= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpBitShiftLeft(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int <<= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpBitShiftRight(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int >>= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpInv(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = ~a_operands[0].m_value.m_int;
}
void GM_CDECL gmIntOpLT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = a_operands[0].m_value.m_int < a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpGT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = a_operands[0].m_value.m_int > a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpLTE(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = a_operands[0].m_value.m_int <= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpGTE(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = a_operands[0].m_value.m_int >= a_operands[1].m_value.m_int;
}
void GM_CDECL gmIntOpEQ(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = (a_operands[0].m_value.m_int == a_operands[1].m_value.m_int);
}
void GM_CDECL gmIntOpNEQ(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = (a_operands[0].m_value.m_int != a_operands[1].m_value.m_int);
}
void GM_CDECL gmIntOpNEG(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = -a_operands[0].m_value.m_int;
}
void GM_CDECL gmIntOpPOS(gmThread * a_thread, gmVariable * a_operands)
{
}
void GM_CDECL gmIntOpNOT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands[0].m_value.m_int = !a_operands[0].m_value.m_int;
}

//
// GM_FLOAT
//

#define INTTOFLOAT(A) (((A)->m_type == GM_FLOAT) ? (A)->m_value.m_float : (float) (A)->m_value.m_int)

void GM_CDECL gmFloatOpAdd(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) + INTTOFLOAT(a_operands + 1);
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpSub(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) - INTTOFLOAT(a_operands + 1);
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpMul(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) * INTTOFLOAT(a_operands + 1);
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpDiv(gmThread * a_thread, gmVariable * a_operands)
{
#if GMMACHINE_GMCHECKDIVBYZERO
  if(INTTOFLOAT(a_operands + 1) != 0)
  {
    a_operands->m_value.m_float = INTTOFLOAT(a_operands) / INTTOFLOAT(a_operands + 1);
    a_operands->m_type = GM_FLOAT;
  }
  else
  {
    a_thread->GetMachine()->GetLog().LogEntry("Divide by zero.");
    a_operands->Nullify(); // NOTE: Should probably return +/- INF, not null
    // NOTE: No proper way to signal exception from here at present
  }
#else // GMMACHINE_GMCHECKDIVBYZERO
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) / INTTOFLOAT(a_operands + 1);
  a_operands->m_type = GM_FLOAT;
#endif // GMMACHINE_GMCHECKDIVBYZERO
}
void GM_CDECL gmFloatOpRem(gmThread * a_thread, gmVariable * a_operands)
{
#if GMMACHINE_GMCHECKDIVBYZERO
  if(INTTOFLOAT(a_operands + 1) != 0)
  {
    a_operands->m_value.m_float = fmodf(INTTOFLOAT(a_operands), INTTOFLOAT(a_operands + 1));
    a_operands->m_type = GM_FLOAT;
  }
  else
  {
    a_thread->GetMachine()->GetLog().LogEntry("Divide by zero.");
    a_operands->Nullify();
    // NOTE: No proper way to signal exception from here at present
  }
#else // GMMACHINE_GMCHECKDIVBYZERO
  a_operands->m_value.m_float = fmodf(INTTOFLOAT(a_operands), INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_FLOAT;
#endif // GMMACHINE_GMCHECKDIVBYZERO
}
void GM_CDECL gmFloatOpInc(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) + 1.0f;
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpDec(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = INTTOFLOAT(a_operands) - 1.0f;
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpLT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) < INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpGT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) > INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpLTE(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) <= INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpGTE(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) >= INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpEQ(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) == INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpNEQ(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = (INTTOFLOAT(a_operands) != INTTOFLOAT(a_operands + 1));
  a_operands->m_type = GM_INT; 
}
void GM_CDECL gmFloatOpNEG(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_float = -INTTOFLOAT(a_operands);
  a_operands->m_type = GM_FLOAT; 
}
void GM_CDECL gmFloatOpPOS(gmThread * a_thread, gmVariable * a_operands)
{
}
void GM_CDECL gmFloatOpNOT(gmThread * a_thread, gmVariable * a_operands)
{
  if(a_operands->m_value.m_float == 0.0f)
  {
    a_operands->m_value.m_int = 1; a_operands->m_type = GM_INT;
  }
  else
  {
    a_operands->m_value.m_int = 0; a_operands->m_type = GM_INT;
  }
}

//
// GM_STRING
//

#define GMSTRING_BUFFERSIZE 64

// we could use gmVariable::AsString here, but this is for types <= string.... is a little more efficient.
// a_buffer must be >= 64
inline const char * gmUnknownToString(gmMachine * a_machine, gmVariable * a_unknown, char * a_buffer, int * a_len = NULL)
{
  if(a_unknown->m_type == GM_STRING)
  {
    gmStringObject * str = (gmStringObject *) GM_MOBJECT(a_machine, a_unknown->m_value.m_ref);
    if(a_len) { *a_len = str->GetLength(); }
    return (const char *) *str;
  }
  if(a_unknown->m_type == GM_INT)
  {
    V_snprintf(a_buffer, 64, "%d", a_unknown->m_value.m_int); // this won't be > 64 chars
  }
  else if(a_unknown->m_type == GM_FLOAT)
  {
    V_snprintf(a_buffer, 64, "%f", a_unknown->m_value.m_float); // this won't be > 64 chars
  }
  else
  {
    strcpy(a_buffer, "null");
  }
  if(a_len) { *a_len = strlen(a_buffer); }
  return a_buffer;
}
void GM_CDECL gmStringOpAdd(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  int len1 = 0, len2 = 0;
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1, &len1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2, &len2);
  char * buffer = (char *) alloca(len1 + len2 + 1);
  memcpy(buffer, str1, len1);
  memcpy(buffer + len1, str2, len2 + 1);
  a_thread->SetTop(a_operands); // so the garbage collector works
  a_operands->m_type = GM_STRING;
  a_operands->m_value.m_ref = (gmptr) machine->AllocStringObject(buffer, len1 + len2);
}
void GM_CDECL gmStringOpLT(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == -1) ? 1 : 0;
}
void GM_CDECL gmStringOpGT(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == 1) ? 1 : 0;
}
void GM_CDECL gmStringOpLTE(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == 1) ? 0 : 1;
}
void GM_CDECL gmStringOpGTE(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == -1) ? 0 : 1;
}
void GM_CDECL gmStringOpEQ(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == 0) ? 1 : 0;
}
void GM_CDECL gmStringOpNEQ(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  char buffer1[GMSTRING_BUFFERSIZE];
  char buffer2[GMSTRING_BUFFERSIZE];
  const char * str1 = gmUnknownToString(machine, a_operands, buffer1);
  const char * str2 = gmUnknownToString(machine, a_operands + 1, buffer2);
  int res = strcmp(str1, str2);
  a_operands->m_type = GM_INT;
  a_operands->m_value.m_ref = (res == 0) ? 0 : 1;
}

void GM_CDECL gmStringOpNOT(gmThread * a_thread, gmVariable * a_operands)
{
  a_operands->m_value.m_int = 0; a_operands->m_type = GM_INT;
}

//
// GM_TABLE
//

void GM_CDECL gmTableGetDot(gmThread * a_thread, gmVariable * a_operands)
{
  gmTableObject * table = (gmTableObject *) GM_OBJECT(a_operands->m_value.m_ref);
  *a_operands = table->Get(a_operands[1]);
}
void GM_CDECL gmTableSetDot(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  gmTableObject * table = (gmTableObject *) GM_MOBJECT(machine, a_operands->m_value.m_ref);
  table->Set(machine, a_operands[2], a_operands[1]);
}
void GM_CDECL gmTableGetInd(gmThread * a_thread, gmVariable * a_operands)
{
  gmTableObject * table = (gmTableObject *) GM_OBJECT(a_operands->m_value.m_ref);
  *a_operands = table->Get(a_operands[1]);
}
void GM_CDECL gmTableSetInd(gmThread * a_thread, gmVariable * a_operands)
{
  gmMachine * machine = a_thread->GetMachine();
  gmTableObject * table = (gmTableObject *) GM_MOBJECT(machine, a_operands->m_value.m_ref);
  table->Set(machine, a_operands[1], a_operands[2]);
}

//
// GM_USER
//

//
// MISC.
//

void GM_CDECL gmRefOpEQ(gmThread * a_thread, gmVariable * a_operands)
{
  if(a_operands[0].m_type == a_operands[1].m_type && a_operands[0].m_value.m_ref == a_operands[1].m_value.m_ref)
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 1;
  }
  else
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 0;
  }
}
void GM_CDECL gmRefOpNEQ(gmThread * a_thread, gmVariable * a_operands)
{
  if(a_operands[0].m_type == a_operands[1].m_type && a_operands[0].m_value.m_ref == a_operands[1].m_value.m_ref)
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 0;
  }
  else
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 1;
  }
}
void GM_CDECL gmRefOpNOT(gmThread * a_thread, gmVariable * a_operands)
{
  if(a_operands->m_type == GM_NULL)
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 1;
  }
  else
  {
    a_operands->m_type = GM_INT;
    a_operands->m_value.m_int = 0;
  }
}



void gmInitBasicType(gmType a_type, gmOperatorFunction * a_operators)
{
  memset(a_operators, 0, sizeof(gmOperatorFunction) * O_MAXOPERATORS);

  if(a_type == GM_NULL)
  {
    a_operators[O_EQ]     = gmRefOpEQ;
    a_operators[O_NEQ]    = gmRefOpNEQ;
    a_operators[O_NOT]    = gmRefOpNOT;
  }
  else if(a_type == GM_INT)
  {
    a_operators[O_ADD]            = gmIntOpAdd;
    a_operators[O_SUB]            = gmIntOpSub;
    a_operators[O_MUL]            = gmIntOpMul;
    a_operators[O_DIV]            = gmIntOpDiv;
    a_operators[O_REM]            = gmIntOpRem;
    a_operators[O_BIT_OR]         = gmIntOpBitOr;
    a_operators[O_BIT_XOR]        = gmIntOpBitXor;
    a_operators[O_BIT_AND]        = gmIntOpBitAnd;
    a_operators[O_BIT_SHIFTLEFT]  = gmIntOpBitShiftLeft;
    a_operators[O_BIT_SHIFTRIGHT] = gmIntOpBitShiftRight;
    a_operators[O_BIT_INV]        = gmIntOpInv;
    a_operators[O_LT]             = gmIntOpLT;
    a_operators[O_GT]             = gmIntOpGT;
    a_operators[O_LTE]            = gmIntOpLTE;
    a_operators[O_GTE]            = gmIntOpGTE;
    a_operators[O_EQ]             = gmIntOpEQ;
    a_operators[O_NEQ]            = gmIntOpNEQ;
    a_operators[O_NEG]            = gmIntOpNEG;
    a_operators[O_POS]            = gmIntOpPOS;
    a_operators[O_NOT]            = gmIntOpNOT;
  }
  else if(a_type == GM_FLOAT)
  {
    a_operators[O_ADD]    = gmFloatOpAdd;
    a_operators[O_SUB]    = gmFloatOpSub;
    a_operators[O_MUL]    = gmFloatOpMul;
    a_operators[O_DIV]    = gmFloatOpDiv;
    a_operators[O_REM]    = gmFloatOpRem;
    a_operators[O_LT]     = gmFloatOpLT;
    a_operators[O_GT]     = gmFloatOpGT;
    a_operators[O_LTE]    = gmFloatOpLTE;
    a_operators[O_GTE]    = gmFloatOpGTE;
    a_operators[O_EQ]     = gmFloatOpEQ;
    a_operators[O_NEQ]    = gmFloatOpNEQ;
    a_operators[O_NEG]    = gmFloatOpNEG;
    a_operators[O_POS]    = gmFloatOpPOS;
    a_operators[O_NOT]    = gmFloatOpNOT;
  }
  else if(a_type == GM_STRING)
  {
    a_operators[O_ADD]    = gmStringOpAdd;
    a_operators[O_LT]     = gmStringOpLT;
    a_operators[O_GT]     = gmStringOpGT;
    a_operators[O_LTE]    = gmStringOpLTE;
    a_operators[O_GTE]    = gmStringOpGTE;
    a_operators[O_EQ]     = gmStringOpEQ;
    a_operators[O_NEQ]    = gmStringOpNEQ;
    a_operators[O_NOT]    = gmStringOpNOT;    
  }
  else if(a_type == GM_TABLE)
  {
    a_operators[O_GETDOT] = gmTableGetDot;
    a_operators[O_SETDOT] = gmTableSetDot;
    a_operators[O_GETIND] = gmTableGetInd;
    a_operators[O_SETIND] = gmTableSetInd;
    a_operators[O_EQ]     = gmRefOpEQ;
    a_operators[O_NEQ]    = gmRefOpNEQ;
    a_operators[O_NOT]    = gmRefOpNOT;
  }
  else if(a_type == GM_FUNCTION)
  {
    a_operators[O_EQ]     = gmRefOpEQ;
    a_operators[O_NEQ]    = gmRefOpNEQ;
    a_operators[O_NOT]    = gmRefOpNOT;
  }
  else if(a_type >= GM_USER)
  {
    a_operators[O_EQ]     = gmRefOpEQ;
    a_operators[O_NEQ]    = gmRefOpNEQ;
    a_operators[O_NOT]    = gmRefOpNOT;
  }
}

