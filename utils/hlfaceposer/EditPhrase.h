//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EDITPHRASE_H
#define EDITPHRASE_H
#ifdef _WIN32
#pragma once
#endif

#include <stdio.h>

//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CEditPhraseParams : public CBaseDialogParams
{
	char		m_szPrompt[ 256 ];

	// i/o input text
	wchar_t		m_szInputText[ 1024 ];
};

// Display/create dialog
int EditPhrase( CEditPhraseParams *params );

#endif // EDITPHRASE_H
