//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WAVEPROPERTIES_H
#define WAVEPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

class CWaveFile;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CWaveParams : public CBaseDialogParams
{
	CWaveParams()
	{
	}

	CWaveParams( const CWaveParams& src )
	{
		int c = src.items.Count();
		for ( int i = 0; i < c; i++ )
		{
			items.AddToTail( src.items[ i ] );
		}
	}

	CUtlVector< CWaveFile * >	items;
};

int WaveProperties( CWaveParams *params );


#endif // WAVEPROPERTIES_H
