//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Encapsultes the job system's version of time (which is != to wall clock time)
//
//=============================================================================

#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

uint64 CJobTime::sm_lTimeCur = 0;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CJobTime::CJobTime() 
{ 
	m_lTime = sm_lTimeCur; 
}


//----------------------------------------------------------------------------
// Purpose: Sets our value to the current time
//-----------------------------------------------------------------------------
void CJobTime::SetToJobTime()
{
	m_lTime = sm_lTimeCur;
}


//-----------------------------------------------------------------------------
// Purpose: Sets our value as an offset from the current time
//-----------------------------------------------------------------------------
void CJobTime::SetFromJobTime( int64 dMicroSecOffset )
{
	m_lTime = sm_lTimeCur + dMicroSecOffset;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the amount of time that's passed between our time and the
//			current time.
// Output:	Time that's passed between our time and the current time
//-----------------------------------------------------------------------------
int64 CJobTime::CServerMicroSecsPassed() const
{
	return( sm_lTimeCur - m_lTime );
}


//-----------------------------------------------------------------------------
// Purpose: Updates our current time value.  We only
//			update the time once per frame-- the rest of the time, we just
//			access a cached copy of the time.  On the server, we use an arbitrary
//			time value beginning at 0, and incremented by 50 milliseconds every
//			frame.  
//			NOTE: This should only be called once per frame.
//-----------------------------------------------------------------------------
void CJobTime::UpdateJobTime( int cMicroSecPerShellFrame )
{
	sm_lTimeCur += cMicroSecPerShellFrame; // force a 50 msec clock
}


//-----------------------------------------------------------------------------
// Purpose: Stomps the current clock with the specified time
// Input  : lCurrentTime - new current time
//-----------------------------------------------------------------------------
void CJobTime::SetCurrentJobTime( uint64 lCurrentTime )
{
	sm_lTimeCur = lCurrentTime;
}

} // namespace GCSDK