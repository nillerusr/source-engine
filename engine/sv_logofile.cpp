//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "quakedef.h"
#include "sv_client.h"
#include "logofile_shared.h"
#include "server.h"
#include "filetransfermgr.h"
#include "filesystem_engine.h"


ConVar sv_logo_rate( "sv_logo_rate", "1024", 0, "How fast (bytes per second) the server sends logo files to clients." );


class CPendingFile
{
public:
	CRC32_t m_nLogoFileCRC;
	bool m_bWaitingForServerToGetFile;
};


class CPerClientLogoInfo
{
public:
	CPerClientLogoInfo()
	{
		m_bLogoFileCRCValid = false;
		m_bSendFileInProgress = false;
	}

	// This client's logo info.
	bool m_bLogoFileCRCValid;
	int m_nLogoFileCRC;

	// Are we sending this client a file right now?
	bool m_bSendFileInProgress;

	// Files that this client has requested but we aren't able to send yet.	
	CUtlVector<CPendingFile> m_PendingFiles;
};


class CServerFileTransferMgr : public CFileTransferMgr
{
public:
	virtual bool SendChunk( INetChannel *pDest, const void *pData, int len )
	{
		SVC_LogoFileData fileData;
		fileData.m_Data.CopyArray( (const char*)pData, len );
		return pDest->SendNetMsg( fileData, true );
	}

	virtual void OnSendCancelled( FileTransferID_t id )
	{
	}

	CGameClient* GetClientByNetChannel( INetChannel *pChan )
	{
		for ( int i=0; i < sv.clients.Count(); i++ )
		{
			CGameClient *pClient = sv.Client( i );
			if ( pClient && pClient->GetNetChannel() == pChan )
				return pClient;
		}
		return NULL;
	}
	
	virtual void OnFinishedSending( 
		INetChannel *pDest, 
		const void *pUserData, 
		int userDataLen, 
		FileTransferID_t id )
	{
		// Start sending the next file to this guy.
		CGameClient *pClient = GetClientByNetChannel( pDest );
		if ( pClient )
		{
			pClient->m_pLogoInfo->m_bSendFileInProgress = false;
			UpdatePendingFiles();
		}
		else
		{
			Warning( "OnFinishedSending: can't get CGameClient from INetChannel.\n" );
		}
	}

	virtual void OnFileReceived( 
		INetChannel *pChan,
		const void *pUserData,
		int userDataLength,
		const char *pFileData, 
		int fileLength )
	{
		// Ok, now the server has received a file the client sent. First, validate the VTF.
		if ( !LogoFile_IsValidVTFFile( pFileData, fileLength ) )
		{
			Warning( "CServerFileTransferMgr::OnFileReceived: received an invalid logo file from a client.\n" );
			return;
		}

		if ( userDataLength < sizeof( CRC32_t ) )
		{
			Warning( "CServerFileTransferMgr::OnFileReceived: invalid userDataLength (%d).\n", userDataLength );
		}

		CRC32_t crcValue = *((CRC32_t*)pUserData);

		// Save this file in our cache.
		if ( SaveCRCFileToCache( crcValue, pFileData, fileLength ) )
		{
			// Start transfers to any clients that we can now.
			MarkPendingFilesWithCRC( crcValue );
			UpdatePendingFiles();
		}
	}


	// If any clients are waiting on this file, mark them so they know they can be sent the file now.
	void MarkPendingFilesWithCRC( CRC32_t crcValue )
	{
		for ( int i=0; i < sv.clients.Count(); i++ )
		{
			CGameClient *pClient = sv.Client( i );
			if ( !pClient || !pClient->m_pLogoInfo )
				continue;
		
			for ( int i=0; i < pClient->m_pLogoInfo->m_PendingFiles.Count(); i++ )
			{
				CPendingFile *pFile = &pClient->m_pLogoInfo->m_PendingFiles[i];
				if ( pFile->m_nLogoFileCRC == crcValue )
					pFile->m_bWaitingForServerToGetFile = false;
			}
		}
	}


	bool SaveCRCFileToCache( CRC32_t crcValue, const void *pFileData, int fileLength )
	{
		CLogoFilename logohex( crcValue, true );
		
		FileHandle_t hFile = g_pFileSystem->Open( logohex.m_Filename, "wb" );
		if ( hFile == FILESYSTEM_INVALID_HANDLE )
		{
			Warning( "SaveCRCFileToCache: couldn't open '%s' for writing.\n", logohex.m_Filename );
			return false;
		}
		else
		{
			int writeRet = g_pFileSystem->Write( pFileData, fileLength, hFile );
			g_pFileSystem->Close( hFile );

			// If we couldn't write it, then delete it.
			if ( writeRet == fileLength )
			{
				return true;
			}
			else
			{
				Warning( "SaveCRCFileToCache: couldn't write data (%d should be %d).\n", writeRet, fileLength );
				return false;
			}
		}
	}

	void UpdatePendingFiles()
	{
		CUtlVector<char> fileData;
		CRC32_t lastCRC = 0;

		// Find clients who want to receive this file.
		for ( int i=0; i < sv.clients.Count(); i++ )
		{
			CGameClient *pClient = sv.Client( i );
			if ( !pClient || !pClient->m_pLogoInfo )
				continue;
		
			// Are we already sending the client a file?
			if ( pClient->m_pLogoInfo->m_bSendFileInProgress )
				continue;

			for ( int iFile=0; iFile < pClient->m_pLogoInfo->m_PendingFiles.Count(); iFile++ )
			{
				CPendingFile *pFile = &pClient->m_pLogoInfo->m_PendingFiles[iFile];

				// If we still have to wait for the server to get this file, then stop.
				if ( pFile->m_bWaitingForServerToGetFile )
					continue;

				pClient->m_pLogoInfo->m_PendingFiles.Remove( iFile );

				// Load the file, if we haven't already.
				if ( fileData.Count() == 0 || lastCRC != pFile->m_nLogoFileCRC )
				{
					// Remember the last CRC so we don't have to reopen the file if 
					// this one is going to a bunch of clients in a row.
					lastCRC = pFile->m_nLogoFileCRC;
					if ( !LogoFile_ReadFile( pFile->m_nLogoFileCRC, fileData ) )
						break;
				}

				StartSending(
					pClient->GetNetChannel(),
					&lastCRC,
					sizeof( lastCRC ),
					fileData.Base(),
					fileData.Count(),
					sv_logo_rate.GetInt() 
					);

				pClient->m_pLogoInfo->m_bSendFileInProgress = true;
				break;
			}
		}
	}
};
CServerFileTransferMgr g_ServerFileTransferMgr;


bool SV_LogoFile_HasLogoFile( CRC32_t crcValue )
{
	CLogoFilename logohex( crcValue, true );
	return g_pFileSystem->FileExists( logohex.m_Filename );
}


PROCESS_MSG_SERVER( CLC_LogoFileData )
{
	g_ServerFileTransferMgr.HandleReceivedData( m_Client->GetNetChannel(), m_Data.Base(), m_Data.Count() );
	return true;
} };


PROCESS_MSG_SERVER( CLC_LogoFileRequest )
{
	// The client is requesting that we send it a specific logo file.
	int index = m_Client->m_pLogoInfo->m_PendingFiles.AddToTail();
	CPendingFile &file = m_Client->m_pLogoInfo->m_PendingFiles[index];
	file.m_nLogoFileCRC = m_nLogoFileCRC;
	file.m_bWaitingForServerToGetFile = SV_LogoFile_HasLogoFile( file.m_nLogoFileCRC );

	// Start sending it if it's time..
	g_ServerFileTransferMgr.UpdatePendingFiles();
	return true;	
} };


CPerClientLogoInfo* SV_LogoFile_CreatePerClientLogoInfo()
{
	CPerClientLogoInfo *pInfo = new CPerClientLogoInfo;
	pInfo->m_bLogoFileCRCValid = false;
	return pInfo;
}


void SV_LogoFile_DeletePerClientLogoInfo( CPerClientLogoInfo *pInfo )
{
	delete pInfo;
}


void SV_LogoFile_HandleClientDisconnect( CGameClient *pClient )
{
	g_ServerFileTransferMgr.HandleClientDisconnect( pClient->GetNetChannel() );
}


void SV_LogoFile_NewConnection( INetChannel *chan, CGameClient *pGameClient )
{
	REGISTER_MSG_SERVER( CLC_LogoFileRequest );
}


bool SV_LogoFile_IsDownloadingLogoFile( CRC32_t crcValue )
{
	for ( int i=g_ServerFileTransferMgr.FirstIncoming(); i != g_ServerFileTransferMgr.InvalidIncoming(); i=g_ServerFileTransferMgr.NextIncoming( i ) )
	{
		const void *pData;
		int dataLen;
		g_ServerFileTransferMgr.GetIncomingUserData( i, pData, dataLen );

		CRC32_t *pTestValue = (CRC32_t*)pData;
		if ( *pTestValue == crcValue )
			return true;
	}
	return false;
}


void SV_LogoFile_OnConnect( CGameClient *pSenderClient, bool bValid, CRC32_t crcValue )
{
	pSenderClient->m_pLogoInfo->m_bLogoFileCRCValid = bValid;
	pSenderClient->m_pLogoInfo->m_nLogoFileCRC = crcValue;

	if ( bValid )
	{
		// Does the server need this file? If so, request it.
		if ( !SV_LogoFile_HasLogoFile( crcValue ) && !SV_LogoFile_IsDownloadingLogoFile( crcValue ) )
		{
			SVC_LogoFileRequest fileRequest;
			fileRequest.m_nLogoFileCRC = crcValue;
			if ( !pSenderClient->SendNetMsg( fileRequest, true ) )
			{
				Host_Error( "SV_LogoFile_OnConnect: Reliable broadcast message would overflow client" );
				return;
			}
		}

		// Tell all clients (except the sending client) about this logo.
		SVC_LogoFileCRC logoNotify;
		logoNotify.m_nLogoFileCRC = crcValue;

		for ( int i=0; i < sv.clients.Count(); i++ )
		{
			CGameClient *pClient = sv.Client( i );
			if ( !pClient || pClient == pSenderClient )
				continue;
			
			bool bReliable = true;
			if ( !pClient->SendNetMsg( logoNotify, bReliable ) )
			{
				Host_Error( "SV_LogoFile_OnConnect: Reliable broadcast message would overflow client" );
				return;
			}
		}
	}

	// Also, tell this client about all other client CRCs so it can aks for the one it needs.
	for ( int i=0; i < sv.clients.Count(); i++ )
	{
		CGameClient *pClient = sv.Client( i );
		if ( !pClient || pClient == pSenderClient || !pClient->m_pLogoInfo->m_bLogoFileCRCValid )
			continue;

		SVC_LogoFileCRC logoNotify;
		logoNotify.m_nLogoFileCRC = pClient->m_pLogoInfo->m_nLogoFileCRC;

		bool bReliable = true;
		if ( !pSenderClient->SendNetMsg( logoNotify, bReliable ) )
		{
			Host_Error( "SV_LogoFile_OnConnect: Reliable broadcast message would overflow client" );
			return;
		}
	}
}

