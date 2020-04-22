//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "basegrenade_shared.h"
#include "engine/IEngineSound.h"
#include "tf_shareddefs.h"
#include "Sprite.h"
#include "grenade_emp.h"
#include "tf_gamerules.h"

#if defined( CLIENT_DLL )

#include "particles_simple.h"

#else

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


int g_iEMPPulseEffectIndex = 0;

// Damage CVars
ConVar	weapon_emp_grenade_duration( "weapon_emp_grenade_duration","5", FCVAR_REPLICATED, "Duration of the EMP grenade's effect." );
ConVar	weapon_emp_grenade_object_duration( "weapon_emp_grenade_object_duration","5", FCVAR_REPLICATED, "Duration of the EMP grenade's effect on objects." );
ConVar	weapon_emp_grenade_radius( "weapon_emp_grenade_radius","256", FCVAR_REPLICATED, "EMP grenade splash radius" );

IMPLEMENT_NETWORKCLASS_ALIASED( GrenadeEMP, DT_GrenadeEMP );

BEGIN_NETWORK_TABLE( CGrenadeEMP, DT_GrenadeEMP )
#if !defined( CLIENT_DLL )
	SendPropEHandle( SENDINFO( m_hLiveSprite ) ),
#else
	RecvPropEHandle( RECVINFO( m_hLiveSprite ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CGrenadeEMP )
	DEFINE_PRED_FIELD( m_hLiveSprite, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( grenade_emp, CGrenadeEMP );
PRECACHE_REGISTER(grenade_emp);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGrenadeEMP::CGrenadeEMP()
{
	SetPredictionEligible( true );

#if defined( CLIENT_DLL )
	m_ParticleEvent.Init( 100 );
#else
	UseClientSideAnimation();
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGrenadeEMP::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( "models/weapons/w_grenade.mdl" );
	PrecacheModel( "sprites/redglow1.vmt" );
	g_iEMPPulseEffectIndex = PrecacheModel( "sprites/lgtning.spr" );

	PrecacheScriptSound( "GrenadeEMP.Bounce" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeEMP::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	//m_flGravity = 1.0;
	SetFriction( 0.75 );
	SetModel( "models/weapons/w_grenade.mdl" );
	SetSize( Vector( -4, -4, -4), Vector(4, 4, 4) );
	SetTouch( BounceTouch );
	SetCollisionGroup( TFCOLLISION_GROUP_GRENADE );

	m_flDetonateTime = gpGlobals->curtime + 4.0;
	SetThink( TumbleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Set my damages to the cvar values
	SetDamage( weapon_emp_grenade_duration.GetFloat() );
	SetDamageRadius( weapon_emp_grenade_radius.GetFloat() );

	// Create a white light
	CBasePlayer *player = ToBasePlayer( GetOwnerEntity() );
	if ( player )
	{
		m_hLiveSprite = SPRITE_CREATE_PREDICTABLE( "sprites/chargeball2.vmt", GetLocalOrigin() + Vector(0,0,1), false );
		if ( m_hLiveSprite )
		{
			m_hLiveSprite->SetOwnerEntity( player );
			m_hLiveSprite->SetPlayerSimulated( player );
			m_hLiveSprite->SetTransparency( kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation );
			m_hLiveSprite->SetBrightness( 255 );
			m_hLiveSprite->SetScale( 0.15, 5.0f );
			m_hLiveSprite->SetAttachment( this, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeEMP::UpdateOnRemove( void )
{
	// Remove our live sprite
	if ( m_hLiveSprite )
	{
		m_hLiveSprite->Remove( );
		m_hLiveSprite = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeEMP::Explode( trace_t *pTrace, int bitsDamageType )
{
#if !defined( CLIENT_DLL )
	// While in scope, this will allow messages to pass through without being filtered.
	CDisablePredictionFiltering dpf;

	// Create EMP pulse effect
	int iEmpRings = 4;
	float fEmpDelay = 0.05f;
	float delay = 0.0f;
	float frac;
	for ( int r = 0 ; r < iEmpRings; r++, delay += fEmpDelay )
	{
		frac = (float)( r )/(float)(iEmpRings - 1);

		CBroadcastRecipientFilter filter;

		// Since this doesn't fire on the client right now, ignore the culling of the local player
		filter.SetIgnorePredictionCull( true );

		te->BeamRingPoint( filter, delay, 
			GetAbsOrigin() + Vector(0,0,32) ,  // origin
			64.0f,					// start radius
			weapon_emp_grenade_radius.GetFloat() * 2,					// end radius
			g_iEMPPulseEffectIndex, 
			0, // halo index
			0, // start frame
			2, // framerate
			0.3f, //  life
			25.0, // width
			50, // spread
			2, // amplitude
			50 + ( 1-frac ) * 200, 
			63, 
			63 + 127 * frac, 
			255 - frac * 127, 
			20 );
	}

	ApplyRadiusEMPEffect( GetThrower(), GetAbsOrigin() + Vector(0,0,16) );
	Remove( );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: EMP enemies around the grenade
//-----------------------------------------------------------------------------
void CGrenadeEMP::ApplyRadiusEMPEffect( CBaseEntity *pOwner, const Vector& vecCenter )
{
	// Oh oh, owner is gone...
	if ( !pOwner )
		return;

#if !defined( CLIENT_DLL )
	CBaseEntity *pEntity = NULL;

	for ( CEntitySphereQuery sphere( vecCenter, weapon_emp_grenade_radius.GetFloat() ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		// Ignore team members, and unaligned targets
		if ( pOwner->InSameTeam( pEntity ) || pEntity->GetTeamNumber() == 0 )
			continue;

		if ( pEntity->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;

		// Make sure it's not blocked by a shield or wall
		trace_t tr;
		if ( TFGameRules()->IsTraceBlockedByWorldOrShield( vecCenter, pEntity->WorldSpaceCenter(), this, DMG_PROBE, &tr ) )
			continue;

		if ( pEntity->CanBePoweredUp() )
		{
			// Is it an object?
			if ( pEntity->Classify() == CLASS_MILITARY )
			{
				pEntity->AttemptToPowerup( POWERUP_EMP, weapon_emp_grenade_object_duration.GetFloat() );
			}
			else
			{
				pEntity->AttemptToPowerup( POWERUP_EMP, weapon_emp_grenade_duration.GetFloat() );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Allow shield parry's
//-----------------------------------------------------------------------------
void CGrenadeEMP::BounceTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	BaseClass::BounceTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Play a distinctive grenade bounce sound to warn nearby players
//-----------------------------------------------------------------------------
void CGrenadeEMP::BounceSound( void )
{
	CPASAttenuationFilter filter( this, "GrenadeEMP.Bounce" );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "GrenadeEMP.Bounce" );
}

//-----------------------------------------------------------------------------
// Purpose: Return the amplitude for the screenshake
//-----------------------------------------------------------------------------
float CGrenadeEMP::GetShakeAmplitude( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
CGrenadeEMP *CGrenadeEMP::Create( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pOwner )
{
	CGrenadeEMP *pGrenade = (CGrenadeEMP*)CREATE_PREDICTED_ENTITY( "grenade_emp" );
	if ( pGrenade )
	{
		UTIL_SetOrigin( pGrenade, vecOrigin );
		pGrenade->SetOwnerEntity( pOwner );
		pGrenade->Spawn();
		pGrenade->SetPlayerSimulated( pOwner );
		pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

		pGrenade->SetThrower( pOwner );

		pGrenade->SetAbsVelocity( vecForward );

		QAngle angles;
		VectorAngles( vecForward, angles );
		pGrenade->SetLocalAngles( angles );

		pGrenade->SetLocalAngularVelocity( SHARED_RANDOMANGLE( -500, 500 ) );
	}

	return pGrenade;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeEMP::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Only think when sapping
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Trail smoke
//-----------------------------------------------------------------------------
void CGrenadeEMP::ClientThink( void )
{
return;

	CSmartPtr<CSimpleEmitter> pEmitter = CSimpleEmitter::Create( "EMPGrenade::Effect" );
	PMaterialHandle	hSphereMaterial = pEmitter->GetPMaterial( "sprites/chargeball" );

	// Add particles at the target.
	float flCur = gpGlobals->frametime;
	while ( m_ParticleEvent.NextEvent( flCur ) )
	{
		Vector vecOrigin = GetAbsOrigin() + RandomVector( -2,2 );
		pEmitter->SetSortOrigin( vecOrigin );

		SimpleParticle *pParticle = (SimpleParticle *) pEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecOrigin );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 0.1f, 0.3f );

		pParticle->m_uchStartSize	= random->RandomFloat(2,4);
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize + 2;

		pParticle->m_vecVelocity = vec3_origin;
		pParticle->m_uchStartAlpha = 128;
		pParticle->m_uchEndAlpha = 0;
		pParticle->m_flRoll	= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta = random->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0] = 128;
		pParticle->m_uchColor[1] = 128;
		pParticle->m_uchColor[2] = 128;
	}
}

int CGrenadeEMP::DrawModel( int flags )
{
	bool bret = BaseClass::DrawModel( flags );

	return bret;
}

#endif

