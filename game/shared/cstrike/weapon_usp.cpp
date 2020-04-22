//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "fx_cs_shared.h"


#if defined( CLIENT_DLL )

	#define CWeaponUSP C_WeaponUSP
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponUSP : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponUSP, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponUSP();

	virtual void Spawn();
	virtual void Precache();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Deploy();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Drop( const Vector &vecVelocity );

 	virtual float GetInaccuracy() const;

	virtual bool Reload();
	virtual void WeaponIdle();

	// We overload this so we can translate all weapon activities to silenced versions.
	virtual bool SendWeaponAnim( int iActivity );

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_USP; }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return m_bSilencerOn; }

	virtual Activity GetDeployActivity( void );

#ifdef CLIENT_DLL
	virtual int GetMuzzleFlashStyle( void );
#endif

	virtual const char		*GetWorldModel( void ) const;
	virtual int				GetWorldModelIndex( void );

private:
	CWeaponUSP( const CWeaponUSP & );

	CNetworkVar( bool, m_bSilencerOn );
	CNetworkVar( float, m_flDoneSwitchingSilencer );	// soonest time switching the silencer will be complete
	float m_flLastFire;

	int m_silencedModelIndex;
	bool m_inPrecache;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponUSP, DT_WeaponUSP )

BEGIN_NETWORK_TABLE( CWeaponUSP, DT_WeaponUSP )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bSilencerOn ) ),
	RecvPropTime( RECVINFO( m_flDoneSwitchingSilencer ) ),
#else
	SendPropBool( SENDINFO( m_bSilencerOn ) ),
	SendPropTime( SENDINFO( m_flDoneSwitchingSilencer ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponUSP )
	DEFINE_PRED_FIELD( m_bSilencerOn, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_flLastFire, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_usp, CWeaponUSP );
PRECACHE_WEAPON_REGISTER( weapon_usp );


Activity g_SilencedTranslations[][2] =
{
	{ ACT_VM_RELOAD, ACT_VM_RELOAD_SILENCED },
	{ ACT_VM_PRIMARYATTACK, ACT_VM_PRIMARYATTACK_SILENCED },
	{ ACT_VM_DRAW, ACT_VM_DRAW_SILENCED },
};



CWeaponUSP::CWeaponUSP()
{
	m_flLastFire = gpGlobals->curtime;
	m_bSilencerOn = false;
	m_flDoneSwitchingSilencer = 0.0f;
	m_inPrecache = false;
}


void CWeaponUSP::Spawn()
{
	//m_iDefaultAmmo = 12;
	m_flAccuracy = 0.92;
	m_bSilencerOn = false;
	m_weaponMode = Primary_Mode;
	m_flDoneSwitchingSilencer = 0.0f;

	//FallInit();// get ready to fall down.
	BaseClass::Spawn();
}


void CWeaponUSP::Precache()
{
	m_inPrecache = true;
	BaseClass::Precache();

	m_silencedModelIndex = CBaseEntity::PrecacheModel( GetCSWpnData().m_szSilencerModel );
	m_inPrecache = false;
}


int CWeaponUSP::GetWorldModelIndex( void )
{
	if ( !m_bSilencerOn || m_inPrecache )
	{
		return m_iWorldModelIndex;
	}
	else
	{
		return m_silencedModelIndex;
	}
}


const char * CWeaponUSP::GetWorldModel( void ) const
{
	if ( !m_bSilencerOn || m_inPrecache )
	{
		return BaseClass::GetWorldModel();
	}
	else
	{
		return GetCSWpnData().m_szSilencerModel;
	}
}


bool CWeaponUSP::Deploy()
{
	m_flAccuracy = 0.92;
	m_flDoneSwitchingSilencer = 0.0f;

	return BaseClass::Deploy();
}

bool CWeaponUSP::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( gpGlobals->curtime < m_flDoneSwitchingSilencer )
	{
		// still switching the silencer.  Cancel the switch.
		m_bSilencerOn = !m_bSilencerOn;
		m_weaponMode = m_bSilencerOn ? Secondary_Mode : Primary_Mode;
		SetWeaponModelIndex( GetWorldModel() );
	}

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponUSP::Drop( const Vector &vecVelocity )
{
	if ( gpGlobals->curtime < m_flDoneSwitchingSilencer )
	{
		// still switching the silencer.  Cancel the switch.
		m_bSilencerOn = !m_bSilencerOn;
		m_weaponMode = m_bSilencerOn ? Secondary_Mode : Primary_Mode;
		SetWeaponModelIndex( GetWorldModel() );
	}

	BaseClass::Drop( vecVelocity );
}

Activity CWeaponUSP::GetDeployActivity( void )
{
	if( IsSilenced() )
	{
		return ACT_VM_DRAW_SILENCED;
	}
	else
	{
		return ACT_VM_DRAW;
	}
}

void CWeaponUSP::SecondaryAttack()
{
	if ( m_bSilencerOn )
	{
		SendWeaponAnim( ACT_VM_DETACH_SILENCER );
	}
	else
	{
		SendWeaponAnim( ACT_VM_ATTACH_SILENCER );
	}
	m_bSilencerOn = !m_bSilencerOn;
	m_weaponMode = m_bSilencerOn ? Secondary_Mode : Primary_Mode;
	m_flDoneSwitchingSilencer = gpGlobals->curtime + 3;

	m_flNextSecondaryAttack = gpGlobals->curtime + 3;
	m_flNextPrimaryAttack = gpGlobals->curtime + 3;
	SetWeaponIdleTime( gpGlobals->curtime + 3 );

	SetWeaponModelIndex( GetWorldModel() );
}


float CWeaponUSP::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;

		if ( m_bSilencerOn )
		{
			if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
				return 1.3f * (1 - m_flAccuracy);
			else if (pPlayer->GetAbsVelocity().Length2D() > 5)
				return 0.25f * (1 - m_flAccuracy);
			else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
				return 0.125f * (1 - m_flAccuracy);
			else
				return 0.15f * (1 - m_flAccuracy);
		}
		else
		{
			if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
				return 1.2f * (1 - m_flAccuracy );
			else if (pPlayer->GetAbsVelocity().Length2D() > 5)
				return 0.225f * (1 - m_flAccuracy);
			else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
				return 0.08f * (1 - m_flAccuracy);
			else
				return 0.1f * (1 - m_flAccuracy);
		}
	}
	else
		return BaseClass::GetInaccuracy();
}


void CWeaponUSP::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	float flCycleTime =  GetCSWpnData().m_flCycleTime;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	m_flAccuracy -= (0.275)*(0.3 - (gpGlobals->curtime - m_flLastFire));

	if (m_flAccuracy > 0.92)
		m_flAccuracy = 0.92;
	else if (m_flAccuracy < 0.6)
		m_flAccuracy = 0.6;

	m_flLastFire = gpGlobals->curtime;

	if (m_iClip1 <= 0)
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			m_bFireOnEmpty = false;
		}

		return;
	}

	pPlayer->m_iShotsFired++;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flCycleTime;

	m_iClip1--;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	
	if ( !m_bSilencerOn )
	{
		pPlayer->DoMuzzleFlash();
	}
	
	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		m_weaponMode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetInaccuracy(),
		GetSpread());

	if (!m_iClip1 && pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}
 
	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	// update accuracy
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyImpulseFire[m_weaponMode];

	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= 2;
	pPlayer->SetPunchAngle( angle );
}


bool CWeaponUSP::Reload()
{
	if ( !DefaultPistolReload() )
		return false;
	
	m_flAccuracy = 0.92;
	return true;
}

void CWeaponUSP::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if (m_iClip1 != 0)
	{
		SetWeaponIdleTime( gpGlobals->curtime + 6.0 );
	}
}

bool CWeaponUSP::SendWeaponAnim( int iActivity )
{
	// Translate the activity?
	if ( m_bSilencerOn )
	{
		for ( int i=0; i < ARRAYSIZE( g_SilencedTranslations ); i++ )
		{
			if ( g_SilencedTranslations[i][0] == iActivity )
			{
				iActivity = g_SilencedTranslations[i][1];
				break;
			}
		}
	}
	
	return BaseClass::SendWeaponAnim( iActivity );
}


#ifdef CLIENT_DLL
int CWeaponUSP::GetMuzzleFlashStyle( void )
{
	if( m_bSilencerOn )
	{
		return CS_MUZZLEFLASH_NONE;
	}
	else
	{
		return CS_MUZZLEFLASH_NORM;
	}
}
#endif
