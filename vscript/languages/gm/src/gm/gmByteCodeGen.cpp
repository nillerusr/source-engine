/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmByteCodeGen.h"

// Must be last header
#include "memdbgon.h"



gmByteCodeGen::gmByteCodeGen(void * a_context)
{
  m_tos = 0;
  m_maxTos = 0;
  m_emitCallback = NULL;
  m_context = a_context;
}



void gmByteCodeGen::Reset(void * a_context)
{
  gmStreamBufferDynamic::Reset();
  m_tos = 0;
  m_maxTos = 0;
  m_emitCallback = NULL;
  m_context = a_context;
}



bool gmByteCodeGen::Emit(gmByteCode a_instruction)
{
  if(m_emitCallback) m_emitCallback(Tell(), m_context);
  AdjustStack(a_instruction);
  *this << (gmuint32) a_instruction;
  return true;
}



bool gmByteCodeGen::Emit(gmByteCode a_instruction, gmuint32 a_operand32)
{
  if(m_emitCallback) m_emitCallback(Tell(), m_context);
  AdjustStack(a_instruction);
  *this << (gmuint32) a_instruction;
  *this << a_operand32;
  return true;
}



bool gmByteCodeGen::EmitPtr(gmByteCode a_instruction, gmptr a_operand)
{
  if(m_emitCallback) m_emitCallback(Tell(), m_context);
  AdjustStack(a_instruction);
  *this << ((gmuint32) a_instruction);
  *this << a_operand;
  return true;
}


unsigned int gmByteCodeGen::Skip(unsigned int p_n, unsigned char p_value)
{
  unsigned int oldPos = Tell();
  if(p_n)
  {
    char * fill = (char *) alloca(p_n);
    memset(fill, p_value, p_n);
    Write(fill, p_n);
  }
  return oldPos;
}



void gmByteCodeGen::AdjustStack(gmByteCode a_instruction)
{
  switch(a_instruction)
  {
    case BC_NOP : break;
    case BC_LINE : break;

    case BC_GETDOT : m_tos += 0; break;
    case BC_SETDOT : m_tos -= 2; break;
    case BC_GETIND : --m_tos; break;
    case BC_SETIND : m_tos -= 3; break;

    case BC_BRA : break;
    case BC_BRZ : --m_tos; break;
    case BC_BRNZ : --m_tos; break;
    case BC_BRZK : break;
    case BC_BRNZK : break;
    case BC_CALL : break;
    case BC_RET : break;
    case BC_RETV : break;
    case BC_FOREACH : ++m_tos; break;
  
    case BC_POP : --m_tos; break;
    case BC_POP2 : m_tos -= 2; break;
    case BC_DUP : ++m_tos; break;
    case BC_DUP2 : m_tos += 2; break;
    case BC_SWAP : break;
    case BC_PUSHNULL : ++m_tos; break;
    case BC_PUSHINT : ++m_tos; break;
    case BC_PUSHINT0 : ++m_tos; break;
    case BC_PUSHINT1 : ++m_tos; break;
    case BC_PUSHFP : ++m_tos; break;
    case BC_PUSHSTR : ++m_tos; break;
    case BC_PUSHTBL : ++m_tos; break;
    case BC_PUSHFN : ++m_tos; break;
    case BC_PUSHTHIS : ++m_tos; break;
  
    case BC_GETLOCAL : ++m_tos; break;
    case BC_SETLOCAL : --m_tos; break;
    case BC_GETGLOBAL : ++m_tos; break;
    case BC_SETGLOBAL : --m_tos; break;
    case BC_GETTHIS : ++m_tos; break;
    case BC_SETTHIS : --m_tos; break;
  
    case BC_OP_ADD : --m_tos; break;
    case BC_OP_SUB : --m_tos; break;
    case BC_OP_MUL : --m_tos; break;
    case BC_OP_DIV : --m_tos; break;
    case BC_OP_REM : --m_tos; break;

    case BC_BIT_OR : --m_tos; break;
    case BC_BIT_XOR : --m_tos; break;
    case BC_BIT_AND : --m_tos; break;
    case BC_BIT_INV : --m_tos; break;
    case BC_BIT_SHL : --m_tos; break;
    case BC_BIT_SHR : --m_tos; break;
  
    case BC_OP_NEG : break;
    case BC_OP_POS : break;
    case BC_OP_NOT : break;
  
    case BC_OP_LT : --m_tos; break;
    case BC_OP_GT : --m_tos; break;
    case BC_OP_LTE : --m_tos; break;
    case BC_OP_GTE : --m_tos; break;
    case BC_OP_EQ : --m_tos; break;
    case BC_OP_NEQ : --m_tos; break;
  }

  if(m_tos > m_maxTos) m_maxTos = m_tos;
}


