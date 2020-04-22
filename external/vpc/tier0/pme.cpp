//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"

#include "pch_tier0.h"
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#pragma warning( disable : 4530 )   // warning: exception handler -GX option

#include "tier0/platform.h"
#include "tier0/vprof.h"
#include "tier0/pmelib.h"
#include "tier0/l2cache.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void InitPME( void )
{
	bool bInit = false;

	PME *pPME = PME::Instance();
	if ( pPME )
	{
		if ( pPME->GetVendor() != INTEL )
			return;
		
		if ( pPME->GetProcessorFamily() != PENTIUM4_FAMILY )
			return;
		
		pPME->SetProcessPriority( ProcessPriorityHigh );

		bInit = true;

		DevMsg( 1, _T("PME Initialized.\n") );
	}
	else
	{
		DevMsg( 1, _T("PME Uninitialized.\n") );
	}

#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.PMEInitialized( bInit );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown
//-----------------------------------------------------------------------------
void ShutdownPME( void )
{
	PME *pPME = PME::Instance();
	if ( pPME )
	{
	   pPME->SetProcessPriority( ProcessPriorityNormal );
	}

#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.PMEInitialized( false );
#endif
}

//=============================================================================
//
// CL2Cache Code.
//

static int s_nCreateCount = 0;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CL2Cache::CL2Cache()
{
	m_nID = s_nCreateCount++;
	m_pL2CacheEvent = new P4Event_BSQ_cache_reference;
	m_iL2CacheMissCount = 0;
	m_i64Start = 0;
	m_i64End = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CL2Cache::~CL2Cache()
{
	if ( m_pL2CacheEvent )
	{
		delete m_pL2CacheEvent;
		m_pL2CacheEvent = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CL2Cache::Start( void )
{
	if ( m_pL2CacheEvent )
	{
		// Set this up to check for L2 cache misses.
		m_pL2CacheEvent->eventMask->RD_2ndL_MISS = 1;
		
		// Set the event mask and set the capture mode. 
//		m_pL2CacheEvent->SetCaptureMode( USR_Only );
		m_pL2CacheEvent->SetCaptureMode( OS_and_USR );
		
		// That's it, now sw capture events
		m_pL2CacheEvent->StopCounter();
		m_pL2CacheEvent->ClearCounter();
		
		m_pL2CacheEvent->StartCounter();
		m_i64Start = m_pL2CacheEvent->ReadCounter();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CL2Cache::End( void )
{
	if ( m_pL2CacheEvent )
	{
		// Stop the counter and find the delta.
		m_i64End = m_pL2CacheEvent->ReadCounter();
		int64 i64Delta = m_i64End - m_i64Start;
		m_pL2CacheEvent->StopCounter(); 
		
		// Save the delta for later query.
		m_iL2CacheMissCount = ( int )i64Delta;
	}
}

#pragma warning( default : 4530 )

#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CL2Cache::Validate( CValidator &validator, tchar *pchName )
{
	validator.Push( _T("CL2Cache"), this, pchName );

	validator.ClaimMemory( m_pL2CacheEvent );

	validator.Pop( );
}
#endif // DBGFLAG_VALIDATE

