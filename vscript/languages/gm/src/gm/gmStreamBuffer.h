/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMSTREAMMEM_H_
#define _GMSTREAMMEM_H_

#include "gmConfig.h"
#include "gmStream.h"
#include "gmArraySimple.h"

/// \class gmStreamBufferStatic
/// \brief gmStreamBufferStatic is a read only stream constructed from a const void *
class gmStreamBufferStatic : public gmStream
{
public:
  
  gmStreamBufferStatic();
  gmStreamBufferStatic(const void * a_buffer, unsigned int a_size);
  virtual ~gmStreamBufferStatic();

  virtual unsigned int Seek(unsigned int p_pos);
  virtual unsigned int Tell() const;
  virtual unsigned int GetSize() const;
  virtual unsigned int Read(void * p_buffer, unsigned int p_n);
  virtual unsigned int Write(const void * p_buffer, unsigned int p_n);

  void Open(const void * a_buffer, unsigned int a_size);
  inline const char* GetData() const { return m_stream; }

private:

  unsigned int m_cursor;
  unsigned int m_size;
  const char * m_stream;
};


/// \class gmStreamBufferDynamic
/// \brief gmStreamBufferDynamic is a read\write memory stream
class gmStreamBufferDynamic : public gmStream
{
public:
  
  gmStreamBufferDynamic();
  virtual ~gmStreamBufferDynamic();

  virtual unsigned int Seek(unsigned int p_pos);
  virtual unsigned int Tell() const;
  virtual unsigned int GetSize() const;
  virtual unsigned int Read(void * p_buffer, unsigned int p_n);
  virtual unsigned int Write(const void * p_buffer, unsigned int p_n);

  void Reset() ;
  void ResetAndFreeMemory();
  inline void SetBlockSize(unsigned int a_blockSize) { m_stream.SetBlockSize(a_blockSize); }
  inline const char* GetData() const { return m_stream.GetData(); }
  inline char* GetUnsafeData() { return m_stream.GetData(); }
  void SetSize(unsigned int a_size) { m_stream.SetCount(a_size); }
  void SetCursor(unsigned int a_cursor) { m_cursor = a_cursor; }

private:

  unsigned int m_cursor;
  gmArraySimple<char> m_stream;
};


#endif // _GMSTREAMMEM_H_
