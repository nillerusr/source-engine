//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SV_PACKEDENTITIES_H
#define SV_PACKEDENTITIES_H
#ifdef _WIN32
#pragma once
#endif


#include "server.h"
#include "framesnapshot.h"
#include "server_class.h"


void SV_ComputeClientPacks( 
	int clientCount, 
	CGameClient** clients,
	CFrameSnapshot *snapshot );

void SV_WriteSendTables( ServerClass *pClasses, bf_write &pBuf );
void SV_WriteClassInfos( ServerClass *pClasses, bf_write &pBuf );

void SV_ComputeClassInfosCRC( CRC32_t* crc );
void SV_EnsureInstanceBaseline( ServerClass *pServerClass, int iEdict, const void *pData, int nBytes );

void SV_EnableChangeFrames( bool state );


#endif // SV_PACKEDENTITIES_H
