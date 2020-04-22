//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOWNLOADLISTGENERATOR_H
#define DOWNLOADLISTGENERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "filesystem.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "networkstringtable.h"

#define DOWNLOADABLE_FILE_TABLENAME	"downloadables"
#define MAX_DOWNLOADABLE_FILES 8192

enum ConsistencyType
{
	CONSISTENCY_NONE,
	CONSISTENCY_EXACT,
	CONSISTENCY_SIMPLE_MATERIAL,	// uses ExactFileUserData
	CONSISTENCY_BOUNDS,
};

struct ExactFileUserData
{
	unsigned char consistencyType;
	unsigned long crc;
};

struct ModelBoundsUserData
{
	unsigned char consistencyType;
	Vector mins;
	Vector maxs;
};

//-----------------------------------------------------------------------------
// Purpose: Handles collating lists of resources on level load
//			Used to generate reslists for in-game downloads
//-----------------------------------------------------------------------------
class CDownloadListGenerator
{
public:
	CDownloadListGenerator();

	void SetStringTable( INetworkStringTable *pStringTable );

	// call to mark level load/end
	void OnLevelLoadStart(const char *levelName);
	void OnLevelLoadEnd();

	// call to mark resources as being precached
	void OnResourcePrecached(const char *relativePathFileName);
	void OnModelPrecached(const char *relativePathFileName);
	void OnSoundPrecached(const char *relativePathFileName);

	void ForceModelBounds( const char *relativePathFileName, const Vector &mins, const Vector &maxs );
	void ForceSimpleMaterial( const char *relativePathFileName );

private:
	void OnResourcePrecachedFullPath(char *fullPathFileName, const char *relativeFileName );
	char m_gameDir[256];
	char m_mapName[64];
	FileHandle_t m_hReslistFile;
	CUtlSymbolTable m_AlreadyWrittenFileNames;
	INetworkStringTable *m_pStringTable;
};

// singleton accessor
CDownloadListGenerator &DownloadListGenerator();


#endif // DOWNLOADLISTGENERATOR_H
