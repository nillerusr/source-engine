//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
//	Weapons.
//
//	CTFWeaponBase
//	|
//	|--> CTFWeaponBaseMelee
//	|		|
//	|		|--> CTFWeaponCrowbar
//	|		|--> CTFWeaponKnife
//	|		|--> CTFWeaponMedikit
//	|		|--> CTFWeaponWrench
//	|
//	|--> CTFWeaponBaseGrenade
//	|		|
//	|		|--> CTFWeapon
//	|		|--> CTFWeapon
//	|
//	|--> CTFWeaponBaseGun
//
//=============================================================================
#ifndef TF_WEAPONBASE_H
#define TF_WEAPONBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_playeranimstate.h"
#include "tf_weapon_parse.h"
#include "npcevent.h"

// Client specific.
#if defined( CLIENT_DLL )
#define CTFWeaponBase C_TFWeaponBase
#define CTFWeaponBaseGrenadeProj C_TFWeaponBaseGrenadeProj
#include "tf_fx_muzzleflash.h"
#endif

#define MAX_TRACER_NAME		128

class CTFPlayer;
class CBaseObject;
class CTFWeaponBaseGrenadeProj;

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// TFTODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );
void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity );

// Reloading singly.
enum
{
	TF_RELOAD_START = 0,
	TF_RELOADING,
	TF_RELOADING_CONTINUE,
	TF_RELOAD_FINISH
};

// structure to encapsulate state of head bob
struct BobState_t
{
	BobState_t() 
	{ 
		m_flBobTime = 0; 
		m_flLastBobTime = 0;
		m_flLastSpeed = 0;
		m_flVerticalBob = 0;
		m_flLateralBob = 0;
	}

	float m_flBobTime;
	float m_flLastBobTime;
	float m_flLastSpeed;
	float m_flVerticalBob;
	float m_flLateralBob;
};

#ifdef CLIENT_DLL
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState );
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState );
#endif

// Interface for weapons that have a charge time
class ITFChargeUpWeapon 
{
public:
	virtual float GetChargeBeginTime( void ) = 0;

	virtual float GetChargeMaxTime( void ) = 0;
};

//=============================================================================
//
// Base TF Weapon Class
//
class CTFWeaponBase : public CBaseCombatWeapon
{
	DECLARE_CLASS( CTFWeaponBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
#if !defined ( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	// Setup.
	CTFWeaponBase();

	virtual void Spawn();
	virtual void Precache();
	virtual bool IsPredicted() const			{ return true; }
	virtual void FallInit( void );

	// Weapon Data.
	CTFWeaponInfo const	&GetTFWpnData() const;
	virtual int GetWeaponID( void ) const;
	bool IsWeapon( int iWeapon ) const;
	virtual int	GetDamageType() const { return g_aWeaponDamageTypes[ GetWeaponID() ]; }
	virtual int GetCustomDamageType() const { return TF_DMG_CUSTOM_NONE; }

	// View model.
	virtual const char *GetViewModel( int iViewModel = 0 ) const;

	virtual void Drop( const Vector &vecVelocity );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool Deploy( void );

	// Attacks.
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	void CalcIsAttackCritical( void );
	virtual bool CalcIsAttackCriticalHelper();
	bool IsCurrentAttackACrit() { return m_bCurrentAttackIsCrit; }

	// Reloads.
	virtual bool Reload( void );
	virtual void AbortReload( void );
	virtual bool DefaultReload( int iClipSize1, int iClipSize2, int iActivity );
	void SendReloadEvents();

	virtual bool CanDrop( void ) { return false; }

	// Sound.
	bool PlayEmptySound();

	// Activities.
	virtual void ItemBusyFrame( void );
	virtual void ItemPostFrame( void );

	virtual void SetWeaponVisible( bool visible );

	virtual acttable_t *ActivityList( int &iActivityCount );
	static acttable_t m_acttablePrimary[];
	static acttable_t m_acttableSecondary[];
	static acttable_t m_acttableMelee[];
	static acttable_t m_acttableBuilding[];
	static acttable_t m_acttablePDA[];

#ifdef GAME_DLL
	virtual void	AddAssociatedObject( CBaseObject *pObject ) { }
	virtual void	RemoveAssociatedObject( CBaseObject *pObject ) { }
#endif

	// Utility.
	CBasePlayer *GetPlayerOwner() const;
	CTFPlayer *GetTFPlayerOwner() const;

#ifdef CLIENT_DLL
	C_BaseEntity *GetWeaponForEffect();
#endif

	bool CanAttack( void );

	// Raising & Lowering for grenade throws
	bool			WeaponShouldBeLowered( void );
	virtual bool	Ready( void );
	virtual bool	Lower( void );

	virtual void	WeaponIdle( void );

	virtual void	WeaponReset( void );

	// Muzzleflashes
	virtual const char *GetMuzzleFlashEffectName_3rd( void ) { return NULL; }
	virtual const char *GetMuzzleFlashEffectName_1st( void ) { return NULL; }
	virtual const char *GetMuzzleFlashModel( void );
	virtual float	GetMuzzleFlashModelLifetime( void );
	virtual const char *GetMuzzleFlashParticleEffect( void );

	virtual const char	*GetTracerType( void );

	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	virtual bool CanFireCriticalShot( bool bIsHeadshot = false ){ return true; }

// Server specific.
#if !defined( CLIENT_DLL )

	// Spawning.
	virtual void CheckRespawn();
	virtual CBaseEntity* Respawn();
	void Materialize();
	void AttemptToMaterialize();

	// Death.
	void Die( void );
	void SetDieThink( bool bDie );

	// Ammo.
	virtual const Vector& GetBulletSpread();

// Client specific.
#else

	virtual void	ProcessMuzzleFlashEvent( void );
	virtual int		InternalDrawModel( int flags );

	virtual bool	ShouldPredict();
	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual int		CalcOverrideModelIndex( void );
	virtual int		GetWorldModelIndex( void );
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	Redraw( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	virtual ShadowType_t	ShadowCastType( void );
	virtual int		GetSkin();
	BobState_t		*GetBobState();

	bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

	// Model muzzleflashes
	CHandle<C_MuzzleFlashModel>		m_hMuzzleFlashModel[2];

#endif

protected:
#ifdef CLIENT_DLL
	virtual void CreateMuzzleFlashEffects( C_BaseEntity *pAttachEnt, int nIndex );
#endif // CLIENT_DLL

	// Reloads.
	void UpdateReloadTimers( bool bStart );
	void SetReloadTimer( float flReloadTime );
	bool ReloadSingly( void );
	void ReloadSinglyPostFrame( void );

protected:

	int				m_iWeaponMode;
	CNetworkVar(	int,	m_iReloadMode );
	CTFWeaponInfo	*m_pWeaponInfo;
	bool			m_bInAttack;
	bool			m_bInAttack2;
	bool			m_bCurrentAttackIsCrit;

	CNetworkVar(	bool,	m_bLowered );

	int				m_iAltFireHint;

	int				m_iReloadStartClipAmount;

	float			m_flCritTime;
	float			m_flLastCritCheckTime;
	int				m_iLastCritCheckFrame;
	int				m_iCurrentSeed;

	char			m_szTracerName[MAX_TRACER_NAME];

	CNetworkVar(	bool, m_bResetParity );

#ifdef CLIENT_DLL
	bool m_bOldResetParity;
#endif

	CNetworkVar(	bool,	m_bReloadedThroughAnimEvent );
private:
	CTFWeaponBase( const CTFWeaponBase & );
};

#define WEAPON_RANDOM_RANGE 10000

#endif // TF_WEAPONBASE_H
