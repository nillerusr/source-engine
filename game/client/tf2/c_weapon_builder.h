//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_WEAPON_BUILDER_H
#define C_WEAPON_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_basecombatweapon.h"
#include "weapon_combat_usedwithshieldbase.h"

//=============================================================================
// Purpose: Client version of CWeaponBuiler
//=============================================================================
class C_WeaponBuilder : public C_WeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( C_WeaponBuilder, C_WeaponCombatUsedWithShieldBase );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponBuilder();
	~C_WeaponBuilder();

	virtual void Redraw();
	virtual bool VisibleInWeaponSelection( void ) { return false; }

	virtual bool	IsPlacingObject( void );
	virtual bool	IsBuildingObject( void );

	virtual const char *GetCurrentSelectionObjectName( void );

	C_BaseObject	*GetPlacementModel( void ) { return m_hObjectBeingBuilt.Get(); }

public:
	// Builder Data
	int			m_iBuildState;
	unsigned int m_iCurrentObject;
	int			m_iCurrentObjectState;
	float		m_flStartTime;
	float		m_flTotalTime;
	vgui::HFont m_hFont;

	// Our placement model
	CHandle<C_BaseObject>	m_hObjectBeingBuilt;

	// Objects that this builder can build
	bool		m_bObjectValidity[ OBJ_LAST ];
	// Buildability of each object
	bool		m_bObjectBuildability[ OBJ_LAST ];

	// Materials
	CMaterialReference	m_pIconFireToSelect;

private:
	C_WeaponBuilder( const C_WeaponBuilder & );
};
#endif // C_WEAPON_BUILDER_H
