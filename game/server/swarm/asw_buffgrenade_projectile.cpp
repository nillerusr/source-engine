#include "cbase.h"

#include "asw_buffgrenade_projectile.h"
#include "asw_marine.h"
#include "asw_shareddefs.h"
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define BUFFGREN_MODEL "models/items/Mine/mine.mdl"


LINK_ENTITY_TO_CLASS( asw_buffgrenade_projectile, CASW_BuffGrenade_Projectile );

BEGIN_DATADESC( CASW_BuffGrenade_Projectile )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_BuffGrenade_Projectile, DT_ASW_BuffGrenade_Projectile )
END_SEND_TABLE()


ConVar asw_buffgrenade_gravity( "asw_buffgrenade_gravity", "1000", FCVAR_CHEAT, "Gravity of buffgrenades" );


CASW_BuffGrenade_Projectile::CASW_BuffGrenade_Projectile()
{
	SetModelName( MAKE_STRING( BUFFGREN_MODEL ) );
	m_pSkill = NULL;
	m_iSkillPoints = 1;
	m_nSkin = 1;
}

void CASW_BuffGrenade_Projectile::Precache( void )
{
	PrecacheScriptSound( "ASW_BuffGrenade.GrenadeActivate" );
	PrecacheScriptSound( "ASW_BuffGrenade.ActiveLoop" );
	PrecacheScriptSound( "ASW_BuffGrenade.StartBuff" );
	PrecacheScriptSound( "ASW_BuffGrenade.BuffLoop" );
	
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheModel("models/items/shield_bubble/shield_bubble.mdl");	
	//PrecacheModel("models/props_combine/coreball.mdl");	

	BaseClass::Precache();
}

float CASW_BuffGrenade_Projectile::GetGrenadeGravity( void )
{
	return asw_buffgrenade_gravity.GetFloat();
}

bool CASW_BuffGrenade_Projectile::ShouldTouchEntity( CBaseEntity *pEntity )
{
	CASW_Marine *pMarine = CASW_Marine::AsMarine( pEntity );
	if ( pMarine )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add the buff to this marine
//-----------------------------------------------------------------------------
void CASW_BuffGrenade_Projectile::StartAOE( CBaseEntity *pEntity )
{
	BaseClass::StartAOE( pEntity );

	CASW_Marine *pMarine = CASW_Marine::AsMarine( pEntity );
	if ( !pMarine )
	{
		return;
	}

	pMarine->AddDamageBuff( this, m_flTimeBurnOut - gpGlobals->curtime );
	//NDebugOverlay::Box( pMarine->GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 200, 0.5 );

	EHANDLE hMarine = pMarine;
	if ( m_hBuffedMarines.Find( hMarine ) == m_hBuffedMarines.InvalidIndex() )
	{
		m_hBuffedMarines.AddToTail( hMarine );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop healing this target
//-----------------------------------------------------------------------------
bool CASW_BuffGrenade_Projectile::StopAOE( CBaseEntity *pEntity )
{
	bool bFound = BaseClass::StopAOE( pEntity );

	CASW_Marine *pMarine = CASW_Marine::AsMarine( pEntity );
	if ( !pMarine )
	{
		return bFound;
	}

	if ( bFound )
	{
		pMarine->RemoveDamageBuff();
	}

	return bFound;
}


CASW_BuffGrenade_Projectile* CASW_BuffGrenade_Projectile::Grenade_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity,
																				   const AngularImpulse &angVelocity, CBaseEntity *pOwner,
																				   float flRadius, float flDuration )
{
	CASW_BuffGrenade_Projectile *pGrenade = (CASW_BuffGrenade_Projectile *)CreateEntityByName( "asw_buffgrenade_projectile" );
	pGrenade->m_flRadius = flRadius;
	pGrenade->m_flDuration = flDuration;
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->SetOwnerEntity( pOwner );
	//Msg("making pBuffGrenade with velocity %f,%f,%f\n", velocity.x, velocity.y, velocity.z);
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );

	return pGrenade;
}
