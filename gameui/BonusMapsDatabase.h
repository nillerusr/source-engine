//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BONUSMAPSDATABASE_H
#define BONUSMAPSDATABASE_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"


struct ChallengeDescription_t
{
	char szName[32];
	char szComment[256];

	int iType;

	int iBronze;
	int iSilver;
	int iGold;

	int iBest;
};

struct BonusMapDescription_t
{
	bool bIsFolder;

	char szShortName[64];
	char szFileName[128];

	char szMapFileName[128];
	char szChapterName[128];
	char szImageName[128];

	char szMapName[64];
	char szComment[256];

	bool bLocked;
	bool bComplete;

	CUtlVector<ChallengeDescription_t>	*m_pChallenges;

	BonusMapDescription_t( void )
	{
		bIsFolder = false;

		szShortName[ 0 ] = '\0';
		szFileName[ 0 ] = '\0';

		szMapFileName[ 0 ] = '\0';
		szChapterName[ 0 ] = '\0';
		szImageName[ 0 ] = '\0';

		szMapName[ 0 ] = '\0';
		szComment[ 0 ] = '\0';

		bLocked = false;
		bComplete = false;

		m_pChallenges = NULL;
	}
};

struct BonusMapChallenge_t
{
	char szFileName[128];
	char szMapName[32];
	char szChallengeName[32];
	int iBest;
};


class KeyValues;


//-----------------------------------------------------------------------------
// Purpose: Keeps track of bonus maps on disk
//-----------------------------------------------------------------------------
class CBonusMapsDatabase
{

public:
	CBonusMapsDatabase( void );
	~CBonusMapsDatabase();

	bool ReadBonusMapSaveData( void );
	bool WriteSaveData( void );

	const char * GetPath( void ) { return m_szCurrentPath; }
	void RootPath( void );
	void AppendPath( const char *pchAppend );
	void BackPath( void );
	void SetPath( const char *pchPath, int iDirDepth );

	void ClearBonusMapsList( void );
	void ScanBonusMaps( void );
	void RefreshMapData( void );

	int BonusCount( void );
	BonusMapDescription_t * GetBonusData( int iIndex ) { return &(m_BonusMaps[ iIndex ]); }
	int InvalidIndex( void ) { return m_BonusMaps.InvalidIndex(); }
	bool IsValidIndex( int iIndex ) { return m_BonusMaps.IsValidIndex( iIndex ); }

	bool GetBlink( void );
	void SetBlink( bool bState );

	bool BonusesUnlocked( void );

	void SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName );
	void GetCurrentChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName );
	void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold );
	void GetCurrentChallengeObjectives( int &iBronze, int &iSilver, int &iGold );

	bool SetBooleanStatus( const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue );
	bool SetBooleanStatus( const char *pchName, int iIndex, bool bValue );
	bool UpdateChallengeBest( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest );

	float GetCompletionPercentage( void );

	int NumAdvancedComplete( void );
	void NumMedals( int piNumMedals[ 3 ] );

private:

	void AddBonus( const char *pCurrentPath, const char *pDirFileName, bool bIsFolder );
	void BuildSubdirectoryList( const char *pCurrentPath, bool bOutOfRoot );
	void BuildBonusMapsList( const char *pCurrentPath, bool bOutOfRoot );

	void ParseBonusMapData( char const *pszFileName, char const *pszShortName, bool bIsFolder );

private:

	KeyValues	*m_pBonusMapsManifest;

	CUtlVector<BonusMapDescription_t>	m_BonusMaps;

	KeyValues	*m_pBonusMapSavedData;
	bool		m_bSavedDataChanged;

	int		m_iX360BonusesUnlocked;		// Only used on 360
	bool	m_bHasLoadedSaveData;

	int		m_iDirDepth;
	char	m_szCurrentPath[_MAX_PATH];
	float	m_fCurrentCompletion;
	int		m_iCompletableLevels;

	BonusMapChallenge_t		m_CurrentChallengeNames;
	ChallengeDescription_t	m_CurrentChallengeObjectives;
};


void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal );
CBonusMapsDatabase *BonusMapsDatabase( void );

extern const char g_pszMedalNames[4][8];


#endif // BONUSMAPSDATABASE_H
