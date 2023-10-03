//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BASEACHIEVEMENT_H
#define BASEACHIEVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
//#include "hl2orange.spa.h"
#include "iachievementmgr.h"

class CAchievementMgr;

//
// Base class for achievements
//

class CBaseAchievement : public CGameEventListener, public IAchievement
{
	DECLARE_CLASS_NOBASE( CBaseAchievement );
public:
	CBaseAchievement();	
	virtual void Init() {}
	virtual void ListenForEvents() {};
	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event );

	int GetAchievementID() { return m_iAchievementID; }
	void SetAchievementID( int iAchievementID ) { m_iAchievementID = iAchievementID; }
	void SetName( const char *pszName ) { m_pszName = pszName; }
	const char *GetName() { return m_pszName; }
	void SetFlags( int iFlags );
	int GetFlags() { return m_iFlags; }
	void SetGoal( int iGoal ) { m_iGoal = iGoal; }
	int GetGoal() { return m_iGoal; }
	void SetGameDirFilter( const char *pGameDir );
	bool HasComponents() { return ( m_iFlags & ACH_HAS_COMPONENTS ) > 0; }	
	void SetPointValue( int iPointValue ) { m_iPointValue = iPointValue; }
	int	GetPointValue() { return m_iPointValue; }
	bool ShouldHideUntilAchieved() { return m_bHideUntilAchieved; }
	void SetHideUntilAchieved( bool bHide ) { m_bHideUntilAchieved = bHide; }
	void SetStoreProgressInSteam( bool bStoreProgressInSteam ) { m_bStoreProgressInSteam = bStoreProgressInSteam; }
	bool StoreProgressInSteam() { return m_bStoreProgressInSteam; }
	virtual bool ShouldShowProgressNotification() { return true; }
	virtual void OnPlayerStatsUpdate() {}

	virtual bool ShouldSaveWithGame();
	bool ShouldSaveGlobal();
	virtual void PreRestoreSavedGame();
	virtual void PostRestoreSavedGame();
	void SetCount( int iCount ) { m_iCount = iCount; }
	int GetCount() { return m_iCount; }
	void SetProgressShown( int iProgressShown ) { m_iProgressShown = iProgressShown; }
	int GetProgressShown() { return m_iProgressShown; }
	virtual bool IsAchieved() { return m_bAchieved; }
	virtual bool IsActive();
	void SetAchieved( bool bAchieved ) { m_bAchieved = bAchieved; }
	uint64 GetComponentBits() { return m_iComponentBits; }
	void SetComponentBits( uint64 iComponentBits );
	void OnComponentEvent( const char *pchComponentName );
	void EnsureComponentBitSetAndEvaluate( int iBitNumber );
	void EvaluateIsAlreadyAchieved();
	virtual void OnMapEvent( const char *pEventName );
	virtual void PrintAdditionalStatus() {}		// for debugging, achievements may report additional status in achievement_status concmd
	virtual void OnSteamUserStatsStored() {}
	virtual void UpdateAchievement( int nData ) {}
	virtual void SetUserSlot( int nUserSlot ) { m_nUserSlot = nUserSlot; }
	virtual void ClearAchievementData();
	virtual const char *GetIconPath() { return NULL; }
	void SetDisplayOrder( int iDisplayOrder ) { m_iDisplayOrder = iDisplayOrder; }
	int GetDisplayOrder( ) { return m_iDisplayOrder; }

	virtual void ReadProgress( IPlayerLocal *pPlayer ) {}
	virtual bool WriteProgress( IPlayerLocal *pPlayer ) { return false; }

protected:
	virtual void FireGameEvent( IGameEvent *event );
	virtual void FireGameEvent_Internal( IGameEvent *event ) {};
	void SetVictimFilter( const char *pClassName );
	void SetAttackerFilter( const char *pClassName );
	void SetInflictorFilter( const char *pClassName );
	void SetInflictorEntityNameFilter( const char *pEntityName );
	void SetMapNameFilter( const char *pMapName );
	void SetComponentPrefix( const char *pPrefix );
	void IncrementCount();
	void EvaluateNewAchievement();
	void AwardAchievement();
	void ShowProgressNotification();
	void HandleProgressUpdate();
	virtual void CalcProgressMsgIncrement();

	const char *m_pszName;								// name of this achievement
	int m_iAchievementID;								// ID of this achievement
	int	m_iFlags;										// ACH_* flags for this achievement
	int	m_iGoal;										// goal # of steps to award this achievement
	int m_iProgressMsgIncrement;						// after how many steps show we show a progress notification
	int m_iProgressMsgMinimum;							// the minimum progress needed before showing progress notification
	int m_iPointValue;									// # of points this achievement is worth (currently only used for XBox Live)
	bool m_bHideUntilAchieved;							// should this achievement be hidden until achieved?
	bool m_bStoreProgressInSteam;						// should incremental progress be stored in Steam.  A counter with same name as achievement must be set up in Steam.
	const char *m_pInflictorClassNameFilter;			// if non-NULL, inflictor class name to filter with
	const char *m_pInflictorEntityNameFilter;			// if non-NULL, inflictor entity name to filter with
	const char *m_pVictimClassNameFilter;				// if non-NULL, victim class name to filter with
	const char *m_pAttackerClassNameFilter;				// if non-NULL, attacker class name to filter with
	const char *m_pMapNameFilter;						// if non-NULL, map name to filter with
	const char *m_pGameDirFilter;						// if non-NULL, game dir name to filter with

	const char **m_pszComponentNames;			
	int			m_iNumComponents;
	const char *m_pszComponentPrefix;
	int			m_iComponentPrefixLen;
	bool		m_bAchieved;							// is this achievement achieved
	int			m_iCount;								// # of steps satisfied toward this achievement (only valid if not achieved)
	int			m_iProgressShown;						// # of progress msgs we've shown
	uint64		m_iComponentBits;						// bitfield of components achieved
	CAchievementMgr *m_pAchievementMgr;					// our achievement manager
	int			m_nUserSlot;
	int			m_iDisplayOrder;						// Order in which the achievement is displayed in the UI

	friend class CAchievementMgr;
public:
	DECLARE_DATADESC();
};

class CFailableAchievement : public CBaseAchievement
{
	DECLARE_CLASS( CFailableAchievement, CBaseAchievement );
public:
	CFailableAchievement();
	void SetFailed();	

	virtual bool ShouldSaveWithGame();
	virtual void PreRestoreSavedGame();
	virtual void PostRestoreSavedGame();
	virtual bool IsAchieved() { return !m_bFailed && BaseClass::IsAchieved(); }
	virtual bool IsActive() { return m_bActivated && !m_bFailed && BaseClass::IsActive(); }
	bool IsFailed() { return m_bFailed; }

	virtual void OnMapEvent( const char *pEventName );
	virtual void OnActivationEvent() { Activate(); }
	virtual void OnEvaluationEvent();
	virtual const char *GetActivationEventName() =0;
	virtual const char *GetEvaluationEventName() =0;

protected:
	void Activate();

	bool	m_bActivated;		// are we activated? (If there is a map event that turns us on, has that happened)
	bool	m_bFailed;			// has this achievement failed
	
public:
	DECLARE_DATADESC();
};

class CMapAchievement : public CBaseAchievement
{
	virtual void Init()
	{
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}
};


//----------------------------------------------------------------------------------------------------------------
class CAchievement_AchievedCount : public CBaseAchievement
{
public:
	void Init();
	virtual void OnSteamUserStatsStored( void );

protected:
	void SetAchievementsRequired( int iNumRequired, int iLowRange, int iHighRange );

private:
	int m_iNumRequired;
	int m_iLowRange;
	int m_iHighRange;
};

//
// Helper class for achievement creation
//
 
typedef CBaseAchievement* (*achievementCreateFunc) (void);
class CBaseAchievementHelper
{
public:
	CBaseAchievementHelper( achievementCreateFunc createFunc )
	{
		m_pfnCreate = createFunc;
		m_pNext = s_pFirst;
		s_pFirst = this;
	}
	achievementCreateFunc m_pfnCreate;
	CBaseAchievementHelper *m_pNext;
	static CBaseAchievementHelper *s_pFirst;
};

#define DECLARE_ACHIEVEMENT_( className, achievementID, achievementName, gameDirFilter, iPointValue, bHidden, iDisplayOrder ) \
static CBaseAchievement *Create_##className( void )					\
{																		\
	CBaseAchievement *pAchievement = new className( );					\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	pAchievement->SetHideUntilAchieved( bHidden );						\
	pAchievement->SetDisplayOrder( iDisplayOrder );						\
	if ( gameDirFilter ) pAchievement->SetGameDirFilter( gameDirFilter ); \
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##className##_Helper( Create_##className );

#define DECLARE_ACHIEVEMENT( className, achievementID, achievementName, iPointValue ) \
	DECLARE_ACHIEVEMENT_( className, achievementID, achievementName, NULL, iPointValue, false )

#define DECLARE_ACHIEVEMENT_ORDER( className, achievementID, achievementName, iPointValue, iDisplayOrder ) \
	DECLARE_ACHIEVEMENT_( className, achievementID, achievementName, NULL, iPointValue, false, iDisplayOrder )

#define DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, gameDirFilter, iPointValue, bHidden ) \
class CAchievement##achievementID : public CMapAchievement {};		\
DECLARE_ACHIEVEMENT_( CAchievement##achievementID, achievementID, achievementName, gameDirFilter, iPointValue, bHidden )	\

#define DECLARE_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )	\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, NULL, iPointValue, false )

#define DECLARE_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )	\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, NULL, iPointValue, true )

#endif // BASEACHIEVEMENT_H
