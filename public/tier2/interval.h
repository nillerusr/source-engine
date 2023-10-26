//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INTERVAL_H
#define INTERVAL_H
#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"
#include "tier0/platform.h"
#include "tier1/strtools.h"
#include "vstdlib/random.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pString - 
// Output : interval_t
//-----------------------------------------------------------------------------
inline interval_t ReadInterval( const char *pString )
{
	interval_t tmp;
	
	tmp.start = 0;
	tmp.range = 0;

	char tempString[128];
	Q_strncpy( tempString, pString, sizeof(tempString) );
	
	char *token = strtok( tempString, "," );
	if ( token )
	{
		tmp.start = atof( token );
		token = strtok( NULL, "," );
		if ( token )
		{
			tmp.range = atof( token ) - tmp.start;
		}
	}

	return tmp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &interval - 
// Output : float
//-----------------------------------------------------------------------------
inline float RandomInterval( const interval_t &interval )
{
	float out = interval.start;
	if ( interval.range != 0 )
	{
		out += RandomFloat( 0, interval.range );
	}

	return out;
}


#endif // INTERVAL_H
