//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Random number generator
//
// $Workfile: $
// $NoKeywords: $
//===========================================================================//


#include "vstdlib/random.h"
#include <math.h>
#include "dbg.h"

#include "tier0/memdbgon.h"

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NDIV (1+(IM-1)/NTAB)
#define MAX_RANDOM_RANGE 0x7FFFFFFFUL

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM (1.0/IM)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
static CUniformRandomStream s_UniformStream;
static CGaussianRandomStream s_GaussianStream;
static IUniformRandomStream *s_pUniformStream = &s_UniformStream;


//-----------------------------------------------------------------------------
// Installs a global random number generator, which will affect the Random functions above
//-----------------------------------------------------------------------------
void InstallUniformRandomStream( IUniformRandomStream *pStream )
{
	s_pUniformStream = pStream ? pStream : &s_UniformStream;
}


//-----------------------------------------------------------------------------
// A couple of convenience functions to access the library's global uniform stream
//-----------------------------------------------------------------------------
void RandomSeed( int iSeed )
{
	s_pUniformStream->SetSeed( iSeed );
}

float RandomFloat( float flMinVal, float flMaxVal )
{
	return s_pUniformStream->RandomFloat( flMinVal, flMaxVal );
}

float RandomFloatExp( float flMinVal, float flMaxVal, float flExponent )
{
	return s_pUniformStream->RandomFloatExp( flMinVal, flMaxVal, flExponent );
}

int RandomInt( int iMinVal, int iMaxVal )
{
	return s_pUniformStream->RandomInt( iMinVal, iMaxVal );
}

float RandomGaussianFloat( float flMean, float flStdDev )
{
	return s_GaussianStream.RandomFloat( flMean, flStdDev );
}


//-----------------------------------------------------------------------------
//
// Implementation of the uniform random number stream
//
//-----------------------------------------------------------------------------
CUniformRandomStream::CUniformRandomStream()
{
	SetSeed(0);
}

void CUniformRandomStream::SetSeed( int iSeed )
{
	AUTO_LOCK( m_mutex );
	m_idum = ( ( iSeed < 0 ) ? iSeed : -iSeed );
	m_iy = 0;
}

int CUniformRandomStream::GenerateRandomNumber()
{
	AUTO_LOCK( m_mutex );
	int j;
	int k;
	
	if (m_idum <= 0 || !m_iy)
	{
		if (-(m_idum) < 1) 
			m_idum=1;
		else 
			m_idum = -(m_idum);

		for ( j=NTAB+7; j>=0; j--)
		{
			k = (m_idum)/IQ;
			m_idum = IA*(m_idum-k*IQ)-IR*k;
			if (m_idum < 0) 
				m_idum += IM;
			if (j < NTAB)
				m_iv[j] = m_idum;
		}
		m_iy=m_iv[0];
	}
	k=(m_idum)/IQ;
	m_idum=IA*(m_idum-k*IQ)-IR*k;
	if (m_idum < 0) 
		m_idum += IM;
	j=m_iy/NDIV;

	// We're seeing some strange memory corruption in the contents of s_pUniformStream. 
	// Perhaps it's being caused by something writing past the end of this array? 
	// Bounds-check in release to see if that's the case.
	if (j >= NTAB || j < 0)
	{
		DebuggerBreakIfDebugging();
		Warning("CUniformRandomStream had an array overrun: tried to write to element %d of 0..31. Contact Tom or Elan.\n", j);
		// Ensure that NTAB is a power of two.
		COMPILE_TIME_ASSERT( ( NTAB & ( NTAB - 1 ) ) == 0 );
		// Clamp j.
		j &= NTAB - 1;
	}

	m_iy=m_iv[j];
	m_iv[j] = m_idum;

	return m_iy;
}

float CUniformRandomStream::RandomFloat( float flLow, float flHigh )
{
	// float in [0,1)
	float fl = AM * GenerateRandomNumber();
	if (fl > RNMX) 
	{
		fl = RNMX;
	}
	return (fl * ( flHigh - flLow ) ) + flLow; // float in [low,high)
}

float CUniformRandomStream::RandomFloatExp( float flMinVal, float flMaxVal, float flExponent )
{
	// float in [0,1)
	float fl = AM * GenerateRandomNumber();
	if (fl > RNMX)
	{
		fl = RNMX;
	}
	if ( flExponent != 1.0f )
	{
		fl = powf( fl, flExponent );
	}
	return (fl * ( flMaxVal - flMinVal ) ) + flMinVal; // float in [low,high)
}

int CUniformRandomStream::RandomInt( int iLow, int iHigh )
{
	//ASSERT(lLow <= lHigh);
	unsigned int maxAcceptable;
	unsigned int x = iHigh-iLow+1;
	unsigned int n;

	// If you hit either of these assert, you're not getting back the random number that you thought you were.
	Assert( x == iHigh-(int64)iLow+1 ); // Check that we didn't overflow int
	Assert( x-1 <= MAX_RANDOM_RANGE ); // Check that the values provide an acceptable range

	if (x <= 1 || MAX_RANDOM_RANGE < x-1)
	{
		Assert( iLow == iHigh ); // This is the only time it is OK to have a range containing a single number
		return iLow;
	}

	// The following maps a uniform distribution on the interval [0,MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	//
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE+1) % x );
	do
	{
		n = GenerateRandomNumber();
	} while (n > maxAcceptable);

	return iLow + (n % x);
}


//-----------------------------------------------------------------------------
//
// Implementation of the gaussian random number stream
// We're gonna use the Box-Muller method (which actually generates 2
// gaussian-distributed numbers at once)
//
//-----------------------------------------------------------------------------
CGaussianRandomStream::CGaussianRandomStream( IUniformRandomStream *pUniformStream )
{
	AttachToStream( pUniformStream );
}


//-----------------------------------------------------------------------------
// Attaches to a random uniform stream
//-----------------------------------------------------------------------------
void CGaussianRandomStream::AttachToStream( IUniformRandomStream *pUniformStream )
{
	AUTO_LOCK( m_mutex );
	m_pUniformStream = pUniformStream;
	m_bHaveValue = false;
}


//-----------------------------------------------------------------------------
// Generates random numbers
//-----------------------------------------------------------------------------
float CGaussianRandomStream::RandomFloat( float flMean, float flStdDev )
{
	AUTO_LOCK( m_mutex );
	IUniformRandomStream *pUniformStream = m_pUniformStream ? m_pUniformStream : s_pUniformStream;
	float fac,rsq,v1,v2;

	if (!m_bHaveValue)
	{
		// Pick 2 random #s from -1 to 1
		// Make sure they lie inside the unit circle. If they don't, try again
		do
		{
			v1 = 2.0f * pUniformStream->RandomFloat() - 1.0f;
			v2 = 2.0f * pUniformStream->RandomFloat() - 1.0f;
			rsq = v1*v1 + v2*v2;
		} while ((rsq > 1.0f) || (rsq == 0.0f));

		// The box-muller transformation to get the two gaussian numbers
		fac = sqrtf( -2.0f * log(rsq) / rsq );

		// Store off one value for later use
		m_flRandomValue = v1 * fac;
		m_bHaveValue = true;

		return flStdDev * (v2 * fac) + flMean;
	}
	else
	{
		m_bHaveValue = false;
		return flStdDev * m_flRandomValue + flMean;
	}
}


//-----------------------------------------------------------------------------
// Creates a histogram (for testing)
//-----------------------------------------------------------------------------
