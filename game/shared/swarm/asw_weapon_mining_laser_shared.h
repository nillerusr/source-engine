#ifndef _INCLUDED_ASW_WEAPON_MINING_LASER_SHARED_H
#define _INCLUDED_ASW_WEAPON_MINING_LASER_SHARED_H
#ifdef _WIN32
#pragma once
#endif



// Mining Laser
//   "Weapon" that causes heat damage at short range.  Can be used to blast through certain kinds of rocks.

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon_Mining_Laser C_ASW_Weapon_Mining_Laser
#define CASW_Weapon C_ASW_Weapon
#else
#include "asw_weapon.h"
#endif

enum MININGLASER_FIRE_STATE { FIRE_OFF, FIRE_STARTUP, FIRE_LASER };
#define ASW_MINING_LASER_RANGE 200

class CSoundPatch;

class CASW_Weapon_Mining_Laser : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Mining_Laser, CASW_Weapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

    CASW_Weapon_Mining_Laser(void);
	virtual ~CASW_Weapon_Mining_Laser();

	virtual bool	Deploy( void );
	void	PrimaryAttack( void );
    virtual void    Precache( void );

	virtual void Spawn();

#ifdef CLIENT_DLL
	virtual void ClientThink();
	virtual const char* GetPartialReloadSound(int iPart);
	virtual float GetLaserPointerRange( void ) { return ASW_MINING_LASER_RANGE; }
#else
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	virtual void GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 );
	float m_fLastForcedFireTime;
#endif
    
	void	SecondaryAttack( void )
	{
		PrimaryAttack();
	}

	virtual const float GetAutoAimAmount() { return 0.26f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void Drop( const Vector &vecVelocity );
	virtual bool HasAmmo();
	virtual bool ShouldMarineMoveSlow();
	virtual void UpdateOnRemove();

	virtual const char* GetPickupClass() { return "asw_pickup_mining_laser"; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MINING_LASER; }

private:
	void	Attack( void );
	void	EndAttack( void );
	bool	Fire( const Vector &vecOrigSrc, const Vector &vecDir );
	void	GetLaserEndPosition( Vector vecStart, Vector vecDir, Vector *pVecEnd );
	void	SetFiringState(MININGLASER_FIRE_STATE state);

	CNetworkVar( int, m_fireState );
	CNetworkVar( bool, m_bCutting );
	CNetworkVar( float, m_flStartFireTime );
	float				m_flAmmoUseTime;	// since we use < 1 point of ammo per update, we subtract ammo on a timer.
	float				m_flShakeTime;
	float				m_flDmgTime;
	//CHandle<CSprite>	m_hSprite;
	//CHandle<CBeam>		m_hBeam;
	//CHandle<CBeam>		m_hNoise;
	bool m_bPlayingCuttingSound;
	float m_fLastDamageSoundTime;
	float m_fLastAttackTime;

	void StartChargingSound();
	void StartRunSound();
	void StopRunSound();

	CSoundPatch *m_pChargeSound;
	CSoundPatch *m_pRunSound;

#ifdef CLIENT_DLL
	CUtlReference<CNewParticleEffect>	m_pLaserEffect;
	CUtlReference<CNewParticleEffect>	m_pChargeEffect;
	bool m_bLastHadTarget;
#endif

};

#endif // _INCLUDED_ASW_WEAPON_MINING_LASER_SHARED_H