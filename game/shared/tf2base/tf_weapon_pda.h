//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: PDA Weapon
//
//=============================================================================

#ifndef TF_WEAPON_PDA_H
#define TF_WEAPON_PDA_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include "tf_weaponbase.h"

// Client specific.
#if defined( CLIENT_DLL ) 
	#define CTFWeaponPDA C_TFWeaponPDA
	#define CTFWeaponPDA_Engineer_Build	C_TFWeaponPDA_Engineer_Build
	#define CTFWeaponPDA_Engineer_Destroy	C_TFWeaponPDA_Engineer_Destroy
	#define CTFWeaponPDA_Spy		C_TFWeaponPDA_Spy
#endif

class CTFWeaponPDA : public CTFWeaponBase
{
public:
	DECLARE_CLASS( CTFWeaponPDA, CTFWeaponBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#if !defined( CLIENT_DLL ) 
	DECLARE_DATADESC();
#endif

	CTFWeaponPDA();

	virtual void	Spawn();

#if !defined( CLIENT_DLL )
		virtual void	Precache();
		virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
#else
		virtual float	CalcViewmodelBob( void );
#endif

	virtual bool	ShouldShowControlPanels( void );

	virtual void	PrimaryAttack();
	virtual void	SecondaryAttack();
	virtual int		GetWeaponID( void ) const						{ return TF_WEAPON_PDA; }
	virtual bool	ShouldDrawCrosshair( void )						{ return false; }
	virtual bool	HasPrimaryAmmo()								{ return true; }
	virtual bool	CanBeSelected()									{ return true; }

	virtual const char *GetPanelName() { return "pda_panel"; }



public:	
	CTFWeaponInfo	*m_pWeaponInfo;

private:

	CTFWeaponPDA( const CTFWeaponPDA & ) {}
};

class CTFWeaponPDA_Engineer_Build : public CTFWeaponPDA
{
public:
	DECLARE_CLASS( CTFWeaponPDA_Engineer_Build, CTFWeaponPDA );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual const char *GetPanelName() { return ""; }
	virtual int		GetWeaponID( void ) const { return TF_WEAPON_PDA_ENGINEER_BUILD; }
};

#ifdef CLIENT_DLL

extern ConVar tf_build_menu_controller_mode;

#endif
class CTFWeaponPDA_Engineer_Destroy : public CTFWeaponPDA
{
public:
	DECLARE_CLASS( CTFWeaponPDA_Engineer_Destroy, CTFWeaponPDA );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual const char *GetPanelName() { return ""; }
	virtual int		GetWeaponID( void ) const { return TF_WEAPON_PDA_ENGINEER_DESTROY; }

	virtual bool	VisibleInWeaponSelection( void )
	{
		if ( IsConsole()
#ifdef CLIENT_DLL
			|| tf_build_menu_controller_mode.GetBool() 
#endif 
			)
		{
			return false;
		}

		return BaseClass::VisibleInWeaponSelection();
	}
};

class CTFWeaponPDA_Spy : public CTFWeaponPDA
{
public:
	DECLARE_CLASS( CTFWeaponPDA_Spy, CTFWeaponPDA );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual const char *GetPanelName() { return ""; }
	virtual int		GetWeaponID( void ) const { return TF_WEAPON_PDA_SPY; }

#ifdef CLIENT_DLL
	virtual bool Deploy( void );
#endif
};

#endif // TF_WEAPON_PDA_H