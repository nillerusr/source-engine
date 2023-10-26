/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMSTREAM_H_
#define _GMSTREAM_H_

#include "gmMem.h"
#include "gmUtil.h"

/// \class gmStream
/// \brief gmStream is an abstract stream class
class gmStream
{
public:

  enum
  {
    ILLEGAL_POS = -1,
  };
  
  enum Flags
  {
    F_EOS       = (1 << 0), ///< is the stream at End Of Stream
    F_READ      = (1 << 1), ///< is the stream readable
    F_WRITE     = (1 << 2), ///< is the stream writable
    F_ERROR     = (1 << 3), ///< has a stream error occured
    F_SIZE      = (1 << 4), ///< are size operations valid on this stream?
    F_SEEK      = (1 << 5), ///< are seeking operations valid on this stream?
    F_TELL      = (1 << 6), ///< are tell operations valid on this stream?
    F_USER      = (1 << 6),
  };
  
  gmStream() : m_flags(0), m_swapEndian(false) {}
  virtual ~gmStream() {}

  /// \brief Seek()
  /// \return the p_pos before the seek if seek is supported, else ILLEGAL_POS.
  virtual unsigned int Seek(unsigned int p_pos) = 0;

  /// \brief Tell()
  /// \return the p_pos if tell is supported, else ILLEGAL_POS
  virtual unsigned int Tell() const { return (unsigned int) ILLEGAL_POS; }

  /// \brief GetSize() will return the size of the stream if the stream supports this feature.
  /// \return ILLEGAL_POS if GetSize is not supported
  virtual unsigned int GetSize() const { return (unsigned int) ILLEGAL_POS; }

  /// \brief Read() will read p_n bytes from the stream into p_buffer.
  /// \return the number of bytes successfully read.
  virtual unsigned int Read(void * p_buffer, unsigned int p_n) = 0;

  /// \brief Write() will write p_n bytes from p_buffer to the stream.
  /// \return the number of bytes successfully written
  virtual unsigned int Write(const void * p_buffer, unsigned int p_n) = 0;

  /// \brief GetFlags() will return the current stream flags
  inline Flags GetFlags() const { return (Flags) m_flags; }

  //
  // Streaming interface (quite slow, but helpful...)
  //

  /// \brief SetSwapEndianOnWrite()
  inline void SetSwapEndianOnWrite(bool a_state)  { m_swapEndian = a_state; }
  inline bool GetSwapEndianOnWrite() const        { return m_swapEndian; }

  /// \brief Explicitly set endian, will swap if necessary.
  inline void SetEndianOnWrite(gmEndian a_endian)
  {
    int isLittle = gmIsLittleEndian();
    bool needSwap = false;

    if(    ((a_endian == GM_ENDIAN_LITTLE) && !isLittle)
        || ((a_endian == GM_ENDIAN_BIG) && isLittle) )
    {
      needSwap = true;
    }

    SetSwapEndianOnWrite(needSwap);
  };

  /// \brief Get the endian based on current machine and swap flag.
  inline gmEndian GetEndianOnWrite() const
  {
    int isLittle = gmIsLittleEndian();
    
    if(m_swapEndian)
    {
      isLittle = !isLittle; 
    }

    if(isLittle)
    {
      return GM_ENDIAN_LITTLE; 
    }
    else
    { 
      return GM_ENDIAN_BIG; 
    }
  }

  template<class T>
  gmStream &operator <<(T a_v)
  {
    if(m_swapEndian)
      SwapEndian(a_v);
    Write(&a_v, sizeof(T));
    return *this;
  }

  template<class T>
  gmStream &operator >>(T &a_v)
  {
    Read(&a_v, sizeof(T));
    return *this;
  }

protected:

  unsigned int m_flags;
  bool m_swapEndian;
  
private:

  inline void SwapEndian(gmuint8 &a_x) { }
  inline void SwapEndian(gmint8 &a_x) { }
  inline void SwapEndian(gmuint16 &a_x) { a_x = (gmuint16)((a_x << 8) | ((a_x >> 8) & 0xff)); }
  inline void SwapEndian(gmint16 &a_x) { a_x = (gmint16)((a_x << 8) | ((a_x >> 8) & 0xff)); }
  inline void SwapEndian(gmuint32 &a_x) { a_x = (a_x << 24) | ((a_x << 8) & 0x00ff0000) | ((a_x >> 8) & 0x0000ff00) | ((a_x >> 24) & 0x000000ff); }
  inline void SwapEndian(gmint32 &a_x) { a_x = (a_x << 24) | ((a_x << 8) & 0x00ff0000) | ((a_x >> 8) & 0x0000ff00) | ((a_x >> 24) & 0x000000ff); }
  inline void SwapEndian(gmfloat &a_x) { SwapEndian((gmuint32 &) a_x); }
};


#endif // _GMSTREAM_H_
