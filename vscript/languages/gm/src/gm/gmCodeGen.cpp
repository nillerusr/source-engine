/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmCodeGen.h"
#include "gmCodeTree.h"
#include "gmByteCodeGen.h"
#include "gmArraySimple.h"
#include "gmListDouble.h"

// Must be last header
#include "memdbgon.h"


//static const char * s_tempVarName0 = "__t0"; // Currently not used
static const char * s_tempVarName1 = "__t1";

#define SIZEOF_BC_BRA   8

/// \brief gmSortDebugLines will sort debug line information
static void gmSortDebugLines(gmArraySimple<gmLineInfo> &a_lineInfo)
{
  int count = a_lineInfo.Count();

  // sort by address
  int i;
  for(i = 0; i < count; ++i)
  {
    int min = i, j;
    for(j = i + 1; j < count; ++j)
    {
      if(a_lineInfo[j].m_address < a_lineInfo[min].m_address) 
      {
        min = j;
      }
    }
    gmLineInfo t = a_lineInfo[min];
    a_lineInfo[min] = a_lineInfo[i];
    a_lineInfo[i] = t;
  }

  // remove duplicate line numbers
  int s, d;
  for(s = 1, d = 0; s < count; ++s)
  {
    if(a_lineInfo[s].m_lineNumber != a_lineInfo[d].m_lineNumber)
    {
      a_lineInfo[++d] = a_lineInfo[s];
    }
  }

  a_lineInfo.SetCount(++d);
}


/*!
  \class gmCodeGenPrivate
  \brief implementation of gmCodeGen
*/
class gmCodeGenPrivate : public gmCodeGen
{
public:

  gmCodeGenPrivate();
  virtual ~gmCodeGenPrivate();

  // implementation

  virtual void FreeMemory();
  virtual int Lock(const gmCodeTreeNode * a_codeTree, gmCodeGenHooks * a_hooks, bool a_debug, gmLog * a_log);
  virtual int Unlock();

  // helpers

  bool Generate(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode, bool a_siblings = true);
  bool GenDeclVariable(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprFunction(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprTable(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtReturn(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtBreak(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtContinue(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtFor(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtForEach(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtWhile(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtDoWhile(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtIf(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenStmtCompound(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpDot(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpUnary(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpArrayIndex(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpAr(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpShift(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpComparison(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpBitwise(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpAnd(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpOr(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprOpAssign(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprConstant(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprIdentifier(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprCall(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);
  bool GenExprThis(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode);

  bool m_locked;
  int m_errors;
  gmLog * m_log;
  gmCodeGenHooks * m_hooks;
  bool m_debug;

  // Variable
  struct Variable
  {
    int m_offset;
    gmCodeTreeVariableType m_type;
    const char * m_symbol;
  };

  // FunctionState
  class FunctionState : public gmListDoubleNode<FunctionState>
  {
  public:
    FunctionState();
    ~FunctionState() {}

    void Reset();
    int SetVariableType(const char * a_symbol, gmCodeTreeVariableType a_type);
    // return -2 if the variable does not exist, -1 if it exists but is not a local
    // set a type to var type if return >= -1
    int GetVariableOffset(const char * a_symbol, gmCodeTreeVariableType &a_type);

    const char * m_debugName; // name of the variable the function is assigned to.
    gmArraySimple<Variable> m_variables;
    int m_numLocals; // number of local variables including parameters.
    gmByteCodeGen m_byteCode;

    // line number debug.
    int m_currentLine;
    gmArraySimple<gmLineInfo> m_lineInfo;
  };

  // Patch
  struct Patch
  {
    gmuint32 m_address;
    int m_next;
  };

  // LoopInfo
  struct LoopInfo
  {
    int m_breaks;
    int m_continues;
  };

  int m_currentLoop; //!< loop top of stack
  FunctionState * m_currentFunction; //!< function top of stack

  gmListDouble<FunctionState> m_functionStack;
  gmArraySimple<LoopInfo> m_loopStack;
  gmArraySimple<Patch> m_patches;

  
  // helper functions
  FunctionState * PushFunction();
  FunctionState * PopFunction();
  void PushLoop();
  void PopLoop();
  void ApplyPatches(int a_patches, gmByteCodeGen * a_byteCode, gmuint32 a_value);
};


//
// gmLineNumberCallback is used to record byte code instruction addresses against source code line numbers.
// The callback records entries into 
//
void GM_CDECL gmLineNumberCallback(int a_address, void * a_context)
{
  gmCodeGenPrivate::FunctionState * state = (gmCodeGenPrivate::FunctionState *) a_context;
  gmLineInfo info, * lastEntry = NULL;
  info.m_address = a_address;
  info.m_lineNumber = state->m_currentLine;
  if(state->m_lineInfo.Count() > 0)
  {
    lastEntry = &state->m_lineInfo[state->m_lineInfo.Count() - 1];
  }
  if(lastEntry == NULL || (lastEntry->m_address != a_address) || (lastEntry->m_lineNumber != state->m_currentLine))
  {
    state->m_lineInfo.InsertLast(info);
  }
}



gmCodeGen& gmCodeGen::Get()
{
  static gmCodeGenPrivate codeGen;
  return codeGen;
}



//
//
// Implementation of gmCodeGenPrivate
//
//

gmCodeGenPrivate::gmCodeGenPrivate()
{
  m_locked = false;
  m_errors = 0;
  m_log = NULL;
  m_hooks = NULL;
  m_debug = false;

  m_currentLoop = -1;
  m_currentFunction = NULL;
}



gmCodeGenPrivate::~gmCodeGenPrivate()
{
  FreeMemory();
}



void gmCodeGenPrivate::FreeMemory()
{
  if(m_locked == false)
  {
    m_currentLoop = -1;
    m_currentFunction = NULL;
    m_loopStack.ResetAndFreeMemory();
    m_functionStack.RemoveAndDeleteAll();
    m_patches.ResetAndFreeMemory();
  }
}



int gmCodeGenPrivate::Lock(const gmCodeTreeNode * a_codeTree, gmCodeGenHooks * a_hooks, bool a_debug, gmLog * a_log)
{
  if(m_locked == true) return 1;

  // set up members
  m_errors = 0;
  m_locked = true;
  m_log = a_log;
  m_hooks = a_hooks;
  m_debug = a_debug;

  GM_ASSERT(m_hooks != NULL);

  // set up memory and stacks.
  m_currentLoop = -1;
  m_currentFunction = NULL;
  m_loopStack.Reset();
  m_patches.Reset();

  // set up the stacks for the first procedure.
  m_hooks->Begin(m_debug);

  PushFunction();
  GM_ASSERT(m_currentFunction);

  // generate the byte code for the root procedure
  if(!Generate(a_codeTree, &m_currentFunction->m_byteCode))
  {
    ++m_errors;
  }
  else
  {
    m_currentFunction->m_byteCode.Emit(BC_RET);

    // Create a locals table
    const char ** locals = NULL;
    if(m_debug)
    {
      locals = (const char **) alloca(sizeof(const char *) * m_currentFunction->m_numLocals);
      memset(locals, 0, sizeof(const char *) * m_currentFunction->m_numLocals);
      for(gmuint v = 0; v < m_currentFunction->m_variables.Count(); ++v)
      {
        Variable &variable = m_currentFunction->m_variables[v];
        if(variable.m_offset != -1)
        {
          locals[variable.m_offset] = variable.m_symbol;
        }
      }
    }
    
    // Fill out a function info struct and add the function to the code gen hooks.

    gmSortDebugLines(m_currentFunction->m_lineInfo);

    gmFunctionInfo info;
    info.m_id = m_hooks->GetFunctionId();
    info.m_root = true;
    info.m_byteCode = m_currentFunction->m_byteCode.GetData();
    info.m_byteCodeLength = m_currentFunction->m_byteCode.Tell();
    info.m_numParams = 0;
    info.m_numLocals = m_currentFunction->m_numLocals;
    info.m_symbols = locals;
    info.m_maxStackSize = m_currentFunction->m_byteCode.GetMaxTos();
    info.m_lineInfoCount = m_currentFunction->m_lineInfo.Count();
    info.m_lineInfo = m_currentFunction->m_lineInfo.GetData();
    info.m_debugName = "__main";
    m_hooks->AddFunction(info);

    //gmByteCodePrint(stdout, info.m_byteCode, info.m_byteCodeLength);
  }

  PopFunction();

  m_hooks->End(m_errors);

  return m_errors;
}



int gmCodeGenPrivate::Unlock()
{
  m_errors = 0;
  m_locked = false;
  m_log = NULL;
  m_hooks = NULL;
  m_debug = false;
  m_currentLoop = -1;
  m_loopStack.Reset();
  m_patches.Reset();
  return 0;
}



bool gmCodeGenPrivate::Generate(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode, bool a_siblings)
{
  bool res = true;

  while(a_node)
  {
    // record line number
    static int s_line = 0;
    if(m_currentFunction) m_currentFunction->m_currentLine = a_node->m_lineNumber;

    // if we are in debug, emit a BC_LINE instruction
    if(m_debug && (s_line != a_node->m_lineNumber) && 
       !(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_COMPOUND))
    {
      a_byteCode->Emit(BC_LINE);
      s_line = a_node->m_lineNumber;
    }

    switch(a_node->m_type)
    {
      case CTNT_DECLARATION :
      {
        switch(a_node->m_subType)
        {
          case CTNDT_VARIABLE : res = GenDeclVariable(a_node, a_byteCode); break;
          default: 
          {
            GM_ASSERT(false);
            return false;
          }
        }
        break;
      }
      case CTNT_STATEMENT :
      {
        switch(a_node->m_subType)
        {
          case CTNST_RETURN : res = GenStmtReturn(a_node, a_byteCode); break;
          case CTNST_BREAK : res = GenStmtBreak(a_node, a_byteCode); break;
          case CTNST_CONTINUE : res = GenStmtContinue(a_node, a_byteCode); break;
          case CTNST_FOR : res = GenStmtFor(a_node, a_byteCode); break;
          case CTNST_FOREACH : res = GenStmtForEach(a_node, a_byteCode); break;
          case CTNST_WHILE : res = GenStmtWhile(a_node, a_byteCode); break;
          case CTNST_DOWHILE : res = GenStmtDoWhile(a_node, a_byteCode); break;
          case CTNST_IF : res = GenStmtIf(a_node, a_byteCode); break;
          case CTNST_COMPOUND : res = GenStmtCompound(a_node, a_byteCode); break;
          default: 
          {
            GM_ASSERT(false);
            return false;
          }
        }
        break;
      }
      case CTNT_EXPRESSION : 
      {
        switch(a_node->m_subType)
        {
          case CTNET_OPERATION : 
          {
            switch(a_node->m_subTypeType)
            {
              case CTNOT_DOT :              res = GenExprOpDot(a_node, a_byteCode); break;
              case CTNOT_UNARY_PLUS :
              case CTNOT_UNARY_MINUS :
              case CTNOT_UNARY_COMPLEMENT :
              case CTNOT_UNARY_NOT : res = GenExprOpUnary(a_node, a_byteCode); break;
              case CTNOT_ARRAY_INDEX :      res = GenExprOpArrayIndex(a_node, a_byteCode); break;
              case CTNOT_TIMES :
              case CTNOT_DIVIDE :
              case CTNOT_REM :
              case CTNOT_MINUS :
              case CTNOT_ADD :              res = GenExprOpAr(a_node, a_byteCode); break;
              case CTNOT_SHIFT_LEFT :
              case CTNOT_SHIFT_RIGHT :      res = GenExprOpShift(a_node, a_byteCode); break;
              case CTNOT_LT :
              case CTNOT_GT :
              case CTNOT_LTE :
              case CTNOT_GTE :
              case CTNOT_EQ :
              case CTNOT_NEQ :              res = GenExprOpComparison(a_node, a_byteCode); break;
              case CTNOT_BIT_AND :
              case CTNOT_BIT_XOR :
              case CTNOT_BIT_OR :           res = GenExprOpBitwise(a_node, a_byteCode); break;
              case CTNOT_AND :              res = GenExprOpAnd(a_node, a_byteCode); break;
              case CTNOT_OR :               res = GenExprOpOr(a_node, a_byteCode); break;
              case CTNOT_ASSIGN :           res = GenExprOpAssign(a_node, a_byteCode); break;
              default:
              {
                GM_ASSERT(false);
                return false;
              }
            }
            break;
          }
          case CTNET_CONSTANT : res = GenExprConstant(a_node, a_byteCode); break;
          case CTNET_IDENTIFIER : res = GenExprIdentifier(a_node, a_byteCode); break;
          case CTNET_CALL : res = GenExprCall(a_node, a_byteCode); break;
          case CTNET_THIS : res = GenExprThis(a_node, a_byteCode); break;
          case CTNET_FUNCTION : res = GenExprFunction(a_node, a_byteCode); break;
          case CTNET_TABLE : res = GenExprTable(a_node, a_byteCode); break;
          default :
          {
            GM_ASSERT(false);
            return false;
          }
        }

        break;
      }
      default: 
      {
        GM_ASSERT(false);
        return false;
      }
    }

    if(!res) 
    {
      return false;
    }

    if((a_node->m_flags & gmCodeTreeNode::CTN_POP) > 0)
    {
      a_byteCode->Emit(BC_POP);
    }

    if(a_siblings)
    {
      a_node = a_node->m_sibling;
    }
    else a_node = NULL;
  }
  return true;
}



bool gmCodeGenPrivate::GenDeclVariable(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_DECLARATION && a_node->m_subType == CTNDT_VARIABLE);
  GM_ASSERT(m_currentFunction);
  m_currentFunction->SetVariableType(a_node->m_children[0]->m_data.m_string, (gmCodeTreeVariableType) a_node->m_subTypeType);
  return true;
}



bool gmCodeGenPrivate::GenExprFunction(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_FUNCTION);

  gmptr id = m_hooks->GetFunctionId();
  a_byteCode->EmitPtr(BC_PUSHFN, id);

  // Create the function
  PushFunction();

  // Get a debug function name as the name of the variable the function is assigned to
  if(m_debug && a_node->m_parent && a_node->m_parent->m_type == CTNT_EXPRESSION && a_node->m_parent->m_subType == CTNET_OPERATION &&
     (a_node->m_parent->m_subTypeType == CTNOT_ASSIGN || a_node->m_parent->m_subTypeType == CTNOT_ASSIGN_FIELD) && a_node->m_parent->m_children[1] == a_node)
  {
    const gmCodeTreeNode * debugName = a_node->m_parent->m_children[0];
    if(debugName && debugName->m_type == CTNT_EXPRESSION && debugName->m_subType == CTNET_IDENTIFIER)
    {
    }
    else if(debugName->m_type == CTNT_EXPRESSION && debugName->m_subType == CTNET_OPERATION && 
            debugName->m_subTypeType == CTNOT_DOT)
    {
      debugName = debugName->m_children[1];
    }
    else
    {
      debugName = NULL;
    }

    if(debugName)
    {
      GM_ASSERT(debugName->m_type == CTNT_EXPRESSION && debugName->m_subType == CTNET_IDENTIFIER);
      m_currentFunction->m_debugName = debugName->m_data.m_string;
    }
  }

  // Parameters
  const gmCodeTreeNode * params = a_node->m_children[0];
  int numParams = 0;
  while(params)
  {
    const gmCodeTreeNode * param = params->m_children[0];
    GM_ASSERT(param->m_type == CTNT_EXPRESSION && param->m_subType == CTNET_IDENTIFIER);
    if(m_currentFunction->SetVariableType(param->m_data.m_string, CTVT_LOCAL) != numParams)
    {
      if(m_log) m_log->LogEntry("error (%d) parameter %s already declared", param->m_lineNumber, param->m_data.m_string);
      PopFunction();
      return false;
    }
    ++numParams;
    params = params->m_sibling;
  }

  // Generate the code
  bool res = Generate(a_node->m_children[1], &m_currentFunction->m_byteCode);

  // Generate a return incase the function didnt have one.
  m_currentFunction->m_byteCode.Emit(BC_RET);

  if(res)
  {
    // Create a locals table
    const char ** locals = NULL;
    if(m_debug)
    {
      locals = (const char **) alloca(sizeof(const char *) * m_currentFunction->m_numLocals);
      memset(locals, 0, sizeof(const char *) * m_currentFunction->m_numLocals);

      for(gmuint v = 0; v < m_currentFunction->m_variables.Count(); ++v)
      {
        Variable &variable = m_currentFunction->m_variables[v];
        if(variable.m_offset != -1)
        {
          locals[variable.m_offset] = variable.m_symbol;
        }
      }
    }

    // Add the function to the hooks.

    gmSortDebugLines(m_currentFunction->m_lineInfo);
    
    gmFunctionInfo info;
    info.m_id = id;
    info.m_root = false;
    info.m_byteCode = m_currentFunction->m_byteCode.GetData();
    info.m_byteCodeLength = m_currentFunction->m_byteCode.Tell();
    info.m_numParams = numParams;
    info.m_numLocals = m_currentFunction->m_numLocals - numParams;
    info.m_symbols = locals;
    info.m_maxStackSize = m_currentFunction->m_byteCode.GetMaxTos();
    info.m_lineInfoCount = m_currentFunction->m_lineInfo.Count();
    info.m_lineInfo = m_currentFunction->m_lineInfo.GetData();
    info.m_debugName = m_currentFunction->m_debugName;
    m_hooks->AddFunction(info);

    //gmByteCodePrint(stdout, info.m_byteCode, info.m_byteCodeLength);
  }

  PopFunction();
  
  return res;
}



bool gmCodeGenPrivate::GenExprTable(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_TABLE);

  gmuint32 index = 0;
  const gmCodeTreeNode * fields = a_node->m_children[0];

  // Create table
  a_byteCode->Emit(BC_PUSHTBL);

  // Create fields
  while(fields)
  {
    a_byteCode->Emit(BC_DUP);
    
    if(fields->m_type == CTNT_EXPRESSION && fields->m_subType == CTNET_OPERATION && fields->m_subTypeType == CTNOT_ASSIGN_FIELD)
    {
      if(!Generate(fields->m_children[1], a_byteCode)) return false;
      a_byteCode->EmitPtr(BC_SETDOT, m_hooks->GetSymbolId(fields->m_children[0]->m_data.m_string));
    }
    else
    {
      a_byteCode->EmitPtr(BC_PUSHINT, index++);
      if(!Generate(fields, a_byteCode, false)) return false;
      a_byteCode->Emit(BC_SETIND);
    }

    fields = fields->m_sibling;
  }

  return true;
}



bool gmCodeGenPrivate::GenStmtReturn(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_RETURN);

  if(a_node->m_children[0])
  {
    if(!Generate(a_node->m_children[0], a_byteCode)) return false;
    return a_byteCode->Emit(BC_RETV);
  }
  return a_byteCode->Emit(BC_RET);
}



bool gmCodeGenPrivate::GenStmtBreak(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_BREAK);

  if(m_currentLoop >= 0)
  {
    a_byteCode->Emit(BC_BRA);
    Patch * patch = &m_patches.InsertLast();
    patch->m_address = a_byteCode->Skip(sizeof(gmuint32));
    patch->m_next = m_loopStack[m_currentLoop].m_breaks;
    m_loopStack[m_currentLoop].m_breaks = m_patches.Count()-1;
    return true;
  }

  if(m_log) m_log->LogEntry("error (%d) illegal break statement", a_node->m_lineNumber);
  return false;
}



bool gmCodeGenPrivate::GenStmtContinue(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_CONTINUE);

  if(m_currentLoop >= 0)
  {
    a_byteCode->Emit(BC_BRA);
    Patch * patch = &m_patches.InsertLast();
    patch->m_address = a_byteCode->Skip(sizeof(gmuint32));
    patch->m_next = m_loopStack[m_currentLoop].m_continues;
    m_loopStack[m_currentLoop].m_continues = m_patches.Count()-1;
    return true;
  }

  if(m_log) m_log->LogEntry("error (%d) illegal continue statement", a_node->m_lineNumber);
  return false;
}



bool gmCodeGenPrivate::GenStmtFor(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_FOR);

  unsigned int loc1, loc2 = 0, continueAddress;

  // Initialisers
  if(!Generate(a_node->m_children[0], a_byteCode)) return false;

  PushLoop();

  loc1 = a_byteCode->Tell();

  // Condition expression
  if(!Generate(a_node->m_children[1], a_byteCode))
  {
    PopLoop();
    return false;
  }
  if(a_node->m_children[1] != NULL) // no branch for no test.
  {
    loc2 = a_byteCode->Skip(SIZEOF_BC_BRA);
  }

  // Body
  if(!Generate(a_node->m_children[3], a_byteCode))
  {
    PopLoop();
    return false;
  }

  // Continue patch
  continueAddress = a_byteCode->Tell();

  // Loop Expression
  if(!Generate(a_node->m_children[2], a_byteCode))
  {
    PopLoop();
    return false;
  }

  a_byteCode->EmitPtr(BC_BRA, loc1);
  loc1 = a_byteCode->Tell();
  if(a_node->m_children[1] != NULL)
  {
    a_byteCode->Seek(loc2);
    a_byteCode->EmitPtr(BC_BRZ, loc1);
    a_byteCode->Seek(loc1);
  }

  ApplyPatches(m_loopStack[m_currentLoop].m_breaks, a_byteCode, loc1);
  ApplyPatches(m_loopStack[m_currentLoop].m_continues, a_byteCode, continueAddress);

  PopLoop();
  return true;
}



bool gmCodeGenPrivate::GenStmtForEach(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  unsigned int breakAddress, continueAddress, loc1, loc2;

  // Generate table
  if(!Generate(a_node->m_children[0], a_byteCode))
  {
    return false;
  }

  PushLoop();

  // Push the first iterator
  a_byteCode->Emit(BC_PUSHINT, (gmuint32) -2); // first iterator value.

  continueAddress = a_byteCode->Tell();

  // Generate call
  const char * keyVar = s_tempVarName1;
  if(a_node->m_children[2]) keyVar = a_node->m_children[2]->m_data.m_string;
  const char * valueVar = a_node->m_children[1]->m_data.m_string;

  gmuint16 keyOffset = (gmuint16) m_currentFunction->SetVariableType(keyVar, CTVT_LOCAL);
  gmuint16 valueOffset = (gmuint16) m_currentFunction->SetVariableType(valueVar, CTVT_LOCAL);
  gmuint32 opcode = (keyOffset << 16) | (valueOffset & 0xffff);

  loc1 = a_byteCode->Tell();
  a_byteCode->Emit(BC_FOREACH, opcode);

  // Skip space for jump
  loc2 = a_byteCode->Skip(SIZEOF_BC_BRA);

  // Generate body
  if(!Generate(a_node->m_children[3], a_byteCode))
  {
    PopLoop();
    return false;
  }

  a_byteCode->Emit(BC_BRA, (gmuint32) loc1);
  breakAddress = a_byteCode->Seek(loc2);
  a_byteCode->EmitPtr(BC_BRZ, breakAddress);
  a_byteCode->Seek(breakAddress);

  // pop table and iterator
  a_byteCode->Emit(BC_POP2);

  ApplyPatches(m_loopStack[m_currentLoop].m_breaks, a_byteCode, breakAddress);
  ApplyPatches(m_loopStack[m_currentLoop].m_continues, a_byteCode, continueAddress);

  PopLoop();
  return true;
}



bool gmCodeGenPrivate::GenStmtWhile(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_WHILE);
  
  unsigned int loc1, loc2, continueAddress; 

  PushLoop();

  // Continue address
  loc1 = continueAddress = a_byteCode->Tell();

  // Condition expression
  if(!Generate(a_node->m_children[0], a_byteCode)) 
  {
    PopLoop();
    return false;
  }

  loc2 = a_byteCode->Skip(SIZEOF_BC_BRA);

  // Loop body
  if(!Generate(a_node->m_children[1], a_byteCode)) 
  {
    PopLoop();
    return false;
  }
  
  a_byteCode->EmitPtr(BC_BRA, loc1);
  loc1 = a_byteCode->Seek(loc2);
  a_byteCode->EmitPtr(BC_BRZ, loc1);
  a_byteCode->Seek(loc1);

  ApplyPatches(m_loopStack[m_currentLoop].m_breaks, a_byteCode, loc1);
  ApplyPatches(m_loopStack[m_currentLoop].m_continues, a_byteCode, continueAddress);

  PopLoop();
  return true;
}



bool gmCodeGenPrivate::GenStmtDoWhile(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_DOWHILE);
  
  unsigned int loc1, continueAddress; 

  PushLoop();

  loc1 = a_byteCode->Tell();

  // Loop body
  if(!Generate(a_node->m_children[1], a_byteCode)) 
  {
    PopLoop();
    return false;
  }

  // Continue address
  continueAddress = a_byteCode->Tell();

  // Condition expression
  if(!Generate(a_node->m_children[0], a_byteCode)) 
  {
    PopLoop();
    return false;
  }

  a_byteCode->EmitPtr(BC_BRNZ, loc1);

  loc1 = a_byteCode->Tell();

  ApplyPatches(m_loopStack[m_currentLoop].m_breaks, a_byteCode, loc1);
  ApplyPatches(m_loopStack[m_currentLoop].m_continues, a_byteCode, continueAddress);

  PopLoop();
  return true;
}



bool gmCodeGenPrivate::GenStmtIf(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_STATEMENT && a_node->m_subType == CTNST_IF);

  unsigned int loc1, loc2, loc3;
  
  if(a_node->m_children[2]) // Is this an if-else, or just an if
  {
    if(!Generate(a_node->m_children[0], a_byteCode)) return false;
    loc1 = a_byteCode->Skip(SIZEOF_BC_BRA);
    if(!Generate(a_node->m_children[1], a_byteCode)) return false;
    loc2 = a_byteCode->Skip(SIZEOF_BC_BRA);
    if(!Generate(a_node->m_children[2], a_byteCode)) return false;
    loc3 = a_byteCode->Seek(loc1);
    a_byteCode->EmitPtr(BC_BRZ, loc2+SIZEOF_BC_BRA);
    a_byteCode->Seek(loc2);
    a_byteCode->EmitPtr(BC_BRA, loc3);
    a_byteCode->Seek(loc3);
  }
  else
  {
    if(!Generate(a_node->m_children[0], a_byteCode)) return false;
    loc1 = a_byteCode->Skip(SIZEOF_BC_BRA);
    if(!Generate(a_node->m_children[1], a_byteCode)) return false;
    loc2 = a_byteCode->Seek(loc1);
    m_currentFunction->m_currentLine = a_node->m_lineNumber;
    a_byteCode->EmitPtr(BC_BRZ, loc2);
    a_byteCode->Seek(loc2);
  }

  return true;
}



bool gmCodeGenPrivate::GenStmtCompound(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  return Generate(a_node->m_children[0], a_byteCode);
}



bool gmCodeGenPrivate::GenExprOpDot(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION && a_node->m_subTypeType == CTNOT_DOT);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;

  // make sure child 1 is an identifier
  const gmCodeTreeNode * id = a_node->m_children[1];
  if(id && id->m_type == CTNT_EXPRESSION && id->m_subType == CTNET_IDENTIFIER)
  {
    return a_byteCode->EmitPtr(BC_GETDOT, m_hooks->GetSymbolId(a_node->m_children[1]->m_data.m_string));
  }

  if(m_log) m_log->LogEntry("error (%d) illegal dot operator", a_node->m_lineNumber);
  return false;
}



bool gmCodeGenPrivate::GenExprOpUnary(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;

  switch(a_node->m_subTypeType)
  {
    case CTNOT_UNARY_PLUS :       return a_byteCode->Emit(BC_OP_POS);
    case CTNOT_UNARY_MINUS :      return a_byteCode->Emit(BC_OP_NEG);
    case CTNOT_UNARY_NOT :        return a_byteCode->Emit(BC_OP_NOT);
    case CTNOT_UNARY_COMPLEMENT : return a_byteCode->Emit(BC_BIT_INV);
    default :
    {
      if(m_log) m_log->LogEntry("error (%d) unkown operator", a_node->m_lineNumber);
    }
  }
  return false;
}



bool gmCodeGenPrivate::GenExprOpArrayIndex(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION && a_node->m_subTypeType == CTNOT_ARRAY_INDEX);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  return a_byteCode->Emit(BC_GETIND);
}



bool gmCodeGenPrivate::GenExprOpAr(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  switch(a_node->m_subTypeType)
  {
    case CTNOT_TIMES : return a_byteCode->Emit(BC_OP_MUL);
    case CTNOT_DIVIDE : return a_byteCode->Emit(BC_OP_DIV);
    case CTNOT_REM : return a_byteCode->Emit(BC_OP_REM);
    case CTNOT_ADD : return a_byteCode->Emit(BC_OP_ADD);
    case CTNOT_MINUS : return a_byteCode->Emit(BC_OP_SUB);
    default :
    {
      if(m_log) m_log->LogEntry("error (%d) unkown arithmatic operator", a_node->m_lineNumber);
    }
  }
  return false;
}



bool gmCodeGenPrivate::GenExprOpShift(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  switch(a_node->m_subTypeType)
  {
    case CTNOT_SHIFT_LEFT :  return a_byteCode->Emit(BC_BIT_SHL);
    case CTNOT_SHIFT_RIGHT : return a_byteCode->Emit(BC_BIT_SHR);
    default :
    {
      if(m_log) m_log->LogEntry("error (%d) unkown shift operator", a_node->m_lineNumber);
    }
  }
  return false;
}



bool gmCodeGenPrivate::GenExprOpComparison(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  switch(a_node->m_subTypeType)
  {
    case CTNOT_LT :  return a_byteCode->Emit(BC_OP_LT);
    case CTNOT_GT :  return a_byteCode->Emit(BC_OP_GT);
    case CTNOT_LTE : return a_byteCode->Emit(BC_OP_LTE);
    case CTNOT_GTE : return a_byteCode->Emit(BC_OP_GTE);
    case CTNOT_EQ :  return a_byteCode->Emit(BC_OP_EQ);
    case CTNOT_NEQ : return a_byteCode->Emit(BC_OP_NEQ);
    default :
    {
      if(m_log) m_log->LogEntry("error (%d) unkown comparison operator", a_node->m_lineNumber);
    }
  }
  return false;
}



bool gmCodeGenPrivate::GenExprOpBitwise(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  switch(a_node->m_subTypeType)
  {
    case CTNOT_BIT_AND : return a_byteCode->Emit(BC_BIT_AND);
    case CTNOT_BIT_XOR : return a_byteCode->Emit(BC_BIT_XOR);
    case CTNOT_BIT_OR  : return a_byteCode->Emit(BC_BIT_OR);
    default :
    {
      if(m_log) m_log->LogEntry("error (%d) unkown bitwise operator", a_node->m_lineNumber);
    }
  }
  return false;
}



bool gmCodeGenPrivate::GenExprOpAnd(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION && a_node->m_subTypeType == CTNOT_AND);

  unsigned int loc1, loc2;

  // Generate expression 1
  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  loc1 = a_byteCode->Skip(SIZEOF_BC_BRA);

  // Generate expression 2
  a_byteCode->Emit(BC_POP);
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  // Seek back and finish expression 1
  loc2 = a_byteCode->Seek(loc1);
  a_byteCode->EmitPtr(BC_BRZK, loc2);
  a_byteCode->Seek(loc2);

  return true;
}



bool gmCodeGenPrivate::GenExprOpOr(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION && a_node->m_subTypeType == CTNOT_OR);

  unsigned int loc1, loc2;
 
  // Generate expression 1
  if(!Generate(a_node->m_children[0], a_byteCode)) return false;
  loc1 = a_byteCode->Skip(SIZEOF_BC_BRA);

  // Generate expression 2
  a_byteCode->Emit(BC_POP);
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  // Seek back and finish expression 1
  loc2 = a_byteCode->Seek(loc1);
  a_byteCode->EmitPtr(BC_BRNZK, loc2);
  a_byteCode->Seek(loc2);

  return true;
}



bool gmCodeGenPrivate::GenExprOpAssign(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_OPERATION);

  // value on left hand side must be an l-value... ie, a dot, array or identifier.
  const gmCodeTreeNode * lValue = a_node->m_children[0];
  int type = 0;

  if(lValue->m_type == CTNT_EXPRESSION && lValue->m_subType == CTNET_OPERATION && lValue->m_subTypeType == CTNOT_DOT)
  {
    // Generate half l-value
    if(!Generate(lValue->m_children[0], a_byteCode)) return false;
    type = 0;
  }
  else if(lValue->m_type == CTNT_EXPRESSION && lValue->m_subType == CTNET_OPERATION && lValue->m_subTypeType == CTNOT_ARRAY_INDEX)
  {
    // Generate half l-value
    if(!Generate(lValue->m_children[0], a_byteCode)) return false;
    if(!Generate(lValue->m_children[1], a_byteCode)) return false;
    type = 1;
  }
  else if(lValue->m_type == CTNT_EXPRESSION && lValue->m_subType == CTNET_IDENTIFIER)
  {
    type = 2;
  }
  else
  {
    if(m_log) m_log->LogEntry("error (%d) illegal l-value for '=' operator", a_node->m_lineNumber);
    return false;
  }

  // Generate r-value
  if(!Generate(a_node->m_children[1], a_byteCode)) return false;

  // complete assignment
  if(type == 0)
  {
    a_byteCode->EmitPtr(BC_SETDOT, m_hooks->GetSymbolId(lValue->m_children[1]->m_data.m_string));
  }
  else if(type == 1)
  {
    a_byteCode->Emit(BC_SETIND);
  }
  else if(type == 2)
  {
    gmCodeTreeVariableType vtype;
    int offset = m_currentFunction->GetVariableOffset(lValue->m_data.m_string, vtype);

    // if local, set local regardless
    // if member set this
    // if global, set global
    // set and add local

    if((lValue->m_flags & gmCodeTreeNode::CTN_MEMBER) > 0)
    {
      return a_byteCode->EmitPtr(BC_SETTHIS, m_hooks->GetSymbolId(lValue->m_data.m_string));
    }
    if(offset >= 0 && vtype == CTVT_LOCAL)
    {
      return a_byteCode->Emit(BC_SETLOCAL, (gmuint32) offset);
    }
    else if(offset == -1)
    {
      if(vtype == CTVT_MEMBER)
      {
        return a_byteCode->EmitPtr(BC_SETTHIS, m_hooks->GetSymbolId(lValue->m_data.m_string));
      }
      else if(vtype == CTVT_GLOBAL)
      {
        return a_byteCode->EmitPtr(BC_SETGLOBAL, m_hooks->GetSymbolId(lValue->m_data.m_string));
      }
      if(m_log) m_log->LogEntry("internal error");
      return false;
    }

    offset = m_currentFunction->SetVariableType(lValue->m_data.m_string, CTVT_LOCAL);
    return a_byteCode->Emit(BC_SETLOCAL, (gmuint32) offset);
  }
  else
  {
    // paranoia
    if(m_log) m_log->LogEntry("internal error");
    return false;
  }

  return true;
}



bool gmCodeGenPrivate::GenExprConstant(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_CONSTANT);

  switch(a_node->m_subTypeType)
  {
    case CTNCT_INT : // INT
    {
      if(a_node->m_data.m_iValue == 0)
      {
        a_byteCode->Emit(BC_PUSHINT0);
      }
      else if(a_node->m_data.m_iValue == 1)
      {
        a_byteCode->Emit(BC_PUSHINT1);
      }
      else
      {
        a_byteCode->EmitPtr(BC_PUSHINT, *((gmptr *) &a_node->m_data.m_iValue));
      }
      break;
    }
    case CTNCT_FLOAT : // FLOAT
    {
      a_byteCode->Emit(BC_PUSHFP, *((gmuint32 *) ((void *) &a_node->m_data.m_fValue)));
      break;
    }
    case CTNCT_STRING : // STRING
    {
      a_byteCode->EmitPtr(BC_PUSHSTR, m_hooks->GetStringId(a_node->m_data.m_string));
      break;
    }
    case CTNCT_NULL : // NULL
    {
      a_byteCode->Emit(BC_PUSHNULL);
      break;
    }
    default:
    {
      if(m_log) m_log->LogEntry("unkown constant type");
      return false;
    }
  }
  
  return true;
}



bool gmCodeGenPrivate::GenExprIdentifier(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_IDENTIFIER);

  // if local, get local regardless
  // if member, get this
  // if global, get global
  // get global

  if((a_node->m_flags & gmCodeTreeNode::CTN_MEMBER) > 0)
  {
    return a_byteCode->EmitPtr(BC_GETTHIS, m_hooks->GetSymbolId(a_node->m_data.m_string));
  }

  gmCodeTreeVariableType type;
  int offset = m_currentFunction->GetVariableOffset(a_node->m_data.m_string, type);

  if(offset >= 0 && type == CTVT_LOCAL)
  {
    return a_byteCode->Emit(BC_GETLOCAL, (gmuint32) offset);
  }
  else if(offset != -2)
  {
    if(type == CTVT_MEMBER)
    {
      return a_byteCode->EmitPtr(BC_GETTHIS, m_hooks->GetSymbolId(a_node->m_data.m_string));
    }
    else if(type == CTVT_GLOBAL)
    {
      return a_byteCode->EmitPtr(BC_GETGLOBAL, m_hooks->GetSymbolId(a_node->m_data.m_string));
    }
    if(m_log) m_log->LogEntry("internal error");
    return false;
  }

  return a_byteCode->EmitPtr(BC_GETGLOBAL, m_hooks->GetSymbolId(a_node->m_data.m_string));
}



bool gmCodeGenPrivate::GenExprCall(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_CALL);

  // mark the stack.
  int stackLevel = a_byteCode->GetTos();

  // if callee is a dot function, push left side of dot as 'this'
  const gmCodeTreeNode * callee = a_node->m_children[0];

  if(callee->m_type == CTNT_EXPRESSION && callee->m_subType == CTNET_OPERATION && callee->m_subTypeType == CTNOT_DOT)
  {
    if(!Generate(callee->m_children[0], a_byteCode)) return false;
    a_byteCode->Emit(BC_DUP);
    a_byteCode->EmitPtr(BC_GETDOT, m_hooks->GetSymbolId(callee->m_children[1]->m_data.m_string));
  }
  else
  {
    if(a_node->m_children[2])
    {
      if(!Generate(a_node->m_children[2], a_byteCode)) return false;
    }
    else
    {
#if GM_COMPILE_PASS_THIS_ALWAYS

      a_byteCode->Emit(BC_PUSHTHIS);

#else // !GM_COMPILE_PASS_THIS_ALWAYS

      // if the lvalue is a member, pass 'this', otherwise pass 'null'
      bool pushed = false;
      if(callee->m_type == CTNT_EXPRESSION && callee->m_subType == CTNET_IDENTIFIER)
      {
        gmCodeTreeVariableType vtype;
        int offset = m_currentFunction->GetVariableOffset(callee->m_data.m_string, vtype);
        if(((callee->m_flags & gmCodeTreeNode::CTN_MEMBER) > 0) || (offset == -1 && vtype == CTVT_MEMBER))
        {
          a_byteCode->Emit(BC_PUSHTHIS);
          pushed = true;
        }
      }
      if(!pushed)
      {
        a_byteCode->Emit(BC_PUSHNULL);
      }

#endif // !GM_COMPILE_PASS_THIS_ALWAYS
    }
    if(!Generate(callee, a_byteCode)) return false;
  }

  // push parameters, count the number of parameters
  gmuint32 numParams = 0;

  const gmCodeTreeNode * params = a_node->m_children[1];

  while(params)
  {
    ++numParams;
    if(!Generate(params, a_byteCode, false)) return false;
    params = params->m_sibling;
  }

  // call
  a_byteCode->Emit(BC_CALL, (gmuint32) numParams);

  // restore the stack level.
  a_byteCode->SetTos(stackLevel + 1);

  return true;
}



bool gmCodeGenPrivate::GenExprThis(const gmCodeTreeNode * a_node, gmByteCodeGen * a_byteCode)
{
  GM_ASSERT(a_node->m_type == CTNT_EXPRESSION && a_node->m_subType == CTNET_THIS);
  return a_byteCode->Emit(BC_PUSHTHIS);
}



gmCodeGenPrivate::FunctionState::FunctionState()
{
  m_debugName = NULL;
  m_numLocals = 0;
  m_currentLine = 1;
  m_byteCode.Reset(this);
}



void gmCodeGenPrivate::FunctionState::Reset()
{
  m_debugName = NULL;
  m_variables.Reset();
  m_numLocals = 0;
  m_currentLine = 1;
  m_byteCode.Reset(this);
  m_lineInfo.Reset();
}



int gmCodeGenPrivate::FunctionState::GetVariableOffset(const char * a_symbol, gmCodeTreeVariableType &a_type)
{
  for(gmuint v = 0; v < m_variables.Count(); ++v)
  {
    Variable &variable = m_variables[v];
    if(strcmp(variable.m_symbol, a_symbol) == 0)
    {
      a_type = variable.m_type;
      if(variable.m_type == CTVT_LOCAL)
      {
        return variable.m_offset;
      }
      return -1;
    }
  }

  a_type = CTVT_GLOBAL;
  return -2;
}



int gmCodeGenPrivate::FunctionState::SetVariableType(const char * a_symbol, gmCodeTreeVariableType a_type)
{
  for(gmuint v = 0; v < m_variables.Count(); ++v)
  {
    Variable &variable = m_variables[v];
    if(strcmp(variable.m_symbol, a_symbol) == 0)
    {
      variable.m_type = a_type;
      // if this variable was previously not a local, be is now being declared as local, get a stack offset.
      if(a_type == CTVT_LOCAL && variable.m_offset == -1)
      {
        variable.m_offset = m_numLocals++;
      }
      return variable.m_offset;
    }
  }

  Variable &variable = m_variables.InsertLast();
  // if the new variable is a local, get a stack offset for it.
  if(a_type == CTVT_LOCAL)
  {
    variable.m_offset = m_numLocals++;
  }
  else variable.m_offset = -1;

  variable.m_type = a_type;
  variable.m_symbol = a_symbol;
  return variable.m_offset;
}



gmCodeGenPrivate::FunctionState * gmCodeGenPrivate::PushFunction()
{
  if(m_currentFunction)
  {
    if(m_currentFunction != m_functionStack.GetLast())
    {
      m_currentFunction = m_functionStack.GetNext(m_currentFunction);
    }
    else
    {
      m_currentFunction = new FunctionState();   
      m_functionStack.InsertLast(m_currentFunction);
    }
  }
  else
  {
    if(m_functionStack.IsEmpty())
    {
      m_currentFunction = new FunctionState();   
      m_functionStack.InsertLast(m_currentFunction);
    }
    else
    {
      m_currentFunction = m_functionStack.GetFirst();
    }  
  }

  m_currentFunction->Reset();

  m_currentFunction->m_byteCode.SetSwapEndianOnWrite(m_hooks->SwapEndian());

  // if we are debugging, set up some line number debugging.
  if(m_debug)
  {
    m_currentFunction->m_byteCode.m_emitCallback = gmLineNumberCallback;
  }

  return m_currentFunction;
}



gmCodeGenPrivate::FunctionState * gmCodeGenPrivate::PopFunction()
{
  if(m_currentFunction)
  {
    m_currentFunction->Reset();
    m_currentFunction = m_functionStack.GetPrev(m_currentFunction);
    if(!m_functionStack.IsValid(m_currentFunction))
    {
      m_currentFunction = NULL;
    }
  }

  return m_currentFunction;
}



void gmCodeGenPrivate::PushLoop()
{
  LoopInfo * loop = &m_loopStack.InsertLast();
  m_currentLoop = m_loopStack.Count()-1;
  loop->m_breaks = -1;
  loop->m_continues = -1;
}


void gmCodeGenPrivate::PopLoop()
{
  m_loopStack.RemoveLast();
  if(m_loopStack.Count())
  {
    m_currentLoop = m_loopStack.Count() - 1;
  }
  else
  {
    m_currentLoop = -1;
  }
}


void gmCodeGenPrivate::ApplyPatches(int a_patches, gmByteCodeGen * a_byteCode, gmuint32 a_value)
{
  unsigned int pos = a_byteCode->Tell();
  while(a_patches >= 0)
  {
    Patch * curPatch = &m_patches[a_patches];

    a_byteCode->Seek(curPatch->m_address);
    *a_byteCode << a_value;
    a_patches = curPatch->m_next;
  }
  a_byteCode->Seek(pos);
}
