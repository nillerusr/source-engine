/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmFunctionObject.h"
#include "gmMachine.h"

// Must be last header
#include "memdbgon.h"


gmFunctionObject::gmFunctionObject()
{
  m_cFunction = NULL;
  m_cUserData = NULL;
  m_debugInfo = NULL;
  m_byteCode = NULL;
  m_byteCodeLength = 0;
  m_maxStackSize = 1; // return value
  m_numLocals = 0;
  m_numParams = 0;
  m_numParamsLocals = 0;
  m_numReferences = 0;
  m_references = NULL;
}

void gmFunctionObject::Destruct(gmMachine * a_machine)
{
  if(m_references)
  {
    a_machine->Sys_Free(m_references);
    m_references = NULL;
  }
  if(m_byteCode)
  {
    a_machine->Sys_Free(m_byteCode);
    m_byteCode = NULL;
  }
  if(m_debugInfo)
  {
    if(m_debugInfo->m_debugName) { a_machine->Sys_Free(m_debugInfo->m_debugName); }
    if(m_debugInfo->m_lineInfo) { a_machine->Sys_Free(m_debugInfo->m_lineInfo); }
    if(m_debugInfo->m_symbols)
    {
      int i;
      for(i = 0; i < m_numParamsLocals; ++i)
      {
        a_machine->Sys_Free(m_debugInfo->m_symbols[i]);
      }
      a_machine->Sys_Free(m_debugInfo->m_symbols);
    }
    a_machine->Sys_Free(m_debugInfo);
    m_debugInfo = NULL;
  }

#if GM_USE_INCGC
  a_machine->DestructDeleteObject(this);
#endif //GM_USE_INCGC
}

#if GM_USE_INCGC


bool gmFunctionObject::Trace(gmMachine * a_machine, gmGarbageCollector* a_gc, const int a_workLeftToGo, int& a_workDone)
{
  int i;
  for(i = 0; i < m_numReferences; ++i)
  {
    gmObject * object = a_machine->GetObject(m_references[i]);
    a_gc->GetNextObject(object);
    ++a_workDone;
  }
    
  ++a_workDone;
  return true;
}

#else //GM_USE_INCGC

void gmFunctionObject::Mark(gmMachine * a_machine, gmuint32 a_mark)
{
  if(m_mark != GM_MARK_PERSIST) m_mark = a_mark;
  int i;
  for(i = 0; i < m_numReferences; ++i)
  {
    gmObject * object = a_machine->GetObject(m_references[i]);
    if(object->NeedsMark(a_mark)) object->Mark(a_machine, a_mark);
  }
}
#endif //GM_USE_INCGC


bool gmFunctionObject::Init(gmMachine * a_machine, bool a_debug, gmFunctionInfo &a_info, gmuint32 a_sourceId)
{
  // byte code
  if(a_info.m_byteCodeLength)
  {
    m_byteCode = (gmuint8 *) a_machine->Sys_Alloc(a_info.m_byteCodeLength);
    memcpy(m_byteCode, a_info.m_byteCode, a_info.m_byteCodeLength);
    m_byteCodeLength = a_info.m_byteCodeLength;
  }
  else
  {
    m_byteCode = NULL;
    m_byteCodeLength = 0;
  }

  // stack info
  m_maxStackSize = a_info.m_maxStackSize;
  m_numLocals = a_info.m_numLocals;
  m_numParams = a_info.m_numParams;
  m_numParamsLocals = a_info.m_numParams + a_info.m_numLocals;

  // references
  m_numReferences = 0;
  m_references = NULL;

  if(m_byteCode)
  {
    // find the objects this function references by iterating over the byte code and collecting them.
    // we could perform this step in the compilation phase if we don't want to iterate over the byte code.
    
    gmptr * references = (gmptr *) GM_NEW( char[a_info.m_byteCodeLength] );

    union
    {
      const gmuint8 * instruction;
      const gmuint32 * instruction32;
    };

    instruction = (const gmuint8 *) m_byteCode;
    const gmuint8 * end = instruction + m_byteCodeLength;
    for(;instruction < end;)
    {
      switch(*(instruction32++))
      {
        case BC_GETDOT :
        case BC_SETDOT :
        case BC_BRA :
        case BC_BRZ :
        case BC_BRNZ :
        case BC_BRZK :
        case BC_BRNZK :
        case BC_FOREACH :
        case BC_PUSHINT :
        case BC_GETGLOBAL :
        case BC_SETGLOBAL :
        case BC_GETTHIS :
        case BC_SETTHIS : instruction += sizeof(gmptr); break;
        case BC_PUSHFP : instruction += sizeof(gmfloat); break;
      
        case BC_CALL :
        case BC_GETLOCAL :
        case BC_SETLOCAL : instruction += sizeof(gmuint32); break;

        case BC_PUSHSTR :
        case BC_PUSHFN :
        {
          // if the reference does not already exist, add it.
          gmptr reference = *((gmptr *) instruction); 
          instruction += sizeof(gmptr);
          int i;
          for(i = 0; i < m_numReferences; ++i)
          {
            if(references[i] == reference) break;
          }
          if(i == m_numReferences) references[m_numReferences++] = reference;
          break;
        }

        default : break;
      }
    }

    if(m_numReferences > 0)
    {
      m_references = (gmptr *) a_machine->Sys_Alloc(sizeof(gmptr) * m_numReferences);
      memcpy(m_references, references, sizeof(gmptr) * m_numReferences);
    }

    delete [] (char*) references;
  }
  
  // debug info
  m_debugInfo = NULL;
  if(a_debug)
  {
    m_debugInfo = (gmFunctionObjectDebugInfo *) a_machine->Sys_Alloc(sizeof(gmFunctionObjectDebugInfo));
    memset(m_debugInfo, 0, sizeof(gmFunctionObjectDebugInfo));

    // source code id
    m_debugInfo->m_sourceId = a_sourceId;

    // debug name
    if(a_info.m_debugName)
    {
      int len = strlen(a_info.m_debugName) + 1;
      m_debugInfo->m_debugName = (char *) a_machine->Sys_Alloc(len);
      memcpy(m_debugInfo->m_debugName, a_info.m_debugName, len);
    }

    // symbols
    if(a_info.m_symbols)
    {
      m_debugInfo->m_symbols = (char **) a_machine->Sys_Alloc(sizeof(char *) * m_numParamsLocals);
      int i;
      for(i = 0; i < m_numParamsLocals; ++i)
      {
        int len = strlen(a_info.m_symbols[i]) + 1;
        m_debugInfo->m_symbols[i] = (char *) a_machine->Sys_Alloc(len);
        memcpy(m_debugInfo->m_symbols[i], a_info.m_symbols[i], len);
      }
    }

    // line number debugging.
    if(a_info.m_lineInfo)
    {
      // alloc and copy
      m_debugInfo->m_lineInfo = (gmLineInfo *) a_machine->Sys_Alloc(sizeof(gmLineInfo) * a_info.m_lineInfoCount);
      memcpy(m_debugInfo->m_lineInfo, a_info.m_lineInfo, sizeof(gmLineInfo) * a_info.m_lineInfoCount);
      m_debugInfo->m_lineInfoCount = a_info.m_lineInfoCount;
    }
  }
  
  return true;
}



int gmFunctionObject::GetLine(int a_address) const
{
  if(m_debugInfo && m_debugInfo->m_lineInfo)
  {
    int i;
    for(i = 0; i < m_debugInfo->m_lineInfoCount; ++i)
    {
      if(a_address < m_debugInfo->m_lineInfo[i].m_address)
      {
        // return entry before
        if(i > 0) --i;
        return m_debugInfo->m_lineInfo[i].m_lineNumber;
      }
    }
    return m_debugInfo->m_lineInfo[i - 1].m_lineNumber;
  }
  return 0;
}



const void * gmFunctionObject::GetInstructionAtLine(int a_line) const
{
  if(m_debugInfo && m_debugInfo->m_lineInfo && m_byteCode)
  {
    // serach for the first address using this line.
    int i;
    for(i = 0; i < m_debugInfo->m_lineInfoCount; ++i)
    {
      if(m_debugInfo->m_lineInfo[i].m_lineNumber == a_line)
      {
        return (void *) ((char *) m_byteCode + m_debugInfo->m_lineInfo[i].m_address);
      }
    }
  }
  return NULL;
}



gmuint32 gmFunctionObject::GetSourceId() const
{
  if(m_debugInfo)
  {
    return m_debugInfo->m_sourceId;
  }
  return 0;
}

