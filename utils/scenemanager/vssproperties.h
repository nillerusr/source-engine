//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VSSPROPERTIES_H
#define VSSPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CVSSParams : public CBaseDialogParams
{
	// Event descriptive name
	char			m_szUserName[ 256 ];
	char			m_szProject[ 256 ];
};

int VSSProperties( CVSSParams *params );

#endif // VSSPROPERTIES_H
