//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef FILECHANGEWATCHER_H
#define FILECHANGEWATCHER_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Purpose: This class provides notifications of changes in directories.
//          Call AddDirectory to tell it which directories to watch, then
//			call Update() periodically to check for updates.
//-----------------------------------------------------------------------------
class CFileChangeWatcher
{
public:
	class ICallbacks
	{
	public:
		// Note: this is called if the file is added, removed, or modified. It's up to the app to figure out 
		// what it wants to do with the change.
		virtual void OnFileChange( const char *pRelativeFilename, const char *pFullFilename ) = 0;
	};
	
	
	CFileChangeWatcher();
	~CFileChangeWatcher();
	
	void Init( ICallbacks *pCallbacks );	
	
	// pSearchPathBase would be like "c:\valve\hl2" and pDirName would be like "materials".
	bool AddDirectory( const char *pSearchPathBase, const char *pDirName, bool bRecursive );
	void Term();
	
	// Call this periodically to update. It'll call your ICallbacks functions for anything it finds.
	// Returns the number of updates it got.
	int Update();

private:
	class CDirWatch
	{
	public:
		char m_SearchPathBase[MAX_PATH];
		char m_DirName[MAX_PATH];
		char m_FullDirName[MAX_PATH];
		OVERLAPPED m_Overlapped;
		HANDLE m_hEvent;
		HANDLE m_hDir;		// Created with CreateFile.
		char m_Buffer[1024 * 16];
	};

	void SendNotification( CFileChangeWatcher::CDirWatch *pDirWatch, const char *pRelativeFilename );
	BOOL CallReadDirectoryChanges( CFileChangeWatcher::CDirWatch *pDirWatch );

private:
	// Override these.
	CUtlVector<CDirWatch*> m_DirWatches;
	ICallbacks *m_pCallbacks;
};


#endif // FILECHANGEWATCHER_H
