//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#ifndef TF_WEAPON_LUNCHBOX_H
#define TF_WEAPON_LUNCHBOX_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFLunchBox C_TFLunchBox
#define CTFLunchBox_Drink C_TFLunchBox_Drink
#endif

enum lunchbox_weapontypes_t
{
	LUNCHBOX_STANDARD = 0,		// Careful, can be the Scout BONK drink, or the Heavy sandvich.
	LUNCHBOX_ADDS_MAXHEALTH,
	LUNCHBOX_ADDS_MINICRITS,
	LUNCHBOX_STANDARD_ROBO,
	LUNCHBOX_STANDARD_FESTIVE,
	LUNCHBOX_ADDS_AMMO,
};

#define TF_SANDWICH_REGENTIME	30
#define TF_CHOCOLATE_BAR_REGENTIME	10

//=============================================================================
//
// TF Weapon Lunchbox.
//
class CTFLunchBox : public CTFWeaponBase
{
public:

	DECLARE_CLASS( CTFLunchBox, CTFWeaponBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFLunchBox();

	virtual void	UpdateOnRemove( void );
	virtual void	Precache();
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_LUNCHBOX; }
	virtual void	PrimaryAttack();
	virtual void	SecondaryAttack();
	virtual void	WeaponReset( void );
	virtual bool	UsesPrimaryAmmo();

	virtual bool	DropAllowed( void );
	int				GetLunchboxType( void ) const { int iMode = 0; CALL_ATTRIB_HOOK_INT( iMode, set_weapon_mode ); return iMode; };

	float			GetProgress( void ) { return GetEffectBarProgress(); }
	const char*		GetEffectLabelText( void )	{ return "#TF_SANDWICH"; }

	void			DrainAmmo( bool bForceCooldown = false );

	virtual float	InternalGetEffectBarRechargeTime( void ) { return GetLunchboxType() == LUNCHBOX_ADDS_MAXHEALTH ? TF_CHOCOLATE_BAR_REGENTIME : TF_SANDWICH_REGENTIME; }
	virtual void	Detach( void ) OVERRIDE;

#ifdef GAME_DLL
	void			ApplyBiteEffects( CTFPlayer *pPlayer );
#endif

private:
	CTFLunchBox( const CTFLunchBox & ) {}

	// Prevent spamming with resupply cabinets: only 1 thrown at a time
	EHANDLE		m_hThrownPowerup;
};

//=============================================================================
//
// TF Weapon Energy drink.
//
class CTFLunchBox_Drink : public CTFLunchBox
{
public:

	DECLARE_CLASS( CTFLunchBox_Drink, CTFLunchBox );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFLunchBox_Drink();

	bool				AllowTaunts( void ) { return false; }

	virtual bool		DropAllowed( void ) { return false; }

	const char*			GetEffectLabelText( void )	{ return "#TF_ENERGYDRINK"; }

#ifdef CLIENT_DLL
	virtual const char* ModifyEventParticles( const char* token );
	virtual bool		Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
#endif
};

#endif // TF_WEAPON_LUNCHBOX_H
