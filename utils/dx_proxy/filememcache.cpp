//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "filememcache.h"

namespace {
unsigned long s_ulCachedFileSignature = 0xCACEF11E;
};


//
// Cached file data implementation
//

CachedFileData * CachedFileData::Create( char const *szFilename )
{
	FILE *f = fopen( szFilename, "rb" );

	int nSize = -1;
	if ( f )
	{
		fseek( f, 0, SEEK_END );
		nSize = ftell( f );
		fseek( f, 0, SEEK_SET );
	}

	CachedFileData *pData = ( CachedFileData * ) malloc( eHeaderSize + max( nSize, 0 ) );
	strcpy( pData->m_chFilename, szFilename );
	pData->m_numRefs = 0;
	pData->m_numDataBytes = nSize;
	pData->m_signature = s_ulCachedFileSignature;

	if ( f )
	{
		fread( pData->m_data, 1, nSize, f );
		fclose( f );
	}

	return pData;
}

void CachedFileData::Free( void )
{
	free( this );
}

CachedFileData *CachedFileData::GetByDataPtr( void const *pvDataPtr )
{
	unsigned char const *pbBuffer = reinterpret_cast< unsigned char const * >( pvDataPtr );
	// Assert( pbBuffer );
	
	CachedFileData const *pData = reinterpret_cast< CachedFileData const * >( pbBuffer - eHeaderSize );
	// Assert( pData->m_signature == s_ulCachedFileSignature );
	
	return const_cast< CachedFileData * >( pData );
}

char const * CachedFileData::GetFileName() const
{
	return m_chFilename;
}

void const * CachedFileData::GetDataPtr() const
{
	return m_data;
}

int CachedFileData::GetDataLen() const
{
	return max( m_numDataBytes, 0 );
}

bool CachedFileData::IsValid() const
{
	return ( m_numDataBytes >= 0 );
}


//
// File cache implementation
//

FileCache::FileCache()
{
	NULL;
}

CachedFileData * FileCache::Get( char const *szFilename )
{
	// Search the cache first
	Mapping::iterator it = m_map.find( szFilename );
	if ( it != m_map.end() )
		return it->second;

	// Create the cached file data
	CachedFileData *pData = CachedFileData::Create( szFilename );
	if ( pData )
		m_map.insert( Mapping::value_type( pData->GetFileName(), pData ) );

	return pData;
}

void FileCache::Clear()
{
	for ( Mapping::iterator it = m_map.begin(), itEnd = m_map.end(); it != itEnd; ++ it )
	{
		it->second->Free();
	}

	m_map.clear();
}

