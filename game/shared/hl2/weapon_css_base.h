//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#ifndef WEAPON_CSS_BASE_H
#define WEAPON_CSS_BASE_H

#ifdef CLIENT_DLL
#include "c_basehlcombatweapon.h"
#define CHLMachineGun C_HLMachineGun
#else
#include "basehlcombatweapon.h"
#endif
#include "in_buttons.h"

// Makes all CS:S weapons precache on map load, just like HL2's weapons.
// This means CS:S weapons are always precached even when not in use by the current level.
// This is left off by default to save resources.
#define PRECACHE_REGISTER_CSS_WEAPONS 0

// TODO: Enables code linking classes to the original CS:S classnames
#define ALLOW_ORIGINAL_CSS_CLASSNAMES 1

//=============================================================================

// Used to determine activity table type
enum CSS_HL2_WeaponActClass
{
	CSSHL2_WEAPON_PISTOL,				// Regular pistol animations. (Magazine is just below the trigger)
	CSSHL2_WEAPON_REVOLVER,				// Custom 357 animations. Falls back to pistol. (Mapbase v7.0 only)

	CSSHL2_WEAPON_SMG1,					// HL2 SMG1 animations. (Handgrip about 1 foot/30 cm from the trigger)
	CSSHL2_WEAPON_SMG2,					// Custom SMG2 animations. Falls back to SMG1. (Left hand grips area near trigger; Mapbase v7.0 only)

	CSSHL2_WEAPON_AR1,					// Custom AR1 animations. Falls back to AR2. (Forestock is about 1 foot/30 cm from the trigger and slightly elevated; Mapbase v7.0 only)
	CSSHL2_WEAPON_AR2,					// HL2 AR2 animations. (Forestock is 6 inches/15 cm from the trigger and on the same level)

	CSSHL2_WEAPON_SHOTGUN,				// HL2 Shotgun animations. (Fired from hip, forestock is a little over 1 foot/30 cm from trigger)

	CSSHL2_WEAPON_SNIPER_RIFLE,			// Custom sniper rifle animations. Falls back to AR2. (TODO; Mapbase v7.0 only)
};

#ifndef CLIENT_DLL
extern acttable_t *GetCSSActTable_Pistol();
extern acttable_t *GetCSSActTable_Revolver();
extern acttable_t *GetCSSActTable_SMG1();
extern acttable_t *GetCSSActTable_SMG2();
extern acttable_t *GetCSSActTable_AR1();
extern acttable_t *GetCSSActTable_AR2();
extern acttable_t *GetCSSActTable_Shotgun();
extern acttable_t *GetCSSActTable_SniperRifle();

extern int GetCSSActTableCount_Pistol();
extern int GetCSSActTableCount_Revolver();
extern int GetCSSActTableCount_SMG1();
extern int GetCSSActTableCount_SMG2();
extern int GetCSSActTableCount_AR1();
extern int GetCSSActTableCount_AR2();
extern int GetCSSActTableCount_Shotgun();
extern int GetCSSActTableCount_SniperRifle();
#endif

//=============================================================================

template <class BASE_WEAPON>
class CBase_CSS_HL2_Weapon : public BASE_WEAPON
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_Weapon, BASE_WEAPON );

public:

	// This is used to replicate CS:S's damage differences between weapons
	virtual float GetDamageMultiplier() const { return 1.0f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }

	virtual bool	IsSilenced() const { return false; }
	virtual void	SetSilenced( bool bSilenced ) {}
	virtual bool	CanToggleSilencer() const { return false; }

	virtual bool	InBurst() const { return false; }
	virtual bool	CanUseBurstMode() const { return false; }

	virtual bool	CanUseScope() const { return false; }

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

#ifndef CLIENT_DLL
	acttable_t *ActivityList( void )
	{
		switch (GetCSSWeaponActClass())
		{
			case CSSHL2_WEAPON_PISTOL:			return GetCSSActTable_Pistol();
			case CSSHL2_WEAPON_REVOLVER:		return GetCSSActTable_Revolver();
			case CSSHL2_WEAPON_SMG1:			return GetCSSActTable_SMG1();
			case CSSHL2_WEAPON_SMG2:			return GetCSSActTable_SMG2();
			case CSSHL2_WEAPON_AR1:				return GetCSSActTable_AR1();
			case CSSHL2_WEAPON_AR2:				return GetCSSActTable_AR2();
			case CSSHL2_WEAPON_SHOTGUN:			return GetCSSActTable_Shotgun();
			case CSSHL2_WEAPON_SNIPER_RIFLE:	return GetCSSActTable_SniperRifle();
		}

		// Default case
		return GetCSSActTable_AR1();
	}

	int ActivityListCount( void )
	{
		switch (GetCSSWeaponActClass())
		{
			case CSSHL2_WEAPON_PISTOL:			return GetCSSActTableCount_Pistol();
			case CSSHL2_WEAPON_REVOLVER:		return GetCSSActTableCount_Revolver();
			case CSSHL2_WEAPON_SMG1:			return GetCSSActTableCount_SMG1();
			case CSSHL2_WEAPON_SMG2:			return GetCSSActTableCount_SMG2();
			case CSSHL2_WEAPON_AR1:				return GetCSSActTableCount_AR1();
			case CSSHL2_WEAPON_AR2:				return GetCSSActTableCount_AR2();
			case CSSHL2_WEAPON_SHOTGUN:			return GetCSSActTableCount_Shotgun();
			case CSSHL2_WEAPON_SNIPER_RIFLE:	return GetCSSActTableCount_SniperRifle();
		}

		// Default case
		return GetCSSActTableCount_AR1();
	}
	
	acttable_t *GetBackupActivityList()
	{
		switch (GetCSSWeaponActClass())
		{
			case CSSHL2_WEAPON_REVOLVER:		return GetCSSActTable_Pistol();
			case CSSHL2_WEAPON_SMG2:			return GetCSSActTable_SMG1();
			case CSSHL2_WEAPON_AR1:
			case CSSHL2_WEAPON_SNIPER_RIFLE:	return GetCSSActTable_AR2();
		}

		return NULL;
	}

	int GetBackupActivityListCount()
	{
		switch (GetCSSWeaponActClass())
		{
			case CSSHL2_WEAPON_REVOLVER:		return GetCSSActTableCount_Pistol();
			case CSSHL2_WEAPON_SMG2:			return GetCSSActTableCount_SMG1();
			case CSSHL2_WEAPON_AR1:
			case CSSHL2_WEAPON_SNIPER_RIFLE:	return GetCSSActTableCount_AR2();
		}

		return 0;
	}
#endif

};

// Vars for all CS:S weapons. Not currently used
#define DEFINE_CSS_WEAPON_DATADESC()

#ifdef CLIENT_DLL

// Vars for all CS:S weapons. Not currently used
#define DEFINE_CSS_WEAPON_NETWORK_TABLE()

#else

// Vars for all CS:S weapons. Not currently used
#define DEFINE_CSS_WEAPON_NETWORK_TABLE()

#endif

//=============================================================================
//=============================================================================

template <class BASE_WEAPON>
class CBase_CSS_HL2_SilencedWeapon : public BASE_WEAPON
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_SilencedWeapon, BASE_WEAPON );

public:

	bool CanToggleSilencer() const { return this->m_bCanToggleSilencer; }
	bool IsSilenced() const { return this->m_bSilenced; }
	void SetSilenced( bool bSilenced ) { this->m_bSilenced = bSilenced; }

	void Spawn()
	{
		BaseClass::Spawn();
		this->Activate();
	}

	void Activate()
	{
		BaseClass::Activate();

		// Get the silenced model
		if (this->CanToggleSilencer())
		{
			V_StripExtension( this->GetWorldModel(), this->m_szSilencedModel, sizeof( this->m_szSilencedModel ) );
			V_strncat( this->m_szSilencedModel, "_silencer.mdl", sizeof( this->m_szSilencedModel ), sizeof( this->m_szSilencedModel ) );

			if ( this->m_szSilencedModel[0] )
			{
				this->m_iSilencedModelIndex = CBaseEntity::PrecacheModel( this->m_szSilencedModel );
			}
		}
	}

	bool SendWeaponAnim( int iActivity )
	{
		if (this->IsSilenced())
		{
			switch (iActivity)
			{
				case ACT_VM_PRIMARYATTACK:
					iActivity = ACT_VM_PRIMARYATTACK_SILENCED;
					break;

				case ACT_VM_RELOAD:
					iActivity = ACT_VM_RELOAD_SILENCED;
					break;

				case ACT_VM_DRYFIRE:
					iActivity = ACT_VM_DRYFIRE_SILENCED;
					break;

				case ACT_VM_IDLE:
					iActivity = ACT_VM_IDLE_SILENCED;
					break;

				case ACT_VM_DRAW:
					iActivity = ACT_VM_DRAW_SILENCED;
					break;
			}
		}

		return BaseClass::SendWeaponAnim( iActivity );
	}

	void WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f )
	{
		if (this->IsSilenced())
		{
			switch (sound_type)
			{
				case SINGLE:
				case SINGLE_NPC:
					sound_type = SPECIAL1;
					break;
			}
		}

		BaseClass::WeaponSound( sound_type, soundtime );
	}

	void SecondaryAttack( void )
	{
		if (this->CanToggleSilencer() && this->m_flNextSecondaryAttack <= gpGlobals->curtime)
		{
			if (IsSilenced())
			{
				this->SendWeaponAnim( ACT_VM_DETACH_SILENCER );
				this->m_bSilenced = false;
			}
			else
			{
				this->SendWeaponAnim( ACT_VM_ATTACH_SILENCER );
				this->m_bSilenced = true;
			}

			this->m_flNextSecondaryAttack = gpGlobals->curtime + this->SequenceDuration();
			this->m_flNextPrimaryAttack = gpGlobals->curtime + this->SequenceDuration();
		}
	}

	bool ShouldDisplayAltFireHUDHint()
	{
		return BaseClass::ShouldDisplayAltFireHUDHint() && this->CanToggleSilencer();
	}

	const char *GetWorldModel( void ) const
	{
		if (this->IsSilenced())
		{
			return this->m_szSilencedModel;
		}
		else
		{
			return BaseClass::GetWorldModel();
		}
	}

#ifdef CLIENT_DLL
	int GetWorldModelIndex( void )
	{
		if (this->IsSilenced())
		{
			return this->m_iSilencedModelIndex;
		}
		else
		{
			return BaseClass::GetWorldModelIndex();
		}
	}
#endif

#ifdef GAME_DLL
	void InputSetSilencer( inputdata_t &inputdata )
	{
		this->m_bSilenced = inputdata.value.Bool();
	}

	void InputToggleSilencer( inputdata_t &inputdata )
	{
		this->m_bSilenced = !this->m_bSilenced;
	}
#endif

public:
	CNetworkVar( bool, m_bCanToggleSilencer );
	CNetworkVar( bool, m_bSilenced );
	CNetworkVar( int, m_iSilencedModelIndex ); // Not saved

	char	m_szSilencedModel[MAX_WEAPON_STRING]; // Not saved
};

#ifdef CLIENT_DLL

#define DEFINE_CSS_WEAPON_SILENCED_NETWORK_TABLE() \
	RecvPropBool( RECVINFO( m_bCanToggleSilencer ) ),	\
	RecvPropBool( RECVINFO( m_bSilenced ) ), \
	RecvPropInt( RECVINFO(m_iSilencedModelIndex) ), \

#define DEFINE_CSS_WEAPON_SILENCED_DATADESC() \
	DEFINE_KEYFIELD( m_bSilenced, FIELD_BOOLEAN, "Silenced" ), \

#else

#define DEFINE_CSS_WEAPON_SILENCED_NETWORK_TABLE() \
	SendPropBool( SENDINFO( m_bCanToggleSilencer ) ),	\
	SendPropBool( SENDINFO( m_bSilenced ) ), \
	SendPropModelIndex( SENDINFO(m_iSilencedModelIndex) ), \

#define DEFINE_CSS_WEAPON_SILENCED_DATADESC() \
	DEFINE_KEYFIELD( m_bCanToggleSilencer, FIELD_BOOLEAN, "CanToggleSilencer" ),	\
	DEFINE_KEYFIELD( m_bSilenced, FIELD_BOOLEAN, "Silenced" ), \
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetSilencer", InputSetSilencer ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleSilencer", InputToggleSilencer ),	\

#endif

//=============================================================================
//=============================================================================

template <class BASE_WEAPON>
class CBase_CSS_HL2_BurstableWeapon : public BASE_WEAPON
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_BurstableWeapon, BASE_WEAPON );

public:

	bool	InBurst() const { return this->m_iBurstSize > 0 || (this->m_bInBurstMode && this->GetOwner() && this->GetOwner()->IsNPC()); }
	bool	CanUseBurstMode() const { return this->m_bCanUseBurstMode; }

	//virtual float	GetBurstCycleRate( void ) { return 1.0f; }
	virtual int		GetBurstSize( void ) { return 3; };

	bool Deploy( void )
	{
		// Forget about any bursts this weapon was firing when holstered
		this->m_iBurstSize = 0;
		return BaseClass::Deploy();
	}

	void PrimaryAttack( void )
	{
		if (this->m_bFireOnEmpty)
		{
			return;
		}

		if (this->m_bInBurstMode)
		{
			// Don't continue if we're already burst firing
			if (InBurst())
				return;

			this->SendWeaponAnim( ACT_VM_IDLE ); // HACKHACK: Ensures that the burst animation begins

			this->m_iBurstSize = this->GetBurstSize();

			// Call the think function directly so that the first round gets fired immediately.
			this->BurstThink();
			this->SetThink( &CBase_CSS_HL2_BurstableWeapon<BASE_WEAPON>::BurstThink );

			// Pick up the rest of the burst through the think function.
			this->SetNextThink( gpGlobals->curtime + this->GetFireRate() );
		}
		else
		{
			BaseClass::PrimaryAttack();
		}
	}

	void SecondaryAttack( void )
	{
		if (this->CanUseBurstMode())
		{
			if (this->m_bInBurstMode)
			{
				// Burst off
				this->m_bInBurstMode = false;
				this->WeaponSound( SPECIAL1 );
			}
			else
			{
				// Burst on
				this->m_bInBurstMode = true;
				this->WeaponSound( SPECIAL2 );
			}

			this->m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f; // TODO: Real cooldown?
		}
	}

	bool ShouldDisplayAltFireHUDHint()
	{
		return BaseClass::ShouldDisplayAltFireHUDHint() && this->CanUseBurstMode();
	}

	virtual void FinishBurst( void )
	{
		this->m_flNextPrimaryAttack = gpGlobals->curtime + this->SequenceDuration(); // TODO: Proper cooldown?
		//this->m_flSoonestPrimaryAttack = gpGlobals->curtime + this->SequenceDuration(); // TODO: Proper cooldown?
	}

	void BurstThink( void )
	{
		this->m_flNextPrimaryAttack = gpGlobals->curtime; // HACK?
		BaseClass::PrimaryAttack();

		this->m_iBurstSize--;

		if( this->m_iBurstSize == 0 )
		{
			// The burst is over!
			this->SetThink(NULL);

			FinishBurst();

			// idle immediately to stop the firing animation
			//SetWeaponIdleTime( gpGlobals->curtime );
			return;
		}

		this->SetNextThink( gpGlobals->curtime + this->GetFireRate() );
	}

#ifdef GAME_DLL
	void InputSetBurstMode( inputdata_t &inputdata )
	{
		m_bInBurstMode = inputdata.value.Bool();
	}

	void InputToggleBurstMode( inputdata_t &inputdata )
	{
		m_bInBurstMode = !m_bInBurstMode;
	}
#endif

public:
	CNetworkVar( bool, m_bCanUseBurstMode );
	CNetworkVar( bool, m_bInBurstMode );
	CNetworkVar( int, m_iBurstSize );
};

#ifdef CLIENT_DLL

#define DEFINE_CSS_WEAPON_BURSTABLE_NETWORK_TABLE() \
	RecvPropBool( RECVINFO( m_bCanUseBurstMode ) ),	\
	RecvPropBool( RECVINFO( m_bInBurstMode ) ),	\
	RecvPropInt( RECVINFO( m_iBurstSize ) ),	\

#define DEFINE_CSS_WEAPON_BURSTABLE_DATADESC() \
	DEFINE_KEYFIELD( m_bCanUseBurstMode, FIELD_BOOLEAN, "CanUseBurstMode" ),	\
	DEFINE_FIELD( m_bInBurstMode, FIELD_BOOLEAN ),	\
	DEFINE_FIELD( m_iBurstSize, FIELD_INTEGER ),	\

#define DEFINE_CSS_WEAPON_BURSTABLE_PREDICTDESC() \
	DEFINE_PRED_FIELD( m_bInBurstMode, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),	\
	DEFINE_PRED_FIELD( m_iBurstSize, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),	\

#else

#define DEFINE_CSS_WEAPON_BURSTABLE_NETWORK_TABLE() \
	SendPropBool( SENDINFO( m_bCanUseBurstMode ) ),	\
	SendPropBool( SENDINFO( m_bInBurstMode ) ), \
	SendPropInt( SENDINFO( m_iBurstSize ) ), \

#define DEFINE_CSS_WEAPON_BURSTABLE_DATADESC() \
	DEFINE_KEYFIELD( m_bCanUseBurstMode, FIELD_BOOLEAN, "CanUseBurstMode" ),	\
	DEFINE_KEYFIELD( m_bInBurstMode, FIELD_BOOLEAN, "InBurstMode" ),	\
	DEFINE_FIELD( m_iBurstSize, FIELD_INTEGER ),	\
	DEFINE_FUNCTION( BurstThink ),	\
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetBurstMode", InputSetBurstMode ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleBurstMode", InputToggleBurstMode ),	\

#endif

//=============================================================================
//=============================================================================

// Base used by both scoped assault rifles and sniper rifles
template <class BASE_WEAPON>
class CBase_CSS_HL2_BaseScopeableWeapon : public BASE_WEAPON
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_BaseScopeableWeapon, BASE_WEAPON );

public:

	bool CanUseScope() const { return this->m_bCanUseScope; }

	bool Reload( void )
	{
		bool bBase = BaseClass::Reload();

		if (bBase)
			this->StopEffects();

		return bBase;
	}

	bool Holster( CBaseCombatWeapon *pSwitchingTo )
	{
		this->StopEffects();
		return BaseClass::Holster( pSwitchingTo );
	}

	void CheckZoomToggle( void )
	{
		if (this->CanUseScope() == false)
			return;

		CBasePlayer *pPlayer = ToBasePlayer( this->GetOwner() );

		if (pPlayer->m_afButtonPressed & IN_ATTACK2)
		{
			this->ToggleZoom();
		}
	}

	virtual void StopZoom( void ) { this->ToggleZoom(); }
	virtual void ToggleZoom( void ) {}

	void StopEffects( void )
	{
		// Stop zooming
		if ( this->IsWeaponZoomed() )
		{
			this->StopZoom();
		}
	}

	void Drop( const Vector &vecVelocity )
	{
		this->StopEffects();
		BaseClass::Drop( vecVelocity );
	}

	void ItemPostFrame( void )
	{
		// Allow zoom toggling
		this->CheckZoomToggle();

		BaseClass::ItemPostFrame();
	}

	bool ShouldDisplayAltFireHUDHint()
	{
		return BaseClass::ShouldDisplayAltFireHUDHint() && this->CanUseScope();
	}

public:
	CNetworkVar( bool, m_bCanUseScope );
};

// For scoped assault rifles
template <class BASE_WEAPON>
class CBase_CSS_HL2_ScopeableWeapon : public CBase_CSS_HL2_BaseScopeableWeapon<BASE_WEAPON>
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_ScopeableWeapon, CBase_CSS_HL2_BaseScopeableWeapon<BASE_WEAPON> );

public:

	virtual bool	IsWeaponZoomed() { return m_bInZoom; }

	virtual int		GetZoomFOV() const { return 55; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	virtual void ToggleZoom( void )
	{
		CBasePlayer *pPlayer = ToBasePlayer( this->GetOwner() );
	
		if ( pPlayer == NULL )
			return;

		if ( this->m_bInZoom )
		{
			if ( pPlayer->SetFOV( this, 0, this->GetUnZoomRate() ) )
			{
				this->m_bInZoom = false;
			}
		}
		else
		{
			if ( pPlayer->SetFOV( this, this->GetZoomFOV(), this->GetZoomRate() ) )
			{
				this->m_bInZoom = true;
			}
		}

		this->WeaponSound( SPECIAL3 );

		// Scope overlay handled by CBase_CSS_HL2_SniperRifle
	}
	
public:
	CNetworkVar( bool, m_bInZoom );
};

#ifdef CLIENT_DLL

#define DEFINE_CSS_WEAPON_SCOPEABLE_NETWORK_TABLE() \
	RecvPropBool( RECVINFO( m_bCanUseScope ) ),	\
	RecvPropBool( RECVINFO( m_bInZoom ) ),	\

#define DEFINE_CSS_WEAPON_SCOPEABLE_PREDICTDESC() \
	DEFINE_PRED_FIELD( m_bInZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),	\

#else

#define DEFINE_CSS_WEAPON_SCOPEABLE_NETWORK_TABLE() \
	SendPropBool( SENDINFO( m_bCanUseScope ) ),	\
	SendPropBool( SENDINFO( m_bInZoom ) ), \

#endif

#define DEFINE_CSS_WEAPON_SCOPEABLE_DATADESC() \
	DEFINE_KEYFIELD( m_bCanUseScope, FIELD_BOOLEAN, "CanUseScope" ),	\
	DEFINE_FIELD( m_bInZoom,		FIELD_BOOLEAN ),	\

//=============================================================================
//=============================================================================

template <class BASE_ITEM>
class CBase_CSS_HL2_Item : public BASE_ITEM
{
	DECLARE_CLASS_NOFRIEND( CBase_CSS_HL2_Item, BASE_ITEM );

public:
};

//---------------------------------------------------

class CBase_CSS_HL2_Pistol : public CBase_CSS_HL2_Weapon<CBaseHLCombatWeapon>
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_Pistol, CBase_CSS_HL2_Weapon<CBaseHLCombatWeapon> );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBase_CSS_HL2_Pistol(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_PISTOL; }

	void	Precache( void );
	void	ItemPostFrame( void );
	void	ItemPreFrame( void );
	void	ItemBusyFrame( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	AddViewKick( void );
	void	DryFire( void );
#ifndef CLIENT_DLL
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
#endif

	void	UpdatePenaltyTime( void );

#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif
	Activity	GetPrimaryAttackActivity( void );

	virtual bool Reload( void );

	//-----------------------------------------------------------------------------

	inline float GetAccuracyPenalty() { return m_flAccuracyPenalty; }

	virtual float GetViewKickBase() { return 0.0f; }

	virtual float GetRefireRate() { return 0.1f; }
	virtual float GetDryRefireRate() { return 0.2f; }

	virtual float GetShotPenaltyTime() { return 0.2f; }

	virtual int	GetMinBurst() { return 1; }
	virtual int	GetMaxBurst() { return 3; }
	
	//-----------------------------------------------------------------------------

protected:
	CNetworkVar( float,	m_flSoonestPrimaryAttack );
private:
	CNetworkVar( float,	m_flLastAttackTime );
	CNetworkVar( float,	m_flAccuracyPenalty );
	CNetworkVar( int,	m_nNumShotsFired );
};

//---------------------------------------------------

class CBase_CSS_HL2_MachineGun : public CBase_CSS_HL2_Weapon<CHLMachineGun>
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_MachineGun, CBase_CSS_HL2_Weapon<CHLMachineGun> );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();

	CBase_CSS_HL2_MachineGun(void);
	
	void	Precache( void );
	void	AddViewKick( void );

	int		GetMinBurst() { return 2; }
	int		GetMaxBurst() { return 5; }

	float	GetFireRate( void ) { return 0.075f; }	// 13.3hz
#ifndef CLIENT_DLL
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif
	Activity	GetPrimaryAttackActivity( void );

	virtual float GetViewKickBase() { return 0.0f; }

	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_5DEGREES;
		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

#ifndef CLIENT_DLL
	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif
	
	//-----------------------------------------------------------------------------

private:
};

//=============================================================================
//=============================================================================

class CBase_CSS_HL2_SMG : public CBase_CSS_HL2_MachineGun
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_SMG, CBase_CSS_HL2_MachineGun );

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG1; }

	int		GetMinBurst( void ) { return 2; }
	int		GetMaxBurst( void ) { return 5; }

private:
};

//=============================================================================
//=============================================================================

class CBase_CSS_HL2_Rifle : public CBase_CSS_HL2_MachineGun
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_Rifle, CBase_CSS_HL2_MachineGun );

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR2; }

	const WeaponProficiencyInfo_t *GetProficiencyValues();

	int		GetMinBurst( void ) { return 2; }
	int		GetMaxBurst( void ) { return 5; }

private:
};

//=============================================================================
//=============================================================================

// TODO: Scope, etc.
class CBase_CSS_HL2_SniperRifle : public CBase_CSS_HL2_BaseScopeableWeapon<CBase_CSS_HL2_Rifle>
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_SniperRifle, CBase_CSS_HL2_BaseScopeableWeapon<CBase_CSS_HL2_Rifle> );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBase_CSS_HL2_SniperRifle();

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SNIPER_RIFLE; }

	const WeaponProficiencyInfo_t *GetProficiencyValues();

	virtual bool	IsWeaponZoomed() { return m_nZoomLevel > 0; }

	virtual int		GetZoom1FOV() const { return 55; }
	virtual int		GetZoom2FOV() const { return 25; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	void	Precache();
	void	AddViewKick( void );

	virtual void FireBullets( const FireBulletsInfo_t &info );

	virtual void	ToggleZoom( void );
	virtual void	StopZoom( void );

	virtual int	GetMinBurst() { return 1; }
	virtual int	GetMaxBurst() { return 1; }

	virtual float	GetMinRestTime() { return 1.55f; }
	virtual float	GetMaxRestTime() { return 1.8f; }

	// Scale based on our damage multiplier by default
	virtual float	GetDamageForceScale() const { return GetDamageMultiplier() * 4.0f; }

private:
	CNetworkVar( int, m_nZoomLevel );
};

//=============================================================================
//=============================================================================

class CBase_CSS_HL2_Shotgun : public CBase_CSS_HL2_Weapon<CBaseHLCombatWeapon>
{
public:
	DECLARE_CLASS( CBase_CSS_HL2_Shotgun, CBase_CSS_HL2_Weapon<CBaseHLCombatWeapon> );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBase_CSS_HL2_Shotgun(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SHOTGUN; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_10DEGREES;
		return cone;
	}

	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 3; }

	bool StartReload( void );
	bool Reload( void );
	void FillClip( void );
	void FinishReload( void );
	void CheckHolsterReload( void );
	void Pump( void );
//	void WeaponIdle( void );
	void ItemHolsterFrame( void );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void DryFire( void );
	bool Deploy( void );

	virtual int GetNumPellets() const { return 8; }
	virtual int GetNumDoublePellets() const { return 12; }

	virtual bool CanPump() { return true; }
	virtual bool PumpsInOtherAnims() { return false; }

#ifndef CLIENT_DLL
	int CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

private:
	CNetworkVar( bool,	m_bNeedPump );		// When emptied completely
	CNetworkVar( bool,	m_bDelayedFire1 );	// Fire primary when finished reloading
	CNetworkVar( bool,	m_bDelayedFire2 );	// Fire secondary when finished reloading
	CNetworkVar( bool,	m_bDelayedReload );	// Reload when finished pump
};

#endif // WEAPON_CSS_BASE_H
