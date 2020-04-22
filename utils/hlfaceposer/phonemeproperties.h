//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHONEMEPROPERTIES_H
#define PHONEMEPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CPhonemeParams : public CBaseDialogParams
{
	// i/o phoneme name
	char		m_szName[ 256 ];

	// Can enter multiple phonemes, and clicking buttons just appends phonemes to string
	bool		m_bMultiplePhoneme;
};

// Display/create actor info
int PhonemeProperties( CPhonemeParams *params );

#endif // PHONEMEPROPERTIES_H
