//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"

#ifdef CLIENT_DLL

bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );
bool IsLocalCSPlayerClass( int iClass );
bool GameRulesAllowsAchievements( void );

//----------------------------------------------------------------------------------------------------------------
// Base class for all CS achievements
class CCSBaseAchievement : public CBaseAchievement
{
    DECLARE_CLASS( CCSBaseAchievement, CBaseAchievement );
public:

    CCSBaseAchievement();

	virtual void GetSettings( KeyValues* pNodeOut );				// serialize
	virtual void ApplySettings( /* const */ KeyValues* pNodeIn );	// unserialize

    // [dwenger] Necessary for sorting achievements by award time
	virtual void OnAchieved();
    bool GetAwardTime( int& year, int& month, int& day, int& hour, int& minute, int& second );

	int64 GetSortKey() const { return GetUnlockTime(); }
};


//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements that check that the player was playing on a game team for the full round
class CCSBaseAchievementFullRound : public CCSBaseAchievement
{
	DECLARE_CLASS( CCSBaseAchievementFullRound, CCSBaseAchievement );
public:
	virtual void Init() ;
	virtual void ListenForEvents();
	void FireGameEvent_Internal( IGameEvent *event );
	bool PlayerWasInEntireRound( float flRoundTime );

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event ) = 0 ;
};


//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements based on other achievements
class CAchievement_Meta : public CCSBaseAchievement
{
	DECLARE_CLASS( CAchievement_Meta, CCSBaseAchievement );
public:
	CAchievement_Meta();
	virtual void Init();

#if !defined(NO_STEAM)
	STEAM_CALLBACK( CAchievement_Meta, Steam_OnUserAchievementStored, UserAchievementStored_t, m_CallbackUserAchievement );
#endif

protected:
	void AddRequirement( int nAchievementId );

private:
	CUtlVector<int> m_requirements;
};



extern CAchievementMgr g_AchievementMgrCS;	// global achievement mgr for CS

#endif // CLIENT_DLL