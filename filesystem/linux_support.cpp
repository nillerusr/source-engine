//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $  
//=============================================================================//

#include "linux_support.h"
#include "tier0/threadtools.h" // For ThreadInMainThread()
#include "tier1/strtools.h"

char selectBuf[PATH_MAX];

int FileSelect(const struct dirent *ent)
{
	const char *mask=selectBuf;
	const char *name=ent->d_name;

	//printf("Test:%s %s\n",mask,name);

	if(!strcmp(name,".") || !strcmp(name,"..") ) return 0;

	if(!strcmp(selectBuf,"*.*")) return 1;

	while( *mask && *name )
	{
		if(*mask=='*')
		{
			mask++; // move to the next char in the mask
			if(!*mask) // if this is the end of the mask its a match 
			{
				return 1;
			}
			while(*name && toupper(*name)!=toupper(*mask)) 
			{ // while the two don't meet up again
				name++;
			}
			if(!*name) 
			{ // end of the name
				break; 
			}
		}
		else if (*mask!='?')
		{
			if( toupper(*mask) != toupper(*name) )
			{	// mismatched!
				return 0;
			}
			else
			{	
				mask++;
				name++;
				if( !*mask && !*name) 
				{ // if its at the end of the buffer
					return 1;
				}
				
			}

		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}	
		
	return( !*mask && !*name ); // both of the strings are at the end
}

int FillDataStruct(FIND_DATA *dat)
{
	struct stat fileStat;

	if(dat->curMatch >= dat->numMatches)
		return -1;

	char szFullPath[MAX_PATH];
	Q_snprintf( szFullPath, sizeof(szFullPath), "%s/%s", dat->cBaseDir, dat->namelist[dat->curMatch]->d_name );  

	if(!stat(szFullPath,&fileStat))
	{
		dat->dwFileAttributes=fileStat.st_mode;           
	}
	else
	{
		dat->dwFileAttributes=0;
	}	

	// now just put the filename in the output data
	Q_snprintf( dat->cFileName, sizeof(dat->cFileName), "%s", dat->namelist[dat->curMatch]->d_name );  

	//printf("%s\n", dat->namelist[dat->curMatch]->d_name);
	free(dat->namelist[dat->curMatch]);

	dat->curMatch++;
	return 1;
}


HANDLE FindFirstFile( const char *fileName, FIND_DATA *dat)
{
	char nameStore[PATH_MAX];
	char *dir=NULL;
	int n,iret=-1;
	
	Q_strncpy(nameStore,fileName, sizeof( nameStore ) );

	if(strrchr(nameStore,'/') )
	{
		dir=nameStore;
		while(strrchr(dir,'/') )
		{
			struct stat dirChk;

			// zero this with the dir name
			dir=strrchr(nameStore,'/');
			if ( dir == nameStore ) // special case for root dir, '/'
			{
				dir[1] = '\0';
			}
			else
			{
				*dir='\0';
				dir=nameStore;
			}

			
			if (stat(dir,&dirChk) < 0)
			{
				continue;
			}

			if( S_ISDIR( dirChk.st_mode ) )
			{
				break;	
			}
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return (HANDLE)-1;
	}

	if( strlen(dir)>0 )
	{
		if ( strlen(dir) == 1 ) // if it was the root dir
			Q_strncpy(selectBuf,fileName+1, sizeof( selectBuf ) );
		else
			Q_strncpy(selectBuf,fileName+strlen(dir)+1, sizeof( selectBuf ) );

		Q_strncpy(dat->cBaseDir,dir, sizeof( dat->cBaseDir ) );
		dat->namelist = NULL;
		n = scandir(dir, &dat->namelist, FileSelect, alphasort);
		if (n < 0)
		{
			if ( dat->namelist )
				free(dat->namelist);
			// silently return, nothing interesting
		}
		else 
		{
			dat->numMatches = n;
			dat->curMatch = 0;
			iret=FillDataStruct(dat);
			if(iret<0)
			{
				if ( dat->namelist )
					free(dat->namelist);
				dat->namelist = NULL;
			}

		}
	}

//	printf("Returning: %i \n",iret);
	return (HANDLE)iret;
}

bool FindNextFile(HANDLE handle, FIND_DATA *dat)
{
	if(dat->curMatch >= dat->numMatches)
	{	
		if ( dat->namelist != NULL )
			free(dat->namelist);
		dat->namelist = NULL;
		return false; // no matches left
	}	

	FillDataStruct(dat);
	return true;
}

bool FindClose(HANDLE handle)
{
	return true;
}



// Pass this function a full path and it will look for files in the specified
// directory that match the file name but potentially with different case.
// The directory name itself is not treated specially.
// If multiple names that match are found then lowercase letters take precedence.
bool findFileInDirCaseInsensitive( const char *file, char* output, size_t bufSize)
{
	// Make sure the output buffer is always null-terminated.
	output[0] = 0;

	// Find where the file part starts.
	const char *dirSep = strrchr(file,'/');
	if( !dirSep )
	{
		dirSep=strrchr(file,'\\');
		if( !dirSep ) 
		{
			return false;
		}
	}

	// Allocate space for the directory portion.
	size_t dirSize = ( dirSep - file ) + 1;
	char *dirName = static_cast<char *>( alloca( dirSize ) ); 

	V_strncpy( dirName , file, dirSize );

	DIR* pDir = opendir( dirName );
	if ( !pDir )
		return false;

	const char* filePart = dirSep + 1;
	// The best matching file name will be placed in this array.
	char outputFileName[ MAX_PATH ];
	bool foundMatch = false;

	// Scan through the directory.
	for ( dirent* pEntry = NULL; ( pEntry = readdir( pDir ) ); /**/ )
	{
		if ( strcasecmp( pEntry->d_name, filePart ) == 0 )
		{
			// If we don't have an existing candidate or if this name is
			// a better candidate then copy it in. A 'better' candidate
			// means that test beats tesT which beats tEst -- more lowercase
			// letters earlier equals victory.
			if ( !foundMatch || strcmp( outputFileName, pEntry->d_name ) < 0 )
			{
				foundMatch = true;
				V_strcpy_safe( outputFileName, pEntry->d_name );
			}
		}
	}

	closedir( pDir );

	// If we didn't find any matching names then lowercase the passed in
	// file name and use that.
	if ( !foundMatch )
	{
		V_strcpy_safe( outputFileName, filePart );
		V_strlower( outputFileName );
	}

	Q_snprintf( output, bufSize, "%s/%s", dirName, outputFileName );
	return foundMatch;
}
