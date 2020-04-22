//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASETFPLAYER_SHARED_H
#define BASETFPLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// Shared header file for players
#if defined( CLIENT_DLL )
#define CBaseTFPlayer C_BaseTFPlayer
#include "c_basetfplayer.h"
#else
#include "tf_player.h"
#endif

#endif // BASETFPLAYER_SHARED_H
