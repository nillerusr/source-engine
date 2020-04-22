//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MULTIPLEREQUEST_H
#define MULTIPLEREQUEST_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CMultipleParams : public CBaseDialogParams
{
	enum
	{
		YES_ALL = 0,
		YES,
		NO,
		NO_ALL,
		CANCEL,
	};

	char		m_szPrompt[ 256 ];
};

// Display/create dialog
int MultipleRequest( char const *prompt );
int _MultipleRequest( CMultipleParams *params );

void MultipleRequestChangeContext();


#endif // MULTIPLEREQUEST_H
