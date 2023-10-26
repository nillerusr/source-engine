/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmStringLib.h"
#include "gmThread.h"
#include "gmMachine.h"
#include "gmHelpers.h"


// Must be last header
#include "memdbgon.h"


//
// String
//


#define GM_WHITE_SPACE " \t\v\r\n"


static int GM_CDECL gmfStringLeft(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(count, 0);

  const gmVariable * var = a_thread->GetThis();

  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();
  count = gmClamp(0, count, length);

  char * buffer = (char *) alloca(count + 1);
  memcpy(buffer, str, count);
  buffer[count] = 0;

  a_thread->PushNewString(buffer);

  return GM_OK;
}


static int GM_CDECL gmfStringRight(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(count, 0);

  const gmVariable * var = a_thread->GetThis();
  
  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();
  count = gmClamp(0, count, length);

  char * buffer = (char *) alloca(count + 1);
  memcpy(buffer, str + length - count, count);
  buffer[count] = 0;

  a_thread->PushNewString(buffer);

  return GM_OK;
}


static int GM_CDECL gmfStringRightAt(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(index, 0);

  const gmVariable * var = a_thread->GetThis();
  
  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();
  index = gmClamp(0, index, length);
  int count = (length - index);
  char * buffer = (char *) alloca(count + 1);
  memcpy(buffer, str + index, count);
  buffer[count] = 0;

  a_thread->PushNewString(buffer, count);

  return GM_OK;
}


static int GM_CDECL gmfStringMid(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(2);

  int first = 0, count = 0;

  if(!gmGetFloatOrIntParamAsInt(a_thread, 0, first)) {return GM_EXCEPTION;}
  if(!gmGetFloatOrIntParamAsInt(a_thread, 1, count)) {return GM_EXCEPTION;}

  const gmVariable * var = a_thread->GetThis();
  
  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();

  //Check bounds
  if (first < 0)
  {
    first = 0;
  }
  if (count < 0)
  {
    count = 0;
  }
  if (first + count > length)
  {
    count = length - first;
  }
  if (first > length)
  {
    count = 0;
  }

  char * buffer = (char *) alloca(count + 1);
  memcpy(buffer, str + first, count);
  buffer[count] = 0;

  a_thread->PushNewString(buffer);

  return GM_OK;
}


static int GM_CDECL gmfStringLength(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);

  a_thread->PushInt(strObj->GetLength());
  
  return GM_OK;
}


static int GM_CDECL gmfStringIsEmpty(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);

  a_thread->PushInt( (strObj->GetLength() == 0) );
  
  return GM_OK;
}


static int GM_CDECL gmfStringCompare(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  if(a_thread->ParamType(0) == GM_STRING)
  {
    const gmVariable * var = a_thread->GetThis();
  
    GM_ASSERT(var->m_type == GM_STRING);

    gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
    const char* thisStr = (const char *) *strObj;
    const char* otherStr = a_thread->ParamString(0);

    a_thread->PushInt(strcmp(thisStr, otherStr));
  
    return GM_OK;
  }

  return GM_EXCEPTION;
}


static int GM_CDECL gmfStringCompareNoCase(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  if(a_thread->ParamType(0) == GM_STRING)
  {
    const gmVariable * var = a_thread->GetThis();
  
    GM_ASSERT(var->m_type == GM_STRING);

    gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
    const char* thisStr = (const char *) *strObj;
    const char* otherStr = a_thread->ParamString(0);

    a_thread->PushInt(_gmstricmp(thisStr, otherStr));
  
    return GM_OK;
  }

  return GM_EXCEPTION;
}


static int GM_CDECL gmfStringLower(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  
  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();
  char * buffer = (char *) alloca(length + 1);
  memcpy(buffer, str, length + 1);

  strlwr(buffer);

  a_thread->PushNewString(buffer, length);

  return GM_OK;
}


static int GM_CDECL gmfStringUpper(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  
  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  
  int length = strObj->GetLength();
  char * buffer = (char *) alloca(length + 1);
  memcpy(buffer, str, length + 1);
  
  strupr(buffer);

  a_thread->PushNewString(buffer, length);

  return GM_OK;
}


static int GM_CDECL gmfStringSpanIncluding(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  if(a_thread->ParamType(0) == GM_STRING)
  {
    const gmVariable * var = a_thread->GetThis();
  
    GM_ASSERT(var->m_type == GM_STRING);

    gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
    const char * thisStr = (const char *) *strObj;
    const char * otherStr = a_thread->ParamString(0);
    
    int offset = strspn(thisStr, otherStr);
    char * buffer = (char *) alloca(offset + 1);
    memcpy(buffer, thisStr, offset);
    buffer[offset] = 0;

    a_thread->PushNewString(buffer, offset);

    return GM_OK;
  }

  return GM_EXCEPTION;
}


static int GM_CDECL gmfStringSpanExcluding(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  if(a_thread->ParamType(0) == GM_STRING)
  {
    const gmVariable * var = a_thread->GetThis();
  
    GM_ASSERT(var->m_type == GM_STRING);

    gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
    const char * thisStr = (const char *) *strObj;
    const char * otherStr = a_thread->ParamString(0);
    
    int offset = strcspn(thisStr, otherStr);
    char * buffer = (char *) alloca(offset + 1);
    memcpy(buffer, thisStr, offset);
    buffer[offset] = 0;

    a_thread->PushNewString(buffer, offset);

    return GM_OK;
  }

  return GM_EXCEPTION;
}


// string.AppendPath(a_appendString, a_endWithSlash);
// Append this string with another string, fixing for slashes
// a_endWithSlash is optional, default is false, removing trailing slashes.
static int GM_CDECL gmfStringAppendPath(gmThread * a_thread)
{
  //Need at least 1 parameter
  if(a_thread->GetNumParams() < 1)
  {
    return GM_EXCEPTION;
  }

  //Optional trailing slash flag
  int PutTrailingSlash = a_thread->ParamInt(1, false);

  if(a_thread->ParamType(0) == GM_STRING)
  {
    const gmVariable * varA = a_thread->GetThis();
  
    GM_ASSERT(varA->m_type == GM_STRING);

    gmStringObject * strObjA = (gmStringObject *) GM_OBJECT(varA->m_value.m_ref);
    gmStringObject * strObjB = a_thread->ParamStringObject(0);
    const char* cStrA = strObjA->GetString();
    const char* cStrB = strObjB->GetString();
    int lenA = strObjA->GetLength();
    int lenB = strObjB->GetLength();

    int curLength = lenA;
    int appLength = lenB;

    //Alloc buffer on stack is fine, path strings cannot be long
    char * buffer = (char *) alloca(curLength + appLength + 2);

    if(lenA > 0)
    {
      memcpy(buffer, cStrA, lenA);


      //Make sure first part has a slash
      if(buffer[curLength-1] != '\\' && buffer[curLength-1] != '/')
      {
        buffer[curLength] = '\\';
        curLength += 1;
      }
    }

    if(lenB > 0)
    {
      //Remove slash from start of append string
      const char* startOfAppend = cStrB;
      if((startOfAppend[0] == '\\') || (startOfAppend[0] == '/'))
      {
        startOfAppend += 1;
        appLength -= 1;
      }

      //Append the string
      memcpy(&buffer[curLength], startOfAppend, appLength);
    }

    int newLength = curLength + appLength;
  
    if(PutTrailingSlash && (newLength > 0)) //Only add slash if string is not empty
    {
      //Make sure path ends with slash
      if(buffer[newLength-1] != '\\' && buffer[newLength-1] != '/')
      {
        buffer[newLength] = '\\';
        newLength += 1;
      }
    }
    else
    {
      //Make sure path does not end with slash
      if(buffer[newLength-1] == '\\' || buffer[newLength-1] == '/')
      {
        newLength -= 1;
      }
    }

    //Make sure it is terminated
    buffer[newLength] = 0;

    a_thread->PushNewString(buffer, newLength);
  
    return GM_OK;
  }

  return GM_EXCEPTION;
}


// string.RemoveInvalidChars(a_replaceChar, a_invalidSet)
// eg. "File Name#1.tga".RemoveInvalidChars("_","# ") returns "File_Name_1.tga"
// Note: Parameters are optional.
static int GM_CDECL gmfStringReplaceCharsInSet(gmThread * a_thread)
{
  GM_INT_PARAM(repCharInt, 0, '_');
  GM_STRING_PARAM(invalidCharSet, 1, " \\/:-+");

  char repChar = (char)repCharInt; //Convert full int to char
  const gmVariable * varA = a_thread->GetThis();
  
  GM_ASSERT(varA->m_type == GM_STRING);

  gmStringObject * strObjA = (gmStringObject *) GM_OBJECT(varA->m_value.m_ref);
  const char* cStrA = strObjA->GetString();
  int lenA = strObjA->GetLength();

  //Alloc buffer on stack is fine, path strings cannot be long
  char * buffer = (char *) alloca(lenA + 1);
  memcpy(buffer, cStrA, lenA + 1);

  int validPos;

  //Check that replacement char is NOT in invalid set, otherwise endless loop...
  if(strchr(invalidCharSet, repChar))
  {
    return GM_EXCEPTION;
  }

  for(;;)
  {
    validPos = strcspn(buffer, invalidCharSet);
    if(validPos != lenA)
    {
      buffer[validPos] = repChar;
    }
    else
    {
      break;
    }
  }

  a_thread->PushNewString(buffer, lenA);
      
  return GM_OK;
}


// string.AppendPath(a_appendString, a_endWithSlash);
// Append this string with another string, fixing for slashes
// a_endWithSlash is optional, default is false, removing trailing slashes.
static void GM_CDECL gmStringOpAppendPath(gmThread * a_thread, gmVariable * a_operands)
{
  // Both types must be strings
  if(a_operands[0].m_type != GM_STRING ||
     a_operands[1].m_type != GM_STRING)
  {
    a_operands[0].m_type = GM_NULL;
    a_operands[0].m_value.m_ref = 0;
    return;
  }

  gmStringObject * strObjA = (gmStringObject *) GM_OBJECT(a_operands[0].m_value.m_ref);
  gmStringObject * strObjB = (gmStringObject *) GM_OBJECT(a_operands[1].m_value.m_ref);
  const char* cStrA = strObjA->GetString();
  const char* cStrB = strObjB->GetString();
  int lenA = strObjA->GetLength();
  int lenB = strObjB->GetLength();

  int curLength = lenA;
  int appLength = lenB;

  //Alloc buffer on stack is fine, path strings cannot be long
  char * buffer = (char *) alloca(curLength + appLength + 2);

  if(lenA <= 0)
  {
    a_operands[0] = a_operands[1];
  }

  if(lenB <= 0)
  {
    return;
  }

  memcpy(buffer, cStrA, lenA);

  //Make sure first part has a slash
  if(buffer[curLength-1] != '\\' && buffer[curLength-1] != '/')
  {
    buffer[curLength] = '\\';
    curLength += 1;
  }

  //Remove slash from start of append string
  const char* startOfAppend = cStrB;
  if((startOfAppend[0] == '\\') || (startOfAppend[0] == '/'))
  {
    startOfAppend += 1;
    appLength -= 1;
  }

  //Append the string
  memcpy(&buffer[curLength], startOfAppend, appLength);

  int newLength = curLength + appLength;

  //Make sure it is terminated
  buffer[newLength] = 0;

  a_operands[0].m_type = GM_STRING;
  a_operands[0].m_value.m_ref = a_thread->GetMachine()->AllocStringObject(buffer, newLength)->GetRef();
}


// int string.Find(char/string a_charOrStringToFind, int a_startOffset == 0);
// Find a character or character string in a string.
// Returns character offset or -1 if not found.
static int GM_CDECL gmStringFind(gmThread * a_thread)
{
  int numParams = GM_THREAD_ARG->GetNumParams();
  int startOffset = 0;
  char* retCharPtr = NULL;
  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char* thisStr = (const char *) *strObj;
  
  if(numParams == 2)
  {
    if(a_thread->ParamType(1) == GM_INT)
    {
      startOffset = a_thread->ParamInt(1); //Optional start offset param
    }
    else
    {
      return GM_EXCEPTION;
    }
  } 
  else if(numParams < 1 || numParams > 2)
  {
    return GM_EXCEPTION;
  }

  //Check if this string is empty, or start offset out of range
  if( (strObj->GetLength() == 0) || 
      (startOffset > strObj->GetLength()) ||
      (startOffset < 0) 
    )
  {
    a_thread->PushInt( -1 ); //return Not Found
    return GM_OK;
  }

  if(a_thread->ParamType(0) == GM_INT)
  {
    const char otherChar = (char)a_thread->ParamInt(0);
    
    //Find character
    retCharPtr = (char*)strchr(thisStr + startOffset, otherChar);
  }
  else if(a_thread->ParamType(0) == GM_STRING)
  {
    const char* otherStr = a_thread->ParamString(0);

    //Find string
    retCharPtr = (char*)strstr(thisStr + startOffset, otherStr);
  }
  else
  {
    return GM_EXCEPTION;
  }

  // return -1 for not found, distance from beginning otherwise
  int retOffset = (retCharPtr == NULL) ? -1 : (int)(retCharPtr - thisStr);
  a_thread->PushInt(retOffset);

  return GM_OK;
}


static int GM_CDECL gmStringReverse(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;

  int len = strlen(str);
  if(len > 0)
  {
    char * buffer = (char *) alloca(len + 1); 
    memcpy(buffer, str, len + 1); //Copy old string

    while(len--)
    {
      buffer[len] = *(str++);
    }

    a_thread->PushNewString(buffer);
  }
  return GM_OK;
}


// int string.ReverseFind(char/string a_charOrStringToFind);
// Find the last instance of a specific character in a string.
// Returns character offset or -1 if not found.
static int GM_CDECL gmStringReverseFind(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  const char* retCharPtr = NULL;
  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * thisStrObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char* thisStr = (const char *) *thisStrObj;
  
  if(a_thread->ParamType(0) == GM_INT)
  {
    const char otherChar = (char)a_thread->ParamInt(0);
    
    //Find character
    retCharPtr = strrchr(thisStr, otherChar);
  }
  else if(a_thread->ParamType(0) == GM_STRING)
  {
    gmStringObject * otherStrObj = a_thread->ParamStringObject(0);
    const char* otherStr = a_thread->ParamString(0);

    //Find string
    const char* lastFoundPtr = NULL;
    const char* newTestPtr = NULL;
    const char* curTestPtr = thisStr;
    const char* endThisStr = thisStr + thisStrObj->GetLength();
    int searchStrLength = otherStrObj->GetLength();

    //Search through string for last occurence
    //Not very efficient, but very rarely used.
    for(;;)
    {
      newTestPtr = strstr(curTestPtr, otherStr);
      if(!newTestPtr)
      {
        break;
      }
      lastFoundPtr = newTestPtr;
      curTestPtr = newTestPtr + searchStrLength;
      if(curTestPtr > endThisStr)
      {
        break;
      }
    };

    retCharPtr = lastFoundPtr;
  }
  else
  {
    return GM_EXCEPTION;
  }

  // return -1 for not found, distance from beginning otherwise
  int retOffset = (retCharPtr == NULL) ? -1 : (int)(retCharPtr - thisStr);
  a_thread->PushInt(retOffset);

  return GM_OK;
}


// int string[int index]
// Note: Because strings are constant and in a table, it is impossible to implement SetInd.
static void GM_CDECL gmStringOpGetInd(gmThread * a_thread, gmVariable * a_operands)
{
  if( a_operands[0].m_type != GM_STRING ||
      a_operands[1].m_type != GM_INT )
  {
    a_operands->Nullify();
    return;
  }

  gmStringObject * strObjA = (gmStringObject *) GM_OBJECT(a_operands[0].m_value.m_ref);
  const char* cStrA = strObjA->GetString();
  int index = a_operands[1].m_value.m_int;
  
  if( index < 0 || index > strObjA->GetLength()-1 )
  {
    a_operands->Nullify();
  }
  else
  {
    a_operands->SetInt( (int)cStrA[index] );
  }
}

// int string.GetAt(int a_index);
// Returns character at offset or null if index out of range.
static int GM_CDECL gmStringGetAt(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(index, 0);

  const gmVariable * var = a_thread->GetThis();

  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;

  if(index < 0 || index >= strObj->GetLength())
  {
    a_thread->PushNull(); //Return null if index out of range
    return GM_OK;
  }

  a_thread->PushInt(str[index]);
  return GM_OK;
}


// string string.SetAt(int a_index, int a_char);
// Returns string with modified character at offset, or original string if index out of range.
static int GM_CDECL gmStringSetAt(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(index, 0);
  GM_CHECK_INT_PARAM(newChar, 1);

  const gmVariable * var = a_thread->GetThis();

  GM_ASSERT(var->m_type == GM_STRING);

  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();

  if(index < 0 || index >= strLength)
  {
    a_thread->PushString(strObj); //Return original string if index out of range
    return GM_OK;
  }

  char * buffer = (char *) alloca(strLength + 1); 
  memcpy(buffer, str, strLength + 1); //Copy old string
  buffer[index] = (char)newChar; //Set character in string

  a_thread->PushNewString(buffer, strLength);
  return GM_OK;
}


static int GM_CDECL gmStringTrimLeft(gmThread * a_thread)
{
  GM_STRING_PARAM(trim, 0, GM_WHITE_SPACE);

  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  if(strlen(str) > 0)
  {
    while(*str && strchr(trim, *str))
      ++str;
    a_thread->PushNewString(str);
  }
  else
    a_thread->PushString(strObj);
  return GM_OK;
}


static int GM_CDECL gmStringTrimRight(gmThread * a_thread)
{
  GM_STRING_PARAM(trim, 0, GM_WHITE_SPACE);

  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();
  if(strLength > 0)
  {
    char * buffer = (char *) alloca(strLength + 1);
    memcpy(buffer, str, strLength + 1); //Copy old string

    // Find beginning of trailing matches by starting at end
    char *lpsz = buffer + strLength;
    while (--lpsz >= buffer && strchr(trim, *lpsz) != NULL) {}
    ++lpsz;
    *lpsz = '\0';

    a_thread->PushNewString(buffer);
  }
  else
  {
    a_thread->PushString(strObj);
  }
  return GM_OK;
}


static int GM_CDECL gmStringGetFilenameNoExt(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();

  char * buffer = (char *) alloca(strLength + 1);
  memcpy(buffer, str, strLength + 1); //Copy old string

  char *lpsz = buffer + strLength;
  while (--lpsz >= buffer && *lpsz != '\\' && *lpsz != '/') {}

  buffer = ++lpsz;
  strLength = strlen(buffer);
  lpsz = buffer + strLength;
  while (--lpsz >= buffer && *lpsz != '.') {}
  if(*lpsz == '.') *lpsz = 0;

  a_thread->PushNewString(buffer);
  return GM_OK;
}


static int GM_CDECL gmStringGetFilename(gmThread * a_thread)
{
  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();

  const char *lpsz = str + strLength;
  while (--lpsz >= str && *lpsz != '\\' && *lpsz != '/') {}
  ++lpsz;

  a_thread->PushNewString(lpsz);
  return GM_OK;
}


static int GM_CDECL gmStringGetExtension(gmThread * a_thread)
{
  GM_INT_PARAM(keepDot, 0, 0);

  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();

  const char *lpsz = str + strLength;
  while (--lpsz >= str && *lpsz != '.') {}

  if(*lpsz == '.')
  {
    if(!keepDot)
    {
      ++lpsz;
    }
    a_thread->PushNewString(lpsz);
  }
  else
  {
    a_thread->PushNewString("");
  }
  return GM_OK;
}


static int GM_CDECL gmStringSetExtension(gmThread * a_thread)
{
  GM_STRING_PARAM(newExt, 0, "");

  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);

  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();
  int extLength = strlen(newExt);

  if (extLength && newExt[0] == '.')
  {
    ++newExt;
    extLength = strlen(newExt);
  }

  char *buffer = (char *) alloca(strLength + 1 + extLength);
  memcpy(buffer, str, strLength + 1);

  char *lpsz = buffer + strLength;
  while (--lpsz >= buffer && *lpsz != '.') {}

  if(*lpsz == '.')
  {
    *lpsz = '\0';
    if (extLength)
      sprintf(buffer, "%s.%s", buffer, newExt);

  }
  else if (extLength)
  {
    sprintf(buffer, "%s.%s", buffer, newExt);
  }

  a_thread->PushNewString(buffer);
  return GM_OK;
}


static int GM_CDECL gmStringGetPath(gmThread * a_thread)
{
  GM_INT_PARAM(keepSlash, 0, 0);

  const gmVariable * var = a_thread->GetThis();
  GM_ASSERT(var->m_type == GM_STRING);
  gmStringObject * strObj = (gmStringObject *) GM_OBJECT(var->m_value.m_ref);
  const char * str = (const char *) *strObj;
  int strLength = strObj->GetLength();

  char * buffer = (char *) alloca(strLength + 1); 
  memcpy(buffer, str, strLength + 1); //Copy old string

  char *lpsz = buffer + strLength;
  while (--lpsz >= buffer && *lpsz != '\\' && *lpsz != '/') {}

  if(*lpsz == '\\' || *lpsz == '/')
  {
    if(keepSlash)
      lpsz[1] = 0;
    else
      lpsz[0] = 0;
    a_thread->PushNewString(buffer);
  }
  else
  {
    a_thread->PushNewString("");
  }
  return GM_OK;
}

extern int GM_CDECL gmfToInt(gmThread * a_thread);
extern int GM_CDECL gmfToFloat(gmThread * a_thread);
extern int GM_CDECL gmfToString(gmThread * a_thread);

static gmFunctionEntry s_stringLib[] = 
{ 
  /*gm
    \lib string
    \brief string operations often store a copy of the string on the stack, so keep string sizes reasonable
  */
  /*gm
    \function IsEmpty
    \brief IsEmpty will test to see if the string is 0 length
    \return non-zero if the string is empty
  */
  {"IsEmpty", gmfStringIsEmpty},
  /*gm
    \function Length
    \brief Length will return the length of the string not including the null terminating character
    \return int length
  */
  {"Length", gmfStringLength},
  /*gm
    \function Left
    \brief Left will return the left count charaters of the string
    \param int count
    \return string
  */
  {"Left", gmfStringLeft},
  /*gm
    \function Right
    \brief Right will return the right count charaters of the string
    \param int count
    \return string
  */
  {"Right", gmfStringRight},
  /*gm
    \function RightAt
    \brief RightAt will return the charaters right of and including the given index
    \param int index
    \return string
  */
  {"RightAt", gmfStringRightAt},
  /*gm
    \function Mid
    \brief Mid will return count characters from the start index
    \param int startIndex
    \param int count
    \return string
  */
  {"Mid", gmfStringMid},
  /*gm
    \function Compare
    \brief Compare will perform a string compare
    \param string to compare
    \return -1 if the this < compare string, 0 if the strings are equal, 1 otherwise
  */
  {"Compare", gmfStringCompare},
  /*gm
    \function CompareNoCase
    \brief CompareNoCase will perform a string compare (case insensitive)
    \param string to compare
    \return -1 if the this < compare string, 0 if the strings are equal, 1 otherwise
  */
  {"CompareNoCase", gmfStringCompareNoCase},
  /*gm
    \function Int
    \brief Int will return the int value of the string
    \return int value
  */
  {"Int", gmfToInt},
  /*gm
    \function Float
    \brief Float will return the float value of the string
    \return float value
  */
  {"Float", gmfToFloat},
  /*gm
    \function String
    \return string
  */
  {"String", gmfToString},
  /*gm
    \function Upper
    \brief Upper will return the string as uppercase
    \return string
  */
  {"Upper", gmfStringUpper},
  /*gm
    \function Lower
    \brief Lower will return the string as lowercase
    \return string
  */
  {"Lower", gmfStringLower},
  /*gm
    \function SpanIncluding
    \brief SpanIncluding will return this string while characters are within the passed string
    \param string charset
    \return string
  */
  {"SpanIncluding", gmfStringSpanIncluding},
  /*gm
    \function SpanExcluding
    \brief SpanExcluding will return this string while characters are not within the passed string
    \param string charset
    \return string
  */
  {"SpanExcluding", gmfStringSpanExcluding},
  /*gm
    \function AppendPath
    \brief AppendPath will append a path make sure one '\' is maintained
    \param string path to append
    \return string
  */
  {"AppendPath", gmfStringAppendPath},
  /*gm
    \function ReplaceCharsInSet
    \brief ReplaceCharsInSet will replace all chars in this that are within the charset with the given int char
    \param int char to replace with
    \param string charset of chars to replace
    \return string
  */
  {"ReplaceCharsInSet", gmfStringReplaceCharsInSet},
  /*gm
    \function Find
    \brief Find will find the first occurance of the passed string within this string
    \param string search string
    \param int start index optional (0)
    \return int index of first occurance, or -1 if the string was not found
  */
  {"Find", gmStringFind},
  /*gm
    \function Reverse
    \brief Reverse characters of a string
    \return string
  */
  {"Reverse", gmStringReverse},
  /*gm
    \function ReverseFind
    \brief ReverseFind will find the first occurance of the passed string within this string starting from the right
    \param string search string
    \return int index of first occurance, or -1 if the string was not found
  */
  {"ReverseFind", gmStringReverseFind},
  /*gm
    \function GetAt
    \brief GetAt will return the char at the given index
    \param int index
    \return int char, or null if index was out of range
  */
  {"GetAt", gmStringGetAt},
  /*gm
    \function SetAt
    \brief SetAt will return the string with the character set at the given position
    \param int index
    \param int char
    \return string
  */
  {"SetAt", gmStringSetAt},
  /*gm
    \function TrimLeft
    \brief TrimLeft will return the string with the chars from the passed char set trimmed from the left
    \param string charset optional (" \r\n\v\t")
    \return string
  */
  {"TrimLeft", gmStringTrimLeft},
  /*gm
    \function TrimRight
    \brief TrimRight will return the string with the chars from the passed char set trimmed from the right
    \param string charset optional (" \r\n\v\t")
    \return string
  */
  {"TrimRight", gmStringTrimRight},

  /*gm
    \function GetFilenameNoExt
    \brief GetFilenameNoExt will return the filename part of a path string
    \return string
  */
  {"GetFilenameNoExt", gmStringGetFilenameNoExt},
  /*gm
    \function GetFilename
    \brief GetFilename will return the filename part of a path string incl. extension
    \return string
  */
  {"GetFilename", gmStringGetFilename},
  /*gm
    \function GetExtension
    \brief GetExtension will return the file extension
    \param int inclDot optional (0) 1 will include '.', 0 won't
    \return string
  */
  {"GetExtension", gmStringGetExtension},
  /*gm
    \function SetExtension
    \brief SetExtension returns a string with the extension change to the given one.
    \param string ext optional (null) the new extension, with or without the dot. null to remove extension.
    \return string
  */
  {"SetExtension", gmStringSetExtension},
  /*gm
    \function GetPath
    \brief GetPath will return the file path from a path string
    \param int inclSlash optional (0) will include a '\' on the end of the path
    \return string
  */
  {"GetPath", gmStringGetPath},
/*
  {"Insert", }, //int Insert(int index, char/string)
  {"Delete", }, //int Delete(int index, int count=1)
  {"Replace", }, //int Replace(char/string,char/string)
  {"Scan", },
*/
};

void gmBindStringLib(gmMachine * a_machine)
{
  a_machine->RegisterTypeOperator(GM_STRING, O_BIT_XOR, NULL, gmStringOpAppendPath);
  a_machine->RegisterTypeOperator(GM_STRING, O_GETIND, NULL, gmStringOpGetInd);
  a_machine->RegisterTypeLibrary(GM_STRING, s_stringLib, sizeof(s_stringLib) / sizeof(s_stringLib[0]));
}

