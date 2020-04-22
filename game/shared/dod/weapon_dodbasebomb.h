//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_DODBASEBOMB_H
#define WEAPON_DODBASEBOMB_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_dodbase.h"

#if defined( CLIENT_DLL )

#define CDODBaseBombWeapon C_DODBaseBombWeapon

#else

#include "dod_bombtarget.h"

#endif

//-----------------------------------------------------------------------------
// RPG
//-----------------------------------------------------------------------------
class CDODBaseBombWeapon : public CWeaponDODBase
{
public:
	DECLARE_CLASS( CDODBaseBombWeapon, CWeaponDODBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CDODBaseBombWeapon();

	virtual void Spawn();
	virtual void Precache( void );
	virtual bool Deploy();
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );

	virtual void ItemPostFrame( void );

	virtual bool CanDrop( void ) { return false; }
	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }
	virtual bool ShouldDrawCrosshair( void ) { return false; }
	virtual bool HasPrimaryAmmo() { return true; }
	virtual bool CanBeSelected() { return false; }

#ifndef CLIENT_DLL
	void StartPlanting( CDODBombTarget *pTarget );
	bool CancelPlanting( void );
	void CompletePlant( void );
	bool IsLookingAtBombTarget( CBasePlayer *pPlayer, CDODBombTarget *pTarget );
#endif

	void SetPlanting( bool bPlanting );
	bool IsPlanting( void );

protected:
	CDODWeaponInfo *m_pWeaponInfo;

private:
	CDODBaseBombWeapon( const CDODBaseBombWeapon & );

	bool m_bPlanting;

	// pointer to the dod_bomb_target that we're planting at
	EHANDLE m_hBombTarget;

	// when the plant will be complete
	float m_flPlantCompleteTime;

	bool m_bUsePlant;	// player hit +use to start this plant
	float m_flNextPlantCheck;

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
};

#endif // WEAPON_DODBASEBOMB_H