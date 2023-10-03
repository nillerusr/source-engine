//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "closedcaptions.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Assumed to be set up by calling code
bool AsyncCaption_t::LoadFromFile( char const *pchFullPath )
{
	FileHandle_t fh = g_pFullFileSystem->Open( pchFullPath, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return false;

	MEM_ALLOC_CREDIT();

	CUtlBuffer dirbuffer;

	// Read the header
	g_pFullFileSystem->Read( &m_Header, sizeof( m_Header ), fh );
	if ( m_Header.magic != COMPILED_CAPTION_FILEID )
	{
		if( IsPS3() )
			return false;
		else
			Error( "Invalid file id for %s\n", pchFullPath );
	}
	if ( m_Header.version != COMPILED_CAPTION_VERSION )
	{
		if( IsPS3() )
			return false;
		else
			Error( "Invalid file version for %s\n", pchFullPath );
	}
	if ( m_Header.directorysize < 0 || m_Header.directorysize > 64 * 1024 )
	{
		if( IsPS3() )
			return false;
		else
			Error( "Invalid directory size %d for %s\n", m_Header.directorysize, pchFullPath );
	}
	//if ( m_Header.blocksize != MAX_BLOCK_SIZE )
	//	Error( "Invalid block size %d, expecting %d for %s\n", m_Header.blocksize, MAX_BLOCK_SIZE, pchFullPath );

	int directoryBytes = m_Header.directorysize * sizeof( CaptionLookup_t );
	m_CaptionDirectory.EnsureCapacity( m_Header.directorysize );
	dirbuffer.EnsureCapacity( directoryBytes );

	g_pFullFileSystem->Read( dirbuffer.Base(), directoryBytes, fh );
	g_pFullFileSystem->Close( fh );

	m_CaptionDirectory.CopyArray( (const CaptionLookup_t *)dirbuffer.PeekGet(), m_Header.directorysize );
	m_CaptionDirectory.RedoSort( true );

	m_DataBaseFile = pchFullPath;
	return true;
}
