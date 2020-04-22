//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_TWOHANDEDCONTAINER_H
#define WEAPON_TWOHANDEDCONTAINER_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfcombatweapon_shared.h"
#include "baseviewmodel_shared.h"
#include "studio.h"

#if defined( CLIENT_DLL )
#define CWeaponTwoHandedContainer C_WeaponTwoHandedContainer
#endif

class CBaseViewModel;

//-----------------------------------------------------------------------------
// Purpose: Client side rep of CBaseTFCombatWeapon 
//-----------------------------------------------------------------------------
class CWeaponTwoHandedContainer : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponTwoHandedContainer, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponTwoHandedContainer();
	~CWeaponTwoHandedContainer();

	virtual void	Spawn( void );
	virtual void	ItemPostFrame( void );
	virtual void	ItemBusyFrame( void );
	virtual void	SetWeapons( CBaseTFCombatWeapon *left, CBaseTFCombatWeapon *right );
	virtual void	UnhideSecondViewmodel( void );
	virtual void	AbortReload( void );
	virtual bool	HasAnyAmmo( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual CBaseCombatWeapon *GetLastWeapon( void );
	virtual int		GetWeight( void );

	virtual const char *GetViewModel( int viewmodelindex = 0 );
	virtual char	   *GetDeathNoticeName( void );
	virtual float	GetDefaultAnimSpeed( void );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )

	virtual bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void GetViewmodelBoneControllers( CBaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS]);

	virtual void OnDataChanged( DataUpdateType_t updateType );

	virtual bool ShouldDraw( void );
	virtual void Redraw(void);
	virtual void DrawAmmo( void );
	virtual void HandleInput( void );
	virtual void OverrideMouseInput( float *x, float *y );
	virtual void ViewModelDrawn( CBaseViewModel *pBaseViewModel );
	void		 HookWeaponEntities( void );
	virtual bool VisibleInWeaponSelection( void );

private:
	CWeaponTwoHandedContainer( const CWeaponTwoHandedContainer & );
public:

#else
	
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

#endif

	CBaseTFCombatWeapon		*GetLeftWeapon( void );
	CBaseTFCombatWeapon		*GetRightWeapon( void );

public:
	CNetworkHandle( CBaseTFCombatWeapon, m_hRightWeapon );
	CNetworkHandle( CBaseTFCombatWeapon, m_hLeftWeapon );
#if defined( CLIENT_DLL )
	CHandle<CBaseTFCombatWeapon>	m_hOldRightWeapon;
	CHandle<CBaseTFCombatWeapon>	m_hOldLeftWeapon;
#endif
};

#endif // WEAPON_TWOHANDEDCONTAINER_H
