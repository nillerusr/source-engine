//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef C_PROP_PORTAL_STATS_DISPLAY_H
#define C_PROP_PORTAL_STATS_DISPLAY_H

#include "cbase.h"
#include "c_portal_player.h"
#include "portal_shareddefs.h"
#include "utlvector.h"


class C_PropPortalStatsDisplay : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_PropPortalStatsDisplay, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	C_PropPortalStatsDisplay();
	virtual ~C_PropPortalStatsDisplay();

	void Spawn( void );

	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void ClientThink( void );

	bool HasCheated( void ) { return m_bHasCheated; }

	bool IsEnabled( void ) { return m_bEnabled; }

	int GetDisplayObjective( void ) { return m_iDisplayObjective; }

	float GetNumPlayerDisplay( void ) { return m_fNumPlayerDisplay; }
	bool IsTime( void ) { return m_iDisplayObjective == PORTAL_LEVEL_STAT_NUM_SECONDS; }

	bool GetGoalVisible( void ) { return m_bGoalVisible; }
	int GetNumGoalDisplay( void ) { return m_iGoalDisplay; }
	int GetGoalSuccess( void ) { return m_iGoalSuccess; }

	int GetGoalLevelDisplay( void ) { return m_iGoalLevelDisplay; }
	bool IsMedalCompleted( int iLevel ) { return m_bMedalCompleted[ iLevel ]; }

private:

	void ResetDisplayAnimation( void );
	void SetDisplayMedals( void );

private:

	bool	m_bPrevEnabled;
	bool	m_bHasCheated;

	bool	m_bEnabled;

	int		m_iNumPortalsPlaced;
	int		m_iNumStepsTaken;
	float	m_fNumSecondsTaken;

	int		m_iBronzeObjective;
	int		m_iSilverObjective;
	int		m_iGoldObjective;

	char	szChallengeFileName[128];
	char	szChallengeMapName[32];
	char	szChallengeName[32];

	int		m_iDisplayObjective;
	int		m_iCurrentDisplayStep;
	float	m_fEnabledCounter;

	float	m_fNumPlayerDisplay;
	bool	m_bGoalVisible;
	int		m_iGoalDisplay;
	int		m_iGoalSuccess;
	int		m_iGoalLevelDisplay;
	bool	m_bMedalCompleted[ 3 ];
};

extern CUtlVector< C_PropPortalStatsDisplay* > g_PropPortalStatsDisplays;

#endif //C_PROP_PORTAL_STATS_DISPLAY_H