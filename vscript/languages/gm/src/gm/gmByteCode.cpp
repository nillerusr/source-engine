/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmByteCode.h"

// Must be last header
#include "memdbgon.h"


#if GM_COMPILE_DEBUG

void gmByteCodePrint(FILE * a_fp, const void * a_byteCode, int a_byteCodeLength)
{
  union 
  {
    const gmuint32 * instruction32;
    const gmuint8 * instruction;
  };

  instruction = (const gmuint8 *) a_byteCode;
  const gmuint8 * end = instruction + a_byteCodeLength;
  const gmuint8 * start = instruction;
  const char * cp;
  bool opiptr, opf32;

  while(instruction < end)
  {
    opiptr = false;
    opf32 = false;

    int addr = instruction - start;

    switch(*instruction)
    {
      case BC_NOP : cp = "nop"; break;
      case BC_LINE : cp = "line"; break;

      case BC_GETDOT : cp = "get dot"; opiptr = true; break;
      case BC_SETDOT : cp = "set dot"; opiptr = true; break;
      case BC_GETIND : cp = "get index"; break;
      case BC_SETIND : cp = "set index"; break;

      case BC_BRA : cp = "bra"; opiptr = true; break;
      case BC_BRZ : cp = "brz"; opiptr = true; break;
      case BC_BRNZ : cp = "brnz"; opiptr = true; break;
      case BC_BRZK : cp = "brzk"; opiptr = true; break;
      case BC_BRNZK : cp = "brnzk"; opiptr = true; break;
      case BC_CALL : cp = "call"; opiptr = true; break;
      case BC_RET : cp = "ret"; break;
      case BC_RETV : cp = "retv"; break;
      case BC_FOREACH : cp = "foreach"; opiptr = true; break;
      
      case BC_POP : cp = "pop"; break;
      case BC_POP2 : cp = "pop2"; break;
      case BC_DUP : cp = "dup"; break;
      case BC_DUP2 : cp = "dup2"; break;
      case BC_SWAP : cp = "swap"; break;
      case BC_PUSHNULL : cp = "push null"; break;
      case BC_PUSHINT : cp = "push int"; opiptr = true; break;
      case BC_PUSHINT0 : cp = "push int 0"; break;
      case BC_PUSHINT1 : cp = "push int 1"; break;
      case BC_PUSHFP : cp = "push fp"; opf32 = true; break;
      case BC_PUSHSTR : cp = "push str"; opiptr = true; break;
      case BC_PUSHTBL : cp = "push tbl"; break;
      case BC_PUSHFN : cp = "push fn"; opiptr = true; break;
      case BC_PUSHTHIS : cp = "push this"; break;
      
      case BC_GETLOCAL : cp = "get local"; opiptr = true; break;
      case BC_SETLOCAL : cp = "set local"; opiptr = true; break;
      case BC_GETGLOBAL : cp = "get global"; opiptr = true; break;
      case BC_SETGLOBAL : cp = "set global"; opiptr = true; break;
      case BC_GETTHIS : cp = "get this"; opiptr = true; break;
      case BC_SETTHIS : cp = "set this"; opiptr = true; break;
      
      case BC_OP_ADD : cp = "add"; break;
      case BC_OP_SUB : cp = "sub"; break;
      case BC_OP_MUL : cp = "mul"; break;
      case BC_OP_DIV : cp = "div"; break;
      case BC_OP_REM : cp = "rem"; break;

      case BC_BIT_OR : cp = "bor"; break;
      case BC_BIT_XOR : cp = "bxor"; break;
      case BC_BIT_AND : cp = "band"; break;
      case BC_BIT_INV : cp = "binv"; break;
      case BC_BIT_SHL : cp = "bshl"; break;
      case BC_BIT_SHR : cp = "bshr"; break;
      
      case BC_OP_NEG : cp = "neg"; break;
      case BC_OP_POS : cp = "pos"; break;
      case BC_OP_NOT : cp = "not"; break;
      
      case BC_OP_LT : cp = "lt"; break;
      case BC_OP_GT : cp = "gt"; break;
      case BC_OP_LTE : cp = "lte"; break;
      case BC_OP_GTE : cp = "gte"; break;
      case BC_OP_EQ : cp = "eq"; break;
      case BC_OP_NEQ : cp = "neq"; break;

      default : cp = "ERROR"; break;
    }

    ++instruction32;

    if(opf32)
    {
      float fval = *((float *) instruction);
      instruction += sizeof(gmint32);
      fprintf(a_fp, "  %04d %s %f"GM_NL, addr, cp, fval);
    }
    else if (opiptr)
    {
      gmptr ival = *((gmptr *) instruction);
      instruction += sizeof(gmptr);
      fprintf(a_fp, "  %04d %s %d"GM_NL, addr, cp, ival);
    }
    else
    {
      fprintf(a_fp, "  %04d %s"GM_NL, addr, cp);
    }
  }
}


#endif // GM_COMPILE_DEBUG

