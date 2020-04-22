//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_LOGOFILE_H
#define SV_LOGOFILE_H
#ifdef _WIN32
#pragma once
#endif


#include "checksum_crc.h"


class CGameClient;
class CPerClientLogoInfo;	// (private to this module)
class CServerLogoInfo;

// Create per-client logo info.
// CPerClientLogoInfo* SV_LogoFile_CreatePerClientLogoInfo();
// void SV_LogoFile_DeletePerClientLogoInfo( CPerClientLogoInfo *pInfo );

// Called when a client's netchan is going away.
// void SV_LogoFile_HandleClientDisconnect( CGameClient *pClient );

// Register whatever messages the logo files use.
// void SV_LogoFile_NewConnection( INetChannel *chan, CGameClient *pGameClient );

// Called when the client connects. The client sends its logo file CRC to the server. If the server
// already has it, then it's fine.
// void SV_LogoFile_OnConnect( CGameClient *pClient, bool bValid, CRC32_t crcValue );


#endif // SV_LOGOFILE_H
