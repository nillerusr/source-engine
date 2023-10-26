#include "cbase.h"
#include "asw_weapon_night_vision.h"
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
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Night_Vision, DT_ASW_Weapon_Night_Vision )

BEGIN_NETWORK_TABLE( CASW_Weapon_Night_Vision, DT_ASW_Weapon_Night_Vision )
#ifdef CLIENT_DLL
	RecvPropBool	( RECVINFO( m_bVisionActive ) ),
	RecvPropFloat	( RECVINFO( m_flPower ) ),
#else
	SendPropBool	( SENDINFO( m_bVisionActive ) ),
	SendPropFloat	( SENDINFO( m_flPower ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Night_Vision )
	DEFINE_PRED_FIELD_TOL( m_flPower, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),		
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_night_vision, CASW_Weapon_Night_Vision );
PRECACHE_WEAPON_REGISTER( asw_weapon_night_vision );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Night_Vision )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

#else

ConVar asw_night_vision_fade_in_speed( "asw_night_vision_fade_in_speed", "400.0", FCVAR_CHEAT );
ConVar asw_night_vision_fade_out_speed( "asw_night_vision_fade_out_speed", "5000.0", FCVAR_CHEAT );
ConVar asw_night_vision_flash_speed( "asw_night_vision_flash_speed", "1000.0", FCVAR_CHEAT );
ConVar asw_night_vision_flash_max( "asw_night_vision_flash_max", "220.0", FCVAR_CHEAT );
ConVar asw_night_vision_flash_min( "asw_night_vision_flash_min", "0.0", FCVAR_CHEAT );

#endif /* not client */

ConVar asw_night_vision_duration( "asw_night_vision_duration", "20.0", FCVAR_CHEAT | FCVAR_REPLICATED );

CASW_Weapon_Night_Vision::CASW_Weapon_Night_Vision()
{
	m_flSoonestPrimaryAttack = 0;
	m_flPower = asw_night_vision_duration.GetFloat();
#ifdef CLIENT_DLL
	m_flVisionAlpha = 0.0f;
	m_flFlashAlpha = 0.0f;
	m_bOldVisionActive = false;
#endif
}


CASW_Weapon_Night_Vision::~CASW_Weapon_Night_Vision()
{

}

void CASW_Weapon_Night_Vision::Spawn()
{
	BaseClass::Spawn();

	m_flPower = asw_night_vision_duration.GetFloat();
}

bool CASW_Weapon_Night_Vision::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;

	PrimaryAttack();	

	return true;
}

void CASW_Weapon_Night_Vision::PrimaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	if ( !IsVisionActive() && GetPower() < 1.0f )
		return;

	// toggle night vision
	m_bVisionActive = !IsVisionActive();

	if ( IsVisionActive() )
	{
		PlaySoundDirectlyToOwner( "ASW_NightVision.TurnOnFP" );
		PlaySoundToOthers( "ASW_NightVision.TurnOn" );
#ifndef CLIENT_DLL
		pMarine->OnWeaponFired( this, 1 );
#endif
	}
	else
	{
		PlaySoundDirectlyToOwner( "ASW_NightVision.TurnOffFP" );
		PlaySoundToOthers( "ASW_NightVision.TurnOff" );
	}
}

void CASW_Weapon_Night_Vision::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "ASW_NightVision.TurnOn" );
	PrecacheScriptSound( "ASW_NightVision.TurnOff" );
	PrecacheScriptSound( "ASW_NightVision.TurnOnFP" );
	PrecacheScriptSound( "ASW_NightVision.TurnOffFP" );
}

// this weapon doesn't reload
bool CASW_Weapon_Night_Vision::Reload()
{
	return false;
}

void CASW_Weapon_Night_Vision::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	UpdateVisionPower();

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

void CASW_Weapon_Night_Vision::HandleFireOnEmpty()
{
	// Nightvision doesn't require ammo, so when firing empty, activate the NV
	PrimaryAttack();
}

void CASW_Weapon_Night_Vision::UpdateVisionPower()
{
	if ( IsVisionActive() )
	{
		float flNewPower = GetPower() - gpGlobals->frametime;
		flNewPower = MAX( 0.0f, flNewPower );
		m_flPower = flNewPower;
		if ( flNewPower <= 0 )
		{
			m_bVisionActive = false;
			PlaySoundDirectlyToOwner( "ASW_NightVision.TurnOffFP" );
			PlaySoundToOthers( "ASW_NightVision.TurnOff" );
		}
	}
	else
	{
		float flNewPower = GetPower() + gpGlobals->frametime * 1.4f;
		flNewPower = MIN( asw_night_vision_duration.GetFloat(), flNewPower );
		m_flPower = flNewPower;
	}
}

#ifdef CLIENT_DLL
float CASW_Weapon_Night_Vision::UpdateVisionAlpha()
{
	if ( IsVisionActive() )
	{
		m_flVisionAlpha = MIN( 255.0f, m_flVisionAlpha + gpGlobals->frametime * asw_night_vision_fade_in_speed.GetFloat() );
	}
	else
	{
		m_flVisionAlpha = MAX( 0.0f, m_flVisionAlpha - gpGlobals->frametime * asw_night_vision_fade_out_speed.GetFloat() );
	}
	return m_flVisionAlpha;
}

float CASW_Weapon_Night_Vision::UpdateFlashAlpha()
{
	if ( IsVisionActive() != m_bOldVisionActive )
	{
		//if ( IsVisionActive() )
		{
			m_flFlashAlpha = asw_night_vision_flash_max.GetFloat();
		}
		m_bOldVisionActive = IsVisionActive();
	}
	float flMin = 0.0f;
	if ( IsVisionActive() )
	{
		flMin = asw_night_vision_flash_min.GetFloat();
	}
	m_flFlashAlpha = MAX( flMin, m_flFlashAlpha - gpGlobals->frametime * asw_night_vision_flash_speed.GetFloat() );
	return m_flFlashAlpha;
}
#endif

int CASW_Weapon_Night_Vision::ASW_SelectWeaponActivity(int idealActivity)
{
	// we just use the normal 'no weapon' anims for this
	return idealActivity;
}

float CASW_Weapon_Night_Vision::GetBatteryCharge()
{
	return GetPower() / asw_night_vision_duration.GetFloat();
}