//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "pch_tier0.h"
#include "tier0_strtools.h"


#define TOLOWERC( x )  (( ( x >= 'A' ) && ( x <= 'Z' ) )?( x + 32 ) : x )
extern "C"
int	V_tier0_stricmp(const char *s1, const char *s2 )
{
	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	for(;;)
	{
		int c1 = *( pS1++ );
		int c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
		c1 = *( pS1++ );
		c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
	}
}
