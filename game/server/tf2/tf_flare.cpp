//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_flare.h"
#include "tf_player.h"
#include "tf_team.h"
#include "IEffects.h"
#include "engine/IEngineSound.h"

// Data Description
BEGIN_DATADESC( CSignalFlare )

	// Function Pointers
	DEFINE_FUNCTION( FlareTouch ),
	DEFINE_FUNCTION( FlareThink ),

END_DATADESC()

//Data Table
IMPLEMENT_SERVERCLASS_ST( CSignalFlare, DT_SignalFlare )
	SendPropFloat( SENDINFO( m_flDuration ), 0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flScale ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_bLight ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bSmoke ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( env_flare, CSignalFlare );
PRECACHE_REGISTER( env_flare );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSignalFlare::CSignalFlare( void )
{
	m_nBounces = 0;
	m_flScale = 1.0f;

	m_bFading = false;
	m_bLight = true;
	m_bSmoke = true;
	
	m_flNextDamage = gpGlobals->curtime;
	m_lifeState = LIFE_ALIVE;
	m_iHealth = 100;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSignalFlare::Precache( void )
{
	// Precache model(s).
	// bug: this model no longer exists:
	//PrecacheModel("models/flare.mdl" );

	PrecacheScriptSound( "SignalFlare.Impact" );
	PrecacheScriptSound( "SignalFlare.BurnLower" );
	PrecacheScriptSound( "SignalFlare.Burn" );

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSignalFlare::Spawn( void )
{
	Precache();
	SetModel( "models/flare.mdl" );

	UTIL_SetSize( this, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	SetFriction( 0.6f );
	SetGravity( UTIL_ScaleForGravity( 400 ) );
	m_flDuration = gpGlobals->curtime + 10;

	if ( HasSpawnFlags( SIGNALFLARE_NO_DLIGHT ) )
	{
		m_bLight = false;
	}

	if ( HasSpawnFlags( SIGNALFLARE_NO_SMOKE ) )
	{
		m_bSmoke = false;
	}

	if ( HasSpawnFlags( SIGNALFLARE_INFINITE ) )
	{
		m_flDuration = -1.0f;
	}

	AddFlag( FL_OBJECT | FL_NOTARGET );

	EmitSound( "SignalFlare.Burn" );

	SetThink( FlareThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vecOrigin - 
//			vecAngles - 
//			*pOwner - 
// Output : CSignalFlare
//-----------------------------------------------------------------------------
CSignalFlare *CSignalFlare::Create( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, float lifetime )
{
	// Create an instance of a flare.
	CSignalFlare *pFlare = ( CSignalFlare* )CreateEntityByName( "env_flare" );
	if ( !pFlare )
		return NULL;

	// Set the initial origin and angle vectors.
	UTIL_SetOrigin( pFlare, vecOrigin );
	pFlare->SetLocalAngles( vecAngles );

	// Spawn the flare.
	pFlare->Spawn();

	// Set the flare's touch and think functions.
	pFlare->SetTouch( FlareTouch );
	pFlare->SetThink( FlareThink );
	
	// Don't start sparking immediately.
	pFlare->SetNextThink( gpGlobals->curtime + 0.5f );

	// Set the burn out time.
	pFlare->m_flDuration = gpGlobals->curtime + lifetime;

	pFlare->AddSolidFlags( FSOLID_NOT_STANDABLE );
	pFlare->RemoveSolidFlags( FSOLID_NOT_SOLID );

	pFlare->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	pFlare->SetOwnerEntity( pOwner );
	pFlare->m_pOwner = pOwner;

	return pFlare;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSignalFlare::FlareThink( void )
{
	float deltaTime = ( m_flDuration - gpGlobals->curtime );

	if ( m_flDuration != -1.0f )
	{
		//Fading away
		if ( ( deltaTime <= 2.0f ) && ( m_bFading == false ) )
		{
			m_bFading = true;

			// Reduce sound!
			EmitSound( "SignalFlare.BurnLower" );
		}

		//Burned out
		if ( m_flDuration < gpGlobals->curtime )
		{
			StopSound( "SignalFlare.Burn" );
			UTIL_Remove( this );
			return;
		}
	}
	
	//Act differently underwater
	if ( GetWaterLevel() > 1 )
	{
		UTIL_Bubbles( GetAbsOrigin() + Vector( -2, -2, -2 ), GetAbsOrigin() + Vector( 2, 2, 2 ), 1 );
		m_bSmoke = false;
	}
	else
	{
		//Shoot sparks
		if ( random->RandomInt( 0, 8 ) == 1 )
		{
			g_pEffects->Sparks( GetAbsOrigin() );
		}
	}

	//Next update
	SetNextThink( gpGlobals->curtime + 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CSignalFlare::FlareTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	if ( ( m_nBounces < 10 ) && ( GetWaterLevel() < 1 ) )
	{
		//Throw some real chunks here
		g_pEffects->Sparks( GetAbsOrigin() );
	}

	// hit the world, check the material type here, see if the flare should stick.
//	const trace_t &tr = CBaseEntity::GetTouchTrace();

	if ( m_nBounces == 0 )
	{
		// Emit an impact sound.
		EmitSound( "SignalFlare.Impact" );
	}
	
	// Change our flight characteristics
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetGravity( UTIL_ScaleForGravity( 640 ) );
	
	m_nBounces++;
	
	// After the first bounce, smacking into whoever fired the flare is fair game
	SetOwnerEntity( this );	
	
	//Slow down
	Vector newVelocity = GetAbsVelocity();
	newVelocity.x *= 0.8f;
	newVelocity.y *= 0.8f;
	
	// Stopped?
	if ( newVelocity.Length() < 64.0f )
	{
		newVelocity.Init();
		SetMoveType( MOVETYPE_NONE );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		AddSolidFlags( FSOLID_TRIGGER );
	}

	SetAbsVelocity( newVelocity );
}
