//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "quakedef.h"
#include "filetransfermgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CFileTransferMgr::CFileTransferMgr()
{
}


CFileTransferMgr::~CFileTransferMgr()
{
}


FileTransferID_t CFileTransferMgr::StartSending( 
	INetChannel *pDest, 
	const void *pUserData,
	int userDataLength,	
	const char *pFileData, 
	int fileLength, 
	int bytesPerSecond )
{
	return 0;
}


void CFileTransferMgr::HandleClientDisconnect( INetChannel *pChannel )
{
}


void CFileTransferMgr::HandleReceivedData( INetChannel *pChannel, const void *pData, int len )
{
}


int CFileTransferMgr::FirstIncoming() const
{
	return 0;
}


int CFileTransferMgr::NextIncoming( int i ) const
{
	return 0;
}


int CFileTransferMgr::InvalidIncoming() const
{
	return 0;
}


void CFileTransferMgr::GetIncomingUserData( int i, const void* &pData, int &dataLen )
{
}



