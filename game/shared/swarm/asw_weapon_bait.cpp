#include "cbase.h"
#include "asw_weapon_bait.h"
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
#include "asw_bait.h"
#endif
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Bait, DT_ASW_Weapon_Bait )

BEGIN_NETWORK_TABLE( CASW_Weapon_Bait, DT_ASW_Weapon_Bait )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Bait )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_bait, CASW_Weapon_Bait );
PRECACHE_WEAPON_REGISTER( asw_weapon_bait );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Bait )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

#endif /* not client */

ConVar asw_bait_launch_delay("asw_bait_launch_delay", "0.15f", FCVAR_REPLICATED, "Delay before bait is thrown");
ConVar asw_bait_refire_time("asw_bait_refire_time", "0.1f", FCVAR_REPLICATED, "Time between starting a new bait throw");
#define ASW_BAIT_FASTEST_REFIRE_TIME		asw_bait_refire_time.GetFloat()

CASW_Weapon_Bait::CASW_Weapon_Bait()
{
	m_flSoonestPrimaryAttack = 0;
}


CASW_Weapon_Bait::~CASW_Weapon_Bait()
{

}

bool CASW_Weapon_Bait::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;

	PrimaryAttack();	

	return true;
}

void CASW_Weapon_Bait::PrimaryAttack( void )
{	
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	CASW_Marine *pMarine = GetMarine();
	bool bThisActive = (pMarine && pMarine->GetActiveWeapon() == this);

	// weapon is lost when all ammo is gone
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
		m_flDelayedFire = gpGlobals->curtime + asw_bait_launch_delay.GetFloat();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = 0.36f;
		// make sure our primary weapon can't fire while we do the throw anim
		if (!bThisActive && pMarine->GetActiveASWWeapon())
		{
			// if we're offhand activating, make sure our primary weapon can't fire until we're done
			pMarine->GetActiveASWWeapon()->m_flNextPrimaryAttack = m_flNextPrimaryAttack  + asw_bait_launch_delay.GetFloat();
			pMarine->GetActiveASWWeapon()->m_bIsFiring = false;
		}
	}
}

void CASW_Weapon_Bait::DelayedAttack()
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
	Vector newVel = UTIL_LaunchVector( vecSrc, vecDest, GetThrowGravity() ) * 28.0f;
		
	CASW_Bait *pEnt = CASW_Bait::Bait_Create( vecSrc, QAngle(90,0,0), newVel, rotSpeed, pMarine );
	if ( pEnt )
	{
		float flDuration = pEnt->GetDuration();

		//CALL_ATTRIB_HOOK_FLOAT( flDuration, mod_duration );

		pEnt->SetDuration( flDuration );
		pEnt->SetGravity( GetThrowGravity() );
	}
#endif
	// decrement ammo
	m_iClip1 -= 1;

#ifndef CLIENT_DLL
	DestroyIfEmpty( true );
#endif

	m_flSoonestPrimaryAttack = gpGlobals->curtime + ASW_BAIT_FASTEST_REFIRE_TIME;
	
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;
}

void CASW_Weapon_Bait::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();	

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "asw_bait" );
#endif
}

// this weapon doesn't reload
bool CASW_Weapon_Bait::Reload()
{
	return false;
}

void CASW_Weapon_Bait::ItemPostFrame( void )
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


int CASW_Weapon_Bait::ASW_SelectWeaponActivity(int idealActivity)
{
	// we just use the normal 'no weapon' anims for this
	return idealActivity;
}