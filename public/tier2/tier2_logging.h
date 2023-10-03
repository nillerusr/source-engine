//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Tier2 logging helpers. Adds support for file I/O
//
//===============================================================================

#ifndef TIER2_LOGGING_H
#define TIER2_LOGGING_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "logging.h"


const int MAX_SIMULTANEOUS_LOGGING_FILE_COUNT = 16;
const int INVALID_LOGGING_FILE_HANDLE = -1;

typedef int LoggingFileHandle_t;
typedef void * FileHandle_t;

#define FILELOGGINGLISTENER_INTERFACE_VERSION	"FileLoggingListener001"

abstract_class IFileLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const char *pMessage ) = 0;

	virtual LoggingFileHandle_t BeginLoggingToFile( const char *pFilename, const char *pOptions, const char *pPathID = NULL ) = 0;
	virtual void EndLoggingToFile( LoggingFileHandle_t fileHandle ) = 0;

	virtual void AssignLogChannel( LoggingChannelID_t channelID, LoggingFileHandle_t loggingFileHandle ) = 0;
	virtual void UnassignLogChannel( LoggingChannelID_t channelID ) = 0;
	virtual void AssignAllLogChannels( LoggingFileHandle_t loggingFileHandle ) = 0;
	virtual void UnassignAllLogChannels() = 0;
};

class CFileLoggingListener : public IFileLoggingListener
{
public:
	CFileLoggingListener();
	~CFileLoggingListener();

	virtual void Log( const LoggingContext_t *pContext, const char *pMessage );

	virtual LoggingFileHandle_t BeginLoggingToFile( const char *pFilename, const char *pOptions, const char *pPathID = NULL );
	virtual void EndLoggingToFile( LoggingFileHandle_t fileHandle );

	virtual void AssignLogChannel( LoggingChannelID_t channelID, LoggingFileHandle_t loggingFileHandle );
	virtual void UnassignLogChannel( LoggingChannelID_t channelID );
	virtual void AssignAllLogChannels( LoggingFileHandle_t loggingFileHandle );
	virtual void UnassignAllLogChannels();

private:
	int GetUnusedFileInfo() const;

	struct FileInfo_t
	{
		FileHandle_t m_FileHandle;

		bool IsOpen() const { return m_FileHandle != 0; }
		void Reset() { m_FileHandle = 0; }
	};

	FileInfo_t m_OpenFiles[MAX_SIMULTANEOUS_LOGGING_FILE_COUNT];

	// Table which maps logging channel IDs to open files
	int m_FileIndices[MAX_LOGGING_CHANNEL_COUNT];
};

#endif // TIER2_LOGGING_H