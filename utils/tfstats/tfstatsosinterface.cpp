//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Contains the Implementations of the OS Interfaces
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "TFStatsOSInterface.h"
#include "util.h"

//------------------------------------------------------------------------------------------------------
// Function:	getNextDirectory
// Purpose:		a wrapper for strtok, to return the next directory name out of a path
// Input:	path - the path (will be modified by the call to strtok)
// Output:	char*
//------------------------------------------------------------------------------------------------------
char* CTFStatsOSInterface::getNextDirectory(char* path)
{
	char seps[3];
	seps[0]=pathSeperator();
	seps[1]=0;
	
	return strtok( path, seps );
}


//------------------------------------------------------------------------------------------------------
// Function:	makeDirectory
// Purpose:	 makes a directory hierarchy. because mkdir can't make nested dirs
// Input:	dir - the string of the path to make
// Output:	Returns true on success, false on failure.
// Note: Notice how easy the linux part of this function is...  no drive letters. :)
//------------------------------------------------------------------------------------------------------
bool CTFStatsOSInterface::makeHier(string dir)
{
	errno=0;
	//printf("TRYING TO MAKE %s\n",dir.c_str());
	char startingDir[500];
	this->getcwd(startingDir,500);

	bool retval=true;
	const char* nextDir=NULL;
	char path[500];
	char* dirs=path;
	strcpy(path,dir.c_str());
	//have to parse out directories one at a time. because mkdir just can't handle making nested directories that don't exist (just like md.)
	//in otherwords, it's lame.
	
	
#ifdef WIN32
	//get drive out of path change to it.
	//if it's only one character, then interpret it as a path, make it and return;
	//only do this test because the tests below rely on at least 2 characters in the path
	if (strlen(path)<2)
	{
		this->mkdir(path);
		return true;
	}
	
	//what should we do with remote machines?
	//hmm, let's force users to use mapped drives.
	if (path[0]=='\\' && path[1]=='\\')
	{
		Util::debug_dir_printf("Cannot make a directory on a remote machine.\nMap the share to a drive and specify that drive instead.\n");
			retval=false;
			goto end;
	}	
	//if it's a drive specification
	if (path[0]=='\\')
	{
		if (this->chdir("\\")!=0)
			Util::debug_dir_printf("Could not change to root on drive %c\n",this->getdrive() + 'a' - 1);
		dirs=&path[1];
	}
	else if (path[1]==':')
	{
		//this little formula turns a drive number into the drive letter
		int drive=path[0]+1 - 'a';
		if (this->chdrive(drive)!=0)
		{
			Util::debug_dir_printf("Drive \"%c:\" does not exist\n",path[0]);
			retval=false;
			goto end;
		}
		if (path[2]=='\\')
		{
			if (this->chdir("\\")!=0)
			{
				Util::debug_dir_printf("Could not change to root on drive %c\n",path[0]);
				retval=false;
				goto end;
			}
			dirs=&path[3];
		}
		else 
		{
			dirs=&path[2];
		}
	}
#else // if linux
	if (path[0]=='/')
	{
		this->chdir("/");
		dirs=&path[1];
		char temp[100];
		this->getcwd(temp,100);
		Util::debug_dir_printf("switched to root.  current dir is %s\n",temp);
	}
#endif

	if (dirs[0]==0)
	{
		retval=true;
		goto end;
	}

	//parse out directories. keep trying to changedir into them one by one
	// when one fails, make it, and continue.
	nextDir=getNextDirectory(path);
	do
	{
		if (this->chdir(nextDir)!=0)
		{
			if (this->mkdir(nextDir)!=0)
			{
				char buf[500];
				Util::debug_dir_printf("Could not create directory (current directory is %s) (failed on %s)\n",getcwd(buf,500),nextDir);
				retval=false;
				goto end;
			}

			else
				Util::debug_dir_printf("created %s\n",nextDir);

			//try one more time
			if (this->chdir(nextDir)!=0)
			{
				char buf[500];
				Util::debug_dir_printf("Could not create directory (current directory is %s) failed on second attempt to change to %s\n",getcwd(buf,500),nextDir);
				retval=false;
				goto end;
			}
			char temp[200];
			this->getcwd(temp,200);
			Util::debug_dir_printf("Now in %s\n",temp);
		}
		nextDir=getNextDirectory(NULL);
	}while(nextDir);
	retval=true;
end:
	this->chdir(startingDir);
	Util::debug_dir_printf("changingDirectory to %s\n",startingDir);
	return retval;
}



string& CTFStatsOSInterface::addDirSlash(string& tempbuf)
{
	if (tempbuf!="")
	{
		int buflen=tempbuf.length();
		if (tempbuf.at(buflen-1) != pathSeperator())
			tempbuf+= pathSeperator();
	}
	return tempbuf;
}
string& CTFStatsOSInterface::removeDirSlash(string& tempbuf)
{
	int buflen=tempbuf.length();
	if (buflen > 0 && tempbuf.at(buflen-1) == pathSeperator())
		tempbuf.erase(tempbuf.length()-1,1);
	return tempbuf;
}

#ifdef WIN32
#include <io.h>
bool CTFStatsWin32Interface::findfirstfile(char* filemask,string& filename)
{
	if (hFindFile!=-1)
		return false;

	_finddata_t fd;
	hFindFile=_findfirst(filemask,&fd);
	filename=fd.name;
	
	return hFindFile != -1;
}
bool CTFStatsWin32Interface::findnextfile(string& filename)
{
	filename="";
	if (hFindFile==-1)
		return false;
	
	_finddata_t fd;
	int result=_findnext(hFindFile,&fd);
	filename=fd.name;
	return result!=-1;
}

bool CTFStatsWin32Interface::findfileclose()
{
	if (hFindFile==-1)
		return false;

	int result = _findclose(hFindFile);
	return result != -1;
}
#endif

#ifndef WIN32
#include <dirent.h>
string CTFStatsLinuxInterface::filemask;
bool CTFStatsLinuxInterface::findfirstfile(char* filemask,string& filename)
{
	if (foundFileIterator >= 0)
		findfileclose();

	foundFileIterator=-1;
	numFiles=0;
	foundFiles=NULL;

	struct dirent** namelist;
	int n;

	CTFStatsLinuxInterface::filemask=filemask;
	n=scandir(".",&namelist,CTFStatsLinuxInterface::filenameCompare,alphasort);
	
	if (n<0)
		return false;
	
	foundFileIterator=0;
	numFiles=n;
	foundFiles=namelist;

	return findnextfile(filename);

}
bool CTFStatsLinuxInterface::findnextfile(string& filename)
{
	if (foundFileIterator == -1 || foundFiles == NULL || numFiles == 0)
		return false;

	if (foundFileIterator >= numFiles)
		return false;

	filename=foundFiles[foundFileIterator]->d_name;
	free(foundFiles[foundFileIterator]);
	foundFileIterator++;
	return true;
}

bool CTFStatsLinuxInterface::findfileclose()
{
	free(foundFiles);
	return true;
}

void CTFStatsLinuxInterface::filemask2RegExp(char* buf)
{
	char* read=filemask.c_str();
	char* write=buf;
	while (*read)
	{
		if (*read=='?')
		{
			*write='.';
		}
		else if (*read=='*')
		{
			*write='.';
			write++;
			*write='*';
		}
		else if (*read=='.' || *read=='*' || *read=='?' || *read=='+' || *read=='(' || *read==')' || *read=='{' || *read=='}' || *read=='[' || *read==']' || *read=='^' || *read=='$' )
		{
			*write='\\';
			write++;
			*write=*read;
		}
		else
			*write=*read;
		read++;
		write++;
	}
	*write=0;
}

#include <regex>
int CTFStatsLinuxInterface::filenameCompare(dirent* file)
{
	//scan the filemask, turn it into a regular expression
	//then fire up the regex engine and scan the filename;
	
	char buf[5000];
	filemask2RegExp(buf);
	
	//printf("trying to match %s against %s\n",buf,file->d_name);
	
	string sbuf=buf;
	regex expression(sbuf);
	cmatch what;
	
	bool result=query_match((const char*)file->d_name, (const char*)(file->d_name + strlen(file->d_name)), what, expression);

	//if (result)
//		printf("\tsuccessful\n");
//	else
		//printf("\tno dice\n");
	
	return result?1:0;
}

char* CTFStatsLinuxInterface::ultoa(unsigned long theNum, char* buf,int radix)
{
{
	char ascii[]={"0123456789abcdefghijklmnopqrstuvwxyz"};
	if (radix > 36)
	{
		buf[0]=0;
		return NULL;
	}

	string holder;
	while (theNum)
	{
		char next;
		int i=theNum % radix;
		next=ascii[i];
		holder+=next;
		theNum/=radix;
	}

	int i=0;
	string::reverse_iterator it;
	for (it=holder.rbegin();it!=holder.rend();++it)
	{
		buf[i++]=*it;
	}
	buf[i]=0;
	return buf;
}
}
#endif