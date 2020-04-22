//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDLOOKUP_H
#define SOUNDLOOKUP_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CSoundLookupParams : public CBaseDialogParams
{
	char				m_szWaveFile[ 256 ];

	char				m_szPrompt[ 256 ];

	CUtlVector< int >	*entryList;

	char				m_szSoundName[ 256 ];
};

// Display/create dialog
int SoundLookup( CSoundLookupParams *params, HWND parent );

#endif // SOUNDLOOKUP_H
