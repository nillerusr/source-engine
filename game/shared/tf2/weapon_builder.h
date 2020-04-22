//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_BUILDER_H
#define WEAPON_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_combat_usedwithshieldbase.h"

class CBaseObject;

//=========================================================
// Builder Weapon
//=========================================================
class CWeaponBuilder : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponBuilder, CWeaponCombatUsedWithShieldBase );
public:
	CWeaponBuilder();

	virtual void	UpdateOnRemove( void );

	DECLARE_SERVERCLASS();

	virtual void	Precache( void );
	virtual bool	CanDeploy( void );
	virtual bool	CanHolster( void );
	virtual CBaseCombatWeapon *GetLastWeapon( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	WeaponIdle( void );
	virtual bool	Deploy( void );	
	virtual const char	*GetViewModel( int viewmodelindex = 0 ) const;

	void	SetCurrentState( int iState );
	void	SetCurrentObject( int iObject );

	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual void	Equip( CBaseCombatCharacter *pOwner );

	// Add a new object type to the list of objects this builder weapon can build
	void	AddBuildableObject( int iObjectType );

	// Placement
	void	StartPlacement( void );
	void	StopPlacement( void );
	bool	UpdatePlacement( void );

	// Building
	void	StartBuilding( void );
	void	StoppedBuilding( int iObjectType );
	bool	IsBuilding( void );
	void	FinishedObject( void );

	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );

	virtual bool			ShouldShowControlPanels( void );

private:
	void	PerformModifications( CBaseObject* pObject );

public:
	CNetworkVar( int, m_iBuildState );
	CNetworkVar( unsigned int, m_iCurrentObject );
	int		m_iCurrentObjectID;
	CNetworkVar( int, m_iCurrentObjectState );

	// Objects that this builder can build
	CNetworkArray( bool, m_bObjectValidity, OBJ_LAST );
	// Buildability of each object
	CNetworkArray( bool, m_bObjectBuildability, OBJ_LAST );

	// Build data for the current object, propagated when the player starts to build it
	CNetworkVar( float, m_flStartTime );
	CNetworkVar( float, m_flTotalTime );

	float	m_flLastRepairTime;

	CNetworkHandle( CBaseObject, m_hObjectBeingBuilt );
};


#endif // WEAPON_BUILDER_H
