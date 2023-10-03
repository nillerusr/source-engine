#include "cbase.h"
#include "asw_weapon_t75.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_t75.h"
#include "asw_marine_skills.h"
#include "asw_marine_speech.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_FLARES_FASTEST_REFIRE_TIME		0.1f

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_T75, DT_ASW_Weapon_T75 )

BEGIN_NETWORK_TABLE( CASW_Weapon_T75, DT_ASW_Weapon_T75 )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_T75 )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_t75, CASW_Weapon_T75 );
PRECACHE_WEAPON_REGISTER( asw_weapon_t75 );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_T75 )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

#endif /* not client */

CASW_Weapon_T75::CASW_Weapon_T75()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 2048;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

	m_flSoonestPrimaryAttack = gpGlobals->curtime;
}


CASW_Weapon_T75::~CASW_Weapon_T75()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_T75::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

bool CASW_Weapon_T75::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	PrimaryAttack();

	return true;
}

#define ASW_MINE_VELOCITY	140

void CASW_Weapon_T75::PrimaryAttack( void )
{	
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
#ifndef CLIENT_DLL
	bool bThisActive = (pMarine && pMarine->GetActiveWeapon() == this);
#endif

	// weapon is lost when all charges are gone
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

	if ( !pMarine || pMarine->GetWaterLevel() == 3 )		// firing from a marine
		return;

	// sets the animation on the weapon model iteself
	SendWeaponAnim( GetPrimaryAttackActivity() );

#ifndef CLIENT_DLL
	Vector	vecSrc		= pMarine->Weapon_ShootPosition( );
	// TODO: Fix for AI
	Vector	vecAiming	= pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());

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
			
	Vector newVel = Manipulator.ApplySpread(GetBulletSpread());

	newVel *= ASW_MINE_VELOCITY;

	if ( !pMarine->IsInhabited() )
	{
		newVel = vec3_origin;
	}

	CASW_T75 *pT75 = CASW_T75::ASW_T75_Create( vecSrc, ang, newVel, rotSpeed, pMarine, this );
	if ( pT75 && !pMarine->IsInhabited() )
	{
		pT75->ActivateUseIcon( pMarine, ASW_USE_RELEASE_QUICK );
	}

	//pMarine->GetMarineSpeech()->Chatter(CHATTER_MINE_DEPLOYED);
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

void CASW_Weapon_T75::Precache()
{
	BaseClass::Precache();	
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "asw_t75" );
#endif
}

// no reloading
bool CASW_Weapon_T75::Reload()
{
	return false;
}

void CASW_Weapon_T75::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = GetCommander();

	if ( pOwner == NULL )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
}
