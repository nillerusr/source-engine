//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODBASEGRENADE_H
#define WEAPON_DODBASEGRENADE_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_dodbase.h"
#include "dod_shareddefs.h"


#ifdef CLIENT_DLL
	
	#define CWeaponDODBaseGrenade C_WeaponDODBaseGrenade

#endif


class CWeaponDODBaseGrenade : public CWeaponDODBase
{
public:
	DECLARE_CLASS( CWeaponDODBaseGrenade, CWeaponDODBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponDODBaseGrenade();

	virtual void	PrimaryAttack();
	virtual bool	CanHolster();

	virtual bool IsArmed( void ) { return m_bArmed == true; }
	void SetArmed( bool bArmed ) { m_bArmed = bArmed; }

	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetDrawActivity( void );

#ifdef CLIENT_DLL

#else
	DECLARE_DATADESC();

	virtual void	Precache();
	virtual void	ItemPostFrame();
	virtual bool    AllowsAutoSwitchFrom( void ) const;
	virtual bool	Deploy();
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual bool	Reload();

	void	DecrementAmmo( CBaseCombatCharacter *pOwner );

	void	StartThrow( int iThrowType );

	void	DropGrenade( void );	//forces the grenade to be dropped

	virtual void ThrowGrenade( bool bDrop );

	// Each derived grenade class implements this.
	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH );

	void SetDetonateTime( float flDetonateTime )
	{
		m_flDetonateTime = flDetonateTime;
		SetArmed( true );
	}

	void InputSetDetonateTime( inputdata_t &inputdata );

	virtual float GetDetonateTimerLength( void ) { return GRENADE_FUSE_LENGTH; }

protected:

#endif

	bool m_bRedraw;	// Draw the weapon again after throwing a grenade
	CNetworkVar( bool, m_bPinPulled );	// Set to true when the pin has been pulled but the grenade hasn't been thrown yet.
	CNetworkVar( bool, m_bArmed );			// is the grenade armed?
	float m_flDetonateTime;	// what time the grenade will explode ( if > 0 )
	CWeaponDODBaseGrenade( const CWeaponDODBaseGrenade & ) {}
};


#endif // WEAPON_DODBASEGRENADE_H
