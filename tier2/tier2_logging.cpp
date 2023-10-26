//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Tier2 logging helpers.
//
//===============================================================================

#include "tier2_logging.h"
#include "filesystem.h"
#include "interfaces/interfaces.h"

CFileLoggingListener::CFileLoggingListener()
{
	for ( int i = 0; i < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT; ++ i )
	{
		m_OpenFiles[i].Reset();
	}
	for ( int i = 0; i < MAX_LOGGING_CHANNEL_COUNT; ++ i )
	{
		m_FileIndices[i] = INVALID_LOGGING_FILE_HANDLE;
	}
}

CFileLoggingListener::~CFileLoggingListener()
{
	for ( int i = 0; i < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT; ++ i )
	{
		if ( m_OpenFiles[i].IsOpen() )
		{
			g_pFullFileSystem->Close( m_OpenFiles[i].m_FileHandle );
			m_OpenFiles[i].Reset();
		}
	}
}

LoggingFileHandle_t CFileLoggingListener::BeginLoggingToFile( const char *pFilename, const char *pOptions, const char *pPathID /* = NULL */ )
{
	int fileHandle = GetUnusedFileInfo();
	if ( fileHandle != INVALID_LOGGING_FILE_HANDLE )
	{
		m_OpenFiles[fileHandle].m_FileHandle = g_pFullFileSystem->Open( pFilename, pOptions, pPathID );
	}
	return fileHandle;
}

void CFileLoggingListener::EndLoggingToFile( LoggingFileHandle_t fileHandle )
{
	Assert( fileHandle >= 0 && fileHandle < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT );
	/* if the file had any channels associated with it, disassociate them. */
	if ( fileHandle != INVALID_LOGGING_FILE_HANDLE )
	{
		for ( int i = 0 ; i < MAX_LOGGING_CHANNEL_COUNT ; ++i )
		{
			if ( m_FileIndices[i] == fileHandle )
				UnassignLogChannel( i );
		}
	}

	g_pFullFileSystem->Close( m_OpenFiles[fileHandle].m_FileHandle );
	m_OpenFiles[fileHandle].Reset();
}

void CFileLoggingListener::AssignLogChannel( LoggingChannelID_t channelID, LoggingFileHandle_t loggingFileHandle )
{
	Assert( loggingFileHandle >= 0 && loggingFileHandle < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT );
	Assert( m_OpenFiles[loggingFileHandle].IsOpen() );
	Assert( channelID >= 0 && channelID < MAX_LOGGING_CHANNEL_COUNT );
	m_FileIndices[channelID] = loggingFileHandle;
}

void CFileLoggingListener::UnassignLogChannel( LoggingChannelID_t channelID )
{
	Assert( channelID >= 0 && channelID < MAX_LOGGING_CHANNEL_COUNT );
	m_FileIndices[channelID] = INVALID_LOGGING_FILE_HANDLE;
}

void CFileLoggingListener::AssignAllLogChannels( LoggingFileHandle_t loggingFileHandle )
{
	Assert( loggingFileHandle >= 0 && loggingFileHandle < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT );
	Assert( m_OpenFiles[loggingFileHandle].IsOpen() );
	for ( int i = 0; i < MAX_LOGGING_CHANNEL_COUNT; ++ i )
	{
		m_FileIndices[i] = loggingFileHandle;
	}
}

void CFileLoggingListener::UnassignAllLogChannels()
{
	for ( int i = 0; i < MAX_LOGGING_CHANNEL_COUNT; ++ i )
	{
		m_FileIndices[i] = INVALID_LOGGING_FILE_HANDLE;
	}
}

void CFileLoggingListener::Log( const LoggingContext_t *pContext, const char *pMessage )
{
	if ( ( pContext->m_Flags & LCF_CONSOLE_ONLY ) != 0 )
	{
		return;
	}

	Assert( pContext->m_ChannelID >= 0 && pContext->m_ChannelID < MAX_LOGGING_CHANNEL_COUNT );
	int nFileIndex = m_FileIndices[pContext->m_ChannelID];
	if ( nFileIndex >= 0 && nFileIndex < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT )
	{
		// Shouldn't be trying to log to a closed file.
		Assert( m_OpenFiles[nFileIndex].IsOpen() );

		g_pFullFileSystem->Write( pMessage, Q_strlen( pMessage ), m_OpenFiles[nFileIndex].m_FileHandle );
		g_pFullFileSystem->Flush( m_OpenFiles[nFileIndex].m_FileHandle );
	}
}

int CFileLoggingListener::GetUnusedFileInfo() const
{
	for ( int i = 0; i < MAX_SIMULTANEOUS_LOGGING_FILE_COUNT; ++ i )
	{
		if ( !m_OpenFiles[i].IsOpen() )
		{
			return i;
		}
	}
	return INVALID_LOGGING_FILE_HANDLE;
}
