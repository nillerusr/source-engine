/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMSTRINGOBJECT_H_
#define _GMSTRINGOBJECT_H_

#include "gmConfig.h"
#include "gmVariable.h"
#include "gmHash.h"

class gmMachine;

/// \class gmStringObject
/// \brief
class gmStringObject : public gmObject, public gmHashNode<const char *, gmStringObject>
{
public:

  inline const char * GetKey() const { return m_string; }

  virtual int GetType() const { return GM_STRING; }
  virtual void Destruct(gmMachine * a_machine);

  inline operator const char *() const { return m_string; }
  inline const char * GetString() const { return m_string; }
  inline int GetLength() const { return m_length; }

protected:

  /// \brief Non-public constructor.  Create via gmMachine.
  gmStringObject(const char * a_string, int a_length) { m_string = a_string; m_length = a_length; }
  friend class gmMachine;

private:

  const char * m_string;
  int m_length;
};

#endif // _GMSTRINGOBJECT_H_
