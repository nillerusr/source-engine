//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMERULES_H
#define TF_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "Teamplay_GameRules.h"
#include "takedamageinfo.h"


#ifdef CLIENT_DLL

	#define CTeamFortress C_TeamFortress

#endif


class CTeamFortress : public CTeamplayRules
{
public:
	DECLARE_CLASS( CTeamFortress, CTeamplayRules );

	int DefaultFOV( void ) { return 90; }

	// Shared implementation between client and server.
	void WeaponTraceLine( const Vector& src, const Vector& end, unsigned int mask, CBaseEntity *pShooter, int damageType, trace_t* pTrace );
	
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	
	virtual void FireBullets( const CTakeDamageInfo &info, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, 
		const Vector &vecSpread, float flDistance, int iBulletType, int iTracerFreq, int firingEntID, 
		int attachmentID, const char *sCustomTracer = NULL );


#ifdef CLIENT_DLL


#else

	CTeamFortress();
	virtual ~CTeamFortress();

	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );

	virtual void Think( void );
	virtual void LevelInitPostEntity( void );

	virtual void CreateStandardEntities();

	// Called when game rules are destroyed by CWorld
	virtual void LevelShutdown( void );

	virtual void ClientDisconnected( edict_t *pClient );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );

	virtual bool PlayTextureSounds( void ) { return true; }
	virtual bool PlayFootstepSounds( CBasePlayer *pl );
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );
	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );
	// Let the game rules specify if fall death should fade screen to black
	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return FALSE; }

	bool	IsTraceBlockedByWorldOrShield( const Vector& src, const Vector& end, CBaseEntity *pShooter, int damageType, trace_t* pTrace );

	virtual float WeaponTraceEntity( CBaseEntity *pEntity, const Vector &vecStart, const Vector &vecEnd, unsigned int mask, trace_t *ptr );

	virtual void UpdateClientData( CBasePlayer *pl );
	
	// Death notices
	virtual void		DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char  *GetDamageCustomString( const CTakeDamageInfo &info );
	CBasePlayer			*GetDeathAssistant( CBaseEntity *pKiller, CBaseEntity *pInflictor );

	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker );
	virtual void			InitDefaultAIRelationships( void );

	virtual const char *GetGameDescription( void ) { return "TeamFortress 2"; }  // this is the game name that gets seen in the server browser
	virtual const char *AIClassText(int classType);

	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );

	// Is the ray blocked by enemy shields?
	bool IsBlockedByEnemyShields( const Vector& src, const Vector& end, int nFriendlyTeam );

public:

	virtual void	SetAllowWeaponSwitch( bool allow );
	virtual bool	GetAllowWeaponSwitch( void );
private:

	// Don't allow switching weapons while gaining new technologies
	bool			m_bAllowWeaponSwitch;

#endif

};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CTeamFortress* TFGameRules()
{
	#ifdef _DEBUG
		Assert( dynamic_cast< CTeamFortress* >( g_pGameRules ) );
	#endif

	return static_cast<CTeamFortress*>(g_pGameRules);
}

// Send the appropriate weapon impact.
void WeaponImpact( trace_t *tr, Vector vecDir, bool bHurt, CBaseEntity *pEntity, int iDamageType );


#ifdef CLIENT_DLL

#else

	//-----------------------------------------------------------------------------
	// Purpose: Useful utility functions
	//-----------------------------------------------------------------------------
	class CTFTeam;
	CTFTeam *GetOpposingTeam( CTeam *pTeam );
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround );

#endif

#endif // TF_GAMERULES_H
