//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDPROPERTIES_H
#define SOUNDPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

class CSoundEntry;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CSoundParams : public CBaseDialogParams
{
	bool addsound;

	CSoundParams()
	{
	}

	CSoundParams( const CSoundParams& src )
	{
		addsound = src.addsound;

		int c = src.items.Count();
		for ( int i = 0; i < c; i++ )
		{
			items.AddToTail( src.items[ i ] );
		}
	}

	CUtlVector< CSoundEntry * >	items;
};

int SoundProperties( CSoundParams *params );

#endif // SOUNDPROPERTIES_H
