//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ASW_TESLATRAP_H
#define ASW_TESLATRAP_H

#ifdef _WIN32
#pragma once
#endif

#include "Color.h"
#include "asw_prop_physics.h"
#include "asw_shareddefs.h"

class CSoundPatch;
class CASW_Marine;

//---------------------------------------------------------
//---------------------------------------------------------
#define ASW_TESLATRAP_HOOK_RANGE		64

class CASW_TeslaTrap : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_TeslaTrap, CBaseCombatCharacter );

public:
	DECLARE_SERVERCLASS();

	CASW_TeslaTrap();
	virtual ~CASW_TeslaTrap();
	void Precache();
	void Spawn();
	void OnRestore();

	void SetTrapState( int iState );
	int GetTrapState() { return m_iTrapState; }
	int DrawDebugTextOverlays();
	void LayFlat( void );

	void TeslaTouch( CBaseEntity *pOther );
	void TeslaSettleThink( void );
	void TeslaChargeThink( void );
	void TeslaSearchThink( void );
	bool ZapTarget( CBaseEntity *pEntity );

	void RunAnimation ( void );
	virtual void ReachedEndOfSequence( void );

	//bool IsCharged() { return m_bCharged; }
	//void SetCharged( bool bSetCharged );
	float FindNearestNPC();
	void SetNearestNPC( CBaseEntity *pNearest ) { m_hNearestNPC.Set( pNearest ); }
	//int OnTakeDamage( const CTakeDamageInfo &info );
	bool IsFriend( CBaseEntity *pEntity );

	void SetRadius( float flRadius ) { m_flRadius = flRadius; }
	void SetDamage( float flDamage ) { m_flDamage = flDamage; }
	void SetNextFullChargeTime( float flNextFullChargeTime ) { m_flNextFullChargeTime = flNextFullChargeTime; }

	bool IsPlayerPlaced() { return m_bPlacedByPlayer; }

	static CASW_TeslaTrap* Tesla_Trap_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );	

	EHANDLE m_hCreatorWeapon;
	Class_T m_CreatorWeaponClass;
	CHandle<CASW_Marine> m_hMarineDeployer;

	DECLARE_DATADESC();

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_TESLA_TRAP_PROJECTILE; }

private:
	//bool	m_bCharged;
	EHANDLE	m_hNearestNPC;

	CSoundPatch	*m_pWarnSound;

	bool	m_bLockSilently;
	bool	m_bFoeNearest;

	float	m_flIgnoreWorldTime;

	bool	m_bDisarmed;

	bool	m_bPlacedByPlayer;
	bool	m_bActive;
	//int     m_iModification;

	float m_flChargeInterval;

	CNetworkVar(int, m_iTrapState);
	CNetworkVar(int, m_iAmmo);
	CNetworkVar(int, m_iMaxAmmo);

	CNetworkVar(float, m_flRadius);
	CNetworkVar(float, m_flDamage);
	CNetworkVar(float, m_flNextFullChargeTime);

	CNetworkVar(bool, m_bAssembled);

};

extern CUtlVector<CASW_TeslaTrap*> g_aTeslaTraps;

#endif // ASW_TESLATRAP_H
