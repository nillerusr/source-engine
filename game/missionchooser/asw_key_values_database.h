#ifndef CASW_KEYVALUESDATABASE_H
#define CASW_KEYVALUESDATABASE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

class KeyValues;

//-----------------------------------------------------------------------------
// A generic database which recursively loads all .txt files in a folder.
// Also sets the "Filename" key to the relative path of the file.
//-----------------------------------------------------------------------------
class CASW_KeyValuesDatabase
{
public:
	CASW_KeyValuesDatabase();
	
	int	GetFileCount() { return m_Files.Count(); }
	KeyValues *GetFile( int nFileIndex ) { return m_Files[nFileIndex].m_pKeyValues; }
	const char* GetFilename( int nFileIndex ) { return m_Files[nFileIndex].m_Filename; }
	KeyValues* GetFileByName( const char *pFilename );
	KeyValues *ReloadFile( const char *pFilename );

	// Folder must have a trailing slash
	void LoadFiles( const char *pFolderName );
	void AddFile( KeyValues *pKeyValues, const char *pFilename );

private:
	void LoadFilesInFolder( const char *pPath );

	static bool m_bLoadedLocalization;

	char m_RootFolder[MAX_PATH];
	
	struct FileEntry_t
	{
		char m_Filename[MAX_PATH];
		KeyValues* m_pKeyValues;
	};
	CUtlVector< FileEntry_t > m_Files;
};

#endif // CASW_KEYVALUESDATABASE_H