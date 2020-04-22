//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_COMBATSHIELD_SHARED_H
#define WEAPON_COMBATSHIELD_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfcombatweapon_shared.h"

// Shield States
enum
{
	SS_DOWN,			// Not in use
	SS_RAISING,			// Going up
	SS_UP,				// Up, active, and blocking
	SS_LOWERING,		// Going down
	SS_PARRYING,		// In the process of parrying
	SS_PARRYING_FINISH_SWING,	// Time to allow the parrying animation to finish
	SS_UNAVAILABLE,		// Post-parry, unavailable for using
};

// Shield Hitboxes
enum
{
	SS_HITBOX_NOSHIELD,
	SS_HITBOX_FULLSHIELD,
	SS_HITBOX_SMALLSHIELD,
};

// Range of the shield bash
#define SHIELD_BASH_RANGE	64.0

#if defined( CLIENT_DLL )
class Beam_t;

#define CWeaponCombatShield C_WeaponCombatShield
#define CWeaponCombatShieldAlien C_WeaponCombatShieldAlien
#endif

//-----------------------------------------------------------------------------
// Purpose: Shared version of CWeaponCombatShield
//-----------------------------------------------------------------------------
class CWeaponCombatShield : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponCombatShield, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

					CWeaponCombatShield();

	virtual void	Precache( void );

	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool	TakeShieldBash( CBaseTFPlayer *pBasher );
	virtual void	ItemPostFrame( void );
	virtual void	ItemBusyFrame( void );
	virtual void	ShieldPostFrame( void );
	virtual	void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual int		UpdateClientData( CBasePlayer *pPlayer );

	void			SetShieldState( int iShieldState );
	void			UpdateShieldState( void );
	int				GetShieldState( void );

	void			ShieldBash( void );

	void			SetAllowPostFrame( bool allow );
	void			SetShieldUsable( bool bUsable );
	bool			ShieldUsable( void );

	// Shield power levels
	float			AttemptToBlock( float flDamage );
	void			ShieldRechargeThink( void );

	virtual bool	VisibleInWeaponSelection( void );
	virtual int		GetOtherWeaponsActivity( int iActivity );
	virtual int		ReplaceOtherWeaponsActivity( int iActivity );
	
	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

	bool			IsUp( void );
	float			GetRaisingTime( void );
	float			GetLoweringTime( void );

	// Shield health handling
	float			GetShieldHealth( void );
	void			AddShieldHealth( float flHealth );
	void			RemoveShieldHealth( float flHealth );

#if defined( CLIENT_DLL )
	virtual void	GetViewmodelBoneControllers( CBaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS]);
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual void	DrawAmmo( void );
	virtual void	HandleInput( void );

	virtual void	ViewModelDrawn( C_BaseViewModel *pViewModel );

	void			InitShieldBeam( void );
	void			InitTeslaBeam( void );
	
	void			DrawBeams( C_BaseViewModel *pViewModel );

#endif

private:
	CWeaponCombatShield( const CWeaponCombatShield& );

	CNetworkVar( int, m_iShieldState );

	bool			m_bUsable;

	CNetworkVar( float, m_flShieldUpStartTime );
	CNetworkVar( float, m_flShieldDownStartTime );
	CNetworkVar( float, m_flShieldParryEndTime );
	CNetworkVar( float, m_flShieldParrySwingEndTime );
	CNetworkVar( float, m_flShieldUnavailableEndTime );

	CNetworkVar( float, m_flShieldRaisedTime );
	CNetworkVar( float, m_flShieldLoweredTime );

	CNetworkVar( float, m_flShieldHealth );

	CNetworkVar( bool, m_bAllowPostFrame );
	CNetworkVar( bool, m_bHasShieldParry );

#if defined( CLIENT_DLL )
	float			m_flFlashTimeEnd;

	float				m_flTeslaSpeed;
	float				m_flTeslaSkitter;
	float				m_flTeslaLeftInc;
	float				m_flTeslaRightInc;
	Beam_t				*m_pTeslaBeam;
	Beam_t				*m_pTeslaBeam2;
	CMaterialReference	m_hTeslaSpriteMaterial;

	bool				m_bLeftToRight;
	int					m_nShieldEdge;
	float				m_flShieldSpeed;
	float				m_flShieldInc;
	Beam_t				*m_pShieldBeam;
	Beam_t				*m_pShieldBeam2;
	Beam_t				*m_pShieldBeam3;
	CMaterialReference	m_hShieldSpriteMaterial;
#endif
};

//-----------------------------------------------------------------------------
// Purpose: Need to do different art on client vs server
//-----------------------------------------------------------------------------
class CWeaponCombatShieldAlien : public CWeaponCombatShield
{
	DECLARE_CLASS( CWeaponCombatShieldAlien, CWeaponCombatShield );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatShieldAlien( void ) {}

private:
	CWeaponCombatShieldAlien( const CWeaponCombatShieldAlien & );
};


#endif // WEAPON_COMBATSHIELD_SHARED_H
