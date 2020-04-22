#ifndef _TOOLLIB_H_
#define _TOOLLIB_H_

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utime.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <dos.h>
#include <stdarg.h>
#include <conio.h>
#include <math.h>
#include <limits.h>
#include <malloc.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <share.h>

#define TL_MAXPATH			128
#define TL_MEMID			0x44434241

typedef unsigned char 		byte_t;
typedef unsigned int 		rgba_t;
typedef unsigned int 		abgr_t;
typedef unsigned char		u8;
typedef signed char			s8;
typedef unsigned short		u16;
typedef signed short		s16;
typedef unsigned long		u32;
typedef signed long			s32;

typedef struct
{
	int	id;
	int	size;
} tlmem_t;

typedef struct
{
	char*	filename;
	time_t	time_write;
} tlfile_t;

#include "scriplib.h"

#define TL_max(a,b)	((a) > (b) ? (a) : (b))
#define TL_min(a,b)	((a) < (b) ? (a) : (b))

extern void		TL_Setup(char* appname, int argc, char** argv);
extern void		TL_End(bool showtime);
extern void		TL_Error(char* error, ...);
extern int		TL_CheckParm(char* check);
extern void		TL_strncpyz(char* dst, char* src, int n);
extern void		TL_strncatz(char* dst, char* src, int dstsize);
extern void*	TL_Malloc(int size);
extern void*	TL_Realloc(void* ptr, int newsize);
extern void		TL_Free(void* ptr);
extern int		TL_SafeOpenRead(const char* filename);
extern int		TL_SafeOpenWrite(const char* filename);
extern void		TL_SafeRead(int handle, void* buffer, long count);
extern void		TL_SafeWrite(int handle, void* buffer, long count);
extern void		TL_SafeClose(int handle, int touch);
extern long		TL_LoadFile(const char* filename, void** bufferptr);
extern void		TL_SaveFile(char* filename, void* buffer, long count);
extern void		TL_TouchFile(char* filename);
extern long		TL_FileLength(int handle);
extern void		TL_StripPath(char* path, char* dest);
extern void		TL_StripExtension(char* path);
extern void		TL_StripFilename(char* path);
extern void		TL_GetExtension(char* path, char* dest);
extern void		TL_DefaultPath(char* path, char* basepath);
extern void		TL_AddSeperatorToPath(char* inpath, char* outpath);
extern void		TL_DefaultExtension(char* path, char* extension, bool bForce = false);
extern void		TL_TempFilename(char* path);
extern int		TL_AlignFile(int handle, int align);
extern int		TL_GetByteOrder(void);
extern void		TL_SetByteOrder(int flag);
extern long		TL_LongSwap(long l);
extern short	TL_ShortSwap(short s);
extern short	TL_LittleShort(short l);
extern short	TL_BigShort(short l);
extern long		TL_LittleLong(long l);
extern long		TL_BigLong(long l);
extern float	TL_BigFloat(float f);
extern bool		TL_Exists(const char* filename);
extern u32		TL_FileTime(char* filename);
extern void		TL_ReplaceDosExtension(char* path, char* extension);
extern void		TL_ReplaceExtension(const char* inPath, const char* extension, char* outPath);
extern int		TL_CPUCount(void);
extern double	TL_CPUTime(int start, int stop);
extern int		TL_BuildFileTree(char* dirpath, char*** dirlist);
extern int		TL_GetFileList(char* dirpath, char* pattern, tlfile_t*** filelist);
extern int		TL_FindFiles(char* filemask, char*** filenames);
extern int		TL_FindFiles2(char* filemask, bool recurse, tlfile_t*** filelist);
extern void		TL_FreeFileList(int count, tlfile_t** filelist);
extern void		TL_CreatePath(const char* inPath);
extern void		TL_printf(const char* format, ...);
extern void		TL_Warning(const char* format, ...);
extern bool		TL_IsWildcardMatch( const char *wildcardString, const char *stringToCheck, bool caseSensitive );
extern char		*TL_CopyString( const char* pString );
extern bool		TL_Check(void* ptr);

extern int 		g_argc;
extern char**	g_argv;

#endif
