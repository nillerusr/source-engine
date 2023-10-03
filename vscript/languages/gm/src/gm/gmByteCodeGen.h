/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h
*/

#ifndef _GMBYTECODEGEN_H_
#define _GMBYTECODEGEN_H_

#include "gmConfig.h"
#include "gmStreamBuffer.h"
#include "gmByteCode.h"

/// \class gmByteCodeBuffer
class gmByteCodeGen : public gmStreamBufferDynamic
{
public:
  gmByteCodeGen(void * a_context = NULL);
  virtual ~gmByteCodeGen() {}

  void Reset(void * a_context = NULL);

  bool Emit(gmByteCode a_instruction);
  bool Emit(gmByteCode a_instruction, gmuint32 a_operand32);
  bool EmitPtr(gmByteCode a_instruction, gmptr a_operand);

  unsigned int Skip(unsigned int p_n, unsigned char p_value = 0);

  /// \brief m_emitCallback will be called whenever code is emitted
  void (GM_CDECL *m_emitCallback)(int a_address, void * a_context);

  inline int GetMaxTos() const { return m_maxTos; }
  inline int GetTos() const { return m_tos; }
  inline void SetTos(int a_tos) { m_tos = a_tos; }

protected:

  void AdjustStack(gmByteCode a_instruction);

  int m_tos;
  int m_maxTos;
  void * m_context;
};

#endif // _GMBYTECODEGEN_H_
