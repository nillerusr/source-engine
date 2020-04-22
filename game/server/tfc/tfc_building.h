//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_BUILDING_H
#define TFC_BUILDING_H
#ifdef _WIN32
#pragma once
#endif


#include "baseanimating.h"
#include "tfc_shareddefs.h"


class CTFBaseBuilding : public CBaseAnimating
{
public:
	EHANDLE real_owner;
};


class CTFTeleporter : public CTFBaseBuilding
{
public:
	void Spawn(void);
	void Precache(void);

	void EXPORT Teleporter_Explode( void );
	void EXPORT TeleporterThink( void );
	void EXPORT TeleporterTouch( CBaseEntity *pOther );

	Class_T Classify(void) { return CLASS_MACHINE; };
	int BloodColor( void ) { return DONT_BLEED; }

	void Remove( void );

	void TeamFortress_TakeEMPBlast(CBaseEntity* pevGren);
	void Finished( void );
	static CTFTeleporter *CreateTeleporter( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, int type );
	BOOL EngineerUse( CBasePlayer *pPlayer );
	void Killed( CBaseEntity *pevInflictor, CBaseEntity *pevAttacker, int iGib );
	void TeleporterSend( CBasePlayer *pPlayer );
	void TeleporterReceive( CBasePlayer *pPlayer, float flDelay );
	void TeleporterKilled( void );
	BOOL TeleportersReady( void );
	void TeleporterFadePlayer( int direction );
	void TeleporterProcessFade( void );
	void SetTeleporterRings( int state );
	void SetTeleporterParticles( int state );
	float GetDamageMultiplier( void );
	CTFTeleporter* FindMatch( void );
	const Vector& GetTeamColor( void );
	bool PlayerIsStandingOnTeleporter( CBaseEntity *pOther );

	CBasePlayer *m_pPlayer;	// player being teleported

	float m_flInitialUseDelay;

	int m_iType;		// entry or exit
	int m_iState;		// state of the teleporter (idle, ready, sending, etc.)
	int m_iDestroyed;	// has this teleporter been destroyed
	
	int	m_iShardIndex;	// Metal shards

	float m_flMyNextThink;	// used to control the pace at which the teleporters work
	float m_flDamageDelay; // damage multiplier that slows the teleporters when they're damaged
};


#endif // TFC_BUILDING_H
