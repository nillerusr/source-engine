//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CCLOOKUP_H
#define CCLOOKUP_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CCloseCaptionLookupParams : public CBaseDialogParams
{
	char				m_szCCToken[ 1024 ];
};

// Display/create dialog
int CloseCaptionLookup( CCloseCaptionLookupParams *params );

#endif // CCLOOKUP_H
