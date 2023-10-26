#include "cbase.h"

#include "asw_weapon_healgrenade_shared.h"
#include "asw_marine_skills.h"
#ifdef CLIENT_DLL
	#include "prediction.h"
	#include "c_asw_aoegrenade_projectile.h"
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#define CASW_Player C_ASW_Player
	#define CASW_Marine C_ASW_Marine
#else
	#include "asw_marine.h"
	#include "asw_player.h"
	#include "asw_gamerules.h"
	#include "asw_fail_advice.h"
	#include "asw_triggers.h"
	#include "asw_aoegrenade_projectile.h"
	#include "shot_manipulator.h"
	#include "asw_marine_speech.h"
	#include "asw_achievements.h"
	#include "asw_marine_resource.h"
	#include "asw_fail_advice.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//////////////////////////
// HEAL GRENADE PROJECTILE
//////////////////////////

#ifdef CLIENT_DLL

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, ASWPrecacheEffectHealGrenades )
	PRECACHE( MATERIAL, "swarm/effects/blueflare" )
	PRECACHE( MATERIAL, "effects/yellowflare" )
	PRECACHE( MATERIAL, "effects/yellowflare_noz" )
PRECACHE_REGISTER_END()

IMPLEMENT_CLIENTCLASS_DT( C_ASW_HealGrenade_Projectile, DT_ASW_HealGrenade_Projectile, CASW_HealGrenade_Projectile )
END_RECV_TABLE()


ConVar asw_healgrenade( "asw_healgrenade", "90 40 40", 0, "Color of grenades" );

Color C_ASW_HealGrenade_Projectile::GetGrenadeColor( void )
{
	return asw_healgrenade.GetColor();
}

#else

#define HEALGREN_MODEL "models/items/Mine/mine.mdl"

LINK_ENTITY_TO_CLASS( asw_healgrenade_projectile, CASW_HealGrenade_Projectile );

IMPLEMENT_AUTO_LIST( IHealGrenadeAutoList );

BEGIN_DATADESC( CASW_HealGrenade_Projectile )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_HealGrenade_Projectile, DT_ASW_HealGrenade_Projectile )
END_SEND_TABLE()


ConVar asw_healgrenade_gravity( "asw_healgrenade_gravity", "1000", FCVAR_CHEAT, "Gravity of healgrenades" );


CASW_HealGrenade_Projectile::CASW_HealGrenade_Projectile()
{
	SetModelName( MAKE_STRING( HEALGREN_MODEL ) );

	m_flHealPerSecond = 3.0f;
	m_flHealAmountLeft = 0.0f;
	m_nSkin = 2;
}

void CASW_HealGrenade_Projectile::Precache( void )
{
	PrecacheScriptSound( "ASW_MedGrenade.GrenadeActivate" );
	PrecacheScriptSound( "ASW_MedGrenade.ActiveLoop" );
	PrecacheScriptSound( "ASW_MedGrenade.StartBuff" );
	PrecacheScriptSound( "ASW_MedGrenade.BuffLoop" );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();
}

float CASW_HealGrenade_Projectile::GetInfestationCureAmount()
{
	return m_flInfestationCureAmount;
}

float CASW_HealGrenade_Projectile::GetGrenadeGravity( void )
{
	return asw_healgrenade_gravity.GetFloat();
}

void CASW_HealGrenade_Projectile::OnBurnout( void )
{
	ASWFailAdvice()->OnMarineOverhealed( m_flHealAmountLeft );
}

bool CASW_HealGrenade_Projectile::ShouldTouchEntity( CBaseEntity *pEntity )
{
	CASW_Marine *pMarine = CASW_Marine::AsMarine( pEntity );
	if ( pMarine )
	{
		return true;
	}

	return false;
}

void CASW_HealGrenade_Projectile::DoAOE( CBaseEntity *pEntity )
{
	CASW_Marine *pTargetMarine = static_cast< CASW_Marine* >( pEntity );
	if ( !pTargetMarine )
		return;

	CASW_Marine *pMarine = dynamic_cast< CASW_Marine* >( GetOwnerEntity() ); // Carful! This might be null

	float flBaseHealAmount = m_flHealPerSecond * ( gpGlobals->curtime - m_flLastDoAOE );

	// At full health... multiplier is 1 + 0. At empty health multiplier is 1 + 3.0.
	float fHealthPercent = pTargetMarine->IsInfested() ? 0.0f : static_cast< float >( pTargetMarine->GetHealth() ) / pTargetMarine->GetMaxHealth();
	float flLowHealthMultiplier = ( 1.0f + 3.0f * ( 1.0f - fHealthPercent ) );
	float flHealAmount = MIN( flBaseHealAmount * flLowHealthMultiplier, pTargetMarine->GetMaxHealth() - ( pTargetMarine->GetHealth() + pTargetMarine->m_iSlowHealAmount ) );

	float flHealUsed = flHealAmount;

	if ( flHealAmount > 0.0f )
	{
		bool bHealedBefore = false;

		for ( int i = 0; i < m_hHealedEntities.Count(); ++i )
		{
			if ( m_hHealedEntities[ i ].Get() == pEntity )
			{
				bHealedBefore = true;
				break;
			}
		}

		if ( !bHealedBefore )
		{
			// Add it to the list of things we've healed at least once
			m_hHealedEntities.AddToTail( pEntity );

			// Fire event
			IGameEvent * event = gameeventmanager->CreateEvent( "player_heal" );
			if ( event )
			{
				CASW_Player *pPlayer = NULL;
				if ( pMarine )
				{
					pPlayer = pMarine->GetCommander();
				}
				event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
				event->SetInt( "entindex", pTargetMarine->entindex() );
				gameeventmanager->FireEvent( event );
			}

			if ( pMarine )
			{
				CASW_Marine_Resource *pMR = pMarine->GetMarineResource();
				if ( pMR && !pMR->m_bAwardedHealBeaconAchievement && m_hHealedEntities.Count() >= 4 && pMarine->IsInhabited() && pMarine->GetCommander() )
				{
					pMR->m_bAwardedHealBeaconAchievement = true;
					pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_GROUP_HEAL );
				}
			}
		}

		pTargetMarine->AddSlowHeal( flHealAmount, 1, pMarine, this );

		float flHalfHealTotal = m_flHealAmountTotal / 2.0f;

		if ( m_flHealAmountLeft > flHalfHealTotal && ( m_flHealAmountLeft - flHealAmount ) <= flHalfHealTotal )
		{
			ASWFailAdvice()->OnHealGrenadeUsedWell();
		}

		m_flHealAmountLeft -= flHealUsed;

		if ( m_flHealAmountLeft <= 0.0f )
		{
			m_flTimeBurnOut = gpGlobals->curtime;
		}
	
		if ( ASWGameRules()->GetInfoHeal() && pMarine )
		{
			ASWGameRules()->GetInfoHeal()->OnMarineHealed( pMarine, pTargetMarine, NULL );
		}
	}

	if ( pTargetMarine->IsInfested() )
	{
		float fCurePercent = GetInfestationCureAmount();

		// cure infestation
		if ( fCurePercent > 0.0f )
		{
			// Cure infestation at 1/20th normal rate per second (the full 20 seconds or amount used does the full cure)
			pTargetMarine->CureInfestation( pMarine, 1.0f - ( ( 1.0f - fCurePercent ) * ( flHealUsed / m_flHealAmountTotal ) * ( gpGlobals->curtime - m_flLastDoAOE ) ) );
		}
	}
}

CASW_AOEGrenade_Projectile* CASW_HealGrenade_Projectile::Grenade_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity,
																							const AngularImpulse &angVelocity, CBaseEntity *pOwner,
																							float flHealPerSecond, float flInfestationCureAmount, float flRadius, float flDuration, float flTotalHealAmount )
{
	CASW_HealGrenade_Projectile *pGrenade = (CASW_HealGrenade_Projectile *)CreateEntityByName( "asw_healgrenade_projectile" );
	pGrenade->m_flHealPerSecond = flHealPerSecond;
	pGrenade->m_flInfestationCureAmount = flInfestationCureAmount;
	pGrenade->m_flRadius = flRadius;
	pGrenade->m_flDuration = flDuration;
	pGrenade->m_flHealAmountLeft = flTotalHealAmount;
	pGrenade->m_flHealAmountTotal = flTotalHealAmount;
	pGrenade->SetAbsAngles( angles );
	pGrenade->Spawn();
	pGrenade->SetOwnerEntity( pOwner );
	//Msg("making pBuffGrenade with velocity %f,%f,%f\n", velocity.x, velocity.y, velocity.z);
	UTIL_SetOrigin( pGrenade, position );
	pGrenade->SetAbsVelocity( velocity );

	return pGrenade;
}

#endif


//////////////////////
// HEAL GRENADE WEAPON
//////////////////////

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_HealGrenade, DT_ASW_Weapon_HealGrenade )

BEGIN_NETWORK_TABLE( CASW_Weapon_HealGrenade, DT_ASW_Weapon_HealGrenade )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_HealGrenade )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_heal_grenade, CASW_Weapon_HealGrenade );
PRECACHE_WEAPON_REGISTER( asw_weapon_heal_grenade );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_HealGrenade )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

#endif /* not client */

ConVar asw_healgrenade_refire_time( "asw_healgrenade_refire_time", "1.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "Time between starting a new healgrenade throw" );


void CASW_Weapon_HealGrenade::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();	

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "asw_healgrenade_projectile" );
#endif
}


#ifndef CLIENT_DLL
CASW_AOEGrenade_Projectile* CASW_Weapon_HealGrenade::CreateProjectile( const Vector &vecSrc, const QAngle &angles, const Vector &vecVel, 
																		const AngularImpulse &rotSpeed, CBaseEntity *pOwner )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return NULL;

	float flHealAmount = MarineSkills()->GetSkillBasedValueByMarine( pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEAL_GRENADE_HEAL_AMOUNT );
	float flDuration = 20.0f;
	float flRadius = 100.0f;

	float flHealthPerSecond = 3.0f;
	float flInfestationCureAmount = MarineSkills()->GetSkillBasedValueByMarine( pMarine, ASW_MARINE_SKILL_XENOWOUNDS ) / 100.0f;

	return CASW_HealGrenade_Projectile::Grenade_Projectile_Create( vecSrc, angles, vecVel, rotSpeed, pOwner,
		flHealthPerSecond, flInfestationCureAmount, flRadius, flDuration, flHealAmount );
}
#endif


float CASW_Weapon_HealGrenade::GetRefireTime( void )
{
	return asw_healgrenade_refire_time.GetFloat();
}

void CASW_Weapon_HealGrenade::PrimaryAttack( void )
{	
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
#ifndef CLIENT_DLL
	bool bThisActive = (pMarine && pMarine->GetActiveWeapon() == this);
#endif

	if ( !pMarine )
		return;

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	// sets the animation on the weapon model iteself
	SendWeaponAnim( GetPrimaryAttackActivity() );

	// sets the animation on the marine holding this weapon
	//pMarine->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
	Vector	vecSrc		= pMarine->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187

	if ( !pMarine->IsInhabited() && vecSrc.DistTo( pMarine->m_vecOffhandItemSpot ) < 150.0f )
	{
		vecSrc.x = pMarine->m_vecOffhandItemSpot.x;
		vecSrc.y = pMarine->m_vecOffhandItemSpot.y;
		vecSrc.z += 50.0f;
	}

	QAngle ang = pPlayer->EyeAngles();
	ang.x = 0;
	ang.z = 0;
	CShotManipulator Manipulator( vecAiming );
	AngularImpulse rotSpeed(0,0,720);

	// create a pellet at some random spread direction			
	Vector newVel = Manipulator.ApplySpread(GetBulletSpread());
	if ( pMarine->GetWaterLevel() != 3 )
	{
		CreateProjectile( vecSrc, ang, newVel, rotSpeed, pMarine );
		pMarine->OnWeaponFired( this, 1 );
	}

	pMarine->GetMarineSpeech()->Chatter(CHATTER_MEDKIT);
#endif

	// decrement ammo
	m_iClip1 -= 1;

#ifndef CLIENT_DLL
	// destroy if empty
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		ASWFailAdvice()->OnMedSatchelEmpty();

		pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_NONE );

		if ( pMarine )
		{
			pMarine->Weapon_Detach(this);
			if ( bThisActive )
				pMarine->SwitchToNextBestWeapon(NULL);
		}
		Kill();

		return;
	}
#endif

	m_flSoonestPrimaryAttack = gpGlobals->curtime + GetRefireTime();
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;

	//m_flLastFireTime = gpGlobals->curtime;
}

bool CASW_Weapon_HealGrenade::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	PrimaryAttack();

	return true;
}