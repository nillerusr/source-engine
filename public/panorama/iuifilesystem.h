//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIFILESYSTEM_H
#define IUIFILESYSTEM_H

#ifdef _WIN32
#pragma once
#endif


#include "panoramatypes.h"
#include "utlbuffer.h"

namespace panorama
{

typedef void( __cdecl *FileChangeCallback_t )( const char *pFullPath );

//-----------------------------------------------------------------------------
// Purpose: FileSystem support
//-----------------------------------------------------------------------------

typedef void (LoadFileIntoBufferCallback_t)( const char * pchFile, CUtlBuffer &buf, bool bSuccess );

class IUIFileSystem
{
public:
	// fully load the file into the buffer object
	virtual bool LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback = NULL ) = 0;

	virtual void LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del ) = 0;

	// replace this file with the contents of the buffer object
	virtual bool SaveBufferToFile( CUtlBuffer &buf, const char *pchFile ) = 0;

	// return true if this file is on disk (or in a resource file)
	virtual bool FileExists( const char *pchFile ) = 0;

	virtual bool RestoreContentFilename( const char *pchFile, CUtlString &fixedFilename ) { return false; }
	virtual bool RestoreResourceFilename( const char *pchFile, CUtlString &fixedFilename ) { return false; }

	// Run frame on main thread
	virtual void RunFrame() = 0;
};


}
#endif // IUIFILESYSTEM_H
