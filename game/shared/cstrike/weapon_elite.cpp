//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "fx_cs_shared.h"


#if defined( CLIENT_DLL )

	#define CWeaponElite C_WeaponElite
	#include "c_cs_player.h"
	#include "c_te_effect_dispatch.h"

#else

	#include "cs_player.h"

#endif


class CWeaponElite : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponElite, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponElite();

	virtual void Spawn();
	virtual void Precache();

	virtual void PrimaryAttack();
	virtual bool Deploy();

	virtual bool Reload();

	virtual void WeaponIdle();

 	virtual float GetInaccuracy() const;
	
	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_ELITE; }

#ifdef CLIENT_DLL
	virtual int		GetMuzzleAttachment( void );
	virtual bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
#endif

	virtual const char		*GetWorldModel( void ) const;
	virtual int				GetWorldModelIndex( void );

protected:
	bool FiringLeft() const;

private:
	
	CWeaponElite( const CWeaponElite & );
	float		m_flLastFire;

	int m_droppedModelIndex;
	bool m_inPrecache;
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponElite, DT_WeaponElite )

BEGIN_NETWORK_TABLE( CWeaponElite, DT_WeaponElite )
END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponElite )
	DEFINE_FIELD( m_flLastFire, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_elite, CWeaponElite );
PRECACHE_WEAPON_REGISTER( weapon_elite );

CWeaponElite::CWeaponElite()
{
	m_flLastFire = gpGlobals->curtime;
	m_inPrecache = false;
}


void CWeaponElite::Spawn( )
{
	m_flAccuracy = 0.88;
	BaseClass::Spawn();
}


void CWeaponElite::Precache()
{
	m_inPrecache = true;
	BaseClass::Precache();

	PrecacheModel( "models/weapons/w_eq_eholster_elite.mdl" );
	PrecacheModel( "models/weapons/w_eq_eholster.mdl" );
	PrecacheModel( "models/weapons/w_pist_elite_single.mdl" );
	m_droppedModelIndex = CBaseEntity::PrecacheModel( GetCSWpnData().m_szDroppedModel );
	m_inPrecache = false;
}

bool CWeaponElite::Deploy( )
{
	m_flAccuracy = 0.88;
	return BaseClass::Deploy();
}

int CWeaponElite::GetWorldModelIndex( void )
{
	if ( GetOwner() || m_inPrecache )
	{
		return m_iWorldModelIndex;
	}
	else
	{
		return m_droppedModelIndex;
	}
}

const char * CWeaponElite::GetWorldModel( void ) const
{
	if ( GetOwner() || m_inPrecache )
	{
		return BaseClass::GetWorldModel();
	}
	else
	{
		return GetCSWpnData().m_szDroppedModel;
	}
}


bool CWeaponElite::FiringLeft() const
{
	// fire left-hand gun with even number of bullets left
	return (m_iClip1 & 1) == 0;
}



float CWeaponElite::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;

		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 1.3f * (1 - m_flAccuracy);

		else if (pPlayer->GetAbsVelocity().Length2D() > 5)
			return 0.175f * (1 - m_flAccuracy);

		else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
			return 0.08f * (1 - m_flAccuracy);

		else
			return 0.1f * (1 - m_flAccuracy);
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponElite::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Mark the time of this shot and determine the accuracy modifier based on the last shot fired...
	m_flAccuracy -= (0.275)*(0.325 - (gpGlobals->curtime - m_flLastFire));

	if (m_flAccuracy > 0.88)
		m_flAccuracy = 0.88;
	else if (m_flAccuracy < 0.55)
		m_flAccuracy = 0.55;

	m_flLastFire = gpGlobals->curtime;

	if (m_iClip1 <= 0)
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.1f;
			m_bFireOnEmpty = false;
		}

		return;
	}

	pPlayer->m_iShotsFired++;

	m_iClip1--;

	pPlayer->DoMuzzleFlash();

	//SetPlayerShieldAnim();
	
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		FiringLeft() ? Secondary_Mode : Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetInaccuracy(),
		GetSpread());
		
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetCSWpnData().m_flCycleTime;
	
	if (!m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}

	SetWeaponIdleTime( gpGlobals->curtime + 2.5 );

	if ( FiringLeft() )
	{
		if ( m_iClip1 > 0 )
			SendWeaponAnim( ACT_VM_SECONDARYATTACK );
		else
			SendWeaponAnim( ACT_VM_DRYFIRE );
	}
	else
	{
		if ( m_iClip1 > 1 )
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		else
			SendWeaponAnim( ACT_VM_DRYFIRE_LEFT );
	}

	// update accuracy
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyImpulseFire[Primary_Mode];

	QAngle punchAngle = pPlayer->GetPunchAngle();
	punchAngle.x -= 2;
	pPlayer->SetPunchAngle( punchAngle );

	//ResetPlayerShieldAnim();
}


bool CWeaponElite::Reload()
{
	if ( !DefaultPistolReload() )
		return false;

	m_flAccuracy = 0.88;
	return true;
}

void CWeaponElite::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

/*
	// switching to the idle with the slide back on the right pistol causes animation pops transitioning
	// from/to the depot/holster animations. The pop transition to the reload is less noticeable, so 
	// we'll live with that one

	if ( m_iClip1 == 1 )
	{
		SendWeaponAnim( ACT_VM_IDLE_EMPTY_LEFT );
	}
*/

	// only idle if either slide isn't back
	if ( m_iClip1 >= 2 )
	{
			SendWeaponAnim( ACT_VM_IDLE );
	}
}

#ifdef CLIENT_DLL

    bool CWeaponElite::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 5001 )
		{
			C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
			if( pPlayer && pPlayer->GetFOV() < pPlayer->GetDefaultFOV() && HideViewModelWhenZoomed() )
				return true;
			
			CEffectData data;
			data.m_fFlags = 0;
			data.m_hEntity = pViewModel->GetRefEHandle();
			data.m_nAttachmentIndex = FiringLeft() ? 1 : 2; // toggle muzzle flash
			data.m_flScale = GetCSWpnData().m_flMuzzleScale;
		
			DispatchEffect( "CS_MuzzleFlash", data );

			return true;
		}

		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	int CWeaponElite::GetMuzzleAttachment( void )
	{
		return LookupAttachment( FiringLeft() ? "muzzle_flash_l" : "muzzle_flash_r" );	
	}

#endif