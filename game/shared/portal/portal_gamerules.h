//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
//=============================================================================//

#ifdef PORTAL_MP



#include "portal_mp_gamerules.h" //redirect to multiplayer gamerules in multiplayer builds



#else

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "hl2_gamerules.h"

#ifdef CLIENT_DLL
	#define CPortalGameRules C_PortalGameRules
	#define CPortalGameRulesProxy C_PortalGameRulesProxy
#endif

#if defined ( CLIENT_DLL )
#include "steam/steam_api.h"
#endif


class CPortalGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CPortalGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CPortalGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CPortalGameRules, CSingleplayRules );

	virtual bool	Init();
	
	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool	ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
#ifndef CLIENT_DLL
	virtual bool	ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual float	GetAutoAimScale( CBasePlayer *pPlayer );
#endif

#ifdef CLIENT_DLL
	virtual bool IsBonusChallengeTimeBased( void );
#endif

private:
	// Rules change for the mega physgun
	CNetworkVar( bool, m_bMegaPhysgun );

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CPortalGameRules();
	virtual ~CPortalGameRules() {}

	virtual void			Think( void );

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);
	virtual const char *GetGameDescription( void ) { return "Portal"; }

	// Ammo
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();

	bool ShouldRemoveRadio( void );
	
public:

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	bool	MegaPhyscannonActive( void ) { return m_bMegaPhysgun;	}

private:

	int						DefaultFOV( void ) { return 75; }
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CPortalGameRules* PortalGameRules()
{
	return static_cast<CPortalGameRules*>(g_pGameRules);
}

#endif // PORTAL_GAMERULES_H
#endif
