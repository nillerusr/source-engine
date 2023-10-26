#ifndef _INCLUDED_ASW_WEAPON_H
#define _INCLUDED_ASW_WEAPON_H
#pragma once

#include "asw_weapon_shared.h"
#include "basecombatweapon_shared.h"
#include "iasw_server_usable_entity.h"

class CASW_Player;
class CASW_Marine;
class CASW_WeaponInfo;

class CASW_Weapon : public CBaseCombatWeapon, public IASW_Server_Usable_Entity
{
public:
	DECLARE_CLASS( CASW_Weapon, CBaseCombatWeapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();

	CASW_Weapon();
	virtual ~CASW_Weapon();	
	virtual void Precache();
	bool DestroyIfEmpty( bool bDestroyWhenActive, bool bCheckSecondaryAmmo=false );

	virtual void ItemPostFrame(void);
	virtual void ItemBusyFrame(void);
	virtual const char* GetPickupClass() { return "asw_pickup_rifle"; }

	virtual void SetClip1(int i) { m_iClip1 = i; }
	virtual void SetClip2(int i) { m_iClip2 = i; }
	virtual int GetNumPellets() { return 1; }	// how many projectiles are fired off per firerate

	int	UpdateTransmitState();
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	void DiffPrint(  char const *fmt, ... );

	// shared code	
	virtual const char *GetASWShootSound( int iIndex, int &iPitch );
	virtual void WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );
	virtual void PrimaryAttack(void);
	virtual void SecondaryAttack();
	virtual bool SecondaryAttackUsesPrimaryAmmo() { return false; }
	virtual bool IsPredicted( void ) const;
	void PlaySoundDirectlyToOwner( const char *szSoundName );
	void PlaySoundToOthers( const char *szSoundName );
	
	virtual float	GetFireRate( void );
	virtual bool HasAmmo();
	virtual bool PrimaryAmmoLoaded( void );

	virtual int LookupAttachment( const char *pAttachmentName );
	virtual bool ShouldMarineMoveSlow();		// is the weapon in slow mode (when firing or reloading)
	virtual float GetMovementScale();			// get overall current movement scale (incorporates slowdown from heavy weapons, reloading, firing)
	virtual float GetTurnRateModifier();
	virtual float GetMadFiringBias() { return 1.0f; }	// scales the rate at which the mad firing counter goes up when we shoot aliens with this weapon
	virtual void OnStoppedFiring();
	virtual void OnStartedRoll();
	virtual bool IsFiring();// const;
	virtual void ClearIsFiring();
	virtual void GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 );
	bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Equip(CBaseCombatCharacter *pOwner);
	virtual void			SetWeaponVisible( bool visible );
	virtual void ApplyWeaponSwitchTime();
	CNetworkVar(bool, m_bSwitchingWeapons);	
	virtual void SendWeaponSwitchEvents();
	virtual const float GetAutoAimAmount();
	virtual const float GetVerticalAdjustOnlyAutoAimAmount();
	virtual void MarineDropped(CASW_Marine* pMarine);
	virtual bool ASWCanBeSelected() { return true; }
	virtual bool ShouldFlareAutoaim() { return false; }
	virtual int GetEquipmentListIndex() { return m_iEquipmentListIndex; }
	const CASW_WeaponInfo* GetWeaponInfo() const;
	virtual bool OffhandActivate() { return false; };
	virtual bool WantsOffhandPostFrame() { return m_bShotDelayed; }
	virtual bool SendWeaponAnim( int iActivity );
	virtual int WeaponRangeAttack1Condition( float flDot, float flDist );
	virtual bool IsOffensiveWeapon() { return true; }		// is this weapon an offensive gun type weapon (as opposed to a utility item)
	virtual bool IsRapidFire() { return true; }
	virtual int AmmoClickPoint(); // when this many rounds or less left in the gun, it will click when firing
	virtual void LowAmmoSound();
	virtual int ASW_SelectWeaponActivity(int idealActivity);
	virtual bool SupportsBayonet();
	virtual float GetWeaponDamage();
	virtual bool ShouldAlienFlinch(CBaseEntity *pEntity, const CTakeDamageInfo &info);	
	virtual bool			AllowsAutoSwitchFrom( void ) const;
	virtual bool WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	virtual bool CanDoForcedAction( int iForcedAction ) { return true; }		// check if we're allowed to perform a forced action (certain abilities limit this)
	virtual float GetPassiveMeleeDamageScale() { return 1.0f; }

	CNetworkVar(bool, m_bIsFiring);	

	CASW_Player* GetCommander();
	CASW_Marine* GetMarine();
	virtual void OnMarineDamage(const CTakeDamageInfo &info) { }	// marine was hurt when holding this weapon

	// delayed firing (flares, hand grenades, etc.)
	virtual void DelayedAttack() { }
	virtual float GetThrowGravity() { return 1.0f; }
	bool m_bShotDelayed;
	float m_flDelayedFire;

	// powerups
	void MakePoweredUp( bool bPoweredUp ) { m_bPoweredUp = bPoweredUp; } 
	CNetworkVar(bool, m_bPoweredUp);

	// reloading
	virtual float GetReloadTime();
	virtual float GetReloadFailTime();
	virtual bool Reload();
	virtual bool ASWReload( int iClipSize1, int iClipSize2, int iActivity );
	virtual void SendReloadEvents();
	bool ReloadOrSwitchWeapons(void);
	bool IsReloading() const;
	virtual void FinishReload( void );
	float m_fReloadClearFiringTime;
	float m_flReloadFailTime;			// time from when you fail an active reload until you can shoot again
	// fast reload
	CNetworkVar(float, m_fReloadStart);
	CNetworkVar(float, m_fFastReloadStart);
	CNetworkVar(float, m_fFastReloadEnd);
	CNetworkVar(bool, m_bFastReloadSuccess);	
	CNetworkVar(bool, m_bFastReloadFailure);	

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iClip1 );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iClip2 );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iPrimaryAmmoType );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iSecondaryAmmoType );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_flNextPrimaryAttack );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_flNextSecondaryAttack );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_flTimeWeaponIdle );

	// effects
	virtual const char* GetUTracerType();

	virtual void Spawn();
	virtual void UpdateOnRemove();

	// Functions for weapons on the ground
	virtual void Drop( const Vector &vecVelocity );
	virtual bool AllowedToPickup(CASW_Marine *pMarine);
	bool IsBeingCarried() const;
	bool IsCarriedByLocalPlayer();

	// check if this weapon wants to perform a sync kill
	virtual bool CheckSyncKill( byte &forced_action, short &sync_kill_ent ) { return false; }

	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet( CBaseEntity *pUser ) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime) { }
	virtual void MarineStartedUsing(CASW_Marine* pMarine) { }
	virtual void MarineStoppedUsing(CASW_Marine* pMarine) { }
	virtual bool NeedsLOSCheck() { return true; }

protected:
	int m_iEquipmentListIndex;
};


#endif /* _INCLUDED_ASW_WEAPON_H */
