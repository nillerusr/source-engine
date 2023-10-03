#include "cbase.h"

#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "weapon_flaregun.h"
#include "decals.h"
#include "ai_basenpc.h"
#include "asw_marine.h"
#include "asw_firewall_piece.h"
#include "asw_marine_skills.h"
#include "asw_player.h"
#include "asw_marine_speech.h"
#include "asw_t75.h"
#include "world.h"
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_fx_shared.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_T75_MODEL "models/swarmprops/miscdeco/bridgeexplosivesmesh.mdl"
#define ASW_MINE_EXPLODE_TIME 0.6f

BEGIN_DATADESC( CASW_T75 )
	//DEFINE_FUNCTION( TouchT75 ),
	//DEFINE_FUNCTION( CountdownThink ),
	DEFINE_FIELD( m_bArmed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDamage, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamageRadius, FIELD_FLOAT ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_T75, DT_ASW_T75 )
	SendPropBool( SENDINFO( m_bArmed ) ),
	SendPropBool(SENDINFO(m_bIsInUse)),
	SendPropFloat(SENDINFO(m_flArmProgress)),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( asw_t75, CASW_T75 );

CASW_T75::CASW_T75()
{
	m_bIsInUse = false;
	m_flArmProgress = 0;
	m_bArmed = false;
}

CASW_T75::~CASW_T75( void )
{

}

void CASW_T75::Spawn( void )
{
	Precache();
	SetModel( ASW_T75_MODEL );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID || FSOLID_TRIGGER);
	SetMoveType( MOVETYPE_FLYGRAVITY );
	m_takedamage	= DAMAGE_NO;

	// check for attaching to elevators
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector(0, 0, 2),
		GetAbsOrigin() - Vector(0, 0, 32), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f && tr.m_pEnt && !tr.m_pEnt->IsWorld() && !tr.m_pEnt->IsNPC() )
	{
		SetParent( tr.m_pEnt );
	}

	m_flDamage = 600.0f;
	m_flDamageRadius = 250.0f;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
	SetTouch( &CASW_T75::TouchT75 );
}

void CASW_T75::TouchT75( CBaseEntity *pOther )
{
	// Slow down
	Vector vecNewVelocity = GetAbsVelocity();
	vecNewVelocity.x *= 0.8f;
	vecNewVelocity.y *= 0.8f;
	SetAbsVelocity( vecNewVelocity );	
}

void CASW_T75::Arm()
{
	m_bArmed = true;
}

extern ConVar asw_medal_explosive_kills;

void CASW_T75::Explode()
{
	m_takedamage	= DAMAGE_NO;	

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);
	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
		this, COLLISION_GROUP_NONE, &tr);

	EmitSound( "ASW_T75.Explode" );
	EmitSound( "ASWGrenade.Incendiary" );

	// throw out some flames
	CEffectData	data;			
	data.m_vOrigin = GetAbsOrigin();
	DispatchEffect( "ASWFireBurst", data );

	if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
	{
		// non-world needs smaller decals
		if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
		{
			UTIL_DecalTrace( &tr, "SmallScorch" );
		}
	}
	else
	{
		UTIL_DecalTrace( &tr, "Scorch" );
	}

	UTIL_ASW_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	UTIL_ASW_GrenadeExplosion( GetAbsOrigin(), m_flDamageRadius );

	int iPreExplosionKills = 0;
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOwnerEntity());
	if (pMarine && pMarine->GetMarineResource())
		iPreExplosionKills = pMarine->GetMarineResource()->m_iAliensKilled;

	ASWGameRules()->RadiusDamage ( CTakeDamageInfo( this, GetOwnerEntity(), m_flDamage, DMG_BLAST ), GetAbsOrigin(), m_flDamageRadius, CLASS_NONE, NULL );

	if (pMarine && pMarine->GetMarineResource())
	{
		int iKilledByExplosion = pMarine->GetMarineResource()->m_iAliensKilled - iPreExplosionKills;
		if (iKilledByExplosion > pMarine->GetMarineResource()->m_iSingleGrenadeKills)
		{
			pMarine->GetMarineResource()->m_iSingleGrenadeKills = iKilledByExplosion;
			if ( iKilledByExplosion > asw_medal_explosive_kills.GetInt() && pMarine->GetCommander() && pMarine->IsInhabited() )
			{
				pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_GRENADE_MULTI_KILL );
			}
		}

		// count as a shot fired
		pMarine->GetMarineResource()->UsedWeapon(NULL, 1);
		pMarine->OnWeaponFired( m_hCreatorWeapon.Get(), 1 );
	}
		
	UTIL_Remove( this );
}

void CASW_T75::Precache( void )
{
	PrecacheModel( ASW_T75_MODEL );
	
	PrecacheScriptSound( "ASW_T75.Explode" );
	PrecacheScriptSound( "ASW_T75.CountdownBeep" );

	BaseClass::Precache();
}

CASW_T75* CASW_T75::ASW_T75_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon /*= NULL */ )
{
	CASW_T75 *pEnt = (CASW_T75*)CreateEntityByName( "asw_t75" );
	pEnt->SetAbsAngles( angles );
	pEnt->Spawn();
	pEnt->SetOwnerEntity( pOwner );
	UTIL_SetOrigin( pEnt, position );
	pEnt->SetAbsVelocity( velocity );
	pEnt->m_hCreatorWeapon.Set( pCreatorWeapon );

	return pEnt;
}

bool CASW_T75::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

// player has used this item
void CASW_T75::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( !m_bIsInUse && !m_bArmed )
	{		
		pMarine->StartUsing( this );
		pMarine->GetMarineSpeech()->Chatter( CHATTER_USE );
	}
}

#define T75_ARM_TIME 3.0f
void CASW_T75::MarineUsing(CASW_Marine* pMarine, float deltatime)
{
	if ( m_bIsInUse && !m_bArmed.Get() && pMarine )
	{
		float fSetupAmount = (deltatime * (1.0f/T75_ARM_TIME));
		m_flArmProgress += fSetupAmount;
		if (m_flArmProgress >= 1.0f)
		{
			m_flArmProgress = 1.0f;

			pMarine->StopUsing();
			m_bArmed = true;
			pMarine->GetMarineSpeech()->Chatter(CHATTER_MINE_DEPLOYED);  // TODO: Chatter for T75 being armed

			// set detonate time
			m_iCountdown = 6;
			SetThink( &CASW_T75::CountdownThink );
			SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}
}

void CASW_T75::MarineStartedUsing(CASW_Marine* pMarine)
{
	m_bIsInUse = true;
}

void CASW_T75::MarineStoppedUsing(CASW_Marine* pMarine)
{
	m_bIsInUse = false;
}

void CASW_T75::CountdownThink()
{
	m_iCountdown--;
	if ( m_iCountdown <= 0 )
	{
		Explode();
		return;
	}
		
	EmitSound( "ASW_T75.CountdownBeep" );

	Vector vecPos = WorldSpaceCenter();
	CPASFilter filter( GetAbsOrigin() );
	UserMessageBegin( filter, "ASWDamageNumber" );
	WRITE_SHORT( m_iCountdown );
	WRITE_SHORT( DAMAGE_FLAG_T75 );
	WRITE_SHORT( entindex() ); 
	WRITE_FLOAT( vecPos.x );
	WRITE_FLOAT( vecPos.y );
	WRITE_FLOAT( vecPos.z );
	MessageEnd();

	SetNextThink( gpGlobals->curtime + 1.0f );
}