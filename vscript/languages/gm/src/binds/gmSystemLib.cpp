/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmSystemLib.h"
#include "gmThread.h"
#include "gmMachine.h"
#include "gmHelpers.h"

#if GM_SYSTEM_LIB

#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#undef GetObject

//
//
// system functions
//
//

extern void gmConcat(gmMachine * a_machine, char * &a_dst, int &a_len, int &a_size, const char * a_src, int a_growBy = 32);


static int GM_CDECL gmfSystem(gmThread * a_thread)
{
  const int bufferSize = 256;
  int len = 0, size = 0, i, ret = -1;
  char * str = NULL, buffer[bufferSize];

  // build the string
  for(i = 0; i < a_thread->GetNumParams(); ++i)
  {
    gmConcat(a_thread->GetMachine(), str, len, size, a_thread->Param(i).AsString(a_thread->GetMachine(), buffer, bufferSize), 64);

    if(str)
    {
      GM_ASSERT(len < size);
      str[len++] = ' ';
      str[len] = '\0';
    }
  }

  // print the string
  if(str)
  {
    ret = system(str);
    a_thread->GetMachine()->Sys_Free(str);
  }

  a_thread->PushInt(ret);

  return GM_OK;
}

static int GM_CDECL gmfDoFile(gmThread * a_thread) // filename, now (1), return thread id, null on error, exception on compile error.
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);
  GM_INT_PARAM(now, 1, 1);
  gmVariable paramThis = a_thread->Param(2, gmVariable::s_null); // 3rd param is 'this'

  int id = GM_INVALID_THREAD;
  if(filename)
  {
    char * string = NULL;

    FILE * fp = fopen(filename, "rb");
    if(fp)
    {
      fseek(fp, 0, SEEK_END);
      int size = ftell(fp);
      rewind(fp);
      string = new char[size + 1];
      fread(string, 1, size, fp);
      string[size] = 0;
      fclose(fp);
    }
    else
    {
      GM_EXCEPTION_MSG("failed to open file '%s'", filename);
      return GM_EXCEPTION;
    }
    if(string == NULL) return GM_OK;

    int errors = a_thread->GetMachine()->ExecuteString(string, &id, (now) ? true : false, filename, &paramThis);
    delete[] string;
    if(errors)
    {
      return GM_EXCEPTION;
    }
    else
    {
      a_thread->PushInt(id);
    }
  }
  return GM_OK;
}

//
//
// Implementation of ansi file binding
//
//

static gmType s_gmFileType = GM_NULL;

static int GM_CDECL gmfFile(gmThread * a_thread)
{
  a_thread->PushNewUser(NULL, s_gmFileType);
  return GM_OK;
}

static int GM_CDECL gmfFileExists(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);

  FILE * fp = fopen(filename, "rb");
  if(fp)
  {
    a_thread->PushInt(1);
    fclose(fp);
    return GM_OK;
  }
  a_thread->PushInt(0);
  return GM_OK;
}

static int GM_CDECL gmfFileOpen(gmThread * a_thread) // path, readonly(true), return 1 on success.
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);
  GM_INT_PARAM(readonly, 1, 1);

  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user) fclose((FILE *) fileObject->m_user);
  fileObject->m_user = (void *) fopen(filename, (readonly) ? "rb" : "wb");
  if(fileObject->m_user) a_thread->PushInt(1);
  else a_thread->PushInt(0);
  return GM_OK;
}

static int GM_CDECL gmfFileOpenText(gmThread * a_thread) // path, readonly(true), return 1 on success.
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);
  GM_INT_PARAM(readonly, 1, 1);

  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user) fclose((FILE *) fileObject->m_user);
  fileObject->m_user = (void *) fopen(filename, (readonly) ? "r" : "w");
  if(fileObject->m_user) a_thread->PushInt(1);
  else a_thread->PushInt(0);
  return GM_OK;
}

static int GM_CDECL gmfFileClose(gmThread * a_thread)
{
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user) fclose((FILE *) fileObject->m_user);
  fileObject->m_user = NULL;
  return GM_OK;
}

static int GM_CDECL gmfFileIsOpen(gmThread * a_thread) // return 1 if open, else 0
{
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  a_thread->PushInt((fileObject->m_user) ? 1 : 0);
  return GM_OK;
}


static void GM_CDECL gmFileOpGetDot(gmThread * a_thread, gmVariable * a_operands)
{
  gmUserObject * user = (gmUserObject *) GM_OBJECT(a_operands->m_value.m_ref);
  if(user && user->m_user)
  {
    gmStringObject * member = (gmStringObject *) GM_OBJECT(a_operands[1].m_value.m_ref);

    GM_ASSERT(sizeof(gmptr) == sizeof(time_t));

    if(strcmp(member->GetString(), "SEEK_CUR") == 0)
      a_operands->SetInt(SEEK_CUR);
    else if(strcmp(member->GetString(), "SEEK_END") == 0)
      a_operands->SetInt(SEEK_END);
    else if(strcmp(member->GetString(), "SEEK_SET") == 0)
      a_operands->SetInt(SEEK_SET);
    else
    {
      a_operands->Nullify();
      return;
    }
    return;
  }
  a_operands->Nullify();
}


static int GM_CDECL gmfFileSeek(gmThread * a_thread) // return false on error
{
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);

  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_INT_PARAM(offset, 0);
  GM_CHECK_INT_PARAM(origin, 1);

  if(    origin != SEEK_CUR 
      && origin != SEEK_END 
      && origin != SEEK_SET )
  {
    return GM_EXCEPTION;
  }

  int result = fseek((FILE*)fileObject->m_user, offset, origin);
  if(result != 0)
  {
    a_thread->PushInt(false);
  }
  a_thread->PushInt(true);

  return GM_OK;
}

static int GM_CDECL gmfFileTell(gmThread * a_thread) // return -1 on error, else file pos.
{
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  long pos = -1;
  if(fileObject->m_user) pos = ftell((FILE *) fileObject->m_user);
  a_thread->PushInt(pos);
  return GM_OK;
}

static int GM_CDECL gmfFileReadLine(gmThread * a_thread) // flag keep \n (0), return string, or null on eof
{
  GM_INT_PARAM(keepLF, 0, 0);
  const int len = GM_SYSTEM_LIB_MAX_LINE;
  char buffer[len];
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user)
  {
    char * str = fgets(buffer, len, (FILE *) fileObject->m_user);
    if(str)
    {
      int slen = strlen(str);
      if(!keepLF)
      {
        if(!feof((FILE *) fileObject->m_user))
          str[--slen] = '\0';
      }
      a_thread->PushNewString(str, slen);
    }
  }
  return GM_OK;
}

static int GM_CDECL gmfFileReadChar(gmThread * a_thread) // return int, return NULL on eof, or on error
{
  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user)
  {
    int c = fgetc((FILE*) fileObject->m_user);
    if(c != EOF) a_thread->PushInt(c);
  }
  return GM_OK;
}

static int GM_CDECL gmfFileWriteChar(gmThread * a_thread) // int, return char written, or NULL on error
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(c, 0);

  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user)
  {
    int r = fputc(c, (FILE *) fileObject->m_user);
    if(r != EOF) a_thread->PushInt(r);
  }
  return GM_OK;
}

static int GM_CDECL gmfFileWriteString(gmThread * a_thread) // string, return 1 on success, or NULL on error
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(s, 0);

  gmUserObject * fileObject = a_thread->ThisUserObject();
  GM_ASSERT(fileObject->m_userType == s_gmFileType);
  if(fileObject->m_user)
  {
    if(fputs(s, (FILE *) fileObject->m_user) != EOF) a_thread->PushInt(1);
  }
  return GM_OK;
}

#if GM_USE_INCGC
static void GM_CDECL gmGCDestructFileUserType(gmMachine * a_machine, gmUserObject* a_object)
{
  if(a_object->m_user) fclose((FILE *) a_object->m_user);
  a_object->m_user = NULL;
}
#else //GM_USE_INCGC
static void GM_CDECL gmGCFileUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  if(a_object->m_user) fclose((FILE *) a_object->m_user);
  a_object->m_user = NULL;
}
#endif //GM_USE_INCGC

//
//
// File Find user type
//
//

static gmType s_gmFileFindType = GM_NULL;

struct gmFileFindUser
{
  WIN32_FIND_DATA m_findData;
  HANDLE m_iterator;
};

//
//
// File Info user type
//
//

static gmType s_gmFileInfoType = GM_NULL;

struct gmFileInfoUser
{
  time_t m_accessedTime; // last access time
  time_t m_creationTime; // creation time
  time_t m_modifiedTime; // last modify time
  unsigned int m_size; // size
};

//
//
// System lib
//
//

static int GM_CDECL gmfFindFirstFile(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);

  gmFileFindUser * fileFind = (gmFileFindUser *) a_thread->GetMachine()->Sys_Alloc(sizeof(gmFileFindUser));
  fileFind->m_iterator = FindFirstFile(filename, &fileFind->m_findData);

  if(fileFind->m_iterator == INVALID_HANDLE_VALUE)
  {
    a_thread->GetMachine()->Sys_Free(fileFind);
    return GM_OK;
  }

  a_thread->PushNewUser(fileFind, s_gmFileFindType);
  return GM_OK;
}


static int GM_CDECL gmfFindNextFile(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);

  if(a_thread->ParamType(0) == s_gmFileFindType)
  {
    gmFileFindUser * fileFind = (gmFileFindUser *) a_thread->ParamUser(0);
    if(fileFind && fileFind->m_iterator != INVALID_HANDLE_VALUE)
    {
      if(FindNextFile(fileFind->m_iterator, &fileFind->m_findData))
      {
        a_thread->PushUser(a_thread->ParamUserObject(0));
      }
    }
  }
  return GM_OK;
}


static int GM_CDECL gmfFileInfo(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(filename, 0);
  
  struct _stat buf;
  int fh, result;

  if((fh = _open(filename, _O_RDONLY)) ==  -1) return GM_OK; // return null
  result = _fstat(fh, &buf); // Get data associated with "fh"
  if(result == 0) //function obtained data correctly (0 == success, -1 == fail)
  {
    // create and push a gmFileInfoUser object
    gmFileInfoUser * fileInfo = (gmFileInfoUser *) a_thread->GetMachine()->Sys_Alloc(sizeof(gmFileInfoUser));
    fileInfo->m_creationTime = buf.st_ctime;
    fileInfo->m_accessedTime = buf.st_atime;
    fileInfo->m_modifiedTime = buf.st_mtime;
    fileInfo->m_size = buf.st_size;
    a_thread->PushNewUser(fileInfo, s_gmFileInfoType);
  }
  _close( fh );
  return GM_OK;
}


static int GM_CDECL gmfCreateFolder(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(path, 0);
  BOOL result = CreateDirectory(path, NULL);
  if(result)
  {
    a_thread->PushInt(1);
  }
  else
  {
    WIN32_FIND_DATA findData;
    HANDLE handle = FindFirstFile(path, &findData);
    if(handle == INVALID_HANDLE_VALUE)
    {
      a_thread->PushInt(0);
    }
    else
    {
      if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
        a_thread->PushInt(2);
      }
      else
      {
        a_thread->PushInt(0);
      }
      FindClose(handle);
    }
  }
  return GM_OK;
}


bool RecurseDeletePath(const char * a_path)
{
  WIN32_FIND_DATA findData;

  char path[MAX_PATH] = "";
  strcpy(path,a_path);

  // remove trailing '\' char
  int last = strlen(path) - 1;
  if(path[last] == '\\')
  {
    path[last] = '\0';
  }

  // is path valid
  HANDLE h = FindFirstFile(path,&findData);

  // path not could not be found OR path is a file, not a folder
  if((h == INVALID_HANDLE_VALUE) || (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
  {
    return false;
  }

  FindClose(h);
  h = NULL;

  // push current working directory
  char currDir[MAX_PATH + 1] = "";
  GetCurrentDirectory(MAX_PATH,currDir);
  SetCurrentDirectory(path);

  // iterate over contents of folder
  h = FindFirstFile("*",&findData);

  if(h != INVALID_HANDLE_VALUE)
  {
    for(;;)
    {
      if(strcmp(findData.cFileName,".") != 0 && strcmp(findData.cFileName,"..") != 0)
      {
        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          RecurseDeletePath(findData.cFileName);
        }
        else
        {
          DWORD attrs = GetFileAttributes(findData.cFileName);
          if(attrs & FILE_ATTRIBUTE_READONLY)
          {
            SetFileAttributes(findData.cFileName,attrs ^ FILE_ATTRIBUTE_READONLY);
          }
          if(!DeleteFile(findData.cFileName))
          {
            DWORD res = GetLastError();
            printf("\nDeleteFile() returned '%d'..\n",(int)res);
          }
        }
      }

      if(!FindNextFile(h,&findData)) break;
    }
  }

  // pop current working directory
  SetCurrentDirectory(currDir);

  FindClose(h);
  h = NULL;

  // remove this directory
  DWORD attrs = GetFileAttributes(path);
  if(attrs & FILE_ATTRIBUTE_READONLY)
  {
    SetFileAttributes(path,attrs ^ FILE_ATTRIBUTE_READONLY);
  }
  return RemoveDirectory(path) != 0;
}


static int GM_CDECL gmfDeleteFolder(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_STRING_PARAM(path, 0);
  GM_INT_PARAM(removeSubFolders, 1, 0);

  if(removeSubFolders)
  {
    a_thread->PushInt(RecurseDeletePath(path) ? 1 : 0);
  }
  else
  {
    a_thread->PushInt(RemoveDirectory(path) ? 1 : 0);
  }
  return GM_OK;
}


static int GM_CDECL gmfTime(gmThread * a_thread)
{
  time_t t;
  time(&t);
  GM_ASSERT(sizeof(time_t) == sizeof(gmptr));
  a_thread->PushInt((gmptr) t);
  return GM_OK;
}


static int GM_CDECL gmfFormatTime(gmThread * a_thread)
{
  GM_INT_PARAM(t, 0, -1);
  GM_STRING_PARAM(format, 1, "%A %d %B %Y, %I:%M:%S %p");
  char buffer[256];

  if(t == -1)
  {
    time_t lt;
    time(&lt);
    t = (int) lt;
  }
  struct tm * ct = localtime((time_t *) &t);
  strftime(buffer, 256, format, ct);
  a_thread->PushNewString(buffer);
  return GM_OK;
}


static int GM_CDECL gmfFileFindGetAttribute(gmThread * a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(attribute, 0);

  GM_ASSERT(a_thread->GetThis()->m_type == s_gmFileFindType);
  gmFileFindUser * fileFind = (gmFileFindUser *) a_thread->ThisUser();

  DWORD attr = 0;
  if(attribute == 'r')
  {
    attr = FILE_ATTRIBUTE_READONLY;
  }
  else if(attribute == 'a')
  {
    attr = FILE_ATTRIBUTE_ARCHIVE;
  }
  else if(attribute == 's')
  {
    attr = FILE_ATTRIBUTE_SYSTEM;
  }
  else if(attribute == 'h')
  {
    attr = FILE_ATTRIBUTE_HIDDEN;
  }
  else if(attribute == 'd')
  {
    attr = FILE_ATTRIBUTE_DIRECTORY;
  }
  else if(attribute == 'c')
  {
    attr = FILE_ATTRIBUTE_COMPRESSED;
  }

  a_thread->PushInt((fileFind->m_findData.dwFileAttributes & attr) ? 1 : 0);
  return GM_OK;
}


static void GM_CDECL gmFileFindOpGetDot(gmThread * a_thread, gmVariable * a_operands)
{
  gmUserObject * user = (gmUserObject *) GM_OBJECT(a_operands->m_value.m_ref);
  if(user && user->m_user)
  {
    gmFileFindUser * fileFind = (gmFileFindUser *) user->m_user;
    gmStringObject * member = (gmStringObject *) GM_OBJECT(a_operands[1].m_value.m_ref);

    if(strcmp(member->GetString(), "filename") == 0)
    {
      a_operands->SetString(a_thread->GetMachine()->AllocStringObject(fileFind->m_findData.cFileName));
      return;
    }
    else if(strcmp(member->GetString(), "size") == 0)
    {
      a_operands->SetInt(fileFind->m_findData.nFileSizeLow);
      return;
    }
  }
  a_operands->Nullify();
}

#if GM_USE_INCGC
static void GM_CDECL gmGCDestructFileFindUserType(gmMachine * a_machine, gmUserObject* a_object)
{
  if(a_object->m_user)
  {
    gmFileFindUser * fileFind = (gmFileFindUser *) a_object->m_user;
    if(fileFind->m_iterator != INVALID_HANDLE_VALUE)
    {
      FindClose(fileFind->m_iterator);
    }
    a_machine->Sys_Free(a_object->m_user);
  }
  a_object->m_user = NULL;
}
#else //GM_USE_INCGC
static void GM_CDECL gmGCFileFindUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  if(a_object->m_user)
  {
    gmFileFindUser * fileFind = (gmFileFindUser *) a_object->m_user;
    if(fileFind->m_iterator != INVALID_HANDLE_VALUE)
    {
      FindClose(fileFind->m_iterator);
    }
    a_machine->Sys_Free(a_object->m_user);
  }
  a_object->m_user = NULL;
}
#endif //GM_USE_INCGC


static void GM_CDECL gmFileInfoOpGetDot(gmThread * a_thread, gmVariable * a_operands)
{
  gmUserObject * user = (gmUserObject *) GM_OBJECT(a_operands->m_value.m_ref);
  if(user && user->m_user)
  {
    gmFileInfoUser * fileInfo = (gmFileInfoUser *) user->m_user;
    gmStringObject * member = (gmStringObject *) GM_OBJECT(a_operands[1].m_value.m_ref);

    GM_ASSERT(sizeof(gmptr) == sizeof(time_t));

    if(strcmp(member->GetString(), "creationTime") == 0)
      a_operands->SetInt((gmptr) fileInfo->m_creationTime);
    else if(strcmp(member->GetString(), "accessedTime") == 0)
      a_operands->SetInt((gmptr) fileInfo->m_accessedTime);
    else if(strcmp(member->GetString(), "modifiedTime") == 0)
      a_operands->SetInt((gmptr) fileInfo->m_modifiedTime);
    else if(strcmp(member->GetString(), "size") == 0)
      a_operands->SetInt((gmptr) fileInfo->m_size);
    else
    {
      a_operands->Nullify();
      return;
    }
    return;
  }
  a_operands->Nullify();
}


#if GM_USE_INCGC
static void GM_CDECL gmGCDestructFileInfoUserType(gmMachine * a_machine, gmUserObject* a_object)
{
  if(a_object->m_user)
  {
    a_machine->Sys_Free(a_object->m_user);
  }
  a_object->m_user = NULL;
}
#else //GM_USE_INCGC
static void GM_CDECL gmGCFileInfoUserType(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
{
  if(a_object->m_user)
  {
    a_machine->Sys_Free(a_object->m_user);
  }
  a_object->m_user = NULL;
}
#endif //GM_USE_INCGC

//
//
// binding
//
//


static gmFunctionEntry s_systemLib[] = 
{ 
  /*gm
    \lib system
    \brief system functions are bound in a "system" table.
  */
  /*gm
    \function Exec
    \brief Exec will execute a system command
    \param string params will be concatinated together with a single space to form the final system command string
    \return integer value returned from system exec call, -1 on error
  */
  {"Exec", gmfSystem},
  /*gm
    \function DoFile
    \brief DoFile will execute the gm script in the named file
    \param string filename
    \param int optional (1) as 1 will execute string before returning, 0 will execute later.
    \param ref optional (null) set 'this'
    \return thread id of new thread created to execute file
  */
  {"DoFile", gmfDoFile},
  /*gm
    \function File
    \brief File will create a file object
    \return file object
  */
  {"File", gmfFile},
  /*gm
    \function FileExists
    \brief FileExists will test to see if a file exists
    \param string filename 
    \return 1 if the file exists, otherwise 0
  */
  {"FileExists", gmfFileExists},
  /*gm
    \function FileFindFirst
    \brief FileFindFirst will start a file search will test to see if a file exists
    \param string filesearch (may contain wildcards, eg, `c:\temp\*.txt`)
    \return fileFind object.  fileFind object has .filename and .size member
    \sa fileFind
  */
  {"FileFindFirst", gmfFindFirstFile},
  /*gm
    \function FileFindNext
    \brief FileFindNext will get the next file matching the file find
    \param fileFind object returned by FileFindFirst call or FileFindNext call
    \return fileFind object
    \sa fileFind
  */
  {"FileFindNext", gmfFindNextFile},

  /*gm
    \function FileInfo
    \brief FileInfo will return a file info object that has readonly members for .creationDate
    \param string path
    \return fileInfo object, fileInfo object has a .creationTime, .accessedTime, .modifiedTime and a .size
  */
  {"FileInfo", gmfFileInfo},

  /*gm
    \function CreateFolder
    \brief CreateFolder will create a file path if it does not already exist
    \param string path
    \return int 0 on failure, 1 on successful create, 2 if folder already exists
  */
  {"CreateFolder", gmfCreateFolder},
  /*gm
    \function DeleteFolder
    \brief DeleteFolder will remove a file path
    \param string path
    \param int remove subfiles and folders optional (0)
    \return int 1 on success, 0 con failure
  */
  {"DeleteFolder", gmfDeleteFolder},

  /*gm
    \function Time
    \brief Time will return a unix style time_t as an int
    \return the current time
  */
  {"Time", gmfTime},
  /*gm
    \function FormatTime
    \brief FormatTime will take a int (time_t) value and format according to the passed format string.
    \param int time (-1) is a (time_t) to be converted to a string, passing -1 gets current time
    \param string format ("%A %d %B %Y, %I:%M:%S %p") is the format string to use.<BR>
            %a : Abbreviated weekday name<BR>
            %A : Full weekday name<BR>
            %b : Abbreviated month name<BR>
            %B : Full month name<BR>
            %c : Date and time representation appropriate for locale<BR>
            %d : Day of month as decimal number (01 – 31)<BR>
            %H : Hour in 24-hour format (00 – 23)<BR>
            %I : Hour in 12-hour format (01 – 12)<BR>
            %j : Day of year as decimal number (001 – 366)<BR>
            %m : Month as decimal number (01 – 12)<BR>
            %M : Minute as decimal number (00 – 59)<BR>
            %p : Current locale’s A.M./P.M. indicator for 12-hour clock<BR>
            %S : Second as decimal number (00 – 59)<BR>
            %U : Week of year as decimal number, with Sunday as first day of week (00 – 53)<BR>
            %w : Weekday as decimal number (0 – 6; Sunday is 0)<BR>
            %W : Week of year as decimal number, with Monday as first day of week (00 – 53)<BR>
            %x : Date representation for current locale<BR>
            %X : Time representation for current locale<BR>
            %y : Year without century, as decimal number (00 – 99)<BR>
            %Y : Year with century, as decimal number<BR>
            %z, %Z : Time-zone name or abbreviation; no characters if time zone is unknown<BR>
            %% : Percent sign<BR>
    \return the time as a string.
  */
  {"FormatTime", gmfFormatTime},
};

static gmFunctionEntry s_fileFindLib[] = 
{
  /*gm
    \lib fileFind
    \brief fileFind object has a "filename" and "size" member
  */
  /*gm
    \function GetAttribute
    \brief GetAttribute will test a file attribute.
    \param int char attribute 'r' readonly, 'a' archive, 's' system, 'h' hidden, 'c' compressed, 'd' directory
    \return 1 if the attribute is set, 0 otherwise
  */
  {"GetAttribute", gmfFileFindGetAttribute},
};


static gmFunctionEntry s_fileLib[] = 
{ 
  /*gm
    \lib file
  */
  /*gm
    \function Open
    \brief Open will open a file in binary mode
    \param string filename
    \param int readonly optional (1)
    \return 1 if the open was successful, 0 otherwise 
  */
  {"Open", gmfFileOpen},
  /*gm
    \function OpenText
    \brief OpenText will open a file in text mode
    \param string filename
    \param int readonly optional (1)
    \return 1 if the open was successful, 0 otherwise 
  */
  {"OpenText", gmfFileOpenText},
  /*gm
    \function Close
    \brief Close will close a file
  */
  {"Close", gmfFileClose},
  /*gm
    \function IsOpen
    \brief IsOpen will test to see if a file is open
    \return 1 if the file is open, 0 otherwise
  */
  {"IsOpen", gmfFileIsOpen},
  /*gm
    \function Seek
    \brief Move to position within file
    \param int offset positional offset relative to origin
    \param int origin eg. myFile.SEEK_CUR, myFile.SEEK_END, myFile.SEEK_SET for current, end, start origins.
    \return int 1 if operation succeeded, 0 if failed.
  */
  {"Seek", gmfFileSeek},
  /*gm
    \function Tell
    \brief Tell will return the current cursor position of the file
    \return int the current cursor position, -1 on error
  */
  {"Tell", gmfFileTell},
  /*gm
    \function ReadLine
    \brief ReadLine will read a line of text from the file.
    \param int keep optional (0) as 1 will keep the "\n" char on the line, otherwise it is removed
    \return string, or null on eof
  */
  {"ReadLine", gmfFileReadLine},
  /*gm
    \function ReadChar
    \brief ReadChar will read a char from the file
    \return int char or null on eof
  */
  {"ReadChar", gmfFileReadChar},
  /*gm
    \function WriteString
    \brief WriteString will write a string to the file
    \param string to write to file
    \return 1 on success, null on error
  */
  {"WriteString", gmfFileWriteString},
  /*gm
    \function WriteChar
    \brief WriteChar will write a char to the file
    \param int char to write to file
    \return 1 on success, null on error
  */
  {"WriteChar", gmfFileWriteChar},
};


void gmBindSystemLib(gmMachine * a_machine)
{
  // system
  a_machine->RegisterLibrary(s_systemLib, sizeof(s_systemLib) / sizeof(s_systemLib[0]), "system");

  // file
  s_gmFileType = a_machine->CreateUserType("file");
#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileType, NULL, gmGCDestructFileUserType);
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileType, NULL, gmGCFileUserType);
#endif //GM_USE_INCGC
  a_machine->RegisterTypeLibrary(s_gmFileType, s_fileLib, sizeof(s_fileLib) / sizeof(s_fileLib[0]));
  a_machine->RegisterTypeOperator(s_gmFileType, O_GETDOT, NULL, gmFileOpGetDot);

  // fileFind
  s_gmFileFindType = a_machine->CreateUserType("fileFind");
  a_machine->RegisterTypeLibrary(s_gmFileFindType, s_fileFindLib, sizeof(s_fileFindLib) / sizeof(s_fileFindLib[0]));
#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileFindType, NULL, gmGCDestructFileFindUserType);
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileFindType, NULL, gmGCFileFindUserType);
#endif //GM_USE_INCGC
  a_machine->RegisterTypeOperator(s_gmFileFindType, O_GETDOT, NULL, gmFileFindOpGetDot);

  // fileInfo
  s_gmFileInfoType = a_machine->CreateUserType("fileInfo");
#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileInfoType, NULL, gmGCDestructFileInfoUserType);
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(s_gmFileInfoType, NULL, gmGCFileInfoUserType);
#endif //GM_USE_INCGC
  a_machine->RegisterTypeOperator(s_gmFileInfoType, O_GETDOT, NULL, gmFileInfoOpGetDot);
}

#endif // GM_SYSTEM_LIB
