/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmThread.h"
#include "gmByteCode.h"
#include "gmMachine.h"
#include "gmFunctionObject.h"
#include "gmOperators.h"
#include "gmMachineLib.h"

// helper macros

#define OPCODE_PTR(I)  *((gmptr *) (I)); (I) += sizeof(gmptr);
#define OPCODE_PTR_NI(I)  *((gmptr *) I);
#define OPCODE_FLOAT(I)  *((gmfloat *) I); I += sizeof(gmfloat);
#define OPERATOR(TYPE, OPERATOR) (m_machine->GetTypeNativeOperator((TYPE), (OPERATOR)))
#define CALLOPERATOR(TYPE, OPERATOR) (m_machine->GetTypeOperator((TYPE), (OPERATOR)))
#define GMTHREAD_LOG m_machine->GetLog().LogEntry
#define PUSHNULL top->m_type = GM_NULL; top->m_value.m_int = 0; ++top;

// helper functions
void gmGetLineFromString(const char * a_string, int a_line, char * a_buffer, int a_len)
{
  const char * cp = a_string, * eol;
  int line = 1;
  while(a_line > line)
  {
    while(*cp && *cp != '\n' && *cp != '\r') ++cp;
    if(*cp == '\n') { ++cp; while(*cp == '\r') ++cp; ++line; }
    else if(*cp == '\r') { ++cp; if(*cp == '\n') ++cp; ++line; }
    if(*cp == '\0') { a_buffer[0] = '\0'; return; }
  }

  eol = cp;
  while(*eol && *eol != '\n' && *eol != '\r') ++eol;
  int len = eol - cp;
  len = ((a_len - 1) < len) ? (a_len - 1) : len;
  memcpy(a_buffer, cp, len);
  a_buffer[len] = '\0';
}

//
//
// Implementation of gmThread
//
//

gmThread::gmThread(gmMachine * a_machine, int a_initialByteSize)
{
  m_frame = NULL;
  m_machine = a_machine;
  m_size = a_initialByteSize / sizeof(gmVariable);
  m_stack = GM_NEW( gmVariable[m_size] );
  m_top = 0;
  m_base = 0;
  m_numParameters = 0;
#if GMDEBUG_SUPPORT
  m_debugUser = 0;
  m_debugFlags = 0;
#endif // GMDEBUG_SUPPORT

  m_timeStamp = 0;
  m_startTime = 0;
  m_instruction = NULL;
  m_state = KILLED;
  m_id = GM_INVALID_THREAD;
  m_blocks = NULL;
  m_signals = NULL;

  m_user = 0;
}



gmThread::~gmThread()
{
  Sys_Reset(0);
  if(m_stack)
  {
    delete [] m_stack;
  }
}


#if GM_USE_INCGC
void gmThread::GCScanRoots(gmMachine* a_machine, gmGarbageCollector* a_gc)
{
  // mark stack
  int i;
  for(i = 0; i < m_top; ++i)
  {
    if(m_stack[i].IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, m_stack[i].m_value.m_ref);
      a_gc->GetNextObject(object);
    }
  }

  // mark signals
  gmSignal * signal = m_signals;
  while(signal)
  {
    if(signal->m_signal.IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, signal->m_signal.m_value.m_ref);
      a_gc->GetNextObject(object);
    }
    signal = signal->m_nextSignal;
  }

  // mark blocks
  gmBlock * block = m_blocks;
  while(block)
  {
    if(block->m_block.IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, block->m_block.m_value.m_ref);
      a_gc->GetNextObject(object);
    }
    block = block->m_nextBlock;
  }
}
#else //GM_USE_INCGC
void gmThread::Mark(gmuint32 a_mark)
{
  // mark stack
  int i;
  for(i = 0; i < m_top; ++i)
  {
    if(m_stack[i].IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, m_stack[i].m_value.m_ref);
      if(object->NeedsMark(a_mark)) object->Mark(m_machine, a_mark);
    }
  }

  // mark signals
  gmSignal * signal = m_signals;
  while(signal)
  {
    if(signal->m_signal.IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, signal->m_signal.m_value.m_ref);
      if(object->NeedsMark(a_mark)) object->Mark(m_machine, a_mark);
    }
    signal = signal->m_nextSignal;
  }

  // mark blocks
  gmBlock * block = m_blocks;
  while(block)
  {
    if(block->m_block.IsReference())
    {
      gmObject * object = GM_MOBJECT(m_machine, block->m_block.m_value.m_ref);
      if(object->NeedsMark(a_mark)) object->Mark(m_machine, a_mark);
    }
    block = block->m_nextBlock;
  }
}
#endif //GM_USE_INCGC

// RAGE AGAINST THE VIRTUAL MACHINE =)
gmThread::State gmThread::Sys_Execute(gmVariable * a_return)
{
  register union
  {
    const gmuint8 * instruction;
    const gmuint32 * instruction32;
  };
  register gmVariable * top;
  gmVariable * base;
  gmVariable * operand;
  const gmuint8 * code;

  if(m_state != RUNNING) return m_state;

#if GMDEBUG_SUPPORT

  if(m_debugFlags && m_machine->GetDebugMode() && m_machine->m_isBroken)
  {
    if(m_machine->m_isBroken(this)) 
      return RUNNING;
  }

#endif // GMDEBUG_SUPPORT

  // make sure we have a stack frame
  GM_ASSERT(m_frame);
  GM_ASSERT(GetFunction()->m_type == GM_FUNCTION);

  // cache our "registers"
  gmFunctionObject * fn = (gmFunctionObject *) GM_MOBJECT(m_machine, GetFunction()->m_value.m_ref);
  code = (const gmuint8 *) fn->GetByteCode();
  if(m_instruction == NULL) instruction = code;
  else instruction = m_instruction;
  top = GetTop();
  base = GetBase();

  //
  // start byte code execution
  //
  for(;;)
  {

#ifdef GM_CHECK_USER_BREAK_CALLBACK // This may be defined in gmConfig_p.h
    // Check external source to break execution with exception eg. Check for CTRL-BREAK
    // Endless loop protection could be implemented with this, or in a similar manner.
    if( gmMachine::s_userBreakCallback && gmMachine::s_userBreakCallback(this) )
    {
      GMTHREAD_LOG("User break. Execution halted.");
      goto LabelException;
    }
#endif //GM_CHECK_USER_BREAK_CALLBACK 

    switch(*(instruction32++))
    {
      //
      // unary operator
      //

      case BC_BIT_INV :
      case BC_OP_NEG :
      case BC_OP_POS :
      case BC_OP_NOT :
      {
        operand = top - 1; 
        gmOperatorFunction op = OPERATOR(operand->m_type, (gmOperator) instruction32[-1]); 
        if(op) 
        { 
          op(this, operand); 
        } 
        else if((fn = CALLOPERATOR(operand->m_type, (gmOperator) instruction32[-1]))) 
        { 
          operand[2] = operand[0]; 
          operand[0] = gmVariable(GM_NULL, 0); 
          operand[1] = gmVariable(GM_FUNCTION, fn->GetRef()); 
          SetTop(operand + 3); 
          State res = PushStackFrame(1, &instruction, &code); 
          top = GetTop();
          base = GetBase();
          if(res == RUNNING) break;
          if(res == SYS_YIELD) return RUNNING;
          if(res == SYS_EXCEPTION) goto LabelException;
          if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
          return res;
        } 
        else 
        { 
          GMTHREAD_LOG("unary operator %s undefined for type %s", gmGetOperatorName((gmOperator) instruction32[-1]), m_machine->GetTypeName(operand->m_type)); 
          goto LabelException; 
        } 
        break;
      }

      //
      // operator
      //

      case BC_OP_ADD :
      case BC_OP_SUB :
      case BC_OP_MUL :
      case BC_OP_DIV :
      case BC_OP_REM :
      case BC_BIT_OR :
      case BC_BIT_XOR :
      case BC_BIT_AND :
      case BC_BIT_SHL :
      case BC_BIT_SHR :
      case BC_OP_LT :
      case BC_OP_GT :
      case BC_OP_LTE :
      case BC_OP_GTE :
      case BC_OP_EQ :
      case BC_OP_NEQ :
      case BC_GETIND :
      {
        operand = top - 2; 
        --top; 
        register gmType t1 = operand[1].m_type; 
        if(operand->m_type > t1) t1 = operand->m_type; 
        gmOperatorFunction op = OPERATOR(t1, (gmOperator) instruction32[-1]); 
        if(op) 
        { 
          op(this, operand); 
        } 
        else if((fn = CALLOPERATOR(t1, (gmOperator) instruction32[-1]))) 
        { 
          operand[2] = operand[0]; 
          operand[3] = operand[1]; 
          operand[0] = gmVariable(GM_NULL, 0); 
          operand[1] = gmVariable(GM_FUNCTION, fn->GetRef()); 
          SetTop(operand + 4); 
          State res = PushStackFrame(2, &instruction, &code); 
          top = GetTop(); 
          base = GetBase();
          if(res == RUNNING) break;
          if(res == SYS_YIELD) return RUNNING;
          if(res == SYS_EXCEPTION) goto LabelException;
          if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
          return res;
        } 
        else 
        { 
          GMTHREAD_LOG("operator %s undefined for type %s and %s", gmGetOperatorName((gmOperator) instruction32[-1]), m_machine->GetTypeName(operand->m_type), m_machine->GetTypeName((operand + 1)->m_type)); 
          goto LabelException; 
        } 

        break;
      }
      case BC_SETIND : 
      { 
        operand = top - 3; 
        top -= 3; 
        gmOperatorFunction op = OPERATOR(operand->m_type, O_SETIND); 
        if(op) 
        { 
          op(this, operand); 
        } 
        else if((fn = CALLOPERATOR(operand->m_type, O_SETIND))) 
        { 
          operand[4] = operand[2]; 
          operand[3] = operand[1]; 
          operand[2] = operand[0]; 
          operand[0] = gmVariable(GM_NULL, 0); 
          operand[1] = gmVariable(GM_FUNCTION, fn->GetRef()); 
          SetTop(operand + 5); 
          State res = PushStackFrame(3, &instruction, &code); 
          top = GetTop(); 
          base = GetBase(); 
          if(res == RUNNING) break; 
          if(res == SYS_YIELD) return RUNNING; 
          if(res == SYS_EXCEPTION) goto LabelException; 
          if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread 
          return res; 
        } 
        else 
        { 
          GMTHREAD_LOG("setind failed."); 
          goto LabelException; 
        } 
        break; 
      } 
      case BC_NOP :
      {
        break;
      }
      case BC_LINE :
      {

#if GMDEBUG_SUPPORT

        if(m_machine->GetDebugMode() && m_machine->m_line)
        {
          SetTop(top);
          m_instruction = instruction;
          if(m_machine->m_line(this)) return RUNNING;
        }

#endif // GMDEBUG_SUPPORT

        break;
      }
      case BC_GETDOT :
      {
        operand = top - 1;
        gmptr member = OPCODE_PTR(instruction);
        top->m_type = GM_STRING;
        top->m_value.m_ref = member;
        gmType t1 = operand->m_type;
        gmOperatorFunction op = OPERATOR(t1, O_GETDOT);
        if(op)
        {
          op(this, operand);
          if(operand->m_type) break;
        }
        if(t1 == GM_NULL)
        {
          GMTHREAD_LOG("getdot failed.");
          goto LabelException;
        }
        *operand = m_machine->GetTypeVariable(t1, gmVariable(GM_STRING, member));
        break;
      }
      case BC_SETDOT :
      {
        operand = top - 2;
        gmptr member = OPCODE_PTR(instruction);
        top->m_type = GM_STRING;
        top->m_value.m_ref = member;
        top -= 2;
        gmOperatorFunction op = OPERATOR(operand->m_type, O_SETDOT);
        if(op)
        {
          op(this, operand);
        }
        else
        {
          GMTHREAD_LOG("setdot failed.");
          goto LabelException;
        }
        break;
      }
      case BC_BRA :
      {
        instruction = code + OPCODE_PTR_NI(instruction);
        break;
      }
      case BC_BRZ :
      {
#if GM_BOOL_OP
        operand = top - 1;
        --top;
        if (operand->m_type > GM_USER)
        {
          // Look for overridden operator.
          gmOperatorFunction op = OPERATOR(operand->m_type, O_BOOL);

          if (op)
          {
            op(this, operand);
          }
          else if ((fn = CALLOPERATOR(operand->m_type, O_BOOL)))
          {
            operand[2] = operand[0];
            operand[0] = gmVariable(GM_NULL, 0);
            operand[1] = gmVariable(GM_FUNCTION, fn->GetRef());
            SetTop(operand + 3);
            // Return to the same instruction after making the call but it will be testing the results of the call.
            --instruction32;
            State res = PushStackFrame(1, &instruction, &code);
            top = GetTop();
            base = GetBase();
            if(res == RUNNING) break;
            if(res == SYS_YIELD) return RUNNING;
            if(res == SYS_EXCEPTION) goto LabelException;
            if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
            return res;
          }
        }

        if(operand->m_value.m_int == 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#else // !GM_BOOL_OP
        --top;
        if(top->m_value.m_int == 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#endif // !GM_BOOL_OP
        break;
      }
      case BC_BRNZ :
      {
#if GM_BOOL_OP
        operand = top - 1;
        --top;
        if (operand->m_type > GM_USER)
        {
          // Look for overridden operator.
          gmOperatorFunction op = OPERATOR(operand->m_type, O_BOOL);

          if (op)
          {
            op(this, operand);
          }
          else if ((fn = CALLOPERATOR(operand->m_type, O_BOOL)))
          {
            operand[2] = operand[0];
            operand[0] = gmVariable(GM_NULL, 0);
            operand[1] = gmVariable(GM_FUNCTION, fn->GetRef());
            SetTop(operand + 3);
            // Return to the same instruction after making the call but it will be testing the results of the call.
            --instruction32;
            State res = PushStackFrame(1, &instruction, &code);
            top = GetTop();
            base = GetBase();
            if(res == RUNNING) break;
            if(res == SYS_YIELD) return RUNNING;
            if(res == SYS_EXCEPTION) goto LabelException;
            if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
            return res;
          }
        }

        if(operand->m_value.m_int != 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#else // !GM_BOOL_OP
        --top;
        if(top->m_value.m_int != 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#endif // !GM_BOOL_OP
        break;
      }
      case BC_BRZK :
      {
#if GM_BOOL_OP
        operand = top - 1;
        if (operand->m_type > GM_USER)
        {
          // Look for overridden operator.
          gmOperatorFunction op = OPERATOR(operand->m_type, O_BOOL);

          if (op)
          {
            op(this, operand);
          }
          else if ((fn = CALLOPERATOR(operand->m_type, O_BOOL)))
          {
            operand[2] = operand[0];
            operand[0] = gmVariable(GM_NULL, 0);
            operand[1] = gmVariable(GM_FUNCTION, fn->GetRef());
            SetTop(operand + 3);
            // Return to the same instruction after making the call but it will be testing the results of the call.
            --instruction32;
            State res = PushStackFrame(1, &instruction, &code);
            top = GetTop();
            base = GetBase();
            if(res == RUNNING) break;
            if(res == SYS_YIELD) return RUNNING;
            if(res == SYS_EXCEPTION) goto LabelException;
            if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
            return res;
          }
        }

        if(operand->m_value.m_int == 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#else // !GM_BOOL_OP
        if(top[-1].m_value.m_int == 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#endif // !GM_BOOL_OP
        break;
      }
      case BC_BRNZK :
      {
#if GM_BOOL_OP
        operand = top - 1;
        if (operand->m_type > GM_USER)
        {
          // Look for overridden operator.
          gmOperatorFunction op = OPERATOR(operand->m_type, O_BOOL);

          if (op)
          {
            op(this, operand);
          }
          else if ((fn = CALLOPERATOR(operand->m_type, O_BOOL)))
          {
            operand[2] = operand[0];
            operand[0] = gmVariable(GM_NULL, 0);
            operand[1] = gmVariable(GM_FUNCTION, fn->GetRef());
            SetTop(operand + 3);
            // Return to the same instruction after making the call but it will be testing the results of the call.
            --instruction32;
            State res = PushStackFrame(1, &instruction, &code);
            top = GetTop();
            base = GetBase();
            if(res == RUNNING) break;
            if(res == SYS_YIELD) return RUNNING;
            if(res == SYS_EXCEPTION) goto LabelException;
            if(res == KILLED) { m_machine->Sys_SwitchState(this, KILLED); GM_ASSERT(0); } // operator should not kill a thread
            return res;
          }
        }

        if(operand->m_value.m_int != 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#else // !GM_BOOL_OP
        if(top[-1].m_value.m_int != 0)
        {
          instruction = code + OPCODE_PTR_NI(instruction);
        }
        else instruction += sizeof(gmptr);
#endif // !GM_BOOL_OP
        break;
      }
      case BC_CALL :
      {
        SetTop(top);
        
        int numParams = (int) OPCODE_PTR(instruction);

        State res = PushStackFrame(numParams, &instruction, &code);
        top = GetTop(); 
        base = GetBase();

        if(res == RUNNING)
        {

#if GMDEBUG_SUPPORT

          if(m_debugFlags && m_machine->GetDebugMode() && m_machine->m_call)
          {
            m_instruction = instruction;
            if(m_machine->m_call(this)) return RUNNING;
          }

#endif // GMDEBUG_SUPPORT

          break;
        }
        if(res == SYS_YIELD) return RUNNING;
        if(res == SYS_EXCEPTION) goto LabelException;
        if(res == KILLED)
        {
          if(a_return) *a_return = m_stack[m_top - 1];
          m_machine->Sys_SwitchState(this, KILLED);
        }
        return res;
      }
      case BC_RET :
      {
        PUSHNULL;
      }
      case BC_RETV :
      {
        SetTop(top);
        int res = Sys_PopStackFrame(instruction, code);
        top = GetTop();
        base = GetBase();

        if(res == RUNNING) 
        {

#if GMDEBUG_SUPPORT

          if(m_debugFlags && m_machine->GetDebugMode() && m_machine->m_return)
          {
            m_instruction = instruction;
            if(m_machine->m_return(this)) return RUNNING;
          }

#endif // GMDEBUG_SUPPORT

          break;
        }
        if(res == KILLED)
        {
          if(a_return) *a_return = *(top - 1);
          m_machine->Sys_SwitchState(this, KILLED);
          return KILLED;
        }
        if(res == SYS_EXCEPTION) goto LabelException;
        break;
      }
      case BC_FOREACH :
      {
        gmuint32 localvalue = OPCODE_PTR(instruction);
        gmuint32 localkey = localvalue >> 16;
        localvalue &= 0xffff;

        // iterator is at tos-1, table is at tos -2, push int 1 if continuing loop. write key and value into localkey and localvalue
        if(top[-2].m_type != GM_TABLE) 
        {
          GMTHREAD_LOG("foreach expression is not table type");
          goto LabelException;
        }
        GM_ASSERT(top[-1].m_type == GM_INT);
        gmTableIterator it = (gmTableIterator) top[-1].m_value.m_int;
        gmTableObject * table = (gmTableObject *) GM_MOBJECT(m_machine, top[-2].m_value.m_ref);
        gmTableNode * node = table->GetNext(it);
        top[-1].m_value.m_int = it;
        if(node)
        {
          base[localkey] = node->m_key;
          base[localvalue] = node->m_value;
          top->m_type = GM_INT; top->m_value.m_int = 1;
        }
        else
        {
          top->m_type = GM_INT; top->m_value.m_int = 0;
        }
        ++top;
        break;
      }
      case BC_POP :
      {
        --top;
        break;
      }
      case BC_POP2 :
      {
        top -= 2;
        break;
      }
      case BC_DUP :
      {
        top[0] = top[-1]; 
        ++top;
        break;
      }
      case BC_DUP2 :
      {
        top[0] = top[-2];
        top[1] = top[-1];
        top += 2;
        break;
      }
      case BC_SWAP :
      {
        top[0] = top[-1];
        top[-1] = top[-2];
        top[-2] = top[0];
        break;
      }
      case BC_PUSHNULL :
      {
        PUSHNULL;
        break;
      }
      case BC_PUSHINT :
      {
        top->m_type = GM_INT;
        top->m_value.m_int = OPCODE_PTR(instruction);
        ++top;
        break;
      }
      case BC_PUSHINT0 :
      {
        top->m_type = GM_INT;
        top->m_value.m_int = 0;
        ++top;
        break;
      }
      case BC_PUSHINT1 :
      {
        top->m_type = GM_INT;
        top->m_value.m_int = 1;
        ++top;
        break;
      }
      case BC_PUSHFP :
      {
        top->m_type = GM_FLOAT;
        top->m_value.m_float = OPCODE_FLOAT(instruction);
        ++top;
        break;
      }
      case BC_PUSHSTR :
      {
        top->m_type = GM_STRING;
        top->m_value.m_ref = OPCODE_PTR(instruction);
        ++top;
        break;
      }
      case BC_PUSHTBL :
      {
        SetTop(top);
        top->m_type = GM_TABLE;
        top->m_value.m_ref = m_machine->AllocTableObject()->GetRef();
        ++top;
        break;
      }
      case BC_PUSHFN :
      {
        top->m_type = GM_FUNCTION;
        top->m_value.m_ref = OPCODE_PTR(instruction);
        ++top;
        break;
      }
      case BC_PUSHTHIS :
      {
        *top = *GetThis();
        ++top;
        break;
      }
      case BC_GETLOCAL :
      {
        gmuint32 offset = OPCODE_PTR(instruction);
        *(top++) = base[offset];
        break;
      }
      case BC_SETLOCAL :
      {
        gmuint32 offset = OPCODE_PTR(instruction);
        base[offset] = *(--top);
        break;
      }
      case BC_GETGLOBAL :
      {
        top->m_type = GM_STRING;
        top->m_value.m_ref = OPCODE_PTR(instruction);
		gmTableObject *pTable = m_machine->GetGlobals();
		gmVariable globalVar = pTable->Get(*top);
		
		if ( globalVar.IsNull() && pTable != m_machine->GetTrueGlobals())
		{
			pTable = m_machine->GetTrueGlobals();
			globalVar = pTable->Get(*top); 
		}
		*top = globalVar; 
		++top;

        break;
      }
      case BC_SETGLOBAL :
      {
        top->m_type = GM_STRING;
        top->m_value.m_ref = OPCODE_PTR(instruction);
        m_machine->GetGlobals()->Set(m_machine, *top, *(top-1)); --top;
        break;
      }
      case BC_GETTHIS :
      {
        gmptr member = OPCODE_PTR(instruction);
        const gmVariable * thisVar = GetThis();
        *top = *thisVar;
        top[1].m_type = GM_STRING;
        top[1].m_value.m_ref = member;
        gmOperatorFunction op = OPERATOR(thisVar->m_type, O_GETDOT);
        if(op)
        {
          op(this, top);
          if(top->m_type) { ++top; break; }
        }
        if(thisVar->m_type == GM_NULL)
        {
          GMTHREAD_LOG("getthis failed. this is null");
          goto LabelException;
        }
        *top = m_machine->GetTypeVariable(thisVar->m_type, top[1]);
        ++top;
        break;
      }
      case BC_SETTHIS :
      {
        gmptr member = OPCODE_PTR(instruction);
        const gmVariable * thisVar = GetThis();
        operand = top - 1;
        *top = *operand;
        *operand = *thisVar;
        top[1].m_type = GM_STRING;
        top[1].m_value.m_ref = member;
        --top;
        gmOperatorFunction op = OPERATOR(thisVar->m_type, O_SETDOT);
        if(op)
        {
          op(this, operand);
        }
        else
        {
          GMTHREAD_LOG("setthis failed.");
          goto LabelException;
        }
        break;
      }
      default :
      {
        break;
      }
    }
  }

LabelException:

  //
  // exception handler
  //
  m_instruction = instruction;

  // spit out error info
  LogLineFile();
  LogCallStack();

  // call machine exception handler
  if(gmMachine::s_machineCallback)
  {
    if(gmMachine::s_machineCallback(m_machine, MC_THREAD_EXCEPTION, this))
    {
#if GMDEBUG_SUPPORT
      // if we are being debugged, put this thread into a limbo state, waiting for delete.
      if(m_machine->GetDebugMode() && m_machine->m_debugUser)
      {
        m_machine->Sys_SwitchState(this, EXCEPTION);
        return EXCEPTION;
      }
#endif
    }
  }

  // kill the thread
  m_machine->Sys_SwitchState(this, KILLED);
  return KILLED;
}



void gmThread::Sys_Reset(int a_id)
{
  m_machine->Sys_RemoveBlocks(this);
  m_machine->Sys_RemoveSignals(this);

  gmStackFrame * frame;
  while(m_frame)
  {
    frame = m_frame->m_prev;
    m_machine->Sys_FreeStackFrame(m_frame);
    m_frame = frame;
  }
  m_top = 0;
  m_base = 0;
  m_instruction = NULL;
  m_timeStamp = 0;
  m_startTime = 0;
  m_id = a_id;
  m_numParameters = 0;
  m_user = 0;
}



gmThread::State gmThread::PushStackFrame(int a_numParameters, const gmuint8 ** a_ip, const gmuint8 ** a_cp)
{
  // calculate new stack base
  int base = m_top - a_numParameters;

  if( base == 2 ) // When initial thread function is ready, signal thread creation
  {
    // This may not be the best place to signal, but we at least want a valid 'this'
    m_base = base; // Init so some thread queries work
    m_machine->Sys_SignalCreateThread(this);
  }

  gmVariable * fnVar = &m_stack[base - 1];
  if(fnVar->m_type != GM_FUNCTION) 
  {
    m_machine->GetLog().LogEntry("attempt to call non function type");
    return SYS_EXCEPTION; 
  }
  gmFunctionObject * fn = (gmFunctionObject *) GM_MOBJECT(m_machine, fnVar->m_value.m_ref);

  if(fn->m_cFunction)
  {
    //
    // Its a native function call, call it now as we cannot stack wind natives.  this avoids
    // pushing a gmStackFrame.
    //

    m_numParameters = (short) a_numParameters;
    int lastBase = m_base;
    int lastTop = m_top;
    m_base = base;

    int result = fn->m_cFunction(this);

    // handle state
    if(result == GM_SYS_STATE)
    {
      // this is special case, a bit messy.
      return PushStackFrame(a_numParameters - GM_STATE_NUM_PARAMS, a_ip, a_cp);
    }

    // NOTE: It is not currently safe for a C binding to kill this thread.
    //       Since we cant unwind mixed script and native functions anyway, 
    //       perhaps the safest thing would be for ALWAYS delay killed threads 
    //       from deletion.

    // push a null if the function did not return anything
    if(lastTop == m_top)
    {
      m_stack[m_base - 2] = gmVariable(GM_NULL, 0);
    }
    else
    {
      m_stack[m_base - 2] = m_stack[m_top - 1];
    }

    // Restore the stack
    m_top = m_base - 1;
    m_base = lastBase;

    // check the call result
    if(result != GM_OK)
    {
      const gmuint8 * returnAddress = (a_ip) ? *a_ip : NULL;

      if(result == GM_SYS_YIELD)
      {
        m_machine->Sys_RemoveSignals(this);
        m_instruction = returnAddress;
        return SYS_YIELD;
      }
      else if(result == GM_SYS_BLOCK)
      {
        m_instruction = returnAddress;
        m_machine->Sys_SwitchState(this, BLOCKED);
        return BLOCKED;
      }
      else if(result == GM_SYS_SLEEP)
      {
        m_instruction = returnAddress;
        m_machine->Sys_SwitchState(this, SLEEPING);
        return SLEEPING;
      }
      else if(result == GM_SYS_KILL)
      {
        return KILLED;
      }
      return SYS_EXCEPTION;
    }

    if(!m_frame) // C called C function, no stack frame, so signal killed.
    {
      return KILLED;
    }

    // return result
    return RUNNING;
  }

  //
  // Its a script function call, push a stack frame
  //

  int clearSize = fn->GetNumParamsLocals() - a_numParameters;
  if(!Touch(clearSize + fn->GetMaxStackSize())) 
  {
    m_machine->GetLog().LogEntry("stack overflow");
    return SYS_EXCEPTION;
  }

  // zero missing params and locals.
  if(a_numParameters <= fn->GetNumParams())
  {
    memset(GetTop(), 0, sizeof(gmVariable) * clearSize);
  }
  else
  {
    memset(&m_stack[base + fn->GetNumParams()], 0, sizeof(gmVariable) * fn->GetNumLocals());
  }

  // push a new stack frame
  gmStackFrame * frame = m_machine->Sys_AllocStackFrame();
  frame->m_prev = m_frame;
  m_frame = frame;

  // cache new frame variables
  m_frame->m_returnBase = m_base;

  if(a_ip)
  {
    m_frame->m_returnAddress = *a_ip;
    *a_ip = (const gmuint8 *) fn->GetByteCode();
    *a_cp = *a_ip;
  }
  else
  {
    m_frame->m_returnAddress = NULL;
  }
  m_base = base;
  m_top = base + fn->GetNumParamsLocals();

  return RUNNING;
}



gmThread::State gmThread::Sys_PopStackFrame(const gmuint8 * &a_ip, const gmuint8 * &a_cp)
{
  if(m_frame == NULL)
  {
    m_machine->GetLog().LogEntry("stack underflow");
    return SYS_EXCEPTION;
  }

  gmStackFrame * frame = m_frame->m_prev;
  if( frame == NULL ) // Final frame, we will exit now
  {
    return KILLED; // Don't clean up stack, let the machine reset it as state changes to killed (so Exit callback can examine valid thread contents)
  }
  a_ip = m_frame->m_returnAddress;
  // Copy old tos to new tos
  m_stack[m_base - 2] = m_stack[m_top - 1];
  m_top = m_base - 1;
  m_base = m_frame->m_returnBase;
  m_machine->Sys_FreeStackFrame(m_frame);
  m_frame = frame;

  // Update instruction and code pointers
  GM_ASSERT(GetFunction()->m_type == GM_FUNCTION);
  gmFunctionObject * fn = (gmFunctionObject *) GM_MOBJECT(m_machine, GetFunction()->m_value.m_ref);
  a_cp = (const gmuint8 *) fn->GetByteCode();

  return RUNNING;
}



void gmThread::LogLineFile()
{
  // spit out the source and line info for the exception.
  if(m_base >= 2 && GetFunction()->m_type == GM_FUNCTION)
  {
    gmFunctionObject * fn = (gmFunctionObject *) GM_MOBJECT(m_machine, GetFunction()->m_value.m_ref);
    if(fn)
    {
      int line = fn->GetLine(m_instruction);
      gmuint32 id = fn->GetSourceId();
      const char * source, * filename;
      if(m_machine->GetSourceCode(id, source, filename))
      {
        char buffer[80];
        gmGetLineFromString(source, line, buffer, 80);
        m_machine->GetLog().LogEntry(GM_NL"%s(%d) : %s", filename, line, buffer);
      }
      else
      {
        m_machine->GetLog().LogEntry(GM_NL"unknown(%d) : ", line);
      }
    }
  }
}



bool gmThread::Touch(int a_extra)
{
  // Grow stack if necessary.  NOTE: Use better growth metric if needed.
  bool reAlloc = false; 
  while((m_top + a_extra + GMTHREAD_SLACKSPACE) >= m_size) 
  { 
    if(sizeof(gmVariable) * m_size > GMTHREAD_MAXBYTESIZE) return false; 
    m_size *= 2; 
    reAlloc = true; 
  } 
 
  if(reAlloc) 
  { 
   gmVariable * stack = new gmVariable[m_size]; 
   //memset(stack, 0, sizeof(gmVariable) * m_size); 
   memcpy(stack, m_stack, m_top * sizeof(gmVariable)); 
   if(m_stack) 
     delete[] m_stack; 
   m_stack = stack; 
  }
  return true;
}



void gmThread::LogCallStack()
{
  m_machine->GetLog().LogEntry(GM_NL"callstack..");
  gmStackFrame * frame = m_frame;
  int base = m_base;
  const gmuint8 * ip = m_instruction;

  while(frame)
  {
    // get the function object
    gmVariable * fnVar = &m_stack[base - 1];
    if(fnVar->m_type == GM_FUNCTION)
    {
      gmFunctionObject * fn = (gmFunctionObject *) GM_MOBJECT(m_machine, fnVar->m_value.m_ref);
      m_machine->GetLog().LogEntry("%3d: %s", fn->GetLine(ip), fn->GetDebugName());
    }
    base = frame->m_returnBase;
    ip = frame->m_returnAddress;
    frame = frame->m_prev;
  }
  m_machine->GetLog().LogEntry("");
}

