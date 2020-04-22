//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GLOBALEVENTPROPERTIES_H
#define GLOBALEVENTPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

class CChoreoScene;

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CGlobalEventParams : public CBaseDialogParams
{
	int				m_nType;

	// GlobalEvent descriptive name
	char			m_szName[ 256 ];

	// Pause start time
	float			m_flStartTime;

	// Pause Scene or Cancel Scene ( pause/cancel )
	char			m_szAction[ 256 ];

	bool			m_bAutomate;

	char			m_szType[ 256 ];

	// Idle/paused time before action is taken
	float			m_flWaitTime;

	// For loop events
	int				m_nLoopCount;
	float			m_flLoopTime;
};

int GlobalEventProperties( CGlobalEventParams *params );

#endif // GLOBALEVENTPROPERTIES_H
