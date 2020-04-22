//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BonusMapsDatabase.h"
#include "EngineInterface.h"

#include "tier1/convar.h"
#include "tier1/utlbuffer.h"

#include "filesystem.h"
#include "ModInfo.h"
#include "EngineInterface.h"
#include "ixboxsystem.h"
#include "KeyValues.h"
#include "BasePanel.h"
#include "GameUI_Interface.h"
#include "BonusMapsDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MOD_DIR ( IsXbox() ? "DEFAULT_WRITE_PATH" : "MOD" )


const char g_pszMedalNames[4][8] =
{
	"none",
	"bronze",
	"silver",
	"gold"
};


const char *COM_GetModDirectory();


bool WriteBonusMapSavedData( KeyValues *data )
{
	if ( IsX360() && ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED ) )
		return false;

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	data->RecursiveSaveToFile( buf, 0 );

	char	szFilename[_MAX_PATH];

	if ( IsX360() )
		Q_snprintf( szFilename, sizeof( szFilename ), "cfg:/bonus_maps_data.bmd" );
	else
		Q_snprintf( szFilename, sizeof( szFilename ), "save/bonus_maps_data.bmd" );

	bool bWriteSuccess = g_pFullFileSystem->WriteFile( szFilename, MOD_DIR, buf );

	xboxsystem->FinishContainerWrites();

	return bWriteSuccess;
}

void GetBooleanStatus( KeyValues *pBonusFilesKey, BonusMapDescription_t &map )
{
	KeyValues *pFileKey = NULL;
	KeyValues *pBonusKey = NULL;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( Q_strcmp( pFileKey->GetName(), map.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( Q_strcmp( pBonusKey->GetName(), map.szMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	if ( pBonusKey )
	{
		map.bLocked = ( pBonusKey->GetInt( "lock" ) != 0 );
		map.bComplete = ( pBonusKey->GetInt( "complete" ) != 0 );
	}
}

bool SetBooleanStatus( KeyValues *pBonusFilesKey, const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue )
{
	// Don't create entries for files that don't exist
	if ( !IsX360() && ! (g_pFullFileSystem->FileExists( pchFileName, "MOD" ) || g_pFullFileSystem->IsDirectory( pchFileName, "MOD" ) ) )
	{
		DevMsg( "Failed to set boolean status for file %s.", pchFileName );
		return false;
	}

	bool bChanged = false;

	KeyValues *pFileKey = NULL;
	KeyValues *pBonusKey = NULL;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( Q_strcmp( pFileKey->GetName(), pchFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( Q_strcmp( pBonusKey->GetName(), pchMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	if ( !pFileKey )
	{
		// Didn't find it, so create a new spot for the data
		pFileKey = new KeyValues( pchFileName );
		pBonusFilesKey->AddSubKey( pFileKey );
	}

	if ( !pBonusKey )
	{
		pBonusKey = new KeyValues( pchMapName, pchName, "0" );
		pFileKey->AddSubKey( pBonusKey );
		bChanged = true;
	}

	if ( ( pBonusKey->GetInt( pchName ) != 0 ) != bValue )
	{
		bChanged = true;
		pBonusKey->SetInt( pchName, bValue );
	}

	return bChanged;
}

float GetChallengeBests( KeyValues *pBonusFilesKey, BonusMapDescription_t &challenge )
{
	// There's no challenges, so bail and assume 0% challenge completion
	if ( challenge.m_pChallenges == NULL || challenge.m_pChallenges->Count() == 0 )
		return 0.0f;

	KeyValues *pFileKey = NULL;
	KeyValues *pBonusKey = NULL;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( Q_strcmp( pFileKey->GetName(), challenge.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( Q_strcmp( pBonusKey->GetName(), challenge.szMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	float fChallengePoints = 0.0f;

	for ( int iChallenge = 0; iChallenge < challenge.m_pChallenges->Count(); ++iChallenge )
	{
		ChallengeDescription_t *pChallengeDescription = &((*challenge.m_pChallenges)[ iChallenge ]);
		pChallengeDescription->iBest = ( ( pBonusKey ) ? ( pBonusKey->GetInt( pChallengeDescription->szName, -1 ) ) : ( -1 ) );

		if ( pChallengeDescription->iBest >= 0 )
		{
			if ( pChallengeDescription->iBest <= pChallengeDescription->iGold )
				fChallengePoints += 3.0f;
			else if ( pChallengeDescription->iBest <= pChallengeDescription->iSilver )
				fChallengePoints += 2.0f;
			else if ( pChallengeDescription->iBest <= pChallengeDescription->iBronze )
				fChallengePoints += 1.0f;
		}
	}

	return fChallengePoints / ( challenge.m_pChallenges->Count() * 3.0f );
}

bool UpdateChallengeBest( KeyValues *pBonusFilesKey, const BonusMapChallenge_t &challenge )
{
	// Don't create entries for files that don't exist
	if ( !IsX360() && !g_pFullFileSystem->FileExists( challenge.szFileName, "MOD" ) )
	{
		DevMsg( "Failed to set challenge best for file %s.", challenge.szFileName );
		return false;
	}

	bool bChanged = false;

	KeyValues *pFileKey = NULL;
	KeyValues *pBonusKey = NULL;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( Q_strcmp( pFileKey->GetName(), challenge.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( Q_strcmp( pBonusKey->GetName(), challenge.szMapName ) == 0 )
				{
					// Found the challenge
					break;
				}
			}
			break;
		}
	}

	if ( !pFileKey )
	{
		// Didn't find it, so create a new spot for data
		pFileKey = new KeyValues( challenge.szFileName );
		pBonusFilesKey->AddSubKey( pFileKey );
	}

	if ( !pBonusKey )
	{
		pBonusKey = new KeyValues( challenge.szMapName, challenge.szChallengeName, -1 );
		pFileKey->AddSubKey( pBonusKey );
		bChanged = true;
	}

	int iCurrentBest = pBonusKey->GetInt( challenge.szChallengeName, -1 );
	if ( iCurrentBest == -1 || iCurrentBest > challenge.iBest )
	{
		bChanged = true;
		pBonusKey->SetInt( challenge.szChallengeName, challenge.iBest );
	}

	return bChanged;
}

void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal )
{
	iBest = pChallengeDescription->iBest;

	if ( iBest == -1 )
		iEarnedMedal = 0;
	else if ( iBest <= pChallengeDescription->iGold )
		iEarnedMedal = 3;
	else if ( iBest <= pChallengeDescription->iSilver )
		iEarnedMedal = 2;
	else if ( iBest <= pChallengeDescription->iBronze )
		iEarnedMedal = 1;
	else
		iEarnedMedal = 0;

	iNext = -1;

	switch ( iEarnedMedal )
	{
	case 0:
		iNext = pChallengeDescription->iBronze;
		iNextMedal = 1;
		break;
	case 1:
		iNext = pChallengeDescription->iSilver;
		iNextMedal = 2;
		break;
	case 2:
		iNext = pChallengeDescription->iGold;
		iNextMedal = 3;
		break;
	case 3:
		iNext = -1;
		iNextMedal = -1;
		break;
	}
}


CBonusMapsDatabase *g_pBonusMapsDatabase = NULL;

CBonusMapsDatabase *BonusMapsDatabase( void )
{
	if ( !g_pBonusMapsDatabase )
		static CBonusMapsDatabase StaticBonusMapsDatabase;

	return g_pBonusMapsDatabase;
}


//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CBonusMapsDatabase::CBonusMapsDatabase( void )
{
	Assert( g_pBonusMapsDatabase == NULL );	// There should only be 1 bonus maps database
	g_pBonusMapsDatabase = this;

	RootPath();

	m_pBonusMapsManifest = new KeyValues( "bonus_maps_manifest" );
	m_pBonusMapsManifest->LoadFromFile( g_pFullFileSystem, "scripts/bonus_maps_manifest.txt", NULL );

	m_iX360BonusesUnlocked = -1;	// Only used on X360
	m_bHasLoadedSaveData = false;

	ReadBonusMapSaveData();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBonusMapsDatabase::~CBonusMapsDatabase()
{
	g_pBonusMapsDatabase = NULL;
}

extern bool g_bIsCreatingNewGameMenuForPreFetching;

bool CBonusMapsDatabase::ReadBonusMapSaveData( void )
{
	if ( !m_pBonusMapSavedData )
		m_pBonusMapSavedData = new KeyValues( "bonus_map_saved_data" );

	if ( g_bIsCreatingNewGameMenuForPreFetching )
	{
		// Although we may have a storage device it's not going to be able to find our file at this point! BAIL!
		return false;
	}

#ifdef _X360
	// Nothing to read
	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
		return false;
#endif

	char	szFilename[_MAX_PATH];

	if ( IsX360() )
		Q_snprintf( szFilename, sizeof( szFilename ), "cfg:/bonus_maps_data.bmd" );
	else
		Q_snprintf( szFilename, sizeof( szFilename ), "save/bonus_maps_data.bmd" );

	m_pBonusMapSavedData->LoadFromFile( g_pFullFileSystem, szFilename, NULL );

	m_bSavedDataChanged = false;
	m_bHasLoadedSaveData = true;

	return true;
}

bool CBonusMapsDatabase::WriteSaveData( void )
{
	bool bSuccess = false;

	if ( m_bSavedDataChanged )
		bSuccess = WriteBonusMapSavedData( m_pBonusMapSavedData );

	if ( bSuccess )
		m_bSavedDataChanged = false;

	return bSuccess;
}

void CBonusMapsDatabase::RootPath( void )
{
	m_iDirDepth = 0;
	V_strcpy_safe( m_szCurrentPath, "." );
}

void CBonusMapsDatabase::AppendPath( const char *pchAppend )
{
	++m_iDirDepth;
	char szCurPathTmp[MAX_PATH];
	V_strcpy_safe( szCurPathTmp, m_szCurrentPath );
	Q_snprintf( m_szCurrentPath, sizeof( m_szCurrentPath ), "%s/%s", szCurPathTmp, pchAppend );
}

void CBonusMapsDatabase::BackPath( void )
{
	if ( m_iDirDepth == 0 )
		return;

	if ( m_iDirDepth == 1 )
	{
		RootPath();	// back to root
		return;
	}

	--m_iDirDepth;
	Q_strrchr( m_szCurrentPath, '/' )[ 0 ] = '\0';	// remove a dir from the end
}

void CBonusMapsDatabase::SetPath( const char *pchPath, int iDirDepth )
{
	V_strcpy_safe( m_szCurrentPath, pchPath );
	m_iDirDepth = iDirDepth;
}

void CBonusMapsDatabase::ClearBonusMapsList( void )
{
	m_BonusMaps.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: builds bonus map list from directory
//-----------------------------------------------------------------------------
void CBonusMapsDatabase::ScanBonusMaps( void )
{
	// Don't load in the bonus maps before we've properly read in the save data
	if ( !m_bHasLoadedSaveData )
	{
		if ( ! ReadBonusMapSaveData() )
			return;
	}

	char *pCurrentPath = &(m_szCurrentPath[2]);

	// Reset completion percentage
	m_iCompletableLevels = 0;
	m_fCurrentCompletion = 0.0f;

	// populate list box with all bonus maps in the current path
	char szDirectory[_MAX_PATH];

	if ( Q_strcmp( m_szCurrentPath, "." ) == 0 )
	{
		// We're at the root, so look at the directories in the manifest
		KeyValues *pKey = NULL;
		for ( pKey = m_pBonusMapsManifest->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
		{
			const char *pchType = pKey->GetName();
			if ( Q_strcmp( pchType, "search" ) == 0 )
			{
				// Search through the directory
				Q_snprintf( szDirectory, sizeof( szDirectory ), "%s/", pKey->GetString() );

				BuildSubdirectoryList( szDirectory, true );
				BuildBonusMapsList( szDirectory, true );
			}
			else if ( Q_strcmp( pchType, "dir" ) == 0 )
			{
				AddBonus( "", pKey->GetString(), true );
			}
			else if ( Q_strcmp( pchType, "map" ) == 0 )
			{
				AddBonus( "", pKey->GetString(), false );
			}
		}
	}
	else
	{
		// Search through the current directory
		Q_snprintf( szDirectory, sizeof( szDirectory ), "%s/", pCurrentPath );

		BuildSubdirectoryList( szDirectory, false );
		BuildBonusMapsList( szDirectory, false );
	}
}

void CBonusMapsDatabase::RefreshMapData( void )
{
	// Reset completion percentage
	m_iCompletableLevels = 0;
	m_fCurrentCompletion = 0.0f;

	for ( int iMap = 0; iMap < m_BonusMaps.Count(); ++iMap )
	{
		BonusMapDescription_t *pMap = &m_BonusMaps[ iMap ];

		float fCompletion = GetChallengeBests( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		// If all the challenges are completed set it as complete
		if ( fCompletion == 1.0f )
			SetBooleanStatus( "complete", pMap->szFileName, pMap->szMapName, true );

		GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		if ( pMap->bComplete )
			fCompletion = 1.0f;

		if ( !pMap->bIsFolder )
		{
			m_fCurrentCompletion += fCompletion;
			++m_iCompletableLevels;
		}
	}
}

int CBonusMapsDatabase::BonusCount( void )
{
	if ( m_BonusMaps.Count() == 0 )
		ScanBonusMaps();

	return m_BonusMaps.Count();
}

bool CBonusMapsDatabase::GetBlink( void )
{
	KeyValues *pBlinkKey = m_pBonusMapSavedData->FindKey( "blink" );
	if ( !pBlinkKey )
		return false;
	
	return ( pBlinkKey->GetInt() != 0 );
}

void CBonusMapsDatabase::SetBlink( bool bState )
{
	KeyValues *pBlinkKey = m_pBonusMapSavedData->FindKey( "blink" );
	if ( pBlinkKey )
	{
		bool bCurrentState = ( pBlinkKey->GetInt() != 0 );
		if ( bState && !bCurrentState )
		{
			pBlinkKey->SetStringValue( "1" );
			m_bSavedDataChanged = true;
		}
		else if ( !bState && bCurrentState )
		{
			pBlinkKey->SetStringValue( "0" );
			m_bSavedDataChanged = true;
		}
	}
}

// Only used on X360
bool CBonusMapsDatabase::BonusesUnlocked( void )
{
	if ( m_iX360BonusesUnlocked == -1 )
	{
		// Never checked, so set up the proper X360 scan
		BonusMapsDatabase()->ClearBonusMapsList();	// clear the current list
		BonusMapsDatabase()->RootPath();
		BonusMapsDatabase()->ScanBonusMaps();

		m_iX360BonusesUnlocked = 0;
	}

	if ( m_iX360BonusesUnlocked == 0 )
	{
		// Hasn't been recorded as unlocked yet
		for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
		{
			BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );
			if ( Q_strcmp( pMap->szMapName, "#Bonus_Map_AdvancedChambers" ) == 0 && !pMap->bLocked )
			{
				// All bonuses unlocked, remember this and set up the proper X360 scan to get info.
				m_iX360BonusesUnlocked = 1;
				break;
			}
		}
	}

	return ( m_iX360BonusesUnlocked != 0 );
}

void CBonusMapsDatabase::SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName )
{
	V_strcpy_safe( m_CurrentChallengeNames.szFileName, pchFileName );
	V_strcpy_safe( m_CurrentChallengeNames.szMapName, pchMapName );
	V_strcpy_safe( m_CurrentChallengeNames.szChallengeName, pchChallengeName );
}

void CBonusMapsDatabase::GetCurrentChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
	Q_strcpy( pchFileName, m_CurrentChallengeNames.szFileName );
	Q_strcpy( pchMapName, m_CurrentChallengeNames.szMapName );
	Q_strcpy( pchChallengeName, m_CurrentChallengeNames.szChallengeName );
}

void CBonusMapsDatabase::SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold )
{
	m_CurrentChallengeObjectives.iBronze = iBronze;
	m_CurrentChallengeObjectives.iSilver = iSilver;
	m_CurrentChallengeObjectives.iGold = iGold;
}

void CBonusMapsDatabase::GetCurrentChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
	iBronze = m_CurrentChallengeObjectives.iBronze;
	iSilver = m_CurrentChallengeObjectives.iSilver;
	iGold = m_CurrentChallengeObjectives.iGold;
}

bool CBonusMapsDatabase::SetBooleanStatus( const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue )
{
	bool bChanged = ::SetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), pchName, pchFileName, pchMapName, bValue );
	if ( bChanged )
		m_bSavedDataChanged = true;

	return bChanged;
}

bool CBonusMapsDatabase::SetBooleanStatus( const char *pchName, int iIndex, bool bValue )
{
	BonusMapDescription_t *pMap = &(m_BonusMaps[iIndex]);

	bool bChanged = SetBooleanStatus( pchName, pMap->szFileName, pMap->szMapName, bValue );
	GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

	return bChanged;
}

bool CBonusMapsDatabase::UpdateChallengeBest( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	BonusMapChallenge_t challenge;
	V_strcpy_safe( challenge.szFileName, pchFileName );
	V_strcpy_safe( challenge.szMapName, pchMapName );
	V_strcpy_safe( challenge.szChallengeName, pchChallengeName );
	challenge.iBest = iBest;

	bool bChanged = ::UpdateChallengeBest( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), challenge );
	if ( bChanged )
		m_bSavedDataChanged = true;

	return bChanged;
}

float CBonusMapsDatabase::GetCompletionPercentage( void )
{
	// Avoid divide by zero
	if ( m_iCompletableLevels <= 0 )
		return 0.0f;

	return m_fCurrentCompletion / m_iCompletableLevels;
}

int CBonusMapsDatabase::NumAdvancedComplete( void )
{
	char szCurrentPath[_MAX_PATH];
	V_strcpy_safe( szCurrentPath, m_szCurrentPath );
	int iDirDepth = m_iDirDepth;

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( "./scripts/advanced_chambers", 1 );
	ScanBonusMaps();

	int iNumComplete = 0;

	// Look through all the bonus maps
	for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
	{
		BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );

		if ( pMap && Q_strstr( pMap->szMapName, "Advanced" ) != NULL )
		{
			// It's an advanced map, so check if it's complete
			if ( pMap->bComplete )
				++iNumComplete;
		}
	}

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( szCurrentPath, iDirDepth );
	ScanBonusMaps();

	return iNumComplete;
}

void CBonusMapsDatabase::NumMedals( int piNumMedals[ 3 ] )
{
	char szCurrentPath[_MAX_PATH];
	V_strcpy_safe( szCurrentPath, m_szCurrentPath );
	int iDirDepth = m_iDirDepth;

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( "./scripts/challenges", 1 );
	ScanBonusMaps();

	for ( int i = 0; i < 3; ++i )
		piNumMedals[ i ] = 0;

	// Look through all the bonus maps
	for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
	{
		BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );

		if ( pMap && pMap->m_pChallenges )
		{
			for ( int iChallenge = 0; iChallenge < pMap->m_pChallenges->Count(); ++iChallenge )
			{
				ChallengeDescription_t *pChallengeDescription = &((*pMap->m_pChallenges)[ iChallenge ]);

				int iBest, iEarnedMedal, iNext, iNextMedal;
				GetChallengeMedals( pChallengeDescription, iBest, iEarnedMedal, iNext, iNextMedal );

				// Increase the count for this medal and every medal below it
				while ( iEarnedMedal > 0 )
				{
					--iEarnedMedal;	// Medals are 1,2&3 so subtract 1 first
					piNumMedals[ iEarnedMedal ]++;
				}
			}
		}
	}

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( szCurrentPath, iDirDepth );
	ScanBonusMaps();
}


void CBonusMapsDatabase::AddBonus( const char *pCurrentPath, const char *pDirFileName, bool bIsFolder )
{
	char szFileName[_MAX_PATH];
	Q_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pDirFileName );

	// Only load bonus maps from the current mod's maps dir
	if( !IsX360() && !( g_pFullFileSystem->IsDirectory( szFileName, "MOD" ) || g_pFullFileSystem->FileExists( szFileName, "MOD" ) ))
		return;

	ParseBonusMapData( szFileName, pDirFileName, bIsFolder );
}

void CBonusMapsDatabase::BuildSubdirectoryList( const char *pCurrentPath, bool bOutOfRoot )
{
	char szDirectory[_MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "%s*", pCurrentPath );

	FileFindHandle_t dirHandle;
	const char *pDirFileName = g_pFullFileSystem->FindFirst( szDirectory, &dirHandle );

	while (pDirFileName)
	{
		// Skip it if it's not a directory, is the root, is back, or is an invalid folder
		if ( !g_pFullFileSystem->FindIsDirectory( dirHandle ) || 
			 Q_strcmp( pDirFileName, "." ) == 0 || 
			 Q_strcmp( pDirFileName, ".." ) == 0 ||
			 Q_stricmp( pDirFileName, "soundcache" ) == 0 ||
			 Q_stricmp( pDirFileName, "graphs" ) == 0 )
		{
			pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
			continue;
		}

		if ( !bOutOfRoot )
			AddBonus( pCurrentPath, pDirFileName, true );
		else
		{
			char szFileName[_MAX_PATH];
			Q_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pDirFileName );
			AddBonus( "", szFileName, true );
		}

		pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
	}

	g_pFullFileSystem->FindClose( dirHandle );
}

void CBonusMapsDatabase::BuildBonusMapsList( const char *pCurrentPath, bool bOutOfRoot )
{
	char szDirectory[_MAX_PATH];
	Q_snprintf( szDirectory, sizeof( szDirectory ), "%s*.bns", pCurrentPath );

	FileFindHandle_t mapHandle;
	const char *pMapFileName = g_pFullFileSystem->FindFirst( szDirectory, &mapHandle );

	while ( pMapFileName && Q_strlen(pMapFileName)>0 )
	{
		// Skip it if it's a directory or is the folder info
		if ( g_pFullFileSystem->FindIsDirectory( mapHandle ) || Q_strstr( pMapFileName, "folderinfo.bns" ) )
		{
			pMapFileName = g_pFullFileSystem->FindNext( mapHandle );
			continue;
		}

		if ( !bOutOfRoot )
			AddBonus( pCurrentPath, pMapFileName, false );
		else
		{
			char szFileName[_MAX_PATH];
			Q_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pMapFileName );
			AddBonus( "", szFileName, false );
		}

		pMapFileName = g_pFullFileSystem->FindNext( mapHandle );
	}

	g_pFullFileSystem->FindClose( mapHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
void CBonusMapsDatabase::ParseBonusMapData( char const *pszFileName, char const *pszShortName, bool bIsFolder )
{
	if ( !pszFileName || !pszShortName )
		return;

	char szMapInfo[_MAX_PATH];

	// if it's a directory, there's no optional info
	if ( bIsFolder )
	{
		// get the folder info file name
		Q_snprintf( szMapInfo, sizeof(szMapInfo), "%s/folderinfo.bns", pszFileName );
	}
	else
	{
		// get the map info file name
		Q_strncpy( szMapInfo, pszFileName, sizeof(szMapInfo) );
	}

	KeyValues *kv = new KeyValues( pszShortName );
	if ( !kv->LoadFromFile( g_pFullFileSystem, szMapInfo, NULL ) )
		DevMsg( "Unable to load bonus map info file %s\n", szMapInfo );

	while ( kv )
	{
		int iMap = m_BonusMaps.AddToTail();

		BonusMapDescription_t *pMap = &m_BonusMaps[ iMap ];

		// set required map data
		Q_strncpy( pMap->szFileName, pszFileName, sizeof(pMap->szFileName) );
		Q_strncpy( pMap->szShortName, pszShortName, sizeof(pMap->szShortName) );
		pMap->bIsFolder = bIsFolder;

		// set optional map data
		V_strcpy_safe( pMap->szMapName, kv->GetName() );
		V_strcpy_safe( pMap->szMapFileName, kv->GetString( "map" ) );
		V_strcpy_safe( pMap->szChapterName, kv->GetString( "chapter" ) );
		V_strcpy_safe( pMap->szImageName, kv->GetString( "image" ) );
		V_strcpy_safe( pMap->szComment, kv->GetString( "comment" ) );
		pMap->bLocked = ( kv->GetInt( "lock", 0 ) != 0 );
		pMap->bComplete = ( kv->GetInt( "complete", 0 ) != 0 );

		float fCompletion = 0.0f;

		KeyValues *pChallenges = kv->FindKey( "challenges" );

		if ( pChallenges )
		{
			for ( KeyValues *pChallengeKey = pChallenges->GetFirstSubKey(); pChallengeKey; pChallengeKey = pChallengeKey->GetNextKey() )
			{
				if ( !pMap->m_pChallenges )
					pMap->m_pChallenges = new CUtlVector<ChallengeDescription_t>;

				int iChallenge = pMap->m_pChallenges->AddToTail();

				ChallengeDescription_t *pChallenge = &(*pMap->m_pChallenges)[ iChallenge ];
				V_strcpy_safe( pChallenge->szName, pChallengeKey->GetName() );
				V_strcpy_safe( pChallenge->szComment, pChallengeKey->GetString( "comment" ) );
				pChallenge->iType = pChallengeKey->GetInt( "type", -1 );
				pChallenge->iBronze = pChallengeKey->GetInt( "bronze" );
				pChallenge->iSilver = pChallengeKey->GetInt( "silver" );
				pChallenge->iGold = pChallengeKey->GetInt( "gold" );
			}

			fCompletion = GetChallengeBests( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

			// If all the challenges are completed set it as complete
			if ( fCompletion == 1.0f )
				SetBooleanStatus( "complete", pMap->szFileName, pMap->szMapName, true );
		}

		// Get boolean status last because it can be altered if all the challenges were completed
		GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		if ( pMap->bComplete )
			fCompletion = 1.0f;

		if ( !pMap->bIsFolder )
		{
			m_fCurrentCompletion += fCompletion;
			++m_iCompletableLevels;
			kv = kv->GetNextTrueSubKey();
		}
		else
			kv = NULL;
	}
}


void CC_BonusMapUnlock( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		GameUI().BonusMapUnlock();
		return;
	}

	GameUI().BonusMapUnlock( args[ 1 ], args[ 2 ] );
}
static ConCommand sv_bonus_map_unlock("sv_bonus_map_unlock", CC_BonusMapUnlock, "Locks a bonus map.", FCVAR_CHEAT );


void CC_BonusMapComplete( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		GameUI().BonusMapComplete();
		return;
	}

	GameUI().BonusMapComplete( args[ 1 ], args[ 2 ] );
}
static ConCommand sv_bonus_map_complete("sv_bonus_map_complete", CC_BonusMapComplete, "Completes a bonus map.", FCVAR_CHEAT );


void CC_BonusMapChallengeUpdate( const CCommand &args )
{
	if ( args.ArgC() < 5 )
	{
		return;
	}

	GameUI().BonusMapChallengeUpdate( args[ 1 ], args[ 2 ], args[ 3 ], atoi( args[ 4 ] ) );
}
static ConCommand sv_bonus_map_challenge_update("sv_bonus_map_challenge_update", CC_BonusMapChallengeUpdate, "Updates a bonus map challenge score.", FCVAR_CHEAT );
