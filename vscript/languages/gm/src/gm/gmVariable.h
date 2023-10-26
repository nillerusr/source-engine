/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMVARIABLE_H_
#define _GMVARIABLE_H_

#include "gmConfig.h"
#if GM_USE_INCGC
#include "gmIncGC.h"
#endif //GM_USE_INCGC

#define GM_MARK_PERSIST 0
#define GM_MARK_START 1

// fwd decls
class gmMachine;
class gmStringObject;
class gmTableObject;
class gmFunctionObject;
class gmUserObject;

/// \enum gmType
/// \brief gmType is an enum of the possible scripting types.
typedef int gmType;

enum
{
  GM_NULL = 0, // GM_NULL must be 0 as i have relied on this in expression testing.
  GM_INT,
  GM_FLOAT,

  GM_STRING,
  GM_TABLE,
  GM_FUNCTION,
  GM_USER,     // User types continue from here.

  GM_FORCEINT = GM_MAX_INT,
};



/// \struct gmVariable
/// \brief a variable is the basic type passed around on the stack, and used as storage in the symbol tables.
///        A variable is either a reference to a gmObject type, or it is a direct value such as null, int or float.
///        The gm runtime stack operates on gmVariable types.
struct gmVariable
{
  static gmVariable s_null;

  gmType m_type;
  union
  {
    int m_int;
    float m_float;
    gmptr m_ref;
  } m_value;

  inline gmVariable() 
  {
#if GMMACHINE_NULL_VAR_CTOR // Disabled by default
    Nullify();
#endif //GMMACHINE_NULL_VAR_CTOR
  }
  inline gmVariable(gmType a_type, gmptr a_ref) : m_type(a_type) { m_value.m_ref = a_ref; }

  explicit inline gmVariable(int a_val) : m_type(GM_INT) { m_value.m_int = a_val; }
  explicit inline gmVariable(float a_val) : m_type(GM_FLOAT) { m_value.m_float = a_val; }
  explicit inline gmVariable(gmStringObject * a_string) { SetString(a_string); }
  explicit inline gmVariable(gmTableObject * a_table) { SetTable(a_table); }
  explicit inline gmVariable(gmFunctionObject * a_func) { SetFunction(a_func); }
  explicit inline gmVariable(gmUserObject * a_user) { SetUser(a_user); }

  inline void SetInt(int a_value) { m_type = GM_INT; m_value.m_int = a_value; }
  inline void SetFloat(float a_value) { m_type = GM_FLOAT; m_value.m_float = a_value; }
  inline void SetString(gmStringObject * a_string);
  void SetString(gmMachine * a_machine, const char * a_cString);
  inline void SetTable(gmTableObject * a_table);
  inline void SetFunction(gmFunctionObject * a_function);
  void SetUser(gmUserObject * a_object);
  void SetUser(gmMachine * a_machine, void * a_userPtr, int a_userType);
  

  inline bool IsReference() const { return m_type > GM_FLOAT; }
  inline void Nullify() { m_type = GM_NULL; m_value.m_int = 0; }
  inline bool IsNull() { return m_type == GM_NULL; }

  /// \brief AsString will get this gm variable as a string if possible.  AsString is used for the gm "print" and system.Exec function bindings.
  /// \param a_buffer is a buffer you must provide for the function to write into.  this buffer needs only be 256 characters long as it is stated that
  ///        user types should give an AsString conversion < 256 characters.  If the type has a longer string type, it may return an interal string
  /// \param a_len must be >= 256
  /// \return a_buffer or internal variable string
  const char * AsString(gmMachine * a_machine, char * a_buffer, int a_len) const;

  /// \brief AsStringWithType will get the gm variable as a string with type name in front of value.  AsStringWithType is used for debugging, and may cut
  ///        the end off some string types.
  /// \sa AsString
  /// \return a_buffer always
  const char * AsStringWithType(gmMachine * a_machine, char * a_buffer, int a_len) const;

  /// Return int/float or zero
  inline int GetIntSafe() const;
  /// Return float/int or zero
  inline float GetFloatSafe() const;
  /// Return string object or null
  inline gmStringObject* GetStringObjectSafe() const;
  /// Return table object or null
  inline gmTableObject* GetTableObjectSafe() const;
  /// Return function object or null
  inline gmFunctionObject* GetFunctionObjectSafe() const;
  /// Return c string or empty string
  const char* GetCStringSafe() const;
  /// Return user type ptr or null
  void* GetUserSafe(int a_userType) const;
  /// Return user type ptr or null
  gmUserObject* GetUserObjectSafe(int a_userType) const;

  static inline gmuint Hash(const gmVariable &a_key)
  {
    gmuint hash = (gmuint) a_key.m_value.m_ref;
    if(a_key.IsReference())
    {
      hash >>= 2; // Reduce pointer address aliasing
    }
    return hash;
  }

  static inline int Compare(const gmVariable &a_keyA, const gmVariable &a_keyB)
  {
    if(a_keyA.m_type < a_keyB.m_type) return -1;
    if(a_keyA.m_type > a_keyB.m_type) return 1;
    if(a_keyA.m_value.m_int < a_keyB.m_value.m_int) return -1;
    if(a_keyA.m_value.m_int > a_keyB.m_value.m_int) return 1;
    return 0;
  }

};



/// \class gmObject
/// \brief gmObject is the base class for gm reference types.  All gmObject types are allocated from the gmMachine
class gmObject
#if GM_USE_INCGC
  : public gmGCObjBase
#endif //GM_USE_INCGC
{
public:

  inline gmptr GetRef() const { return (gmptr) this; }
  virtual int GetType() const = 0;

#if !GM_USE_INCGC
  /// \brief Only call Mark() if NeedsMark() returns true.
  virtual void Mark(gmMachine * a_machine, gmuint32 a_mark) { if(m_mark != GM_MARK_PERSIST) m_mark = a_mark; }
  inline bool NeedsMark(gmuint32 a_mark) { return ((m_mark != GM_MARK_PERSIST) && (m_mark != a_mark)); }
  virtual void Destruct(gmMachine * a_machine) = 0;

protected:

  gmuint32 m_mark;

private:

  gmObject * m_sysNext;
#endif //!GM_USE_INCGC

protected:

  /// \brief Non-public constructor.  Create via gmMachine.
  inline gmObject() 
#if !GM_USE_INCGC
    : m_mark(GM_MARK_START) 
#endif //!GM_USE_INCGC
  {}
  friend class gmMachine;
};


//
// INLINE IMPLEMENTATION
//


inline void gmVariable::SetString(gmStringObject * a_string)
{
  m_type = GM_STRING;
  m_value.m_ref = ((gmObject *) a_string)->GetRef();
}

inline void gmVariable::SetTable(gmTableObject * a_table)
{
  m_type = GM_TABLE;
  m_value.m_ref = ((gmObject *) a_table)->GetRef();
}

inline void gmVariable::SetFunction(gmFunctionObject * a_function)
{
  m_type = GM_FUNCTION;
  m_value.m_ref = ((gmObject *) a_function)->GetRef();
}

int gmVariable::GetIntSafe() const
{
  if( GM_INT == m_type )
  {
    return m_value.m_int;
  }
  else if( GM_FLOAT == m_type )
  {
    return (int)m_value.m_float;
  }
  return 0;
}

float gmVariable::GetFloatSafe() const
{
  if( GM_FLOAT == m_type )
  {
    return m_value.m_float;
  }
  else if( GM_INT == m_type )
  {
    return (float)m_value.m_int;
  }
  return 0.0f;
 
}

gmStringObject * gmVariable::GetStringObjectSafe() const
{
  if( GM_STRING == m_type )
  {
    return (gmStringObject *)m_value.m_ref;
  }
  return NULL;
}


inline gmTableObject * gmVariable::GetTableObjectSafe() const
{
  if( GM_TABLE == m_type )
  {
    return (gmTableObject *)m_value.m_ref;
  }
  return NULL;
}

inline gmFunctionObject * gmVariable::GetFunctionObjectSafe() const
{
  if( GM_FUNCTION == m_type )
  {
    return (gmFunctionObject *)m_value.m_ref;
  }
  return NULL;
}


#endif // _GMVARIABLE_H_
