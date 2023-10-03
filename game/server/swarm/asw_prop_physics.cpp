#include "cbase.h"
#include "asw_prop_physics.h"
#include "asw_burning.h"
#include "asw_player.h"
#include "soundent.h"
#include "asw_marine.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS(asw_prop_physics, CASW_Prop_Physics);

// Remap old prop physics entities to infested versions too
LINK_ENTITY_TO_CLASS( physics_prop, CASW_Prop_Physics );
LINK_ENTITY_TO_CLASS( prop_physics, CASW_Prop_Physics );	
LINK_ENTITY_TO_CLASS( prop_physics_override, CASW_Prop_Physics );	

IMPLEMENT_SERVERCLASS_ST( CASW_Prop_Physics, DT_ASW_Prop_Physics )
	SendPropBool( SENDINFO( m_bIsAimTarget ) ),	// asw - is this prop an autoaim target?	
	SendPropBool( SENDINFO( m_bOnFire) ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Prop_Physics )
	DEFINE_FIELD( m_bIsAimTarget, FIELD_BOOLEAN ),	
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),	
	DEFINE_KEYFIELD( m_bBulletForceImmune, FIELD_BOOLEAN, "BulletForceImmune" ),	
	DEFINE_FIELD( m_bMeleeHit, FIELD_BOOLEAN ),	
END_DATADESC()

void CASW_Prop_Physics::OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker )
{
	// spawn some smoke/clouds, etc
	// UTIL_Smoke(GetAbsOrigin(), random->RandomInt(45,55), random->RandomInt(10,15));

	BaseClass::OnBreak(vecVelocity, angVel, pBreaker);
}

void CASW_Prop_Physics::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	if( IsOnFire() )
		return;

	if( !HasInteraction( PROPINTER_FIRE_FLAMMABLE ) )
		return;

	AddFlag( FL_ONFIRE );
	m_bOnFire = true;

	if (ASWBurning())
	{
		ASWBurning()->BurnEntity(this, NULL, flFlameLifetime, 0.4f, 5.0f * 0.4f);	// 5 dps, applied every 0.4 seconds
	}

	if ( g_pGameRules->ShouldBurningPropsEmitLight() )
	{
		GetEffectEntity()->AddEffects( EF_DIMLIGHT );
	}

	// Frighten AIs, just in case this is an exploding thing.
	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), 128.0f, 1.0f, this, SOUNDENT_CHANNEL_REPEATED_DANGER );

	m_OnIgnite.FireOutput( this, this );
}

int CASW_Prop_Physics::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(inputInfo.GetAttacker());

	// check for notifying a marine that he's shooting a breakable prop
	if ( inputInfo.GetAttacker() && inputInfo.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		if ( pMarine )
		{
			pMarine->HurtJunkItem(this, inputInfo);
		}
	}

	if ( m_bBulletForceImmune )
	{
		if ( inputInfo.GetDamageType() & DMG_BULLET )
		{
			CTakeDamageInfo newInfo( inputInfo );
			newInfo.SetDamageForce(vec3_origin);
			return BaseClass::OnTakeDamage(newInfo);
		}
	}

	if( !m_bMeleeHit && inputInfo.GetDamageType() & DMG_CLUB )
	{
		CASW_Player *pPlayerAttacer = NULL;
		if ( pMarine )
		{
			pPlayerAttacer = pMarine->GetCommander();
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "physics_melee" );
		if ( event )
		{
			event->SetInt( "attacker", ( pPlayerAttacer ? pPlayerAttacer->GetUserID() : 0 ) );
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event );
		}

		m_bMeleeHit = true;
	}

	return BaseClass::OnTakeDamage(inputInfo);
}

bool CASW_Prop_Physics::KickingVismonCallback( CBaseEntity *pPhysProp, CBasePlayer *pViewingPlayer )
{
	CASW_Prop_Physics *pASWPropPhysics = static_cast< CASW_Prop_Physics* >( pPhysProp );
	pASWPropPhysics->m_bMeleeHit = false;

	IGameEvent * event = gameeventmanager->CreateEvent( "physics_visible" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "subject", pPhysProp->entindex() );
		event->SetString( "type", "kicking" );
		event->SetString( "entityname", STRING( pPhysProp->GetEntityName() ) );
		gameeventmanager->FireEvent( event );
	}

	return false;
}

bool CASW_Prop_Physics::DetonateVismonCallback( CBaseEntity *pPhysProp, CBasePlayer *pViewingPlayer )
{
	IGameEvent * event = gameeventmanager->CreateEvent( "physics_visible" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "subject", pPhysProp->entindex() );
		event->SetString( "type", "detonate" );
		event->SetString( "entityname", STRING( pPhysProp->GetEntityName() ) );
		gameeventmanager->FireEvent( event );
	}

	return false;
}

void CASW_Prop_Physics::Spawn( void )
{
	BaseClass::Spawn();

	if ( HasSpawnFlags( SF_PHYSPROP_AIMTARGET ) )
	{
		AddFlag( FL_AIMTARGET );
		m_bIsAimTarget = true;	// let the client know
	}

	if ( m_bBulletForceImmune )
	{
		if ( m_iMinHealthDmg >= 40 )
		{
			VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, &CASW_Prop_Physics::DetonateVismonCallback, NULL );
		}
		else
		{
			VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, &CASW_Prop_Physics::KickingVismonCallback, NULL );
		}
	}
}

void CASW_Prop_Physics::Event_Killed( const CTakeDamageInfo &info )
{
	// clear autoaim target flag when we die, so our shards don't inherit it
	RemoveSpawnFlags( SF_PHYSPROP_AIMTARGET);

	BaseClass::Event_Killed( info );
}