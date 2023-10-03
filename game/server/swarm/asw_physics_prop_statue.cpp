\//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "asw_physics_prop_statue.h"
#include "particle_parse.h"
#include "asw_alien.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( asw_physics_prop_statue, CASWStatueProp );

IMPLEMENT_SERVERCLASS_ST( CASWStatueProp, DT_ASWStatueProp )
END_SEND_TABLE()

BEGIN_DATADESC( CASWStatueProp )
	DEFINE_THINKFUNC( AutoShatterThink ),
	DEFINE_THINKFUNC( BreakThink ),
END_DATADESC()

extern ConVar asw_drop_money;
extern ConVar asw_money;
extern ConVar asw_alien_money_chance;

//ConVarRef *s_vcollide_wireframe = NULL;


CASWStatueProp::CASWStatueProp( void )
{
	m_pszSourceClass = NULL;
}

void CASWStatueProp::Spawn( void )
{
	BaseClass::Spawn();
	SetHealth( 1 );

	//AddFlag( FL_AIMTARGET );
	if ( m_hInitBaseAnimating.Get() )
	{
		CASW_Alien *pAlien = dynamic_cast< CASW_Alien* >( m_hInitBaseAnimating.Get() );
		if ( pAlien )
		{
			m_pszSourceClass = pAlien->GetClassname();
		}
		SetModelScale( m_hInitBaseAnimating->GetModelScale() );
	}

	m_flFrozen = 1.0f;
}

void CASWStatueProp::Precache( void )
{
	PrecacheScriptSound("IceBody.Shatter");
}

int	CASWStatueProp::OnTakeDamage( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage( info );
}

void CASWStatueProp::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	// FIXME: Short delay before we actually remove so that the client statue gets a network update before we need it
	// This isn't a reliable way to do this and needs to be rethought.
	AddSolidFlags( FSOLID_NOT_SOLID );

	CPASAttenuationFilter filter2( this, "IceBody.Shatter" );
	EmitSound( filter2, entindex(), "IceBody.Shatter" );

	if ( m_pszSourceClass && ASWGameRules() )
	{
		ASWGameRules()->DropPowerup( this, info, m_pszSourceClass );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
	SetThink( &CBaseEntity::SUB_Remove );
	// can't easily get the gibs to receive a "frozen" amount which is needed for the frozen detail overlay effect
	//SetThink( &CASWStatueProp::BreakThink );
}

void CASWStatueProp::Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay )
{
	// Can't freeze a statue
	TakeDamage( CTakeDamageInfo( pFreezer, pFreezer, 1, DMG_GENERIC ) );
}

void CASWStatueProp::EnableAutoShatter( const CTakeDamageInfo &dmgInfo )
{
	m_ShatterDamageInfo = dmgInfo; 

	SetThink( &CASWStatueProp::AutoShatterThink );
	SetNextThink( gpGlobals->curtime );
}

void CASWStatueProp::AutoShatterThink()
{
	if ( m_ShatterDamageInfo.GetDamage() < GetHealth() )
	{
		m_ShatterDamageInfo.SetDamage( GetHealth() * 2.0f );
	}
	TakeDamage( m_ShatterDamageInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWStatueProp::BreakThink( void )
{
	//CTakeDamageInfo info;
	//info.SetAttacker( this );
	Break( m_ShatterDamageInfo.GetInflictor(), m_ShatterDamageInfo );
}

CBaseEntity *CreateASWServerStatue( CBaseAnimating *pAnimating, int collisionGroup, const CTakeDamageInfo &dmgInfo, bool bAutoShatter )
{
	CASWStatueProp *pStatue = static_cast<CASWStatueProp *>( CreateEntityByName( "asw_physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();
		if ( bAutoShatter )
		{
			pStatue->EnableAutoShatter( dmgInfo );
		}
	}

	return pStatue;
}

CBaseEntity *CreateASWServerStatueFromOBBs( const CUtlVector<outer_collision_obb_t> &vecSphereOrigins, CBaseAnimating *pAnimating )
{
	Assert( vecSphereOrigins.Count() > 0 );

	if ( vecSphereOrigins.Count() <= 0 )
		return NULL;

	CASWStatueProp *pStatue = static_cast<CASWStatueProp *>( CreateEntityByName( "asw_physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_pInitOBBs = &vecSphereOrigins;

		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();

		pStatue->AddEffects( EF_NODRAW );
		pStatue->CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );
		pStatue->AddSolidFlags( ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMBOXTEST ) | ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMRAYTEST ) );

		pAnimating->SetParent( pStatue );

		// You'll need to keep track of the child for collision rules
		pStatue->SetThink( &CASWStatueProp::CollisionPartnerThink );
		pStatue->SetNextThink( gpGlobals->curtime + 1.0f );
	}

	return pStatue;
}

