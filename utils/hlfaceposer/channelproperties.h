//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHANNELPROPERTIES_H
#define CHANNELPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

class CChoreoScene;

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CChannelParams : public CBaseDialogParams
{
	// i/o channel name
	char			m_szName[ 256 ];

	// For creating a new channel:
	// i
	bool			m_bShowActors;
	// i/o
	char			m_szSelectedActor[ 256 ];
	// i
	CChoreoScene	*m_pScene;
};

// set/create channel properties
int ChannelProperties( CChannelParams *params );

#endif // CHANNELPROPERTIES_H
