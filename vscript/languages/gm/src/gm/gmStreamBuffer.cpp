/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmStreamBuffer.h"


gmStreamBufferStatic::gmStreamBufferStatic()
{
  m_cursor = 0;
  m_size = 0;
  m_stream = NULL;
}


gmStreamBufferStatic::gmStreamBufferStatic(const void * p_buffer, unsigned int a_size)
{
  Open(p_buffer, a_size);
}


gmStreamBufferStatic::~gmStreamBufferStatic()
{
}


unsigned int gmStreamBufferStatic::Seek(unsigned int p_pos)
{
  int oldCursor = m_cursor;
  int cursor = p_pos;
  if(cursor < 0) return (unsigned int)ILLEGAL_POS;
  if((unsigned int) cursor > m_size) return (unsigned int)ILLEGAL_POS;
  m_cursor = cursor;
  return oldCursor;  
}


unsigned int gmStreamBufferStatic::Tell() const
{
  return m_cursor;
}


unsigned int gmStreamBufferStatic::GetSize() const
{
  return m_size;
}


unsigned int gmStreamBufferStatic::Read(void * p_buffer, unsigned int p_n)
{
  unsigned int remain = m_size - m_cursor;
  if(p_n > remain)
  {
    m_flags |= F_EOS;
    p_n = remain;
  }
  memcpy(p_buffer, &m_stream[m_cursor], p_n);
  m_cursor += p_n;
  return p_n;
}


unsigned int gmStreamBufferStatic::Write(const void * p_buffer, unsigned int p_n)
{
  m_flags |= F_ERROR;
  return 0;
}


void gmStreamBufferStatic::Open(const void * p_buffer, unsigned int a_size)
{
  m_cursor = 0;
  m_size = a_size;
  m_stream = (const char *) p_buffer;
  m_flags = F_READ | F_SIZE | F_SEEK | F_TELL;
}


//
// gmStreamBufferDynamic
//


gmStreamBufferDynamic::gmStreamBufferDynamic()
{
  m_cursor = 0;
  m_flags = F_READ | F_WRITE | F_SIZE | F_SEEK | F_TELL;
}


gmStreamBufferDynamic::~gmStreamBufferDynamic()
{
}


unsigned int gmStreamBufferDynamic::Seek(unsigned int p_pos)
{
  int oldCursor = m_cursor;
  int cursor = p_pos;
  if(cursor < 0) 
    return (unsigned int)ILLEGAL_POS;
  if((unsigned int) cursor > m_stream.Count()) 
    return (unsigned int)ILLEGAL_POS;
  m_cursor = cursor;
  return oldCursor;
}


unsigned int gmStreamBufferDynamic::Tell() const
{
  return m_cursor;
}


unsigned int gmStreamBufferDynamic::GetSize() const
{
  return m_stream.Count();
}


unsigned int gmStreamBufferDynamic::Read(void * p_buffer, unsigned int p_n)
{
  unsigned int remain = m_stream.Count() - m_cursor;
  if(p_n > remain)
  {
    // set eof
    m_flags |= F_EOS;
    p_n = remain;
  }
  memcpy(p_buffer, m_stream.GetData() + m_cursor, p_n);
  m_cursor += p_n;
  return p_n;
}


unsigned int gmStreamBufferDynamic::Write(const void * p_buffer, unsigned int p_n)
{
  unsigned int remain = m_stream.Count() - m_cursor;
  if(p_n > remain)
  {
    // grow the stream
    m_stream.SetCount(m_cursor + p_n);
  }
  memcpy(m_stream.GetData() + m_cursor, p_buffer, p_n);
  m_cursor += p_n;
  return p_n;
}


void gmStreamBufferDynamic::Reset()
{
  m_cursor = 0;
  m_flags = F_READ | F_WRITE | F_SIZE | F_SEEK | F_TELL;
  m_stream.Reset();
}


void gmStreamBufferDynamic::ResetAndFreeMemory()
{
  m_cursor = 0;
  m_flags = F_READ | F_WRITE | F_SIZE | F_SEEK | F_TELL;
  m_stream.ResetAndFreeMemory();
}

