/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMARRAYSIMPLE_H_
#define _GMARRAYSIMPLE_H_

#include "gmConfig.h"
#include "gmUtil.h"

#define TMPL template <class T>
#define QUAL gmArraySimple<T>

/// \class gmArraySimple
/// \brief templated array class for simple types with power of 2 auto size option.
///        Elements may be moved around in memory using memcpy() during resize operations.
TMPL class gmArraySimple
{  
public:
  
  enum
  {
    NULL_INDEX = ~0
  };
  
  gmArraySimple(void);
  gmArraySimple(const QUAL &a_array);
  ~gmArraySimple(void);
  
  template <class J>
  inline void InsertLast(const J &a_elem)
  {
    if(m_count >= m_size)
    {
      Resize(m_count + 1);
    }
    m_elem[m_count++] = a_elem;
  }

  /// \brief SetBlockSize() will set the hysteresis memory grow by in elements. 
  /// \param a_blockSize as 0 will set automatic power of 2.
  inline void SetBlockSize(gmuint a_blockSize) { m_blockSize = a_blockSize; }

  inline bool InsertLastIfUnique(const T &a_elem);
  inline T& InsertLast(void);
  inline void InsertBefore(gmuint a_index, const T &a_elem);
  inline void Remove(gmuint a_index);
  inline void RemoveSwapLast(gmuint a_index);
  inline void RemoveLast(void);
  
  inline T &operator[](gmuint a_index);
  inline const T &operator[](gmuint a_index) const;

  inline gmuint Count(void) const { return m_count; }
  inline bool IsEmpty(void) const { return (m_count == 0); }

  inline void Reset(void) { SetCount(0); }
  inline void ResetAndFreeMemory(void);

  inline void SetCount(gmuint a_count);
  inline void SetCountAndFreeMemory(gmuint a_count);
  inline void Touch(gmuint a_element);

  inline T* GetData(void) { return m_elem; }
  inline const T* GetData(void) const { return m_elem; }
  inline gmuint GetSize(void) { return m_size; }
  bool IsValid(const T* a_elem) const;
  
  inline QUAL &operator=(const QUAL &a_array);
  
  inline bool FindRemove(const T &a_elem);
  
  template <class Q>
  inline gmuint FindIndex(const Q &a_elem) const
  {
    // iterate backwards, better chance of finding a_elem, given InsertLast() is 
    // used commonly which presents possible element coherence
    if(m_count == 0) return NULL_INDEX;
    gmuint i = m_count - 1;
    do
    {
      if (m_elem[i] == a_elem)    // used commonly which presents possible element coherence
        return i;
    } while(i-- > 0);
    return NULL_INDEX;
  }
  
private:
    
  T *m_elem;
  gmuint m_count, m_size;
  gmuint m_blockSize; //!< 0 and will be power of 2 sizing.
  
  /// \brief Resize() will resize the array.
  /// \param a_size is the required size.
  void Resize(gmuint a_size, bool a_shrinkIfPossible = false);
};


//
// implementation
//


TMPL
inline QUAL::gmArraySimple(void)
{
  m_elem = NULL;
  m_count = 0;
  m_size = 0;
  m_blockSize = 0; // power of 2 auto
}


TMPL inline QUAL::gmArraySimple(const QUAL &a_array)
{
  m_elem = NULL;
  m_count = 0;
  m_size = 0;
  m_blockSize = 0; // power of 2 auto
  operator=(a_array);
}


TMPL
inline QUAL::~gmArraySimple(void)
{
  if(m_elem)
  {
    delete[] (char *) m_elem;
  }
}


TMPL bool QUAL::InsertLastIfUnique(const T &a_elem)
{
  if(FindIndex(a_elem) == NULL_INDEX)
  {
    InsertLast(a_elem);
    return true;
  }
  return false;
}


TMPL inline T& QUAL::InsertLast(void)
{
  if(m_count >= m_size)
  {
    Resize(m_count + 1);
  }
  return m_elem[m_count++];
}


TMPL
inline void QUAL::InsertBefore(gmuint a_index, const T &a_elem)
{
  if(a_index >= m_count)
  {
    InsertLast(a_elem);
  }
  else
  {
    if(m_count >= m_size)
    {
      Resize(m_count + 1);
    }
    memmove(&m_elem[a_index+1], &m_elem[a_index], (m_count - a_index) * sizeof(T));
    m_elem[a_index] = a_elem;
    ++m_count;
  }
}


TMPL
inline void QUAL::Remove(gmuint a_index)
{
  if(a_index >= m_count) return;
  memmove(&m_elem[a_index], &m_elem[a_index+1], (m_count - (a_index + 1)) * sizeof(T));
  --m_count;
}


TMPL
inline void QUAL::RemoveSwapLast(gmuint a_index)
{
  if (a_index >= m_count) return;
  if(--m_count != a_index)
  {
    m_elem[a_index] = m_elem[m_count];
  }
}


TMPL
inline void QUAL::RemoveLast(void)
{
  GM_ASSERT(m_count > 0);
  --m_count;
}


TMPL
inline T &QUAL::operator[](gmuint a_index)
{
  GM_ASSERT(a_index >= 0 && a_index < m_count);
  return m_elem[a_index];
}


TMPL
inline const T &QUAL::operator[](gmuint a_index) const
{
  GM_ASSERT(a_index >= 0 && a_index < m_count);
  return m_elem[a_index];
}


TMPL
inline void QUAL::ResetAndFreeMemory(void)
{
  if(m_elem)
  {
    delete[] (char *) m_elem;
    m_elem = NULL;
  }
  m_count = m_size = 0;
}


TMPL
inline void QUAL::SetCount(gmuint a_count)
{
  if(a_count > m_size)
  {
    Resize(a_count);
  }
  m_count = a_count;
}


TMPL
inline void QUAL::SetCountAndFreeMemory(gmuint a_count)
{
  Resize(a_count, true);
  m_count = a_count;
}


TMPL
inline void QUAL::Touch(gmuint a_element)
{
  if(a_element >= m_count)
  {
    SetCount(a_element + 1);
  }
}


TMPL bool QUAL::IsValid(const T* a_elem) const
{
  gmuint index = (a_elem - m_elem);
  return (index < m_count);
}


TMPL
inline QUAL &QUAL::operator=(const QUAL &a_array)
{
  SetCount(a_array.m_count);
  memcpy((char*)m_elem, (const char*)a_array.m_elem, m_count * sizeof(T));
  return *this;
}


TMPL bool QUAL::FindRemove(const T &a_elem)
{
  gmuint index = FindIndex(a_elem);
  if(index != NULL_INDEX)
  {
    Remove(index);
    return true;
  }
  return false;
}


TMPL
void QUAL::Resize(gmuint a_size, bool a_shrinkIfPossible)
{
  if(m_size >= a_size)
  {
    // TODO: handle a_shrinkIfPossible.
    return;
  }

  // we need to grow, figure out a new size.
  gmuint size = 0;
  if(m_blockSize > 0)
  {
    size = ((a_size / m_blockSize) + 1) * m_blockSize;
  }
  else
  {
    size = gmLog2ge(gmMax<gmuint>(4, a_size + 1));
  }

  // alloc, copy, free
  {
    T * t = (T*) GM_NEW(char[sizeof(T) * size]);
    if(m_elem)
    {
      memcpy(t, m_elem, m_count * sizeof(T));
      delete[] (char *) m_elem;
    }
    m_elem = t;
  }

  m_size = size;
}

#undef QUAL
#undef TMPL

#endif
