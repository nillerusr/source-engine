/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmVariable.h"
#include "gmThread.h"
#include "gmStringObject.h"
#include "gmUserObject.h"

// Must be last header
#include "memdbgon.h"


// Init statics and constants
gmVariable gmVariable::s_null = gmVariable(GM_NULL, 0);


const char * gmVariable::AsString(gmMachine * a_machine, char * a_buffer, int a_len) const
{
  switch(m_type)
  {
    case GM_NULL : 
      _gmsnprintf(a_buffer, a_len, "null");
      break;
    case GM_INT :
      _gmsnprintf(a_buffer, a_len, "%d", m_value.m_int);
      break;
    case GM_FLOAT :
      _gmsnprintf(a_buffer, a_len, "%g", m_value.m_float);
      break;
    case GM_STRING :
      return ((gmStringObject *) GM_MOBJECT(a_machine, m_value.m_ref))->GetString();
    default:
      gmAsStringCallback asStringCallback = a_machine->GetUserAsStringCallback(m_type);
      if(asStringCallback)
      {
        asStringCallback((gmUserObject *) GM_MOBJECT(a_machine, m_value.m_ref), a_buffer, a_len );
      }
      else
      {
        _gmsnprintf(a_buffer, a_len, "%s:0x%x", a_machine->GetTypeName(m_type), m_value.m_ref);
      }
      break;
  }
  return a_buffer;
}


const char * gmVariable::AsStringWithType(gmMachine * a_machine, char * a_buffer, int a_len) const
{
  // Copy the type first
  _gmsnprintf(a_buffer, a_len, "%s: ", a_machine->GetTypeName(m_type));

  // Update for used portion
  int usedLen = strlen(a_buffer);
  char* newBufferPos = a_buffer + usedLen;
  int newLen = a_len - usedLen;

  if(newLen > 0) //Paranoid check some string buffer remaining
  {
    const char * str = AsString(a_machine, newBufferPos, newLen);
    if(str != newBufferPos)
    {
      // copy what we can... this is used for debug purposes so it doesnt matter if we chop some off
      V_strncpy(newBufferPos, str, newLen);
    }
  }

  return a_buffer;
}


void gmVariable::SetUser(gmUserObject * a_object)
{
  m_type = (gmType) a_object->m_userType;
  m_value.m_ref = ((gmObject *) a_object)->GetRef();
}


void gmVariable::SetUser(gmMachine * a_machine, void * a_userPtr, int a_userType)
{
  SetUser( a_machine->AllocUserObject(a_userPtr, a_userType) );
}


void gmVariable::SetString(gmMachine * a_machine, const char * a_cString)
{
  SetString( a_machine->AllocStringObject(a_cString) );
}


const char * gmVariable::GetCStringSafe() const
{
  if( m_type == GM_STRING )
  {
    return ((gmStringObject *)m_value.m_ref)->GetString();
  }
  return "";
}


void * gmVariable::GetUserSafe(int a_userType) const
{
  if( m_type == a_userType )
  {
    return ((gmUserObject *)m_value.m_ref)->m_user;
  }
  return NULL;
}

gmUserObject* gmVariable::GetUserObjectSafe(int a_userType) const
{
	if( m_type == a_userType )
	{
		return ((gmUserObject *)m_value.m_ref);
	}
	return NULL;
}

