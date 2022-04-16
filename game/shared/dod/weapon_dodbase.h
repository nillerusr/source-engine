//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODBASE_H
#define WEAPON_DODBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_playeranimstate.h"
#include "dod_weapon_parse.h"

#if defined( CLIENT_DLL )
	#define CWeaponDODBase C_WeaponDODBase
#endif

extern int	AliasToWeaponID( const char *alias );
extern const char *WeaponIDToAlias( int id );
extern bool	IsPrimaryWeapon( int id );
extern bool IsSecondaryWeapon( int id );

class CDODPlayer;

// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.

#define DOD_AMMO_SUBMG		"DOD_AMMO_SUBMG"
#define DOD_AMMO_ROCKET		"DOD_AMMO_ROCKET"
#define DOD_AMMO_COLT		"DOD_AMMO_COLT"
#define DOD_AMMO_P38		"DOD_AMMO_P38"
#define DOD_AMMO_C96		"DOD_AMMO_C96"	
#define DOD_AMMO_WEBLEY		"DOD_AMMO_WEBLEY"
#define DOD_AMMO_GARAND		"DOD_AMMO_GARAND"
#define DOD_AMMO_K98		"DOD_AMMO_K98"
#define DOD_AMMO_M1CARBINE	"DOD_AMMO_M1CARBINE"
#define DOD_AMMO_ENFIELD	"DOD_AMMO_ENFIELD"
#define DOD_AMMO_SPRING		"DOD_AMMO_SPRING"
#define DOD_AMMO_FG42		"DOD_AMMO_FG42"		
#define DOD_AMMO_BREN		"DOD_AMMO_BREN"
#define DOD_AMMO_BAR		"DOD_AMMO_BAR"		
#define DOD_AMMO_30CAL		"DOD_AMMO_30CAL"	
#define DOD_AMMO_MG34		"DOD_AMMO_MG34"		
#define DOD_AMMO_MG42		"DOD_AMMO_MG42"
#define DOD_AMMO_HANDGRENADE	"DOD_AMMO_HANDGRENADE"
#define DOD_AMMO_HANDGRENADE_EX	"DOD_AMMO_HANDGRENADE_EX"	// the EX is for EXploding! :)
#define DOD_AMMO_STICKGRENADE	"DOD_AMMO_STICKGRENADE"
#define DOD_AMMO_STICKGRENADE_EX	"DOD_AMMO_STICKGRENADE_EX"
#define DOD_AMMO_SMOKEGRENADE_US	"DOD_AMMO_SMOKEGRENADE_US"
#define DOD_AMMO_SMOKEGRENADE_GER	"DOD_AMMO_SMOKEGRENADE_GER"
#define DOD_AMMO_SMOKEGRENADE_US_LIVE	"DOD_AMMO_SMOKEGRENADE_US_LIVE"
#define DOD_AMMO_SMOKEGRENADE_GER_LIVE	"DOD_AMMO_SMOKEGRENADE_GER_LIVE"
#define DOD_AMMO_RIFLEGRENADE_US		"DOD_AMMO_RIFLEGRENADE_US"
#define DOD_AMMO_RIFLEGRENADE_GER		"DOD_AMMO_RIFLEGRENADE_GER"
#define DOD_AMMO_RIFLEGRENADE_US_LIVE	"DOD_AMMO_RIFLEGRENADE_US_LIVE"
#define DOD_AMMO_RIFLEGRENADE_GER_LIVE	"DOD_AMMO_RIFLEGRENADE_GER_LIVE"



#define CROSSHAIR_CONTRACT_PIXELS_PER_SECOND	7.0f

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// MIKETODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );


typedef enum
{
	WEAPON_NONE = 0,

	//Melee
	WEAPON_AMERKNIFE,
	WEAPON_SPADE,

	//Pistols
	WEAPON_COLT,
	WEAPON_P38,
	WEAPON_C96,

	//Rifles
	WEAPON_GARAND,
	WEAPON_M1CARBINE,
	WEAPON_K98,
	
	//Sniper Rifles
	WEAPON_SPRING,
	WEAPON_K98_SCOPED,

	//SMG
	WEAPON_THOMPSON,
	WEAPON_MP40,
	WEAPON_MP44,
	WEAPON_BAR,

	//Machine guns
	WEAPON_30CAL,
	WEAPON_MG42,

	//Rocket weapons
	WEAPON_BAZOOKA,
	WEAPON_PSCHRECK,

	//Grenades
	WEAPON_FRAG_US,
	WEAPON_FRAG_GER,

	WEAPON_FRAG_US_LIVE,
	WEAPON_FRAG_GER_LIVE,

	WEAPON_SMOKE_US,
	WEAPON_SMOKE_GER,

	WEAPON_RIFLEGREN_US,
	WEAPON_RIFLEGREN_GER,

	WEAPON_RIFLEGREN_US_LIVE,
	WEAPON_RIFLEGREN_GER_LIVE,

	// not actually separate weapons, but defines used in stats recording
	// find a better way to do this without polluting the list of actual weapons.
	WEAPON_THOMPSON_PUNCH,
	WEAPON_MP40_PUNCH,

	WEAPON_GARAND_ZOOMED,	
	WEAPON_K98_ZOOMED,
	WEAPON_SPRING_ZOOMED,
	WEAPON_K98_SCOPED_ZOOMED,

	WEAPON_30CAL_UNDEPLOYED,
	WEAPON_MG42_UNDEPLOYED,

	WEAPON_BAR_SEMIAUTO,
	WEAPON_MP44_SEMIAUTO,
		
	WEAPON_MAX,		// number of weapons weapon index

} DODWeaponID;


//Class Heirarchy for dod weapons

/*

  CWeaponDODBase
	|
	|
	|--> CWeaponDODBaseMelee
	|		|
	|		|--> CWeaponSpade
	|		|--> CWeaponUSKnife
	|
	|--> CWeaponDODBaseGrenade
	|		|
	|		|--> CWeaponHandgrenade
	|		|--> CWeaponStickGrenade
	|		|--> CWeaponSmokeGrenadeUS
	|		|--> CWeaponSmokeGrenadeGER
	|
	|--> CWeaponBaseRifleGrenade
	|		|
	|		|--> CWeaponRifleGrenadeUS
	|		|--> CWeaponRifleGrenadeGER
	|
	|--> CDODBaseRocketWeapon
	|		|
	|		|--> CWeaponBazooka
	|		|--> CWeaponPschreck
	|
	|--> CWeaponDODBaseGun
			|
			|--> CDODFullAutoWeapon
			|		|
			|		|--> CWeaponC96
			|		|
			|		|--> CDODFullAutoPunchWeapon
			|		|		|
			|		|		|--> CWeaponThompson
			|		|		|--> CWeaponMP40
			|		|
			|		|--> CDODBipodWeapon
			|				|
			|				|->	CWeapon30Cal
			|				|->	CWeaponMG42
			|
			|--> CDODFireSelectWeapon
			|		|
			|		|--> CWeaponMP44
			|		|--> CWeaponBAR
			|
			|
			|--> CDODSemiAutoWeapon
					|
					|--> CWeaponColt
					|--> CWeaponP38
					|--> CWeaponM1Carbine
					|--> CDODSniperWeapon
						|
						|--> CWeaponSpring
						|--> CWeaponScopedK98
						|--> CWeaponGarand
						|--> CWeaponK98

*/

void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity );

typedef enum
{
	Primary_Mode = 0,
	Secondary_Mode,
} DODWeaponMode;


class CWeaponDODBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponDODBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponDODBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();

		virtual void CheckRespawn();
		virtual CBaseEntity* Respawn();
		
		virtual const Vector& GetBulletSpread();
		virtual float	GetDefaultAnimSpeed();

		virtual void	ItemBusyFrame();
		virtual bool	ShouldRemoveOnRoundRestart();

		void Materialize();
		void AttemptToMaterialize();
		
	#else

		void PlayWorldReloadSound( CDODPlayer *pPlayer );

	#endif

	virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	Drop( const Vector &vecVelocity );
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;

	virtual int	    ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_USE_IN_RADIUS; }

	CBasePlayer* GetPlayerOwner() const;
	CDODPlayer* GetDODPlayerOwner() const;

	virtual void WeaponIdle( void );
	virtual Activity GetIdleActivity( void );

	// Get DOD-specific weapon data.
	CDODWeaponInfo const	&GetDODWpnData() const;

	// Get specific DOD weapon ID (ie: WEAPON_GARAND, etc)
	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_NONE; }
	virtual DODWeaponID GetStatsWeaponID( void )	{ return GetWeaponID(); }
	virtual DODWeaponID GetAltWeaponID( void ) const	{ return WEAPON_NONE; }

	// return true if this weapon is an instance of the given weapon type (ie: "IsA" WEAPON_GLOCK)
	bool IsA( DODWeaponID id ) const						{ return GetWeaponID() == id; }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return false; }

	void KickBack( float up_base, float lateral_base, float up_modifier, float lateral_modifier, float up_max, float lateral_max, int direction_change );

	virtual void SetWeaponModelIndex( const char *pName );

	virtual bool CanDrop( void ) { return false; }

	virtual bool ShouldDrawCrosshair( void ) { return true; }
	virtual bool ShouldDrawViewModel( void ) { return true; }
	virtual bool ShouldDrawMuzzleFlash( void ) { return true; }

	virtual float GetWeaponAccuracy( float flPlayerSpeed ) { return 0; }

	virtual bool HideViewModelWhenZoomed( void ) { return false; }

	virtual bool CanAttack( void );
	virtual bool ShouldAutoReload( void );

	CNetworkVar( int, m_iReloadModelIndex );
	CNetworkVector( m_vInitialDropVelocity );

	virtual void FinishReload( void	) {}

public:
	#if defined( CLIENT_DLL )

		virtual void	ProcessMuzzleFlashEvent();
		virtual bool	ShouldPredict();


		virtual void	PostDataUpdate( DataUpdateType_t type );
		virtual void	OnDataChanged( DataUpdateType_t type );

		virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

		virtual bool ShouldAutoEjectBrass( void );
		virtual bool GetEjectBrassShellType( void );

		void SetUseAltModel( bool bUseAlt );
		virtual int GetWorldModelIndex( void );
		virtual void CheckForAltWeapon( int iCurrentState );

		virtual Vector GetDesiredViewModelOffset( C_DODPlayer *pOwner );
		virtual float GetViewModelSwayScale( void ) { return 1.0; }

		virtual void OnWeaponDropped( void ) {}

		virtual bool ShouldDraw( void );

		float			m_flCrosshairDistance;
		int				m_iAmmoLastCheck;
		int				m_iAlpha;
		int				m_iScopeTextureID;

		bool			m_bUseAltWeaponModel;	//use alternate left handed world model? reset on new sequence
	#else

		virtual bool	Reload();
		virtual void	Spawn();

		void			SetDieThink( bool bDie );
		void			Die( void );

		void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	#endif

	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	bool IsUseable();
	virtual bool	CanDeploy( void );
	virtual bool	CanHolster( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	Precache( void );
	virtual bool	CanBeSelected( void );
	virtual bool	DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual bool	Deploy();
	bool PlayEmptySound();
	virtual void	ItemPostFrame();

	virtual const char		*GetViewModel( int viewmodelindex = 0 ) const;

	bool	m_bInAttack;		//True after a semi-auto weapon fires - will not fire a second time on the same button press

	void SetExtraAmmoCount( int count ) { m_iExtraPrimaryAmmo = count; }
	int GetExtraAmmoCount( void ) { return m_iExtraPrimaryAmmo; }

	virtual const char *GetSecondaryDeathNoticeName( void ) { return "world"; }

	virtual CBaseEntity *MeleeAttack( int iDamageAmount, int iDamageType, float flDmgDelay, float flAttackDelay );
	void EXPORT Smack( void );
	//Secondary Attacks
	void RifleButt( void );
	void Bayonet( void );
	void Punch( void );

	virtual Activity GetMeleeActivity( void ) { return ACT_VM_SECONDARYATTACK; }
	virtual Activity GetStrongMeleeActivity( void ) { return ACT_VM_SECONDARYATTACK; }

	virtual float GetRecoil( void ) { return 0.0f; }

protected:
	CNetworkVar( float, m_flSmackTime );
	int m_iSmackDamage;
	int m_iSmackDamageType;
	EHANDLE m_pTraceHitEnt;
	trace_t m_trHit;

	int		m_iAltFireHint;

private:

	void EjectBrassLate();

	float	m_flDecreaseShotsFired;

	CWeaponDODBase( const CWeaponDODBase & );

	int		m_iExtraPrimaryAmmo;

#ifdef CLIENT_DLL
	int m_iCrosshairTexture;
#endif
};


#endif // WEAPON_DODBASE_H
