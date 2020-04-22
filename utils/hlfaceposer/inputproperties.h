//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INPUTPROPERTIES_H
#define INPUTPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CInputParams : public CBaseDialogParams
{
	char		m_szPrompt[ 256 ];

	// i/o input text
	char		m_szInputText[ 1024 ];
};

// Display/create dialog
int InputProperties( CInputParams *params );

#endif // INPUTPROPERTIES_H
