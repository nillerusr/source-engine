/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmLibHooks.h"
#include "gmMachine.h"
#include "gmFunctionObject.h"
#include "gmStringObject.h"

// Must be last header
#include "memdbgon.h"


#define ID_func GM_MAKE_ID32('f','u','n','c')
#define ID_gml0 GM_MAKE_ID32('g','m','l','0')


gmLibHooks::gmLibHooks(gmStream &a_stream, const char * a_source) :
  m_allocator(1, GMCODETREE_CHAINSIZE)
{
  m_stream = &a_stream;
  m_source = a_source;
  m_swapEndian = m_stream->GetSwapEndianOnWrite();
}


gmLibHooks::~gmLibHooks()
{
}


bool gmLibHooks::Begin(bool a_debug)
{
  m_debug = a_debug;
  m_symbolOffset = 0;
  m_functionId = 0;
  m_functionStream.Reset();
  m_functionStream.SetSwapEndianOnWrite(m_swapEndian);
  return true;
}


bool gmLibHooks::AddFunction(gmFunctionInfo &a_info)
{
  // write the function into the stream
  m_functionStream << (gmuint32) ID_func;
  m_functionStream << (gmuint32) a_info.m_id;
  m_functionStream << (gmuint32) ((a_info.m_root) ? 1 : 0);
  m_functionStream << (gmuint32) a_info.m_numParams;
  m_functionStream << (gmuint32) a_info.m_numLocals;
  m_functionStream << (gmuint32) a_info.m_maxStackSize;
  m_functionStream << (gmuint32) a_info.m_byteCodeLength;
  m_functionStream.Write(a_info.m_byteCode, a_info.m_byteCodeLength);

  if(m_debug)
  {
    int numSymbols = a_info.m_numLocals + a_info.m_numParams, i;

    // debug name
    m_functionStream << (gmuint32) GetSymbolId(a_info.m_debugName);

    // line info
    m_functionStream << (gmuint32) a_info.m_lineInfoCount;
    for(i = 0; i < a_info.m_lineInfoCount; ++i)
    {
      m_functionStream << (gmuint32) a_info.m_lineInfo[i].m_address;
      m_functionStream << (gmuint32) a_info.m_lineInfo[i].m_lineNumber;
    }

    // symbol info
    for(i = 0; i < numSymbols; ++i)
    {
      if(a_info.m_symbols)
      {
        m_functionStream << (gmuint32) GetSymbolId(a_info.m_symbols[i]);
      }
      else
      {
        m_functionStream << (gmuint32) (~0);
      }
    }
  }
  return true;
}


bool gmLibHooks::End(int a_errors)
{
  if(a_errors == 0)
  {
    gmuint32 offsets[3] = {0}, offsetPos;
    
    gmuint32 t = ID_gml0, t1 = 0;
    *m_stream << t;
    t = (m_debug) ? 1 : 0;
    *m_stream << t;

    offsetPos = m_stream->Tell();
    *m_stream << offsets[0] << offsets[1] << offsets[2];

    // write the string table
    offsets[0] = m_stream->Tell();
    t = m_symbolOffset;
    *m_stream << t;
    USymbol * symbol = m_symbols.GetLast();
    while(m_symbols.IsValid(symbol))
    {
      m_stream->Write(symbol->m_string, strlen(symbol->m_string) + 1);
      symbol = m_symbols.GetPrev(symbol);
    }
    GM_ASSERT(m_stream->Tell() == m_symbolOffset + offsets[0] + sizeof(gmuint32));

    // write the source code
    if(m_debug && m_source)
    {
      offsets[1] = m_stream->Tell();
      t = strlen(m_source) + 1;
      t1 = 0;
      *m_stream << t << t1;
      m_stream->Write(m_source, t);
    }
    else
    {
      offsets[1] = 0;
    }

    // write the functions
    offsets[2] = m_stream->Tell();
    t = m_functionId;
    *m_stream << t;
    m_stream->Write(m_functionStream.GetData(), m_functionStream.GetSize());
    m_functionStream.ResetAndFreeMemory();

    // write the offsets
    m_stream->Seek(offsetPos);
    *m_stream << offsets[0] << offsets[1] << offsets[2];

    return true;
  }
  return false;
}


gmptr gmLibHooks::GetFunctionId()
{
  return m_functionId++;
}


gmptr gmLibHooks::GetSymbolId(const char * a_symbol)
{
  if(a_symbol == NULL) a_symbol = "";

  // see if we already have sybmol
  USymbol * symbol = m_symbols.GetFirst();
  while(m_symbols.IsValid(symbol))
  {
    if(strcmp(symbol->m_string, a_symbol) == 0)
    {
      return symbol->m_offset;
    }
    symbol = m_symbols.GetNext(symbol);
  }

  // add a new symbol
  unsigned int len = strlen(a_symbol) + 1;
  symbol = (USymbol *) m_allocator.AllocBytes(sizeof(USymbol), GM_DEFAULT_ALLOC_ALIGNMENT);
  symbol->m_string = (char *) m_allocator.AllocBytes(len, GM_DEFAULT_ALLOC_ALIGNMENT);
  memcpy(symbol->m_string, a_symbol, len);
  symbol->m_offset = m_symbolOffset;
  m_symbolOffset += len;
  m_symbols.InsertFirst(symbol);
  return symbol->m_offset;
}


gmptr gmLibHooks::GetStringId(const char * a_string)
{
  return GetSymbolId(a_string);
}


//
//
// Library Loading
//
//

struct gmlHeader
{
  gmuint32 m_id;
  gmuint32 m_flags;
  gmuint32 m_stOffset;
  gmuint32 m_scOffset;
  gmuint32 m_fnOffset;
};

struct gmlStrings
{
  gmuint32 m_size;
};

struct gmlLineInfo
{
  gmuint32 m_byteCodeAddress;
  gmuint32 m_lineNumber;
};

struct gmlSource
{
  gmuint32 m_size;
  gmuint32 m_flags;
};

struct gmlFunction
{
  gmuint32 m_func;
  gmuint32 m_id;
  gmuint32 m_flags;
  gmuint32 m_numParams;
  gmuint32 m_numLocals;
  gmuint32 m_maxStackSize;
  gmuint32 m_byteCodeLen;
};


gmFunctionObject * gmLibHooks::BindLib(gmMachine &a_machine, gmStream &a_stream, const char * a_filename)
{
  gmlHeader header;
  gmlStrings strings;
  gmlSource source;
  gmlFunction function;
  gmFunctionObject * functionObject = NULL;
  gmFunctionObject ** functionObjects = NULL;
  bool error = true, debug = false;
  char * stringTable = NULL;
  char * sourceCode = NULL;
  char * byteCode = NULL;
  unsigned int i, j;
  gmuint32 numFunctions = 0;
  gmuint32 sourceCodeId = 0;
  gmuint32 scratchSize = 2048;
  gmuint8 * scratch = GM_NEW( gmuint8[scratchSize] );

  // Turn garbage collection off.
  bool gc = a_machine.IsGCEnabled();
  a_machine.EnableGC(false);

  // Ensure stream is read from the start
  a_stream.Seek( 0 );

  // Load the gmlib header
  if((a_stream.Read(&header, sizeof(header)) != sizeof(header)) || header.m_id != ID_gml0) { goto done; }
  debug = (header.m_flags & 1);

  // Load the string table
  a_stream.Seek(header.m_stOffset);
  if(a_stream.Read(&strings, sizeof(strings)) != sizeof(strings)) { goto done; }
  stringTable = GM_NEW( char[strings.m_size] );
  if(a_stream.Read(stringTable, strings.m_size) != strings.m_size) { goto done; }

  // Read the source code 
  if(header.m_scOffset && a_machine.GetDebugMode())
  {
    a_stream.Seek(header.m_scOffset);
    if(a_stream.Read(&source, sizeof(source)) != sizeof(source)) { goto done; }
    sourceCode = GM_NEW( char[source.m_size] );
    if(a_stream.Read(sourceCode, source.m_size) != source.m_size) { goto done; }
    sourceCodeId = a_machine.AddSourceCode(sourceCode, a_filename);
    delete[] sourceCode;
    sourceCode = NULL;
  }

  // Read in the functions
  a_stream.Seek(header.m_fnOffset);
  if(a_stream.Read(&numFunctions, sizeof(numFunctions)) != sizeof(numFunctions)) { goto done; }

  // Allocate n function objects.
  functionObjects = GM_NEW( gmFunctionObject *[numFunctions] );
  for(i = 0; i < numFunctions; ++i)
  {
    functionObjects[i] = a_machine.AllocFunctionObject();
  }

  // Load each function
  for(i = 0; i < numFunctions; ++i)
  {
    if((a_stream.Read(&function, sizeof(function)) != sizeof(function)) || function.m_func != ID_func) { goto done; }
    // Read in the byte code
    if(byteCode) { delete[] byteCode; }
    // use the byte code allocation for the debug params and locals symbol array
    byteCode = GM_NEW( char[function.m_byteCodeLen] );
    if(a_stream.Read(byteCode, function.m_byteCodeLen) != function.m_byteCodeLen) { goto done; }

    // Load all symbols
    union
    {
      gmuint8 * instruction;
      gmuint32 * instruction32;
    };

    instruction = (gmuint8 *) byteCode;
    gmuint8 * end = instruction + function.m_byteCodeLen;
    for(;instruction < end;)
    {
      switch(*(instruction32++))
      {
        case BC_BRA :
        case BC_BRZ :
        case BC_BRNZ :
        case BC_BRZK :
        case BC_BRNZK :
        case BC_FOREACH :
        case BC_PUSHINT :
        case BC_PUSHFP : instruction += sizeof(gmfloat); break;

        case BC_CALL :
        case BC_GETLOCAL :
        case BC_SETLOCAL : instruction += sizeof(gmuint32); break;

        case BC_GETDOT :
        case BC_SETDOT :
        case BC_GETTHIS :
        case BC_SETTHIS :
        case BC_GETGLOBAL :
        case BC_SETGLOBAL :
        {
          gmptr * reference = (gmptr *) instruction; 
          GM_ASSERT(*reference >= 0 && *reference < (gmptr) strings.m_size);
          *reference = a_machine.AllocPermanantStringObject(&stringTable[*reference])->GetRef();
          instruction += sizeof(gmptr);
          break;
        }
        case BC_PUSHSTR :
        {
          gmptr * reference = (gmptr *) instruction; 
          GM_ASSERT(*reference >= 0 && *reference < (gmptr) strings.m_size);
          *reference = a_machine.AllocStringObject(&stringTable[*reference])->GetRef();
          instruction += sizeof(gmptr);
          break;
        }

        case BC_PUSHFN :
        {
          gmptr * reference = (gmptr *) instruction; 
          GM_ASSERT(*reference >= 0 && *reference < (gmptr) numFunctions);
          *reference = functionObjects[*reference]->GetRef();
          instruction += sizeof(gmptr);
          break;
        }

        default : break;
      }
    }

    // Initialise our function object.
    gmFunctionInfo functionInfo;
    gmFunctionObject * currFunction = functionObjects[function.m_id];
    functionInfo.m_id = currFunction->GetRef();
    functionInfo.m_root = (function.m_flags & 1);
    functionInfo.m_byteCode = byteCode;
    functionInfo.m_byteCodeLength = function.m_byteCodeLen;
    functionInfo.m_numParams = function.m_numParams;
    functionInfo.m_numLocals = function.m_numLocals;
    functionInfo.m_maxStackSize = function.m_maxStackSize;
    functionInfo.m_symbols = NULL;
    functionInfo.m_lineInfo = NULL;

    // We have now loaded all objects into the byte code....  Load the debug info
    if(debug)
    {
      gmuint32 stringOffset, lineInfoCount, numSymbols = function.m_numLocals + function.m_numParams;

      // debug name
      if(a_stream.Read(&stringOffset, sizeof(stringOffset)) != sizeof(stringOffset)) { goto done; }
      GM_ASSERT(stringOffset < strings.m_size);
      functionInfo.m_debugName = &stringTable[stringOffset];

      // Make sure our scratch memory is large enough
      if(a_stream.Read(&lineInfoCount, sizeof(lineInfoCount)) != sizeof(lineInfoCount)) { goto done; }
      gmuint32 reqdScratchSize = (lineInfoCount * sizeof(gmLineInfo)) + (sizeof(const char *) * numSymbols);
      if(scratchSize < reqdScratchSize)
      {
        scratchSize = reqdScratchSize;
        delete[] scratch;
        scratch = GM_NEW( gmuint8[scratchSize] );
      }
      gmLineInfo * lineInfo = (gmLineInfo *) scratch;
      functionInfo.m_lineInfo = lineInfo;
      functionInfo.m_symbols = (const char **) (scratch + (lineInfoCount * sizeof(gmLineInfo)));
      functionInfo.m_lineInfoCount = lineInfoCount;

      // Debug line info
      for(j = 0; j < lineInfoCount; ++j)
      {
        gmlLineInfo libLineInfo;
        if(a_stream.Read(&libLineInfo, sizeof(libLineInfo)) != sizeof(libLineInfo)) { goto done; }
        lineInfo[j].m_address = libLineInfo.m_byteCodeAddress;
        lineInfo[j].m_lineNumber = libLineInfo.m_lineNumber;
      }

      // Debug symbols
      for(j = 0; j < numSymbols; ++j)
      {
        if(a_stream.Read(&stringOffset, sizeof(stringOffset)) != sizeof(stringOffset)) { goto done; }
        GM_ASSERT(stringOffset < strings.m_size);
        functionInfo.m_symbols[j] = &stringTable[stringOffset];
      }

    }

    // AND FINALLY, INITIALISE OUR FUNCTION
    currFunction->Init(&a_machine, debug && a_machine.GetDebugMode(), functionInfo, sourceCodeId);
    if(functionInfo.m_root)
    {
      functionObject = currFunction;
    }
  }

  error = false;

done:

  // turn gc off.
  a_machine.EnableGC(gc);
  if(stringTable) { delete[] stringTable; }
  if(sourceCode) { delete[] sourceCode; }
  if(functionObjects) { delete[] functionObjects; }
  if(byteCode) { delete[] byteCode; }
  if(scratch) { delete[] scratch; }

  if(error)
  {
    a_machine.GetLog().LogEntry("Error loading library");
    return NULL;
  }
  return functionObject;
}

