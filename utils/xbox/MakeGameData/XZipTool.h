//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#pragma once

#include <stdio.h>
#include "utlbuffer.h"
#include "zip_uncompressed.h"
#include "generichash.h"
#include "zip_utils.h"
#include "byteswap.h"
#include "tier1/UtlVector.h"
#include "UtlSortVector.h"

struct CRCEntry_t
{
	unsigned int	fileNameCRC;
	CUtlString		filename;
};

struct preloadRemap_t
{
	CUtlString		filename;
	unsigned short	preloadDirIndex;
};

class CZipCRCLessFunc
{
public:
	bool Less( CRCEntry_t const& src1, CRCEntry_t const& src2, void *pCtx )
	{
		return ( src1.fileNameCRC < src2.fileNameCRC );
	}
};

class CXZipTool
{
public:
	CXZipTool();
	~CXZipTool();

	void Reset();
	bool Begin( const char* pFileName, unsigned int alignment = 0 );
	bool End();
	bool AddBuffer( const char* pFileName, CUtlBuffer &buffer, bool bPreload = true );
	bool AddFile( const char* pFileName, bool bPreload = true );
	void SpewPreloadInfo( const char *pZipName );

private:
	IZip											*m_pZip;
	char											m_PreloadFilename[MAX_PATH];
	HANDLE											m_hPreloadFile;
	HANDLE											m_hOutputZipFile;
	ZIP_PreloadHeader								m_ZipPreloadHeader;
	CUtlVector<	ZIP_PreloadDirectoryEntry >			m_ZipPreloadDirectoryEntries;
	CUtlSortVector< CRCEntry_t, CZipCRCLessFunc >	m_ZipCRCList;
	CUtlVector< preloadRemap_t >					m_ZipPreloadRemapEntries;
};