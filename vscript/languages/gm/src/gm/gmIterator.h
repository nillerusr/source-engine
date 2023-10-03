/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMITERATOR_H_
#define _GMITERATOR_H_

//
// Intrusive Container Iterator
//

//
// Iterator must declare the following functions
//
// void Inc()
// void Dec()
// T* Resolve()
// const T* Resolve() const
// bool IsValid() const
//

#define GM_INCLUDE_ITERATOR_KERNEL(T)                                \
inline void operator++() { Inc(); }                                  \
inline void operator--() { Dec(); }                                  \
inline void operator++(int) { Inc(); }                               \
inline void operator--(int) { Dec(); }                               \
inline T& operator*(void) { return *Resolve(); }                     \
inline const T& operator*(void) const { return *Resolve(); }         \
inline T* operator->(void) { return Resolve(); }                     \
inline const T* operator->(void) const { return Resolve(); }         \
                                                                     \
inline operator bool(void) { return IsValid(); }                     \
inline operator bool(void) const { return IsValid(); }               \
inline bool operator !(void) { return !IsValid(); }                  \
inline bool operator !(void) const { return !IsValid(); }            \
                                                                     \
private:                                                             \
inline operator unsigned int(void) { return 0xDEADBEEF; }            \
inline operator int(void) { return 0xDEADBEEF; }                     \
inline operator unsigned int(void) const { return 0xDEADBEEF; }      \
inline operator int(void) const { return 0xDEADBEEF; }               \
public:

#endif // _GMITERATOR_H_
