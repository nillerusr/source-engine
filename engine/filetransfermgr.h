//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FILETRANSFERMGR_H
#define FILETRANSFERMGR_H
#ifdef _WIN32
#pragma once
#endif


#include "inetchannel.h"


typedef int FileTransferID_t;


abstract_class CFileTransferMgr
{
public:

	CFileTransferMgr();
	virtual ~CFileTransferMgr();

	// Start transmitting a file.
	// The user data is sent in the header and can include the filename, its ID, or whatever.
	FileTransferID_t StartSending( 
		INetChannel *pDest, 
		const void *pUserData,
		int userDataLength,	
		const char *pFileData, 
		int fileLength, 
		int bytesPerSecond );

	// Kill all file transfers on this channel.
	void HandleClientDisconnect( INetChannel *pChannel );

	// Call this when data comes in.
	void HandleReceivedData( INetChannel *pChannel, const void *pData, int len );

	// Iterate the list of files being downloaded.
	int FirstIncoming() const;
	int NextIncoming( int i ) const;
	int InvalidIncoming() const;
	void GetIncomingUserData( int i, const void* &pData, int &dataLen );

// Overridables.
public:

	// Send outgoing data for a file (reliably).
	// Returns false if it was unable to send the chunk. If this happens, the file transfer manager
	// will retry the chunk a few times, and eventually cancel the file transfer if the problem keeps happening.
	virtual bool SendChunk( INetChannel *pDest, const void *pData, int len ) = 0;
	
	// Had to stop sending because there was a problem sending a chunk, or
	// the net channel went away.
	virtual void OnSendCancelled( FileTransferID_t id ) = 0;

	// Called when it's done transmitting a file.
	virtual void OnFinishedSending( 
		INetChannel *pDest, 
		const void *pUserData, 
		int userDataLen, 
		FileTransferID_t id ) = 0;

	// Called when a file is received.
	virtual void OnFileReceived( 
		INetChannel *pChan,
		const void *pUserData,
		int userDataLength,
		const char *pFileData, 
		int fileLength ) = 0;
};


#endif // FILETRANSFERMGR_H
