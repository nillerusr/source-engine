//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interfaces to the OS Interface classes.  
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef TFSTATSOSINTERFACE_H
#define TFSTATSOSINTERFACE_H
#ifdef WIN32
#pragma warning (disable:4786)
#pragma once
#endif
#include "util.h"
#include <string>
using std::string;

//----------------------------------------------------------------------------------------------------
// Purpose: The OS Interface classes contain OS / Compiler specific code in 
// them, so that it is abstracted away from TFStats. This removes compatibility
//issues from TFStats and isolates them to this class.
// Subclasses of this abstract class implement each platform.
//------------------------------------------------------------------------------------------------------
class CTFStatsOSInterface
{
public:
	virtual char pathSeperator()=0;
	virtual int mkdir(const char* ccs)=0;
	virtual int chdir(const char* ccs)=0;
	virtual char* getcwd(char* buf,int maxlen)=0;
	virtual int chdrive(int drive)=0;
	virtual int getdrive()=0;
	
	virtual char** gettzname()=0;

	virtual bool makeHier(string dir);
	virtual char*getNextDirectory(char* path);//helper for above function

	virtual string& addDirSlash(string& s);
	virtual string& removeDirSlash(string& s);

	//change the normal _find function interface somewhat to make it simpler
	//and to make it a little more compatible with the linux interface (scandir())
	virtual bool findfirstfile(char* filemask,string& filename)=0;
	virtual bool findnextfile(string& filename)=0;
	virtual bool findfileclose()=0;


	virtual char* ultoa(unsigned long theNum, char* buf,int radix)=0;
};



#ifdef WIN32
class CTFStatsWin32Interface : public CTFStatsOSInterface
{
public:
	char pathSeperator(){return '\\';}
	int mkdir(const char* ccs){return ::_mkdir(ccs);}
	int chdir(const char* ccs){return ::_chdir(ccs);}
	char* getcwd(char* buf,int maxlen){return ::_getcwd(buf,maxlen);}
	int chdrive(int drive){return ::_chdrive(drive);}
	int getdrive(){return ::_getdrive();}

	char** gettzname(){return _tzname;}

	long hFindFile;

	bool findfirstfile(char* filemask,string& filename);
	bool findnextfile(string& filename);
	bool findfileclose();

	CTFStatsWin32Interface():hFindFile(-1){}

	char* ultoa(unsigned long theNum, char* buf,int radix){return _ultoa(theNum,buf,radix);}
};
#else
#include <dirent.h>
#include <errno.h>
class CTFStatsLinuxInterface : public CTFStatsOSInterface
{
public:
	char pathSeperator(){return '/';}
	int mkdir(const char* ccs)
	{
		int result=::mkdir(ccs,PERMIT);
		chmod(ccs,PERMIT);
		return result;
	}
	
	int chdir(const char* ccs){return ::chdir(ccs);}
	char* getcwd(char* buf,int maxlen){return ::getcwd(buf,maxlen);}
	int chdrive(int drive){return -1;}
	int getdrive(){return -1;}
	
	char** gettzname(){return tzname;}

	int foundFileIterator;
	int numFiles;
	dirent** foundFiles;

	
	static string filemask;
	static void filemask2RegExp(char* buf);
	static int filenameCompare(dirent* file);
	
	bool findfirstfile(char* filemask,string& filename);
	bool findnextfile(string& filename);
	bool findfileclose();

	CTFStatsLinuxInterface():foundFileIterator(-1),numFiles(0),foundFiles(0){}
	char* ultoa(unsigned long theNum, char* buf,int radix);
};
#endif


#endif // TFSTATSOSINTERFACE_H
