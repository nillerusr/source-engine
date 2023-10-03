#include "cbase.h"
#include "asw_weapon_flares_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "prediction.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_flare_projectile.h"
#endif
#include "engine/IVDebugOverlay.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Flares, DT_ASW_Weapon_Flares )

BEGIN_NETWORK_TABLE( CASW_Weapon_Flares, DT_ASW_Weapon_Flares )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Flares )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_flares, CASW_Weapon_Flares );
PRECACHE_WEAPON_REGISTER( asw_weapon_flares );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Flares )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

ConVar asw_flare_throw_speed("asw_flare_throw_speed", "50.0f", FCVAR_CHEAT, "Velocity of thrown flares");

#endif /* not client */

ConVar asw_flare_launch_delay("asw_flare_launch_delay", "0.15f", FCVAR_REPLICATED, "Delay before flares are thrown");
ConVar asw_flare_refire_time("asw_flare_refire_time", "0.1f", FCVAR_REPLICATED, "Time between starting a new flare throw");
#define ASW_FLARES_FASTEST_REFIRE_TIME		asw_flare_refire_time.GetFloat()

CASW_Weapon_Flares::CASW_Weapon_Flares()
{
	m_flSoonestPrimaryAttack = 0;
}


CASW_Weapon_Flares::~CASW_Weapon_Flares()
{

}

bool CASW_Weapon_Flares::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;

	PrimaryAttack();	

	return true;
}

#define FLARE_PROJECTILE_AIR_VELOCITY	400

void CASW_Weapon_Flares::PrimaryAttack( void )
{	
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	bool bThisActive = (pMarine && pMarine->GetActiveWeapon() == this);

	// flare weapon is lost when all flares are gone
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		//Reload();
#ifndef CLIENT_DLL
		if (pMarine)
		{
			pMarine->Weapon_Detach(this);
			if (bThisActive)
				pMarine->SwitchToNextBestWeapon(NULL);
		}
		Kill();
#endif
		return;
	}

	if (pMarine && gpGlobals->curtime > m_flDelayedFire)
	{
		
#ifdef CLIENT_DLL
		if ( !prediction->InPrediction() || prediction->IsFirstTimePredicted() )
		{
			pMarine->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );
		}
#else
		pMarine->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );
		pMarine->OnWeaponFired( this, 1 );
#endif

		// start our delayed attack
		m_bShotDelayed = true;
		m_flDelayedFire = gpGlobals->curtime + asw_flare_launch_delay.GetFloat();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = 0.36f;
		// make sure our primary weapon can't fire while we do the throw anim
		if (!bThisActive && pMarine->GetActiveASWWeapon())
		{
			// if we're offhand activating, make sure our primary weapon can't fire until we're done
			pMarine->GetActiveASWWeapon()->m_flNextPrimaryAttack = m_flNextPrimaryAttack  + asw_flare_launch_delay.GetFloat();
			pMarine->GetActiveASWWeapon()->m_bIsFiring = false;
		}
	}
}

void CASW_Weapon_Flares::DelayedAttack()
{
	m_bShotDelayed = false;
	
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetWaterLevel() == 3 )
		return;

	// sets the animation on the marine holding this weapon
	//pMarine->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
	Vector vecSrc = pMarine->GetOffhandThrowSource();
	AngularImpulse rotSpeed(0,0,720);

	Vector vecDest = pPlayer->GetCrosshairTracePos();
	if ( !pMarine->IsInhabited() )
	{
		vecDest = pMarine->GetOffhandItemSpot();
	}
	Vector newVel = UTIL_LaunchVector( vecSrc, vecDest, GetThrowGravity() ) * 28.0f;
		
	CASW_Flare_Projectile *pFlare = CASW_Flare_Projectile::Flare_Projectile_Create( vecSrc, QAngle(90,0,0), newVel, rotSpeed, pMarine );
	if ( pFlare )
	{
		float flDuration = pFlare->GetDuration();

		//CALL_ATTRIB_HOOK_FLOAT( flDuration, mod_duration );

		pFlare->SetDuration( flDuration );
		pFlare->SetGravity( GetThrowGravity() );
	}
#endif
	// decrement ammo
	m_iClip1 -= 1;

#ifndef CLIENT_DLL
	DestroyIfEmpty( true );
#endif

	m_flSoonestPrimaryAttack = gpGlobals->curtime + ASW_FLARES_FASTEST_REFIRE_TIME;
	
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;
}

void CASW_Weapon_Flares::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();	

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "asw_flare_projectile" );
#endif
}

// this weapon doesn't reload
bool CASW_Weapon_Flares::Reload()
{
	return false;
}

void CASW_Weapon_Flares::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = GetCommander();
	if ( !pOwner )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
}


int CASW_Weapon_Flares::ASW_SelectWeaponActivity(int idealActivity)
{
	// we just use the normal 'no weapon' anims for this
	return idealActivity;
}

#ifdef CLIENT_DLL

void CASW_Weapon_Flares::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	BaseClass::OnDataChanged( updateType );
}

void CASW_Weapon_Flares::ClientThink()
{
	BaseClass::ClientThink();

	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( pMarine && pMarine->IsInhabited() && pMarine->GetActiveWeapon() == this )
	{
		ShowThrowArc();
	}
}



extern ConVar sv_gravity;

void CASW_Weapon_Flares::ShowThrowArc()
{
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetWaterLevel() == 3 )
		return;

	Vector vecSrc = pMarine->GetOffhandThrowSource();

	Vector vecDest = pPlayer->GetCrosshairTracePos();
	Vector vecVelocity = UTIL_LaunchVector( vecSrc, vecDest, GetThrowGravity() ) * 28.0f;

	//debugoverlay->AddLineOverlay( vecSrc, vecDest, 255, 0, 0, true, 0.01f );

	
}
#endif