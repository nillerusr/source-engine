//  See Copyright Notice in gmMachine.h

#include <stdio.h>
#include <stdlib.h>

#pragma pack( push, 1 )

#include "gmConfig.h"
#include "gmByteCode.h"

#define ID_func GM_MAKE_ID32('f','u','n','c')
#define ID_gml0 GM_MAKE_ID32('g','m','l','0')


struct gmlHeader
{
  gmuint32 id;
  gmuint32 flags;
  gmuint32 stOffset;
  gmuint32 scOffset;
  gmuint32 fnOffset;
};

struct gmlStrings
{
  gmuint32 size;
};

struct gmlLineInfo
{
  gmuint32 byteCodeAddress;
  gmuint32 lineNumber;
};

struct gmlSource
{
  gmuint32 size;
  gmuint32 flags;
};

struct gmlFunction
{
  gmuint32 func;
  gmuint32 id;
  gmuint32 flags;
  gmuint32 numParams;
  gmuint32 numLocals;
  gmuint32 maxStackSize;
  gmuint32 byteCodeLen;
};


#pragma pack( pop )


static void PrintByteCode(FILE * a_fp, const void * a_byteCode, int a_byteCodeLength, const char * symbols)
{
  union
  {
    const gmuint8 * instruction;
    const gmuint32 * instruction32;
  };

  instruction = (const gmuint8 *) a_byteCode;

  const gmuint8 * end = instruction + a_byteCodeLength;
  const gmuint8 * start = instruction;
  const char * cp;
  bool opiptr, opf32, opisymbol;

  while(instruction < end)
  {
    opiptr = false;
    opf32 = false;
    opisymbol = false;

    int addr = instruction - start;

    switch(*instruction32)
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
      //case BC_OP_INC : cp = "inc"; break;
      //case BC_OP_DEC : cp = "dec"; break;

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


void main(int argc, const char * argv[])
{
  gmlHeader header;
  gmlStrings strings;
  gmlSource source;
  gmlFunction function;
  FILE * fp = NULL;
  char * stringTable = NULL;
  char * sourceCode = NULL;
  bool error = true;
  unsigned int i, j;
  gmuint32 numFunctions = 0;
  bool debug = false;

  // process command line arguments
  if(argc < 2)
  {
    printf("Usage, %s <filename> where filename is a gmlib file you wish to view\n", argv[0]);
    return;
  }

  const char * libfile = argv[1];
  fp = fopen(libfile, "rb");
  if(fp == NULL)
  {
    printf("Could not open file %s\n", libfile);
    return;
  }

  // Load the gmlib header
  if(fread(&header, sizeof(header), 1, fp) != 1) goto done;
  if(header.id != ID_gml0) goto done;
  debug = (header.flags & 1);
  printf("GM LIBRARY %s, DEBUG=%d\n\n", libfile, debug);


  // Load the string table
  fseek(fp, header.stOffset, SEEK_SET);
  if(fread(&strings, sizeof(strings), 1, fp) != 1) goto done;
  printf("STRING TABLE, SIZE=%5d\n"
          "===============================================================================\n", 
          strings.size);
  stringTable = new char[strings.size];
  if(fread(stringTable, strings.size, 1, fp) != 1) goto done;
  printf("%05d:%s\n", 0, stringTable);
  for(i = 0; i < strings.size-1; ++i)
    if(stringTable[i]==0)
      printf("%05d:%s\n", i+1,&stringTable[i+1]);

  // Read the source code
  if(header.scOffset)
  {
    printf("\n===============================================================================\n");
    printf("SOURCE CODE\n");
    printf("===============================================================================\n");

    fseek(fp, header.scOffset, SEEK_SET);
    if(fread(&source, sizeof(source), 1, fp) != 1) goto done;
    sourceCode = new char[source.size];
    if(fread(sourceCode, source.size, 1, fp) != 1) goto done;
    printf("%s", sourceCode);
    printf("\n===============================================================================\n");
  }
  else
  {
    printf("\nNO SOURCE CODE IN LIB\n");
  }

  // Read the functions
  fseek(fp, header.fnOffset, SEEK_SET);
  // read in the number of functions
  if(fread(&numFunctions, sizeof(numFunctions), 1, fp) != 1) goto done;
  printf("FUNCTIONS, COUNT=%d\n"
         "===============================================================================\n\n", 
         numFunctions);

  for(i = 0; i < numFunctions; ++i)
  {
    printf("===============================================================================\n");

    // Read in the function
    if(fread(&function, sizeof(function), 1, fp) != 1) goto done;
    if(function.func != ID_func) goto done;

    printf("FUNCTION ID %d\n", function.id);
    printf("FLAGS %d\n", function.flags);
    printf("NUM PARAMS %d\n", function.numParams);
    printf("NUM LOCALS %d\n", function.numLocals);
    printf("MAX STACK %d\n", function.maxStackSize);
    printf("BYTE CODE SIZE %d\n", function.byteCodeLen);

    char * byteCode = new char[function.byteCodeLen];
    // Read in the byte code
    if(fread(byteCode, function.byteCodeLen, 1, fp) != 1)
    {
      delete[] byteCode;
      goto done;
    }

    // Print the byte code
    PrintByteCode(stdout, byteCode, function.byteCodeLen, stringTable);
    delete[] byteCode;

    // Read in the function debug information
    if(debug)
    {
      // debug name
      gmuint32 ui32;
      if(fread(&ui32, sizeof(ui32), 1, fp) != 1) goto done;
      if(ui32 < strings.size)
      {
        printf("FUNCTION DEBUG NAME %s\n", stringTable + ui32);
      }
      else goto done;

      // line info
      printf("\nLINE DEBUGGING INFO\n");
      if(fread(&ui32, sizeof(ui32), 1, fp) != 1) goto done;
      for(j = 0; j < ui32; ++j)
      {
        gmlLineInfo lineInfo;
        if(fread(&lineInfo, sizeof(lineInfo), 1, fp) != 1) goto done;
        printf("BYTE CODE ADDR %5d SOURCE LINE NUMBER %5d\n", lineInfo.byteCodeAddress, lineInfo.lineNumber);
      }

      // params and locals
      printf("PARAMS AND LOCALS\n");
      for(j = 0; j < function.numLocals + function.numParams; ++j)
      {
        if(fread(&ui32, sizeof(ui32), 1, fp) != 1) goto done;
        if(ui32 < strings.size)
        {
          if(j < function.numParams)
            printf("PARAM %d %s\n", j, stringTable + ui32);
          else
            printf("LOCAL %d %s\n", j - function.numParams, stringTable + ui32);
        }
        else goto done;
      }
    }
    printf("\n\n");
  }

  printf("END LIB\n");
  error = false;

done:

  if(fp) { fclose(fp); }
  if(stringTable) delete[] stringTable;
  if(sourceCode) delete[] sourceCode;
  if(error) printf("Error reading lib file\n");
}

/*

  flags                       [4 bytes]  // 0x01 - debug lib, 

  string_table_offset         [4 bytes]  // relative to start of lib
  source_code_offset          [4 bytes]  // 0 for not there, otherwise relative to start of lib
  functions_offset            [4 bytes]  // relative to start of lib

  string_table
  {
    string_table_size         [4 bytes]
    data                      [1 byte ] * string_table_size of 0 terminated strings
  }

  source_code
  {
    source_code_size          [4 bytes]
    flags                     [4 bytes] // possibly encryption and compression flags
    data                      [1 byte ] * source code size.
  }

  functions
  {
    num_functions             [4 bytes]  // number of functions

    function[]
    {
      'func'                  [4 bytes]
      function_id             [4 bytes]
      num_params              [4 bytes]
      num_locals              [4 bytes]
      max_stack_size          [4 bytes]
      byte_code_length        [4 bytes]
      byte_code               [1 byte ] * byte_code_length // contains offsets rel to string tab 
                                                           // and function ids for certian op codes.

      if(debug lib)
      {
        debug_name_offset     [4 bytes]
        symbol_name_offsets[] [4 bytes] * (num_params + num_locals) (~0) on no offset
        line_info_count       [4 bytes]

        line_info[]
        {
          byte_code_address   [4 bytes]
          line_number         [4 bytes]
        }
      }
    }
  }  

*/
