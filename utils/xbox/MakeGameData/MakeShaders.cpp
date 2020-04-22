//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 file creation of shaders
//
//=====================================================================================//

#include "MakeGameData.h"
#include "materialsystem/shader_vcs_version.h"

#define SHADER_FILE_THRESHOLD		32*1024

//-----------------------------------------------------------------------------
// Get the preload data for a vcs file
//-----------------------------------------------------------------------------
bool GetPreloadData_VCS( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	ShaderHeader_t *pHeader = (ShaderHeader_t *)fileBufferIn.Base();

	unsigned int version = BigLong( pHeader->m_nVersion );

	// ensure caller's buffer is clean
	// caller determines preload size, via TellMaxPut()
	preloadBufferOut.Purge();

	unsigned int nPreloadSize;
	if ( fileBufferIn.TellMaxPut() <= SHADER_FILE_THRESHOLD )
	{
		// include the whole file
		nPreloadSize = fileBufferIn.TellMaxPut();
	}
	else
	{
		if ( version < SHADER_VCS_VERSION_NUMBER )
		{
			// not supporting old versions
			return false;
		}

		if ( version != SHADER_VCS_VERSION_NUMBER )
		{
			// bad version
			Msg( "Can't preload: '%s', expecting version %d got version %d\n", pFilename, SHADER_VCS_VERSION_NUMBER, version );
			return false;
		}

		nPreloadSize = sizeof( ShaderHeader_t ) + BigLong( pHeader->m_nNumStaticCombos ) * sizeof( StaticComboRecord_t );
	}

	preloadBufferOut.Put( fileBufferIn.Base(), nPreloadSize );

	return true;
}