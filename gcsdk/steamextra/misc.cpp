//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Miscellaneous code
//
//=============================================================================

#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Tells us whether an account name looks like a VTT account name
// (used as an exception for IP-based rate limiting)
//-----------------------------------------------------------------------------
bool IsVTTAccountName( const char *szAccountName )
{
	const static char *k_szCafe = "valvecafepc";

	if ( 0 == Q_strncmp( szAccountName, k_szCafe, Q_strlen( k_szCafe ) ) )
		return true;
	return false;
}


//-----------------------------------------------------------------------------
// Random number generation
// Use this system for all random number generation.  It's very fast, and is
// integrated with our automated tests so that we can perform reproducibly "random"
// test cases.
//-----------------------------------------------------------------------------
uint32 g_unRandCur = 0;
int g_iunRandMask = 0;

// k_rgunMask
// Set of masks used by our quick and dirty random number generator.
const uint32 k_rgunMask[17] = 
{
	0x1739a3b0,
	0xb8907fe1,
	0x8290d3b7,
	0x72839cd0,
	0x242df096,
	0x3829750b,
	0x38de7a77,
	0x72f0924c,
	0x44783927,
	0x01925372,
	0x20902714,
	0x27585920,
	0x27890632,
	0x82910476,
	0x72906721,
	0x28798904,
	0x78592700,
};


//-----------------------------------------------------------------------------
// Purpose: Quickly generates a vaguely random number.
// Output:	A vaguely random number.
//-----------------------------------------------------------------------------
uint32 UNRandFast()
{
	g_iunRandMask++;
	g_unRandCur += 637429601;		// Just add a large prime number (we'll wrap frequently)
	return ( g_unRandCur ^ k_rgunMask[ g_iunRandMask % 17 ] );
}


//-----------------------------------------------------------------------------
// Purpose: Quickly generates a vaguely random character.
// Output:	A vaguely random char in the range [32,126].
//-----------------------------------------------------------------------------
char CHRandFast()
{
	return ( UNRandFast() % 95 ) + 32;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the random number seed (note that we actually break this down
//			into two parts: g_unRandCur and g_iunRandMask).
// Input:	ulRandSeed:			Value to use as our seed
//-----------------------------------------------------------------------------
void SetRandSeed( uint64 ulRandSeed )
{
	g_unRandCur = ulRandSeed >> 32;
	g_iunRandMask = ulRandSeed & 0xffffffff;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current random number seed (actually a composite of
//			g_unRandCur and g_iunRandMask)
// Output:	Our current 64 bit random number seed.
//-----------------------------------------------------------------------------
uint64 GetRandSeed()
{
	return ( ( ((uint64)g_unRandCur) << 32 ) + g_iunRandMask );
}


//-----------------------------------------------------------------------------
// Purpose: Quickly fill a memory block with random bytes
//-----------------------------------------------------------------------------
void RandMem(void *dest, int count)
{
	unsigned char *pDest = (unsigned char *)dest;

	while ( count >= 4 )
	{
		*(uint32*)(pDest) = UNRandFast();
		pDest+=4;
		count-=4;
	}

	while ( count > 0 )
	{
		*pDest = UNRandFast();
		pDest++;
		count--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the percentage of numerator/demoninator, or 0 if 
//			denominator is 0.
//-----------------------------------------------------------------------------
float SafeCalcPct( uint64 ulNumerator, uint64 ulDenominator )
{
	if ( 0 == ulDenominator )
		return 0;
	return ( 100.0f * (float) ulNumerator / (float) ulDenominator );
}


//-----------------------------------------------------------------------------
// Purpose: Common code to reject an operation due to a time backlog.
//			Does a gradual fade of 0% rejections at the specified threshold
//			up to 100% at the limit.  The gradual fade reduces system oscillations
//			that could occur if you abruptly stop allowing all operations.
// Input:	nBacklogCur - the current backlog (in arbitrary units)
//			nBacklogThreshold - the threshold backlog at which to begin rejecting
//			nBacklogLimit - hard limit at which to reject 100% of operations
//			iItem - a monotonically increasing counter of items submitted.  Used
//				to determine which operations are allowed if there is a partial
//				rejection rate.
//-----------------------------------------------------------------------------
bool BRejectDueToBacklog( int nBacklogCur, int nBacklogThreshold, int nBacklogLimit, int iItem )
{
	bool bRefuse = false;

	if ( nBacklogCur >= nBacklogLimit )
	{
		// if we're over the hard backlog limit, refuse all operations
		bRefuse = true;
	}
	else if ( nBacklogCur >= nBacklogThreshold )
	{
		// if we're near the hard backlog limit, start refusing an increasing % of operations,
		// so we don't snap abruptly in and out of accepting operations and potentially cause oscillations

		// ramp from refusing 0% of operations at the backlog threshold up to 100% at the backlog hard limit

		// calculate refuse % to nearest 10%, to make it easy to mod the item # and get a good distribution
		float nRefusePctDecile = 10 * (float) ( nBacklogCur - nBacklogThreshold ) /
			(float) ( nBacklogLimit - nBacklogThreshold );
		Assert( nRefusePctDecile >= 0.0 );
		Assert( nRefusePctDecile <= 10.0 );

		// compare the operations submitted count mod 10 to the refusal percent decile to decide if we should
		// accept or refuse this particular operation
		if ( ( iItem % 10 ) < nRefusePctDecile )
			bRefuse = true;
	}

	return bRefuse;
}


//-----------------------------------------------------------------------------
// Purpose: Defines the head of the CDumpMemFnReg linked list
//-----------------------------------------------------------------------------
CDumpMemFnReg *CDumpMemFnReg::sm_Head = NULL;