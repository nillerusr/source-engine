#include "toollib.h"

#ifdef _DEBUG
#define HEAP_CHECK
#endif

int		g_tl_argc;
char**	g_tl_argv;
int		g_tl_byteorder;
int		g_tl_dircount;
char**	g_tl_dirlist;
int		g_tl_start;
int		g_tl_abort;
bool	g_tl_quiet;

#pragma warning(disable:4311)
#pragma warning(disable:4267)

/*****************************************************************************
	TL_Setup

*****************************************************************************/
void TL_Setup(char* appname, int argc, char** argv)
{
	const char* buildStr;

	g_tl_argc = argc;
	g_tl_argv = argv;

	g_tl_quiet = (TL_CheckParm("q") > 0) || (TL_CheckParm("quiet") > 0) || (TL_CheckParm("noheader") > 0);

	if (appname)
	{
		TL_printf("\n%s \n",appname);
#ifdef _DEBUG
		buildStr = "Debug Build";
#else
		buildStr = "Release Build";
#endif
		TL_printf("%s - %s %s\n\n", buildStr, __DATE__, __TIME__);
	}

	g_tl_abort = TL_CheckParm("abort");
	g_tl_start = TL_CPUCount();
}

/*****************************************************************************
	TL_End

*****************************************************************************/
void TL_End(bool showtime)
{
	int	end;

	if (showtime && !g_tl_quiet)
	{
		end = TL_CPUCount();
		TL_printf("\n%f seconds.\n",TL_CPUTime(g_tl_start,end));
	}
}

/*****************************************************************************
	TL_Error

*****************************************************************************/
void TL_Error(char* error, ...)
{
	va_list argptr;

	va_start(argptr,error);
	vprintf(error,argptr);
	va_end(argptr);
	
	printf("\n");

#if !defined( _X360 )
	__asm 
	{
		int 3; 
	}
#endif

	if (g_tl_abort)
		abort();

	exit(-1);
}

/*****************************************************************************
	TL_CheckParm

	Returns the argument number (1 to argc-1) or 0 if not present
*****************************************************************************/
int TL_CheckParm(char* check)
{
	int     i;
	char*	parm;

	for (i=1; i<g_tl_argc; i++)
	{
		parm = g_tl_argv[i];

		if (!isalpha(*parm))
			if (!*++parm)
				continue;

		if (!stricmp(check,parm))
			return (i);
	}

	return (0);
}

/*****************************************************************************
	TL_SafeRead

*****************************************************************************/
void TL_SafeRead(int handle, void* buffer, long count)
{
	if (_read(handle,buffer,count) != count)
		TL_Error("SafeRead(): read failure");
}

/*****************************************************************************
	TL_SafeOpenRead

*****************************************************************************/
int TL_SafeOpenRead(const char* filename)
{
	int handle;

	handle = _open(filename,_O_RDONLY|_O_BINARY);
	if (handle == -1)
		TL_Error("TL_SafeOpenRead(): Error opening %s: %s",filename,strerror(errno));

	return (handle);
}

/*****************************************************************************
	TL_SafeOpenWrite

*****************************************************************************/
int TL_SafeOpenWrite(const char* filename)
{
	int handle;

	handle = _open(filename,_O_RDWR|_O_BINARY|_O_CREAT|_O_TRUNC,0666);
	if (handle == -1)
		TL_Error("TL_SafeOpenWrite(): Error opening %s: %s",filename,strerror(errno));

	return (handle);
}

/*****************************************************************************
	TL_SafeWrite

*****************************************************************************/
void TL_SafeWrite(int handle, void* buffer, long count)
{
	int	status;

	status = _write(handle,buffer,count);
	if (status != count)
		TL_Error("TL_SafeWrite(): write failure %d, errno=%d",status,errno);
}

/*****************************************************************************
	TL_SafeClose

*****************************************************************************/
void TL_SafeClose(int handle, int touch)
{
	// ensure date and time of modification get set
	if (touch)
		_futime(handle,NULL);

	close(handle);
}

/*****************************************************************************
	TL_Malloc

*****************************************************************************/
void* TL_Malloc(int size)
{
	void*	ptr;
	int		newsize;

	newsize = size + sizeof(tlmem_t);
	newsize = (newsize + 3) & ~3;

	ptr = malloc(newsize);
	if (!ptr)
		TL_Error("TL_Malloc(): failure for %lu bytes",size);

	memset(ptr,0,newsize);

	((tlmem_t*)ptr)->id   = TL_MEMID;
	((tlmem_t*)ptr)->size = size;

	return ((byte_t*)ptr + sizeof(tlmem_t));
}

/*****************************************************************************
	TL_Free

*****************************************************************************/
void TL_Free(void* ptr)
{
	tlmem_t*	memptr;

	if (!ptr)
		TL_Error("TL_Free(): null pointer");

	memptr = (tlmem_t*)((byte_t*)ptr - sizeof(tlmem_t));

	if (((u32)memptr) & 3)
		TL_Error("TL_Free(): bad pointer %8.8x",ptr);

	if (memptr->id != TL_MEMID)
		TL_Error("TL_Free(): corrupted pointer %8.8x",ptr);

	memptr->id   = 0;
	memptr->size = 0;

	free(memptr);

#ifdef HEAP_CHECK
	if (_heapchk() != _HEAPOK)
		TL_Error("TL_Free(): heap corrupted");
#endif
}

bool TL_Check(void* ptr)
{
	tlmem_t*	memptr;

	if (!ptr)
		return false;

	memptr = (tlmem_t*)((byte_t*)ptr - sizeof(tlmem_t));

	if (((u32)memptr) & 3)
		return false;

	if (memptr->id != TL_MEMID)
		return false;

	return true;
}

/*****************************************************************************
	TL_Realloc

*****************************************************************************/
void* TL_Realloc(void* ptr, int newsize)
{
	int			len;
	tlmem_t*	oldmemptr;
	void*		newptr;	

	if (!ptr)
	{
		newptr = TL_Malloc(newsize);
		return (newptr);
	}

	oldmemptr = (tlmem_t*)((byte_t*)ptr - sizeof(tlmem_t));

	if ((u32)oldmemptr & 3)
		TL_Error("TL_Realloc(): bad pointer %8.8x",ptr);

	if (oldmemptr->id != TL_MEMID)
		TL_Error("TL_Realloc(): corrupted pointer %8.8x",ptr);

	newptr = TL_Malloc(newsize);

	len = TL_min(newsize,oldmemptr->size);

	memcpy(newptr,ptr,len);

	TL_Free(ptr);

	return (newptr);
}

/*****************************************************************************
	TL_strncpyz

	Copy up to (N) bytes including appending null.
*****************************************************************************/
void TL_strncpyz(char* dst, char* src, int n)
{
	if (n <= 0)
		return;

	if (n > 1)
		strncpy(dst,src,n-1);
		
	dst[n-1] = '\0';
}

/*****************************************************************************
	TL_strncatz

	Concatenate up to dstsize bytes including appending null.
*****************************************************************************/
void TL_strncatz(char* dst, char* src, int dstsize)
{
	int	len;

	if (dstsize <= 0)
		return;

	len = (int)strlen(dst);

	TL_strncpyz(dst+len,src,dstsize-len);
}

/*****************************************************************************
	TL_LoadFile

*****************************************************************************/
long TL_LoadFile(const char* filename, void** bufferptr)
{
	int		handle;
	long	length;
	char*	buffer;

	handle = TL_SafeOpenRead(filename);
	length = TL_FileLength(handle);
	buffer = (char*)TL_Malloc(length+1);
	TL_SafeRead(handle,buffer,length);
	close(handle);

	// for parsing
	buffer[length] = '\0';

	*bufferptr = (void*)buffer;

	return (length);
}

/*****************************************************************************
	TL_TouchFile

*****************************************************************************/
void TL_TouchFile(char* filename)
{
	int	h;

	h = _open(filename,_O_RDWR|_O_BINARY,0666);
	if (h < 0)
		return;

	_futime(h,NULL);
	_close(h);
}

/*****************************************************************************
	TL_SaveFile

*****************************************************************************/
void TL_SaveFile(char* filename, void* buffer, long count)
{
	int	handle;

	handle = TL_SafeOpenWrite(filename);
	TL_SafeWrite(handle,buffer,count);

	TL_SafeClose(handle,true);
}

/*****************************************************************************
	TL_FileLength

*****************************************************************************/
long TL_FileLength(int handle)
{
	long	pos;
	long	length;

	pos = lseek(handle,0,SEEK_CUR);
	length = lseek(handle,0,SEEK_END);
	lseek(handle,pos,SEEK_SET);

	return (length);
}

/*****************************************************************************
	TL_StripFilename

	Removes filename from path.
*****************************************************************************/
void TL_StripFilename(char* path)
{
	int	length;

	length = (int)strlen(path)-1;
	while ((length > 0) && (path[length] != '\\') && (path[length] != '/') && (path[length] != ':'))
		length--;

	/* leave possible seperator */
	if (!length)
		path[0] = '\0';
	else		
		path[length+1] = '\0';
}

/*****************************************************************************
	TL_StripExtension

	Removes extension from path.
*****************************************************************************/
void TL_StripExtension(char* path)
{
	int	length;

	length = (int)strlen(path)-1;
	while (length > 0 && path[length] != '.')
		length--;

	if (length && path[length] == '.')
		path[length] = 0;
}

/*****************************************************************************
	TL_StripPath

	Removes path from full path.
*****************************************************************************/
void TL_StripPath(char* path, char* dest)
{
	char*	src;

	src = path + strlen(path);
	while ((src != path) && (*(src-1) != '\\') && (*(src-1) != '/') && (*(src-1) != ':'))
		src--;

	strcpy(dest,src);
}

/*****************************************************************************
	TL_GetExtension

	Gets any extension from the full path.
*****************************************************************************/
void TL_GetExtension(char* path, char* dest)
{
	char*	src;

	src = path + strlen(path) - 1;

	// back up until a . or the start
	while (src != path && *(src-1) != '.')
		src--;

	if (src == path)
	{
		*dest = '\0';	// no extension
		return;
	}

	strcpy(dest,src);
}

/*****************************************************************************
	TL_DefaultPath

	Adds basepath to head of path.
*****************************************************************************/
void TL_DefaultPath(char* path, char* basepath)
{
	char	temp[TL_MAXPATH];
	char*	ptr;
	char	ch;

	if (path[0] == '\\')
	{
		// path is absolute
		return; 
	}

	ptr = path;
	while (1)
	{
		ch = *ptr++;
		if (!ch)
			break;

		if (ch == ':')
		{
			// path has a device - must be absolute
			return;
		}
	}

	// place basepath at head of path
	// do intermediate copy to preserve any arg wierdness
	strcpy(temp,path);
	strcpy(path,basepath);
	strcat(path,temp);
}

/*****************************************************************************
	TL_AddSeperatorToPath

*****************************************************************************/
void TL_AddSeperatorToPath(char* inpath, char* outpath)
{
	int	len;

	strcpy(outpath,inpath);

	len = (int)strlen(outpath);
	if (outpath[len-1] != '\\')
	{
		outpath[len]   = '\\';
		outpath[len+1] = '\0';
	}
}

/*****************************************************************************
	TL_DefaultExtension

	Adds extension a path that has no extension.
*****************************************************************************/
void TL_DefaultExtension(char* path, char* extension, bool bForce)
{
	char*	src;
	
	if ( !bForce && path[0] )
	{
		src = path + strlen(path) - 1;
		while ((src != path) && (*src != '\\') && (*src != '/'))
		{
			if (*src == '.')
				return;
			src--;
		}
	}

	strcat(path,extension);
}

/*****************************************************************************
	TL_ReplaceDosExtension

	Handles files of the form xxxx.xxxxxxx.xxxxx.zzz
*****************************************************************************/
void TL_ReplaceDosExtension(char* path, char* extension)
{
	int	len;

	len = (int)strlen(path);
	if (!len)
		return;

	if (path[len-1] == '.')
	{
		path[len-1] = '\0';
		strcat(path,extension);
		return;
	}	

	if (len-4 > 0 && path[len-4] == '.')
		path[len-4] = '\0';

	strcat(path,extension);
}

/*****************************************************************************
	TL_ReplaceExtension

	Replaces any extension found after '.'
*****************************************************************************/
void TL_ReplaceExtension(const char* inPath, const char* extension, char* outPath)
{
	int		len;
	char*	src;

	if (outPath != inPath)
		strcpy(outPath, inPath);

	len = (int)strlen(outPath);
	if (!len)
		return;

	if (outPath[len-1] == '.')
	{
		outPath[len-1] = '\0';
		strcat(outPath, extension);
		return;
	}	

	src = outPath + len - 1;
	while ((src != outPath) && (*src != '\\') && (*src != '/'))
	{
		if (*src == '.')
		{
			*src = '\0';
			break;
		}
		src--;
	}

	strcat(outPath, extension);
}

/*****************************************************************************
	TL_TempFilename

	Builds a temporary filename at specified path.
*****************************************************************************/
void TL_TempFilename(char* path)
{
	int	len;

	len = (int)strlen(path);
	if (len)
	{
		/* tack on appending seperator */
		if (path[len-1] != '\\')
		{
			path[len]   = '\\';
			path[len+1] = '\0';
		}
	}

	strcat(path,tmpnam(NULL));
}

/*****************************************************************************
	TL_AlignFile

	TL_Aligns data in file to any boundary.
*****************************************************************************/
int TL_AlignFile(int handle, int align)
{
	int	i;
	int	pos;
	int	empty;
	int	count;

	empty = 0;
	pos   = lseek(handle,0,SEEK_CUR);
	count = ((pos+align-1)/align)*align - pos;

	for (i=0; i<count; i++)
		TL_SafeWrite(handle,&empty,1);

	return (pos+count);	
}

/*****************************************************************************
	TL_GetByteOrder

	Gets byte ordering, true is bigendian.
*****************************************************************************/
int TL_GetByteOrder(void)
{
	return (g_tl_byteorder);
}

/*****************************************************************************
	TL_SetByteOrder

	Sets byte ordering, true is bigendian.
*****************************************************************************/
void TL_SetByteOrder(int flag)
{
	g_tl_byteorder = flag;
}

/*****************************************************************************
	TL_LongSwap

	Swap according to set state.
*****************************************************************************/
long TL_LongSwap(long l)
{
	if (!g_tl_byteorder)
		return (l);

	return (TL_BigLong(l));
}

/*****************************************************************************
	TL_ShortSwap

	Swap according to set state.
*****************************************************************************/
short TL_ShortSwap(short s)
{
	if (!g_tl_byteorder)
		return (s);

	return (TL_BigShort(s));
}

/*****************************************************************************
	TL_BigShort

	Converts native short to big endian
*****************************************************************************/
short TL_BigShort(short l)
{
	byte_t	b1;
	byte_t	b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

/*****************************************************************************
	TL_LittleShort

	Converts native short to little endian
*****************************************************************************/
short TL_LittleShort(short l)
{
	return (l);
}

/*****************************************************************************
	TL_BigLong

	Converts native long to big endian
*****************************************************************************/
long TL_BigLong(long l)
{
	byte_t	b1;
	byte_t	b2;
	byte_t	b3;
	byte_t	b4;

	b1 = (byte_t)(l&255);
	b2 = (byte_t)((l>>8)&255);
	b3 = (byte_t)((l>>16)&255);
	b4 = (byte_t)((l>>24)&255);

	return ((long)b1<<24) + ((long)b2<<16) + ((long)b3<<8) + b4;
}

/*****************************************************************************
	TL_LittleLong

	Converts native long to little endian
*****************************************************************************/
long TL_LittleLong(long l)
{
	return (l);
}

/*****************************************************************************
	TL_BigFloat

	Converts native float to big endian
*****************************************************************************/
float TL_BigFloat(float f)
{
	union
	{
		float	f;
		byte_t	b[4];
	} dat1,dat2;

	dat1.f    = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return (dat2.f);
}

/*****************************************************************************
	TL_Exists

	Returns TRUE if file exists.
*****************************************************************************/
bool TL_Exists(const char* filename)
{
   FILE*	test;

	if (!filename || !filename[0])
		return (false);

   if ((test = fopen(filename,"rb")) == NULL)
      return (false);

   fclose(test);

   return (true);
}

/*****************************************************************************
	TL_FileTime

	Returns a file's time and data word.
*****************************************************************************/
u32 TL_FileTime(char* filename)
{
	struct _finddata_t	finddata;
	intptr_t			h;

	h = _findfirst(filename, &finddata);
	if (h == -1)
		return (0);

	_findclose(h);

	return (finddata.time_write);		
}

/*****************************************************************************
	TL_SortNames

*****************************************************************************/
int TL_SortNames(const void *a, const void *b)
{
   return (strcmp(*((char **)a), *((char **)b)));
}

/*****************************************************************************
	TL_FindFiles

*****************************************************************************/
int TL_FindFiles(char* filemask, char*** filenames) 
{
	struct _finddata_t	finddata;
	intptr_t			h;
	char				sourcepath[TL_MAXPATH];
	int					count;
	int					len;
	char**				names = NULL;
	char*				ptr;

	h = _findfirst(filemask,&finddata);
	if (h == -1)
		return (0);
	
	TL_strncpyz(sourcepath,filemask,TL_MAXPATH);
	TL_StripFilename(sourcepath);
	if (!sourcepath[0])
		strcpy(sourcepath,".\\");
	else
	{
		len = (int)strlen(sourcepath);
		if (sourcepath[len-1] != '\\')
			TL_strncatz(sourcepath,"\\",TL_MAXPATH);
	}

	count = 0;
	do
	{
		if (finddata.attrib & _A_SUBDIR)
			continue;

		if (!count)
			names = (char**)TL_Malloc(sizeof(char*));
		else
			names = (char**)TL_Realloc(names,(count+1)*sizeof(char*));
		
		ptr = (char*)TL_Malloc(TL_MAXPATH);

		names[count] = ptr;
		TL_strncpyz(names[count],sourcepath,TL_MAXPATH);
		TL_strncatz(names[count],finddata.name,TL_MAXPATH);

		count++;
	}
	while (!_findnext(h,&finddata));

	_findclose(h);

	// ascending sort the names
	qsort(names,count,sizeof(char*),TL_SortNames);

	*filenames = names;
	return (count);
}

/*****************************************************************************
	TL_GetFileList

*****************************************************************************/
int TL_GetFileList(char* dirpath, char* pattern, tlfile_t*** filelist)
{
	struct _finddata_t	finddata;
	char				sourcepath[TL_MAXPATH];
	char				fullpath[TL_MAXPATH];
	char*				filename;
	intptr_t			h;
	int					filecount;
	int					finddirs;
	int					len;

	filecount = 0;

	strcpy(sourcepath,dirpath);
	len = (int)strlen(sourcepath);

	if (!len)
		strcpy(sourcepath,".\\");
	else if (sourcepath[len-1] != '\\')
	{
		sourcepath[len]   = '\\';
		sourcepath[len+1] = '\0';
	}

	strcpy(fullpath,sourcepath);

	if (pattern[0] == '\\' && pattern[1] == '\0')
	{
		// find directories
		finddirs = true;
		strcat(fullpath,"*");
	}
	else
	{
		finddirs = false;
		strcat(fullpath,pattern);
	}

	h = _findfirst(fullpath,&finddata);
	if (h == -1)
		return (0);

	do
	{
		// dos attribute complexities i.e. _A_NORMAL is 0
		if (finddirs)
		{
			// skip non dirs
			if (!(finddata.attrib & _A_SUBDIR))
				continue;
		}
		else
		{
			// skip dirs
			if (finddata.attrib & _A_SUBDIR)
				continue;
		}

		if (!stricmp(finddata.name,"."))
			continue;

		if (!stricmp(finddata.name,".."))
			continue;

		if (!filecount)
			*filelist = (tlfile_t**)TL_Malloc(sizeof(tlfile_t*));
		else
			*filelist = (tlfile_t**)TL_Realloc(*filelist,(filecount+1)*sizeof(tlfile_t*));
		
		(*filelist)[filecount] = (tlfile_t*)TL_Malloc(sizeof(tlfile_t));

		len = (int)strlen(sourcepath) + (int)strlen(finddata.name) + 1;
		filename = (char*)TL_Malloc(len);

		strcpy(filename,sourcepath);
		strcat(filename,finddata.name);

		(*filelist)[filecount]->filename   = filename;
		(*filelist)[filecount]->time_write = finddata.time_write;		

		filecount++;
	}
	while (!_findnext(h,&finddata));

	_findclose(h);

	return (filecount);
}

/*****************************************************************************
	_RecurseFileTree

*****************************************************************************/
void _RecurseFileTree(char* dirpath, int depth)
{
	tlfile_t**	filelist;
	int			numfiles;
	int			i;
	int			len;

	// recurse from source directory
	numfiles = TL_GetFileList(dirpath,"\\",&filelist);
	if (!numfiles)
	{
		// add directory name to search tree
		if (!g_tl_dircount)
			g_tl_dirlist = (char**)TL_Malloc(sizeof(char*));
		else
			g_tl_dirlist = (char**)TL_Realloc(g_tl_dirlist,(g_tl_dircount+1)*sizeof(char*));

		len = (int)strlen(dirpath);
		g_tl_dirlist[g_tl_dircount] = (char*)TL_Malloc(len+1);
		strcpy(g_tl_dirlist[g_tl_dircount],dirpath);

		g_tl_dircount++;
		return;
	}

	for (i=0; i<numfiles; i++)
	{
		// form new path name
		_RecurseFileTree(filelist[i]->filename,depth+1);
	}

	g_tl_dirlist = (char**)TL_Realloc(g_tl_dirlist,(g_tl_dircount+1)*sizeof(char*));

	len = (int)strlen(dirpath);
	g_tl_dirlist[g_tl_dircount] = (char*)TL_Malloc(len+1);
	strcpy(g_tl_dirlist[g_tl_dircount],dirpath);

	g_tl_dircount++;
}

/*****************************************************************************
	TL_BuildFileTree

*****************************************************************************/
int TL_BuildFileTree(char* dirpath, char*** dirlist)
{
	g_tl_dircount = 0;
	g_tl_dirlist  = NULL;

	_RecurseFileTree(dirpath,0);

	*dirlist = g_tl_dirlist;	
	return (g_tl_dircount);
}

/*****************************************************************************
	TL_FindFiles2

*****************************************************************************/
int TL_FindFiles2(char* filemask, bool recurse, tlfile_t*** filelist)
{
	char		dirpath[TL_MAXPATH];
	char		pattern[TL_MAXPATH];
	char**		dirlist;
	tlfile_t***	templists;
	tlfile_t**	list;
	int*		numfiles;
	int			numoutfiles;
	int			count;
	int			numdirs;
	int			i;
	int			j;
	int			k;

	// get path only
	strcpy(dirpath,filemask);
	TL_StripFilename(dirpath);

	// get pattern only
	TL_StripPath(filemask,pattern);

	numoutfiles = 0;

	if (recurse)
	{
		// get the tree
		numdirs  = TL_BuildFileTree(dirpath,&dirlist);
		if (numdirs)
		{
			templists = (tlfile_t***)TL_Malloc(numdirs * sizeof(tlfile_t**));
			numfiles  = (int*)TL_Malloc(numdirs * sizeof(int));

			// iterate each directory found
			for (i=0; i<numdirs; i++)
				numfiles[i] = TL_GetFileList(dirlist[i],pattern,&templists[i]);

			// count all the files
			numoutfiles = 0;
			for (i=0; i<numdirs; i++)
				numoutfiles += numfiles[i];		
		
			// allocate single list
			if (numoutfiles)
			{
				*filelist = (tlfile_t**)TL_Malloc(numoutfiles*sizeof(tlfile_t*));

				k = 0;
				for (i=0; i<numdirs; i++)
				{
					count = numfiles[i];
					list  = templists[i];
					for (j=0; j<count; j++,k++)
					{
						(*filelist)[k] = list[j];
					}
				}		
			}

			// free the directory lists
			for (i=0; i<numdirs; i++)
			{
				TL_Free(dirlist[i]);

				if (numfiles[i])
					TL_Free(templists[i]);
			}

			TL_Free(dirlist);
			TL_Free(templists);
			TL_Free(numfiles);
		}
	}
	else
	{
		numoutfiles = TL_GetFileList(dirpath,pattern,filelist);
	}

	return (numoutfiles);
}

/*****************************************************************************
	TL_FreeFileList

*****************************************************************************/
void TL_FreeFileList(int count, tlfile_t** filelist)
{
	int	i;

	for (i=0; i<count; i++)
	{
		TL_Free(filelist[i]->filename);
		TL_Free(filelist[i]);
	}

	if (count)
		TL_Free(filelist);
}

/*****************************************************************************
	TL_CPUCount

*****************************************************************************/
int TL_CPUCount(void)
{
	int	time;

	time = clock();

	return (time);
}

/*****************************************************************************
	TL_CPUTime

*****************************************************************************/
double TL_CPUTime(int start, int stop)
{
	double	duration;

	duration = (double)(stop - start)/CLOCKS_PER_SEC;

	return (duration);
}

/*****************************************************************************
	TL_CreatePath

*****************************************************************************/
void TL_CreatePath(const char* inPath)
{
	char*	ptr;
	char	dirPath[TL_MAXPATH];

	// prime and skip to first seperator
	strcpy(dirPath, inPath);
	ptr = strchr(dirPath, '\\');
	while (ptr)
	{		
		ptr = strchr(ptr+1, '\\');
		if (ptr)
		{
			*ptr = '\0';
			mkdir(dirPath);
			*ptr = '\\';
		}
	}
}

/*****************************************************************************
	TL_Warning

*****************************************************************************/
void TL_Warning(const char* format, ...)
{
	char	msg[4096];
	va_list	argptr;

	if (g_tl_quiet)
		return;

	va_start(argptr, format);
	vsprintf(msg, format, argptr);
	va_end(argptr);

	printf("WARNING: %s", msg);
}

/*****************************************************************************
	TL_printf

*****************************************************************************/
void TL_printf(const char* format, ...)
{
	char	msg[4096];
	va_list	argptr;

	if (g_tl_quiet)
		return;

	va_start(argptr, format);
	vsprintf(msg, format, argptr);
	va_end(argptr);

	printf(msg);
}

//-----------------------------------------------------------------------------
//	TL_IsWildcardMatch
//
//	See if a string matches a wildcard specification that uses * or ?
//-----------------------------------------------------------------------------
bool TL_IsWildcardMatch( const char *wildcardString, const char *stringToCheck, bool caseSensitive )
{
	char wcChar;
	char strChar;

	// use the starMatchesZero variable to determine whether an asterisk
	// matches zero or more characters ( TRUE ) or one or more characters
	// ( FALSE )
	bool starMatchesZero = true;

	while ( ( strChar = *stringToCheck ) && ( wcChar = *wildcardString ) )
	{
		// we only want to advance the pointers if we successfully assigned
		// both of our char variables, so we'll do it here rather than in the
		// loop condition itself
		*stringToCheck++;
		*wildcardString++;

		// if this isn't a case-sensitive match, make both chars uppercase
		// ( thanks to David John Fielder ( Konan ) at http://innuendo.ev.ca
		// for pointing out an error here in the original code )
		if ( !caseSensitive )
		{
			wcChar = toupper( wcChar );
			strChar = toupper( strChar );
		}

		// check the wcChar against our wildcard list
		switch ( wcChar )
		{
			// an asterisk matches zero or more characters
			case '*' :
				// do a recursive call against the rest of the string,
				// until we've either found a match or the string has
				// ended
				if ( starMatchesZero )
					*stringToCheck--;

				while ( *stringToCheck )
				{
					if ( TL_IsWildcardMatch( wildcardString, stringToCheck++, caseSensitive ) )
						return true;
				}

				break;

			// a question mark matches any single character
			case '?' :
				break;

			// if we fell through, we want an exact match
			default :
				if ( wcChar != strChar )
					return false;
				break;
		}
	}

	// if we have any asterisks left at the end of the wildcard string, we can
	// advance past them if starMatchesZero is TRUE ( so "blah*" will match "blah" )
	while ( ( *wildcardString ) && ( starMatchesZero ) )
	{
		if ( *wildcardString == '*' )
			wildcardString++;
		else
			break;
	}
	
	// if we got to the end but there's still stuff left in either of our strings,
	// return false; otherwise, we have a match
	if ( ( *stringToCheck ) || ( *wildcardString ) )
		return false;
	else
		return true;
}

//-----------------------------------------------------------------------------
//	TL_CopyString
//
//-----------------------------------------------------------------------------
char *TL_CopyString( const char* pString )
{
	int size = strlen( pString ) + 1;
	char *pNewString = (char *)TL_Malloc( size );
	memcpy( pNewString, pString, size );

	return pNewString;
}

