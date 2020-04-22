//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFC_CROWBAR_H
#define WEAPON_TFC_CROWBAR_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_tfcbase.h"


#if defined( CLIENT_DLL )

	#define CTFCCrowbar C_TFCCrowbar

#endif


// ----------------------------------------------------------------------------- //
// CTFCCrowbar class definition.
// ----------------------------------------------------------------------------- //

class CTFCCrowbar : public CWeaponTFCBase
{
public:
	DECLARE_CLASS( CTFCCrowbar, CWeaponTFCBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	#ifndef CLIENT_DLL
		DECLARE_DATADESC();
	#endif

	
	CTFCCrowbar();

	// We say yes to this so the weapon system lets us switch to it.
	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	virtual void ItemPostFrame();
	virtual void Precache();

	void Spawn();
	void Smack();
	void PrimaryAttack();

	bool Deploy();
	void Holster( int skiplocal = 0 );
	bool CanDrop();

	void WeaponIdle();

	virtual TFCWeaponID GetWeaponID( void ) const;


// Overrideables.
public:

#ifdef GAME_DLL
	// This is called first to determine if the axe should apply damage to the entity.
	virtual void AxeHit( 
		CBaseEntity *pHit, 
		bool bFirstSwing, 
		trace_t &tr,
		float *flDamage,
		bool *bDoEffects
		);
#endif


public:
	
	trace_t m_trHit;
	EHANDLE m_pTraceHitEnt;
	float m_flStoredPrimaryAttack;


private:
	CTFCCrowbar( const CTFCCrowbar & ) {}
};


#endif // WEAPON_TFC_CROWBAR_H
