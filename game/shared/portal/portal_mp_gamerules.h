//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal multiplayer testing.
//
//=============================================================================//

#ifndef PORTAL_MP
#pragma message( __FILE__ "(" __LINE__AS_STRING ") : error custom: This file should not be included anywhere except in the portal multiplayer testing builds" )
#endif

#ifndef PORTAL_MP_GAMERULES_H
#define PORTAL_MP_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
//#include "hl2mp_gamerules.h"
//#include "multiplay_gamerules.h"
#include "teamplay_gamerules.h"

class CPortal_Player;

#ifdef CLIENT_DLL
#define CPortalMPGameRules C_PortalMPGameRules
#define CPortalMPGameRulesProxy C_PortalMPGameRulesProxy
#endif


enum
{
	TEAM_COMBINE = 2,
	TEAM_REBELS,
};


class CPortalMPGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CPortalMPGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class PortalMPViewVectors : public CViewVectors
{
public:
	PortalMPViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vCrouchTraceMin,
		Vector vCrouchTraceMax ) :
	CViewVectors( 
		vView,
		vHullMin,
		vHullMax,
		vDuckHullMin,
		vDuckHullMax,
		vDuckView,
		vObsHullMin,
		vObsHullMax,
		vDeadViewHeight )
	{
		m_vCrouchTraceMin = vCrouchTraceMin;
		m_vCrouchTraceMax = vCrouchTraceMax;
	}

	Vector m_vCrouchTraceMin;
	Vector m_vCrouchTraceMax;	
};

class CPortalMPGameRules : public CTeamplayRules
{
public:
	//DECLARE_CLASS( CPortalGameRules, CSingleplayRules );
	//DECLARE_CLASS( CPortalGameRules, CMultiplayRules );
	DECLARE_CLASS( CPortalMPGameRules, CTeamplayRules );

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif

	CPortalMPGameRules( void );
	virtual ~CPortalMPGameRules( void );

	virtual void Precache( void );
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );

	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	virtual float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon );
	virtual Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon );
	virtual int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon );
	virtual void Think( void );
	virtual void CreateStandardEntities( void );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual void GoToIntermission( void );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char *GetGameDescription( void );
	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"x9Ke0BY7"; }
	virtual const CViewVectors* GetViewVectors() const;
	const PortalMPViewVectors* GetPortalMPViewVectors() const;

	float GetMapRemainingTime();
	void CleanUpMap();
	void CheckRestartGame();
	void RestartGame();

#ifndef CLIENT_DLL
	virtual Vector VecItemRespawnSpot( CItem *pItem );
	virtual QAngle VecItemRespawnAngles( CItem *pItem );
	virtual float	FlItemRespawnTime( CItem *pItem );
	virtual bool	CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );
	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	void	AddLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	RemoveLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	ManageObjectRelocation( void );

	virtual float	GetLaserTurretDamage( void );
	virtual float	GetLaserTurretMoveSpeed( void );
	virtual float	GetRocketTurretDamage( void );
#endif
	virtual void ClientDisconnected( edict_t *pClient );

	bool CheckGameOver( void );
	bool IsIntermission( void );

	void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );


	bool	IsTeamplay( void ) { return m_bTeamPlayEnabled;	}
	void	CheckAllPlayersReady( void );

private:

	CNetworkVar( bool, m_bTeamPlayEnabled );
	CNetworkVar( float, m_flGameStartTime );
	CUtlVector<EHANDLE> m_hRespawnableItemsAndWeapons;
	float m_tmNextPeriodicThink;
	float m_flRestartGameTime;
	bool m_bCompleteReset;
	bool m_bAwaitingReadyRestart;
	bool m_bHeardAllPlayersReady;
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CPortalMPGameRules* PortalMPGameRules()
{
	return static_cast<CPortalMPGameRules*>(g_pGameRules);
}

inline CPortalMPGameRules* PortalGameRules()
{
	return static_cast<CPortalMPGameRules*>(g_pGameRules);
}



#endif // PORTAL_MP_GAMERULES_H
