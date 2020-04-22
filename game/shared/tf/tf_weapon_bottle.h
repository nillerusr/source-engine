//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_BOTTLE_H
#define TF_WEAPON_BOTTLE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_melee.h"

#ifdef CLIENT_DLL
#define CTFBottle C_TFBottle
#define CTFStickBomb C_TFStickBomb
#endif

//=============================================================================
//
// Bottle class.
//
class CTFBottle : public CTFWeaponBaseMelee
{
public:

	DECLARE_CLASS( CTFBottle, CTFWeaponBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFBottle();
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_BOTTLE; }

	virtual void		Smack( void );
	virtual void		WeaponReset( void );
	virtual bool		DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );

	virtual void		SwitchBodyGroups( void );

private:

	CTFBottle( const CTFBottle & ) {}

protected:

	CNetworkVar( bool,	m_bBroken  );
};

//=============================================================================
//
// StickBomb class.
//
class CTFStickBomb : public CTFBottle
{
public:

	DECLARE_CLASS( CTFStickBomb, CTFBottle );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFStickBomb();

	virtual void		Precache( void );
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_STICKBOMB; }
	virtual void		Smack( void );
	virtual void		WeaponReset( void );
	virtual void		WeaponRegenerate( void );
	virtual void		SwitchBodyGroups( void );
	virtual const char*	GetWorldModel( void ) const;
#ifdef CLIENT_DLL
	virtual int			GetWorldModelIndex( void );
#endif

	void				SetDetonated( int iVal ) { m_iDetonated = iVal; }
	int					GetDetonated( void ) { return m_iDetonated; }

private:

	CNetworkVar( int,	m_iDetonated ); // int, not bool so we can use a recv proxy
};

#endif // TF_WEAPON_BOTTLE_H
