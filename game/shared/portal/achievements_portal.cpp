//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"
#include "achievementmgr.h"
#include "baseachievement.h"

#ifdef GAME_DLL
#include "prop_portal.h"
#include "util.h"

CAchievementMgr g_AchievementMgrPortal;	// global achievement mgr for Portal

class CAchievementPortalInfiniteFall : public CBaseAchievement
{
	DECLARE_CLASS( CAchievementPortalInfiniteFall, CBaseAchievement );

public:
	void Init() 
	{
		SetFlags( ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "portal" );
		SetGoal( 1 );
		m_fAccumulatedDistance = 0.0f;
		m_bIsFlinging = false;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "portal_player_portaled" );
		ListenForGameEvent( "portal_player_touchedground" );
	}
	virtual void PreRestoreSavedGame()
	{
		m_fAccumulatedDistance = 0.0f;
		m_bIsFlinging = false;
		BaseClass::PreRestoreSavedGame();
	}

protected:
	virtual void FireGameEvent( IGameEvent *event )
	{
		const char *name = event->GetName();
		if ( 0 == Q_strcmp( name, "portal_player_portaled" ) )
		{
			bool bIsPortal2 = event->GetBool( "portal2", false );
			// Get the portals that they teleported through
			CProp_Portal *pInPortal = CProp_Portal::FindPortal( 0, bIsPortal2, false );
			CProp_Portal *pOutPortal = CProp_Portal::FindPortal( 0, !bIsPortal2, false );

			if ( pInPortal && pOutPortal )
			{
				if ( m_bIsFlinging )
				{
					// Add up how far we traveled since the last teleport
					m_fAccumulatedDistance += m_fZPortalPosition - pInPortal->GetAbsOrigin().z;

					if ( m_fAccumulatedDistance > 30000.0f * 12 )
						IncrementCount();
				}

				// Remember the Z position to get the distance when the teleport again or land
				m_fZPortalPosition = pOutPortal->GetAbsOrigin().z;
				m_bIsFlinging = true;
			}
		}
		else if ( 0 == Q_strcmp( name, "portal_player_touchedground" ) )
		{
			if ( m_bIsFlinging )
			{
				CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();

				if ( pLocalPlayer )
				{
					m_fAccumulatedDistance += m_fZPortalPosition - pLocalPlayer->GetAbsOrigin().z;

					if ( m_fAccumulatedDistance > 30000.0f * 12 )
						IncrementCount();

					m_fAccumulatedDistance = 0.0f;
					m_bIsFlinging = false;
				}
			}
		}
	}

private:
	bool		m_bIsFlinging;
	float		m_fAccumulatedDistance;
	float		m_fZPortalPosition;
};
DECLARE_ACHIEVEMENT( CAchievementPortalInfiniteFall, ACHIEVEMENT_PORTAL_INFINITEFALL, "PORTAL_INFINITEFALL", 5 );

class CAchievementPortalLongJump : public CBaseAchievement
{
	DECLARE_CLASS( CAchievementPortalLongJump, CBaseAchievement );

public:
	void Init() 
	{
		SetFlags( ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "portal" );
		SetGoal( 1 );
		m_fAccumulatedDistance = 0.0f;
		m_bIsFlinging = false;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "portal_player_portaled" );
		ListenForGameEvent( "portal_player_touchedground" );
	}
	virtual void PreRestoreSavedGame()
	{
		m_fAccumulatedDistance = 0.0f;
		m_bIsFlinging = false;
		BaseClass::PreRestoreSavedGame();
	}

protected:
	virtual void FireGameEvent( IGameEvent *event )
	{
		const char *name = event->GetName();
		if ( 0 == Q_strcmp( name, "portal_player_portaled" ) )
		{
			bool bIsPortal2 = event->GetBool( "portal2", false );
			// Get the portals that they teleported through
			CProp_Portal *pInPortal = CProp_Portal::FindPortal( 0, bIsPortal2, false );
			CProp_Portal *pOutPortal = CProp_Portal::FindPortal( 0, !bIsPortal2, false );

			if ( pInPortal && pOutPortal )
			{
				if ( m_bIsFlinging )
				{
					// Add up how far we traveled since the last teleport
					float flDist = pInPortal->GetAbsOrigin().AsVector2D().DistTo( m_vec2DPortalPosition );
				
					// Ignore small distances that can be caused by microadjustments in infinite falls
					if ( flDist > 63.0f )
					{
						m_fAccumulatedDistance += flDist;
					}

					if ( m_fAccumulatedDistance > 300.0f * 12 )
						IncrementCount();
				}

				// Remember the 2D position to get the distance when the teleport again or land
				m_vec2DPortalPosition = pOutPortal->GetAbsOrigin().AsVector2D();
				m_bIsFlinging = true;
			}
		}
		else if ( 0 == Q_strcmp( name, "portal_player_touchedground" ) )
		{
			if ( m_bIsFlinging )
			{
				CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();

				if ( pLocalPlayer )
				{
					float flDist = pLocalPlayer->GetAbsOrigin().AsVector2D().DistTo( m_vec2DPortalPosition );

					// Ignore small distances that can be caused by microadjustments in infinite falls
					if ( flDist > 63.0f )
					{
						m_fAccumulatedDistance += flDist;
					}

					if ( m_fAccumulatedDistance > 300.0f * 12 )
						IncrementCount();

					m_fAccumulatedDistance = 0.0f;
					m_bIsFlinging = false;
				}
			}
		}
	}

private:
	bool		m_bIsFlinging;
	float		m_fAccumulatedDistance;
	Vector2D	m_vec2DPortalPosition;
};
DECLARE_ACHIEVEMENT( CAchievementPortalLongJump, ACHIEVEMENT_PORTAL_LONGJUMP, "PORTAL_LONGJUMP", 5 );

class CAchievementPortalBeat2AdvancedMaps: public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 2 );
		m_iProgressMsgMinimum = 0;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "advanced_map_complete" );
	}

protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "advanced_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iNumAdvanced = event->GetInt( "numadvanced" );

				SetCount ( iNumAdvanced );
				if ( iNumAdvanced >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalBeat2AdvancedMaps, ACHIEVEMENT_PORTAL_BEAT_2ADVANCEDMAPS, "PORTAL_BEAT_2ADVANCEDMAPS", 10 );

class CAchievementPortalBeat4AdvancedMaps : public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 4 );
		m_iProgressMsgMinimum = 3;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "advanced_map_complete" );
	}

protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "advanced_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iNumAdvanced = event->GetInt( "numadvanced" );

				SetCount ( iNumAdvanced );
				if ( iNumAdvanced >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalBeat4AdvancedMaps, ACHIEVEMENT_PORTAL_BEAT_4ADVANCEDMAPS, "PORTAL_BEAT_4ADVANCEDMAPS", 20 );

class CAchievementPortalBeat6AdvancedMaps : public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 6 );
		m_iProgressMsgMinimum = 5;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "advanced_map_complete" );
	}

protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "advanced_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iNumAdvanced = event->GetInt( "numadvanced" );

				SetCount ( iNumAdvanced );
				if ( iNumAdvanced >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalBeat6AdvancedMaps, ACHIEVEMENT_PORTAL_BEAT_6ADVANCEDMAPS, "PORTAL_BEAT_6ADVANCEDMAPS", 30 );

class CAchievementPortalGetAllBronze : public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 18 );
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "challenge_map_complete" );
	}

protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "challenge_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iBronzeCount = event->GetInt( "numbronze" );

				SetCount ( iBronzeCount );
				if ( iBronzeCount >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalGetAllBronze, ACHIEVEMENT_PORTAL_GET_ALLBRONZE, "PORTAL_GET_ALLBRONZE", 10 );

class CAchievementPortalGetAllSilver : public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 18 );
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "challenge_map_complete" );
	}

protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "challenge_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iSilverCount = event->GetInt( "numsilver" );

				SetCount ( iSilverCount );
				if ( iSilverCount >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalGetAllSilver, ACHIEVEMENT_PORTAL_GET_ALLSILVER, "PORTAL_GET_ALLSILVER", 20 );

class CAchievementPortalGetAllGold : public CBaseAchievement
{
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 18 );
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "challenge_map_complete" );
	}
	
protected:
	virtual void FireGameEvent( IGameEvent* event )
	{
		if ( !Q_stricmp( event->GetName(), "challenge_map_complete" ) )
		{
			if ( !IsAchieved() )
			{
				int iGoldCount = event->GetInt( "numgold" );

				SetCount ( iGoldCount );
				if ( iGoldCount >= GetGoal() )
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalGetAllGold, ACHIEVEMENT_PORTAL_GET_ALLGOLD, "PORTAL_GET_ALLGOLD", 40 );


class CAchievementPortalDetachAllCameras : public CBaseAchievement
{
protected:
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "security_camera_detached" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "security_camera_detached" ) )
		{
			IncrementCount();
		}
	}
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_WITH_GAME );
		SetGoal( 33 );
		ListenForGameEvent( "security_camera_detached" );
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalDetachAllCameras, ACHIEVEMENT_PORTAL_DETACH_ALL_CAMERAS, "PORTAL_DETACH_ALL_CAMERAS", 5 );


class CAchievementPortalHitTurretWithTurret : public CBaseAchievement
{
protected:
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "turret_hit_turret" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "turret_hit_turret" ) )
		{
			IncrementCount();
		}
	}
public:
	virtual void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
		ListenForGameEvent( "turret_hit_turret" );
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalHitTurretWithTurret, ACHIEVEMENT_PORTAL_HIT_TURRET_WITH_TURRET, "PORTAL_HIT_TURRET_WITH_TURRET", 5 );


#ifndef _XBOX
class CAchievementPortalFindAllDinosaurs : public CBaseAchievement
{
	DECLARE_CLASS( CAchievementPortalFindAllDinosaurs, CBaseAchievement );
	void Init() 
	{
		SetFlags( ACH_HAS_COMPONENTS | ACH_SAVE_GLOBAL );
		m_iNumComponents = 26;
		SetStoreProgressInSteam( true );
		SetGoal( m_iNumComponents );
		BaseClass::Init();
		m_iProgressMsgMinimum = 1;
	}
	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dinosaur_signal_found" );
	}
	virtual void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "dinosaur_signal_found" ) )
		{
			int id = event->GetInt( "id", -1 );
			Assert( id >= 0 && id < m_iNumComponents );
			if ( id >= 0 && id < m_iNumComponents )
			{
				EnsureComponentBitSetAndEvaluate( id );
				
				// Update our Steam stat
				steamapicontext->SteamUserStats()->SetStat( "PORTAL_TRANSMISSION_RECEIVED_STAT", m_iCount );
			}
			else
			{
				Warning( "Failed to set achievement progress. Dinosaur ID(%d) out of range (0 to %d)\n", id, m_iNumComponents );
			}
		}
	}
	virtual void CalcProgressMsgIncrement()
	{
		// Show progress every tick
		m_iProgressMsgIncrement = 1;
	}
};
DECLARE_ACHIEVEMENT( CAchievementPortalFindAllDinosaurs, ACHIEVEMENT_PORTAL_TRANSMISSION_RECEIVED, "PORTAL_TRANSMISSION_RECEIVED", 0 );
#endif // _XBOX

// achievements which are won by a map event firing once
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_PORTAL_GET_PORTALGUNS, "PORTAL_GET_PORTALGUNS", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_PORTAL_KILL_COMPANIONCUBE, "PORTAL_KILL_COMPANIONCUBE", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_PORTAL_ESCAPE_TESTCHAMBERS, "PORTAL_ESCAPE_TESTCHAMBERS", 5 );
DECLARE_MAP_EVENT_ACHIEVEMENT( ACHIEVEMENT_PORTAL_BEAT_GAME, "PORTAL_BEAT_GAME", 10 );

#endif // GAME_DLL
