//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basegrenade_shared.h"
#include "engine/IEngineSound.h"
#include "grenade_base_empable.h"
#include "IEffects.h"

#if !defined( CLIENT_DLL )
// Global Savedata 
BEGIN_DATADESC( CBaseEMPableGrenade )
	// Function Pointers
	DEFINE_THINKFUNC( FizzleThink ),
END_DATADESC()
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( BaseEMPableGrenade, DT_BaseEMPableGrenade )

BEGIN_NETWORK_TABLE( CBaseEMPableGrenade, DT_BaseEMPableGrenade )
#if !defined( CLIENT_DLL )
	SendPropFloat( SENDINFO( m_flFizzleDuration ), 10, SPROP_ROUNDDOWN, 0.0, 256.0f ),
#else
	RecvPropFloat( RECVINFO( m_flFizzleDuration ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( base_empable_grenade, CBaseEMPableGrenade );

BEGIN_PREDICTION_DATA( CBaseEMPableGrenade  )

	DEFINE_PRED_FIELD( m_flFizzleDuration, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

#define GRENADE_FIZZLE_DURATION		0.5

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEMPableGrenade::CBaseEMPableGrenade( void )
{
	m_flFizzleDuration = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Apply EMP damage to class
//-----------------------------------------------------------------------------
bool CBaseEMPableGrenade::TakeEMPDamage( float duration )
{
	// If we're fizzling already, ignore extra EMP damage
	if ( m_flFizzleDuration )
		return true;

	// Fizzle away in a couple of seconds
	m_flFizzleDuration = gpGlobals->curtime + MIN( duration, GRENADE_FIZZLE_DURATION );
	SetThink( FizzleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fizzle out and remove self from the world.
//-----------------------------------------------------------------------------
void CBaseEMPableGrenade::FizzleThink( void )
{
	float flDeltaTime = m_flFizzleDuration - gpGlobals->curtime;

	// Keep fizzling until it's time to go
	if ( flDeltaTime > 0.0f )
	{
		// Emit a fizzle sound
		EmitSound( "BaseEMPableGrenade.Fizzle" );

		// Smoke & Spark
		g_pEffects->Sparks( GetAbsOrigin() );
		UTIL_Smoke( GetAbsOrigin(), random->RandomInt( 4, 7), 10 );
	}
	else
	{
		Remove( );
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}
