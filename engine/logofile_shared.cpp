//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "quakedef.h"
#include "logofile_shared.h"
#include "filesystem_engine.h"
#include "vtf/vtf.h"


// ------------------------------------------------------------------------------------ //
// CLogoFilename implementation.
// ------------------------------------------------------------------------------------ //

CLogoFilename::CLogoFilename( CRC32_t value, bool bInDownloadsDirectory )
{
	char hex[16];
	Q_binarytohex( (byte *)&value, sizeof( value ), hex, sizeof( hex ) );

	if ( bInDownloadsDirectory )
	{
		Q_snprintf( m_Filename, sizeof( m_Filename ), "materials/decals/downloads/%s.vtf", hex );
	}
	else
	{
		Q_strncpy( m_Filename, hex, sizeof( m_Filename ) );
	}
}


// ------------------------------------------------------------------------------------ //
// SVC_LogoFileData.
// ------------------------------------------------------------------------------------ //

bool SVC_LogoFileData::ReadFromBuffer( bf_read &buffer )
{
	m_Data.SetSize( buffer.ReadShort() );
	buffer.ReadBytes( m_Data.Base(), m_Data.Count() );
	return !buffer.IsOverflowed();
}

bool SVC_LogoFileData::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort( m_Data.Count() );
	buffer.WriteBytes( m_Data.Base(), m_Data.Count() );
	return !buffer.IsOverflowed();
}

const char* SVC_LogoFileData::ToString() const
{
	return "SVC_LogoFileData";
}


// ------------------------------------------------------------------------------------ //
// CLC_LogoFileData.
// ------------------------------------------------------------------------------------ //

bool CLC_LogoFileData::ReadFromBuffer( bf_read &buffer )
{
	m_Data.SetSize( buffer.ReadShort() );
	buffer.ReadBytes( m_Data.Base(), m_Data.Count() );
	return !buffer.IsOverflowed();
}

bool CLC_LogoFileData::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort( m_Data.Count() );
	buffer.WriteBytes( m_Data.Base(), m_Data.Count() );
	return !buffer.IsOverflowed();
}

const char* CLC_LogoFileData::ToString() const
{
	return "CLC_LogoFileData";
}


// ------------------------------------------------------------------------------------ //
// SVC_LogoFileCRC.
// ------------------------------------------------------------------------------------ //

bool SVC_LogoFileCRC::ReadFromBuffer( bf_read &buffer )
{
	m_nLogoFileCRC = buffer.ReadLong();
	return !buffer.IsOverflowed();
}

bool SVC_LogoFileCRC::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nLogoFileCRC );
	return !buffer.IsOverflowed();
}

const char* SVC_LogoFileCRC::ToString() const
{
	return "LogoFileCRC";
}


// ------------------------------------------------------------------------------------ //
// CLC_LogoFileRequest.
// ------------------------------------------------------------------------------------ //

bool CLC_LogoFileRequest::ReadFromBuffer( bf_read &buffer )
{
	m_nLogoFileCRC = buffer.ReadLong();
	return !buffer.IsOverflowed();
}

bool CLC_LogoFileRequest::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nLogoFileCRC );
	return !buffer.IsOverflowed();
}

const char* CLC_LogoFileRequest::ToString() const
{
	return "LogoFileRequest";
}


// ------------------------------------------------------------------------------------ //
// CLC_LogoFileRequest.
// ------------------------------------------------------------------------------------ //

bool SVC_LogoFileRequest::ReadFromBuffer( bf_read &buffer )
{
	m_nLogoFileCRC = buffer.ReadLong();
	return !buffer.IsOverflowed();
}

bool SVC_LogoFileRequest::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nLogoFileCRC );
	return !buffer.IsOverflowed();
}

const char* SVC_LogoFileRequest::ToString() const
{
	return "LogoFileRequest";
}


// ------------------------------------------------------------------------------------ //
// Other functions.
// ------------------------------------------------------------------------------------ //

bool LogoFile_IsValidVTFFile( const void *pData, int len )
{
	CUtlBuffer buf( pData, len, CUtlBuffer::READ_ONLY );

	IVTFTexture *pTex = CreateVTFTexture();
	bool bUnserialized = pTex->Unserialize( buf );
	DestroyVTFTexture( pTex );

	return bUnserialized;
}


bool LogoFile_ReadFile( CRC32_t crcValue, CUtlVector<char> &fileData )
{
	CLogoFilename filename( crcValue, true );
	FileHandle_t hFile = g_pFileSystem->Open( filename.m_Filename, "rb" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "LogoFile_ReadFile: can't open file '%s'.\n", filename.m_Filename );
		return false;
	}

	fileData.SetSize( g_pFileSystem->Size( hFile ) );
	int amtRead = g_pFileSystem->Read( fileData.Base(), fileData.Count(), hFile );
	g_pFileSystem->Close( hFile );
	if ( fileData.Count() == 0 || amtRead != fileData.Count() )
	{
		Warning( "LogoFile_ReadFile: error reading file '%s'.\n", filename.m_Filename );
		return false;
	}
	
	// Validate it one last time.
	if ( !LogoFile_IsValidVTFFile( fileData.Base(), fileData.Count() ) )
	{
		g_pFileSystem->RemoveFile( filename.m_Filename );
		Warning( "LogoFile_ReadFile: !IsValidVTFFile for '%s'.\n", filename.m_Filename );
		return false;
	}
	
	return true;
}
