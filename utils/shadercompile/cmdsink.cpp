//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Command sink interface implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#include "cmdsink.h"


namespace CmdSink
{

// ------ implementation of CResponseFiles --------------

CResponseFiles::CResponseFiles( char const *szFileResult, char const *szFileListing ) :
	m_fResult(NULL),
	m_fListing(NULL),
	m_lenResult(0),
	m_dataResult(NULL),
	m_dataListing(NULL)
{
	sprintf( m_szFileResult, szFileResult );
	sprintf( m_szFileListing, szFileListing );
}

CResponseFiles::~CResponseFiles( void )
{
	if ( m_fResult )
		fclose( m_fResult );

	if ( m_fListing )
		fclose( m_fListing );
}

bool CResponseFiles::Succeeded( void )
{
	OpenResultFile();
	return ( m_fResult != NULL );
}

size_t CResponseFiles::GetResultBufferLen( void )
{
	ReadResultFile();
	return m_lenResult;
}

const void * CResponseFiles::GetResultBuffer( void )
{
	ReadResultFile();
	return m_dataResult;
}

const char * CResponseFiles::GetListing( void )
{
	ReadListingFile();
	return ( ( m_dataListing && *m_dataListing ) ? m_dataListing : NULL );
}

void CResponseFiles::OpenResultFile( void )
{
	if ( !m_fResult )
	{
		m_fResult = fopen( m_szFileResult, "rb" );
	}
}

void CResponseFiles::ReadResultFile( void )
{
	if ( !m_dataResult )
	{
		OpenResultFile();

		if ( m_fResult )
		{
			fseek( m_fResult, 0, SEEK_END );
			m_lenResult = (size_t) ftell( m_fResult );

			if ( m_lenResult != size_t(-1) )
			{
				m_bufResult.EnsureCapacity( m_lenResult );
				fseek( m_fResult, 0, SEEK_SET );
				fread( m_bufResult.Base(), 1, m_lenResult, m_fResult );
				m_dataResult = m_bufResult.Base();
			}
		}
	}
}

void CResponseFiles::ReadListingFile( void )
{
	if ( !m_dataListing )
	{
		if ( !m_fListing )
			m_fListing = fopen( m_szFileListing, "rb" );

		if ( m_fListing )
		{
			fseek( m_fListing, 0, SEEK_END );
			size_t len = (size_t) ftell( m_fListing );

			if ( len != size_t(-1) )
			{
				m_bufListing.EnsureCapacity( len );
				fseek( m_fListing, 0, SEEK_SET );
				fread( m_bufListing.Base(), 1, len, m_fListing );
				m_dataListing = (const char *) m_bufListing.Base();
			}
		}
	}
}

}; // namespace CmdSink
