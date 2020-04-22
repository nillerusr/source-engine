//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHOICEPROPERTIES_H
#define CHOICEPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

#define MAX_CHOICE_TEXT_SIZE 128

struct ChoiceText
{
	char	choice[ MAX_CHOICE_TEXT_SIZE ];
};

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CChoiceParams : public CBaseDialogParams
{

	char		m_szPrompt[ 256 ];

	CUtlVector< ChoiceText > m_Choices;

	// i/o active choice and output choice
	int			m_nSelected; // -1 for none
};

// Display/create dialog
int ChoiceProperties( CChoiceParams *params );

#endif // CHOICEPROPERTIES_H
