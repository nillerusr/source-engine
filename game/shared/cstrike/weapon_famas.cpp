//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"
#include "fx_cs_shared.h"


#if defined( CLIENT_DLL )

	#define CWeaponFamas C_WeaponFamas
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponFamas : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponFamas, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponFamas();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Deploy();

 	virtual float GetInaccuracy() const;

	virtual void ItemPostFrame();

	void FamasFire( float flSpread, bool bFireBurst );
	void FireRemaining();
	
	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_FAMAS; }

private:
	
	CWeaponFamas( const CWeaponFamas & );
	CNetworkVar( bool, m_bBurstMode );
	CNetworkVar( int, m_iBurstShotsRemaining );	
	float	m_fNextBurstShot;			// time to shoot the next bullet in burst fire mode
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFamas, DT_WeaponFamas )

BEGIN_NETWORK_TABLE( CWeaponFamas, DT_WeaponFamas )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bBurstMode ) ),
		RecvPropInt( RECVINFO( m_iBurstShotsRemaining ) ),
	#else
		SendPropBool( SENDINFO( m_bBurstMode ) ),
		SendPropInt( SENDINFO( m_iBurstShotsRemaining ) ),
	#endif
END_NETWORK_TABLE()

#if defined(CLIENT_DLL)
BEGIN_PREDICTION_DATA( CWeaponFamas )
DEFINE_PRED_FIELD( m_iBurstShotsRemaining, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_fNextBurstShot, FIELD_FLOAT, 0 ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_famas, CWeaponFamas );
PRECACHE_WEAPON_REGISTER( weapon_famas );


const float kFamasBurstCycleTime = 0.075f;


CWeaponFamas::CWeaponFamas()
{
	m_bBurstMode = false;
}


bool CWeaponFamas::Deploy( )
{
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;
	m_flAccuracy = 0.9f;

	return BaseClass::Deploy();
}


// Secondary attack could be three-round burst mode
void CWeaponFamas::SecondaryAttack()
{	
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_bBurstMode )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#Switch_To_FullAuto" );
		m_bBurstMode = false;
		m_weaponMode = Primary_Mode;
	}
	else
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#Switch_To_BurstFire" );
		m_bBurstMode = true;
		m_weaponMode = Secondary_Mode;
	}
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3;
}

float CWeaponFamas::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		float fAutoPenalty = m_bBurstMode ? 0.0f : 0.01f;

		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )	// if player is in air
			return 0.03f + 0.3f * m_flAccuracy + fAutoPenalty;
	
		else if ( pPlayer->GetAbsVelocity().Length2D() > 140 )	// if player is moving
			return 0.03f + 0.07f * m_flAccuracy + fAutoPenalty;
		/* new code */
		else
			return 0.02f * m_flAccuracy + fAutoPenalty;
	}
	else
		return BaseClass::GetInaccuracy();
}


void CWeaponFamas::ItemPostFrame()
{
	if ( m_iBurstShotsRemaining > 0 && gpGlobals->curtime >= m_fNextBurstShot )
		FireRemaining();

	BaseClass::ItemPostFrame();
}



// GOOSEMAN : FireRemaining used by Glock18
void CWeaponFamas::FireRemaining()
{
	m_iClip1--;

	if (m_iClip1 < 0)
	{
		m_iClip1 = 0;
		m_iBurstShotsRemaining = 0;
		m_fNextBurstShot = 0.0f;
		return;
	}

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		Error( "!pPlayer" );

	// Famas burst mode
	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		Secondary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetInaccuracy(),
		GetSpread(),
		m_fNextBurstShot);
	
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	pPlayer->m_iShotsFired++;

	--m_iBurstShotsRemaining;
	if ( m_iBurstShotsRemaining > 0 )
		m_fNextBurstShot += kFamasBurstCycleTime;
	else
		m_fNextBurstShot = 0.0f;

	// update accuracy
	m_fAccuracyPenalty += GetCSWpnData().m_fInaccuracyImpulseFire[Secondary_Mode];
}


void CWeaponFamas::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// don't fire underwater
	if (pPlayer->GetWaterLevel() == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15;
		return;
	}

	pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	float flCycleTime = GetCSWpnData().m_flCycleTime;

	// change a few things if we're in burst mode
	if ( m_bBurstMode )
	{
		flCycleTime = 0.55f;
		m_iBurstShotsRemaining = 2;
		m_fNextBurstShot = gpGlobals->curtime + kFamasBurstCycleTime;
	}

	if ( !CSBaseGunFire( flCycleTime, m_weaponMode ) )
		return;
	
	if ( pPlayer->GetAbsVelocity().Length2D() > 5 )
		pPlayer->KickBack ( 1, 0.45, 0.275, 0.05, 4, 2.5, 7 );
	
	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack ( 1.25, 0.45, 0.22, 0.18, 5.5, 4, 5 );
	
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack ( 0.575, 0.325, 0.2, 0.011, 3.25, 2, 8 );
	
	else
		pPlayer->KickBack ( 0.625, 0.375, 0.25, 0.0125, 3.5, 2.25, 8 );
}


