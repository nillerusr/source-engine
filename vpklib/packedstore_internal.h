//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#define VPKFILENUMBER_EMBEDDED_IN_DIR_FILE  0x7fff		// if a chunk refers to this file number, it is data embedded in the same file as the directory block.

#define VPK_HEADER_MARKER 0x55aa1234						// significes that this is a new vpk header format
#define VPK_CURRENT_VERSION 2
#define VPK_PREVIOUS_VERSION 1


struct VPKDirHeader_t
{
	int32 m_nHeaderMarker;
	int32 m_nVersion;
	int32 m_nDirectorySize;
	int32 m_nEmbeddedChunkSize;
	int32 m_nChunkHashesSize;
	int32 m_nSelfHashesSize;
	int32 m_nSignatureSize;

	VPKDirHeader_t( void )
	{
		m_nHeaderMarker = VPK_HEADER_MARKER;
		m_nVersion = VPK_CURRENT_VERSION;
		m_nDirectorySize = 0;
		m_nEmbeddedChunkSize = 0;
		m_nChunkHashesSize = 0;
		m_nSelfHashesSize = 0;
		m_nSignatureSize = 0;
	}

	uint32 ComputeSizeofSignedDataAfterHeader() const
	{
		return m_nDirectorySize + m_nEmbeddedChunkSize + m_nChunkHashesSize + m_nSelfHashesSize;
	}

};

struct VPKDirHeaderOld_t
{
	int32 m_nHeaderMarker;
	int32 m_nVersion;
	int32 m_nDirectorySize;

	VPKDirHeaderOld_t( void )
	{
		m_nHeaderMarker = VPK_HEADER_MARKER;
		m_nVersion = VPK_PREVIOUS_VERSION;
		m_nDirectorySize = 0;
	}

};


#include "vpklib/packedstore.h"


