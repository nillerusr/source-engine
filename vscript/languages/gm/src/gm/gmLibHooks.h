/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmCodeGenHooks.h"
#include "gmListDouble.h"
#include "gmMemChain.h"
#include "gmStream.h"
#include "gmStreamBuffer.h"

class gmMachine;
class gmFunctionObject;

/// \class gmLibHooks
/// \brief gmLibHooks is a compiler hook class that allows compiling to a lib for disk storage.
class gmLibHooks : public gmCodeGenHooks
{
public:

  // a_source and must exist untill destruction of the libhooks
  gmLibHooks(gmStream &a_stream, const char * a_source); 
  virtual ~gmLibHooks();

  virtual bool Begin(bool a_debug);
  virtual bool AddFunction(gmFunctionInfo &a_info);
  virtual bool End(int a_errors);
  virtual gmptr GetFunctionId();
  virtual gmptr GetSymbolId(const char * a_symbol);
  virtual gmptr GetStringId(const char * a_string);
  virtual bool SwapEndian() const { return m_swapEndian; }

  /// \brief BindLib will bind the lib to the machine, and return the root function for executing.
  static gmFunctionObject * BindLib(gmMachine &a_machine, gmStream &a_stream, const char * a_filename);

private:

  class USymbol : public gmListDoubleNode<USymbol>
  {
  public: 
    USymbol();
    ~USymbol();
    char * m_string;
    gmptr m_offset; // offset into symbol table.
  };

  gmStream * m_stream;
  bool m_swapEndian;
  bool m_debug;
  const char * m_source;
  gmptr m_symbolOffset;
  gmptr m_functionId;
  gmStreamBufferDynamic m_functionStream;
  gmListDouble<USymbol> m_symbols;
  gmMemChain m_allocator;
};

/*

  Library file format for .gmlib files (gm library files) (native endian)

  // header

  'gml0'                      [4 bytes]
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
      function_id             [4 bytes] // FUNCTION IDS ARE FROM 0-(num_functions-1), BC_PUSHFN calls reference this id.
      flags                   [4 bytes] // 0x01 - is root
      num_params              [4 bytes]
      num_locals              [4 bytes]
      max_stack_size          [4 bytes]
      byte_code_length        [4 bytes]
      byte_code               [1 byte ] * byte_code_length // contains offsets rel to string tab 
                                                           // and function ids for certian op codes.

      // byte code must be changed on load such that BC_PUSHFN calls reference their respective function object, 
      // and BC_PUSHSTR reference their string object created by their string table offset.

      if(debug lib)
      {
        debug_name_offset     [4 bytes]
        line_info_count       [4 bytes]
        line_info[]
        {
          byte_code_address   [4 bytes]
          line_number         [4 bytes]
        }
        symbol_name_offsets[] [4 bytes] * (num_params + num_locals) (~0) on no offset
      }
    }
  }

*/
