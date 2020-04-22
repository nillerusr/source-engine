//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "env_lightrail_endpoint_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_lightrail_endpoint, CEnv_Lightrail_Endpoint );

BEGIN_DATADESC( CEnv_Lightrail_Endpoint )
	DEFINE_KEYFIELD( m_flSmallScale, FIELD_FLOAT, "small_fx_scale" ),
	DEFINE_KEYFIELD( m_flLargeScale, FIELD_FLOAT, "large_fx_scale" ),
	DEFINE_FIELD( m_nState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStartTime, FIELD_TIME ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "StartCharge", InputStartCharge ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StartSmallFX", InputStartSmallFX ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StartLargeFX", InputStartLargeFX ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Stop", InputStop ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CEnv_Lightrail_Endpoint, DT_Env_Lightrail_Endpoint )
	SendPropFloat( SENDINFO(m_flSmallScale), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flLargeScale), 0, SPROP_NOSCALE),
	SendPropInt( SENDINFO(m_nState), 8, SPROP_UNSIGNED),
	SendPropFloat( SENDINFO(m_flDuration), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flStartTime), 0, SPROP_NOSCALE),
	SendPropInt( SENDINFO(m_spawnflags), 0, SPROP_UNSIGNED),
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Precache: 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::Precache()
{
	BaseClass::Precache();
	PrecacheMaterial( "effects/light_rail_endpoint" );		//Sprite used for the effects
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::Spawn( void )
{
	Precache();

	UTIL_SetSize( this, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ) );

	// See if we start active
	if ( HasSpawnFlags( SF_ENDPOINT_START_SMALLFX ) )
	{
		m_nState = (int)ENDPOINT_STATE_LARGEFX;		//THIS NEEDS TO BE CHANGED TO SMALL FX STATE
		m_flStartTime = gpGlobals->curtime;
	}

	// No model but we still need to force this!
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flWarmUpTime - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::StartCharge( float flWarmUpTime )
{
	m_nState = (int)ENDPOINT_STATE_CHARGING;
	m_flDuration = flWarmUpTime;
	m_flStartTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::StartLargeFX( void )
{
	m_nState = (int)ENDPOINT_STATE_LARGEFX;
	m_flStartTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::StartSmallFX( void )
{
	m_nState = (int)ENDPOINT_STATE_SMALLFX;
	m_flStartTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCoolDownTime - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::StopLargeFX( float flCoolDownTime )
{
	m_nState = (int)ENDPOINT_STATE_OFF;
	m_flDuration = flCoolDownTime;
	m_flStartTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCoolDownTime - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::StopSmallFX( float flCoolDownTime )
{
	m_nState = (int)ENDPOINT_STATE_OFF;
	m_flDuration = flCoolDownTime;
	m_flStartTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::InputStartCharge( inputdata_t &inputdata )
{
	StartCharge( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::InputStartLargeFX( inputdata_t &inputdata )
{
	StartLargeFX();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::InputStartSmallFX( inputdata_t &inputdata )
{
	StartSmallFX();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnv_Lightrail_Endpoint::InputStop( inputdata_t &inputdata )
{
	StopLargeFX( inputdata.value.Float() );
}

CBaseViewModel *IsViewModelMoveParent_( CBaseEntity *pEffect )
{
	if ( pEffect->GetMoveParent() )
	{
		CBaseViewModel *pViewModel = dynamic_cast<CBaseViewModel *>( pEffect->GetMoveParent() );

		if ( pViewModel )
		{
			return pViewModel;
		}
	}

	return NULL;
}

int CEnv_Lightrail_Endpoint::UpdateTransmitState( void )
{
	if ( IsViewModelMoveParent_( this ) )
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	return BaseClass::UpdateTransmitState();
}

int CEnv_Lightrail_Endpoint::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	CBaseViewModel *pViewModel = IsViewModelMoveParent_( this );

	if ( pViewModel )
	{
		return pViewModel->ShouldTransmit( pInfo );
	}

	return BaseClass::ShouldTransmit( pInfo );
}
