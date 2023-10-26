#include "asw_location_grid.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "asw_mission_chooser.h"
#include "asw_key_values_database.h"
#include "ConVar.h"
#include "utlbuffer.h"
#include "layout_system/tilegen_mission_preprocessor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_location_grid_debug( "asw_location_grid_debug", "0", FCVAR_NONE, "Outputs mission grid hexes for server and client" );
ConVar asw_unlock_all_locations( "asw_unlock_all_locations", "0", FCVAR_CHEAT, "Unlocks all mission hexes" );

char* TileGenCopyString( const char *szString );

CASW_Location_Grid::CASW_Location_Grid()
{
	m_pMissionDatabase = new CASW_KeyValuesDatabase();
	m_pMissionDatabase->LoadFiles( "tilegen/new_missions/" );
	
	// Preprocess all of the new missions.

	// Load the rules.
	CASW_KeyValuesDatabase *pRulesDatabase = new CASW_KeyValuesDatabase();
	pRulesDatabase->LoadFiles( "tilegen/rules/" );

	// Create a mission preprocessor from the rules.
	m_pPreprocessor = new CTilegenMissionPreprocessor();
	for ( int i = 0; i < pRulesDatabase->GetFileCount(); ++ i )
	{
		m_pPreprocessor->ParseAndStripRules( pRulesDatabase->GetFile( i ) );
	}
	delete pRulesDatabase;

	// Preprocess the missions in-memory.
	for ( int i = 0; i < m_pMissionDatabase->GetFileCount(); ++ i )
	{
		KeyValues *pOriginalMission = m_pMissionDatabase->GetFile( i );
		if ( !m_pPreprocessor->SubstituteRules( pOriginalMission ) )
		{
			Warning( "Error pre-processing mission '%s'.\n", m_pMissionDatabase->GetFilename( i ) );
		}
	}

	LoadLocationGrid();
}

CASW_Location_Grid::~CASW_Location_Grid()
{	
	m_Groups.PurgeAndDeleteElements();
	delete m_pMissionDatabase;
	delete m_pPreprocessor;
}

bool CASW_Location_Grid::LoadLocationGrid()
{
	m_Groups.PurgeAndDeleteElements();

	// open our mission grid file
	KeyValues *pGridKeys = new KeyValues( "MissionGrid" );
	if ( !pGridKeys->LoadFromFile( g_pFullFileSystem, "resource/mission_grid.txt", "GAME" ) )
	{
		Msg( "Failed to load resource/mission_grid.txt\n" );
		pGridKeys->deleteThis();
		return false;
	}

	KeyValues *pKeys = pGridKeys;
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), "Group" ) )
		{
			CASW_Location_Group *pGroup = new CASW_Location_Group( this );
			pGroup->LoadFromKeyValues( pKeys );
			m_Groups.AddToTail( pGroup );
		}
		pKeys = pKeys->GetNextKey();
	}
	return true;
}

bool CASW_Location_Grid::SaveLocationGrid()
{
	const char *filename = "resource/mission_grid.txt";

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( int i = 0; i < m_Groups.Count(); i++ )
	{
		KeyValues *pKey = m_Groups[i]->GetKeyValuesForEditor();
		pKey->RecursiveSaveToFile( buf, 0 );
		pKey->deleteThis();
	}
	if ( !g_pFullFileSystem->WriteFile( filename, "GAME", buf ) )
	{
		Warning( "Failed to SaveLocationGrid %s\n", filename );
		return false;
	}
	return true;
}

IASW_Location_Group* CASW_Location_Grid::GetGroup( int iIndex )
{
	if ( iIndex < 0 || iIndex >= m_Groups.Count() )
		return NULL;

	return m_Groups[iIndex];
}

CASW_Location_Group* CASW_Location_Grid::GetCGroup( int iIndex )
{
	if ( iIndex < 0 || iIndex >= m_Groups.Count() )
		return NULL;

	return m_Groups[iIndex];
}

CASW_Location_Group* CASW_Location_Grid::GetGroupByName( const char *szName )
{
	int nGroups = m_Groups.Count();
	for ( int i = 0; i < nGroups; i++ )
	{
		if ( !Q_stricmp( m_Groups[i]->m_szGroupName, szName ) )
			return m_Groups[i];
	}
	return NULL;
}

IASW_Location* CASW_Location_Grid::GetLocationByID( int iLocationID )
{
	for ( int i = 0; i < m_Groups.Count(); i++ )
	{
		for ( int k = 0; k < m_Groups[i]->GetNumLocations(); k++ )
		{
			if ( m_Groups[i]->m_Locations[k]->m_iLocationID == iLocationID )
			{
				return m_Groups[i]->m_Locations[k];
			}
		}
	}
	return NULL;
}

void CASW_Location_Grid::SetLocationComplete( int iLocationID )
{
	for ( int i = 0; i < m_Groups.Count(); i++ )
	{
		for ( int k = 0; k < m_Groups[i]->GetNumLocations(); k++ )
		{
			if ( m_Groups[i]->m_Locations[k]->m_iLocationID == iLocationID )
			{
				m_Groups[i]->m_Locations[k]->m_bCompleted = true;
			}
		}
	}
}

int CASW_Location_Grid::GetFreeLocationID()
{
	int iHighestID = 0;
	for ( int i = 0; i < m_Groups.Count(); i++ )
	{
		for ( int k = 0; k < m_Groups[i]->GetNumLocations(); k++ )
		{
			if ( m_Groups[i]->m_Locations[k]->m_iLocationID > iHighestID )
			{
				iHighestID = m_Groups[i]->m_Locations[k]->m_iLocationID;
			}
		}
	}
	return ++iHighestID;
}

KeyValues *CASW_Location_Grid::GetMissionData( const char *pMissionName )
{
	return m_pMissionDatabase->GetFileByName( pMissionName );
}

// delete a group and all locations in it
void CASW_Location_Grid::DeleteGroup( int iIndex )
{
	m_Groups.Remove( iIndex );
}

void CASW_Location_Grid::CreateNewGroup()
{
	CASW_Location_Group *pGroup = new CASW_Location_Group( this );
	m_Groups.AddToTail( pGroup );
}


//======================================================================================
//  Reward
//======================================================================================
CASW_Reward::CASW_Reward()
{
	m_RewardType = ASW_REWARD_MONEY;
	m_iRewardAmount = 0;
	m_szRewardName = NULL;
	m_iRewardLevel = 1;
	m_iRewardQuality = 0;
}

CASW_Reward::~CASW_Reward()
{
	delete m_szRewardName;
}

bool CASW_Reward::LoadFromKeyValues( KeyValues *pKeys, int iMissionDifficulty )
{
	if ( !Q_stricmp( pKeys->GetName(), "MoneyReward") )
	{
		m_RewardType = ASW_REWARD_MONEY;
		m_iRewardAmount = pKeys->GetInt();
		return true;
	}
	else if ( !Q_stricmp( pKeys->GetName(), "XPReward") )
	{
		m_RewardType = ASW_REWARD_XP;
		//TODO: this was the previous average of how much XP you got with the Tier system per objective.
		// we probably want a more robust system for determining the amount of XP we want to reward on the mission
		m_iRewardAmount = pKeys->GetInt( NULL, 400 * iMissionDifficulty );  
		if ( m_iRewardAmount == 0 )
		{
			m_iRewardAmount = 400 * iMissionDifficulty;
		}
		return true;
	}
	else if ( !Q_stricmp( pKeys->GetName(), "ItemReward") )
	{
		m_RewardType = ASW_REWARD_ITEM;
		m_iRewardAmount = 0;
		m_szRewardName = TileGenCopyString( pKeys->GetString( "ItemName" ) );
		m_iRewardLevel = pKeys->GetInt( "ItemLevel" );
		m_iRewardQuality = pKeys->GetInt( "ItemQuality" );
	}
	return false;
}

KeyValues* CASW_Reward::GetKeyValuesForEditor()
{
	switch( m_RewardType )
	{
	case ASW_REWARD_MONEY:
		{
			KeyValues *pReward = new KeyValues( "MoneyReward");
			pReward->SetInt( NULL, m_iRewardAmount );
			return pReward;
		}
	case ASW_REWARD_XP:
		{
			KeyValues *pReward = new KeyValues( "XPReward");
			pReward->SetInt( NULL, m_iRewardAmount );
			return pReward;
		}
	case ASW_REWARD_ITEM:
		{
			KeyValues *pReward = new KeyValues( "ItemReward");
			pReward->SetString( "ItemName", m_szRewardName );
			pReward->SetInt( "ItemLevel", m_iRewardLevel );
			pReward->SetInt( "ItemQuality", m_iRewardQuality );
			return pReward;
		}
	}
	return NULL;
}

//======================================================================================
//  Group
//======================================================================================
CASW_Location_Group::CASW_Location_Group( CASW_Location_Grid *pLocationGrid ) :
m_pLocationGrid( pLocationGrid )
{
	m_szGroupName = NULL;
	m_szTitleText = NULL;
	m_szDescriptionText = NULL;
	m_szImageName = NULL;
	m_iRequiredUnlocks = 0;
	m_Color = Color( 128, 128, 128, 128 );
	m_Locations.PurgeAndDeleteElements();
	m_Random.SetSeed( 0 );
}

CASW_Location_Group::~CASW_Location_Group()
{
	m_Locations.PurgeAndDeleteElements();
	delete m_szGroupName;
	delete m_szTitleText;
	delete m_szDescriptionText;
	delete m_szImageName;
}

KeyValues* CASW_Location_Group::GetKeyValuesForEditor()
{
	char buffer[64];
	KeyValues *pKeys = new KeyValues( "Group" );
	pKeys->SetInt( "RequiredUnlocks", m_iRequiredUnlocks );
	Q_snprintf( buffer, sizeof( buffer ), "%d %d %d %d", m_Color.r(), m_Color.g(), m_Color.b(), m_Color.a() );
	pKeys->SetString( "Color", buffer );
	pKeys->SetString( "Name", m_szGroupName );
	pKeys->SetString( "TitleText", m_szTitleText );
	pKeys->SetString( "DescriptionText", m_szDescriptionText );	
	pKeys->SetString( "ImageName", m_szImageName );

	for ( int i = 0; i < m_UnlockedBy.Count(); i++ )
	{
		KeyValues *pKey = new KeyValues( "UnlockMissionID" );		
		Q_snprintf( buffer, sizeof( buffer ), "%d", m_UnlockedBy[i] );
		pKey->SetStringValue( buffer );
		pKeys->AddSubKey( pKey );
	}
	for ( int i = 0; i < m_Locations.Count(); i++ )
	{
		KeyValues *pKey = m_Locations[i]->GetKeyValuesForEditor();
		pKeys->AddSubKey( pKey );
	}
	return pKeys;
}
void CASW_Location_Group::LoadFromKeyValues( KeyValues *pKeys )
{
	m_iRequiredUnlocks = pKeys->GetInt( "RequiredUnlocks", 0 );
	m_Color = pKeys->GetColor( "Color" );
	m_szGroupName = TileGenCopyString( pKeys->GetString( "Name" ) );
	m_szTitleText = TileGenCopyString( pKeys->GetString( "TitleText" ) );
	m_szDescriptionText = TileGenCopyString( pKeys->GetString( "DescriptionText" ) );	
	m_szImageName = TileGenCopyString( pKeys->GetString( "ImageName" ) );	

	// check all keys for unlock IDs
	m_UnlockedBy.Purge();
	m_Locations.PurgeAndDeleteElements();
	for ( KeyValues *pSubKey = pKeys->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		if ( !Q_stricmp( pSubKey->GetName(), "UnlockMissionID" ) )
		{
			m_UnlockedBy.AddToTail( pSubKey->GetInt() );
		}
		else if ( !Q_stricmp( pSubKey->GetName(), "Location" ) )
		{
			CASW_Location *pLocation = new CASW_Location( m_pLocationGrid );
			pLocation->LoadFromKeyValues( pSubKey, &m_Random );
			pLocation->m_pGroup = this;
			m_Locations.AddToTail( pLocation );
		}
	}
}

IASW_Location* CASW_Location_Group::GetLocation( int iIndex )
{
	if ( iIndex < 0 || iIndex >= m_Locations.Count() )
		return NULL;

	return m_Locations[iIndex];
}

CASW_Location* CASW_Location_Group::GetCLocation( int iIndex )
{
	if ( iIndex < 0 || iIndex >= m_Locations.Count() )
		return NULL;

	return m_Locations[iIndex];
}

int CASW_Location_Group::GetHighestUnlockMissionID()
{
	int iMissionID = 0;
	if ( m_UnlockedBy.Count() <= 0 )
		return iMissionID;

	for ( int i = 0; i < m_UnlockedBy.Count(); i++ )
	{
		if ( i == 0 )
		{
			iMissionID = m_UnlockedBy[i];
			continue;
		}

		if ( m_UnlockedBy[i-1] && m_UnlockedBy[i] > m_UnlockedBy[i-1])
			iMissionID = m_UnlockedBy[i];
	}
	return iMissionID;
}

bool CASW_Location_Group::IsGroupLocked( CUtlVector<int> &completedMissions )
{
	if ( asw_unlock_all_locations.GetBool() )
		return false;

	if ( m_UnlockedBy.Count() <= 0 )
		return false;

	for ( int i = 0; i < m_UnlockedBy.Count(); i++ )
	{
		if ( completedMissions.Find( m_UnlockedBy[i] ) != completedMissions.InvalidIndex() )	// this mission has been completed, so it won't make this group locked
			continue;

		// one of our required missions is incomplete, so this group is locked
		return true;
	}
	return false;
}

//======================================================================================
// Location
//======================================================================================

CASW_Location::CASW_Location( CASW_Location_Grid *pLocationGrid ) :
m_pMissionKV( NULL ),
m_pLocationGrid( pLocationGrid )
{
	m_szMapName[0] = 0;
	m_iDifficulty = 0;
	m_iMinDifficulty = 1;
	m_iMaxDifficulty = 100;
	m_pGroup = NULL;
	m_bCompleted = false;
	m_iXPos = 0;
	m_iYPos = 0;
	m_pszCustomMission = NULL;
	m_iCompanyIndex = 0;
	m_bIsMissionOptional = true;
	m_szStoryScene = NULL;
	m_szImageName = NULL;
	m_iLocationID = -1;
}

CASW_Location::~CASW_Location()
{
	if ( m_pMissionKV != NULL )
	{
		m_pMissionKV->deleteThis();
	}
	m_Rewards.PurgeAndDeleteElements();
}

KeyValues* CASW_Location::GetMissionSettings()
{
	return m_pMissionKV != NULL ? m_pMissionKV->FindKey( "mission_settings" ) : NULL;
}

KeyValues* CASW_Location::GetMissionDefinition()
{
	return m_pMissionKV;
}

// TODO: Move this company stuff into some data driven class?
const char* CASW_Location::GetCompanyName( void )
{
/*
asw_company_name_deuce		"Deuce Company"
asw_company_name_iaf		"IAF (Interstellar Armed Forces)"
asw_company_name_totem		"Totem Corp."
asw_company_name_oldeearth	"Olde Earth"
asw_company_name_magnus		"Magnus-Parke Industries"
asw_company_name_civilised	"Civilised LTD."
*/
	switch( m_iCompanyIndex )
	{
	case 1:
		return "#asw_company_name_deuce";

	case 2:
		return "#asw_company_name_iaf";

	case 3:
		return "#asw_company_name_totem";

	case 4:
		return "#asw_company_name_oldeearth";

	case 5:
		return "#asw_company_name_magnus";

	case 6:
		return "#asw_company_name_civilised";

	default:
		return "#asw_company_name_none";
	}
}

const char* CASW_Location::GetCompanyImage( void )
{
	/*
	asw_company_name_deuce		"Deuce Company"
	asw_company_name_iaf		"IAF (Interstellar Armed Forces)"
	asw_company_name_totem		"Totem Corp."
	asw_company_name_oldeearth	"Olde Earth"
	asw_company_name_magnus		"Magnus-Parke Industries"
	asw_company_name_civilised	"Civilised LTD."
	*/
	switch( m_iCompanyIndex )
	{
	case 1:
		return "briefing/companylogo/deuce";

	case 2:
		return "briefing/companylogo/iaf";

	case 3:
		return "briefing/companylogo/totem";

	case 4:
		return "briefing/companylogo/oldeearth";

	case 5:
		return "briefing/companylogo/magnus";

	case 6:
		return "briefing/companylogo/civilised";

	default:
		return "briefing/companylogo/none";
	}
}

KeyValues* CASW_Location::GetKeyValuesForEditor()
{
	KeyValues *pKeys = new KeyValues( "Location" );
	pKeys->SetInt( "x", m_iXPos );
	pKeys->SetInt( "y", m_iYPos );
	pKeys->SetInt( "MinDifficulty", m_iMinDifficulty );
	pKeys->SetInt( "MaxDifficulty", m_iMaxDifficulty );
	pKeys->SetString( "StoryScene", m_szStoryScene );
	pKeys->SetString( "ImageName", m_szImageName );
	// if mission doesn't have a valid ID then find one
	if ( m_iLocationID == -1 )
	{
		m_iLocationID = LocationGrid()->GetFreeLocationID();
	}
	pKeys->SetInt( "ID", m_iLocationID );
	pKeys->SetString( "CustomMission", m_pszCustomMission );
	pKeys->SetInt( "Company", m_iCompanyIndex );
	//pKeys->SetInt( "MoneyReward", m_iMoneyReward );
	//pKeys->SetInt( "XPReward", m_iXPReward );
	pKeys->SetInt( "Optional", m_bIsMissionOptional );

	if ( m_Rewards.Count() > 0 )
	{
		KeyValues *pRewards = new KeyValues( "Rewards");
		pKeys->AddSubKey( pRewards );
		for ( int i = 0; i < m_Rewards.Count(); i++ )
		{
			KeyValues *pReward = m_Rewards[i]->GetKeyValuesForEditor();
			if ( pReward )
			{
				pRewards->AddSubKey( pReward );
			}
		}
	}
	return pKeys;
}

void CASW_Location::LoadFromKeyValues( KeyValues *pKeys, CUniformRandomStream* pStream)
{
	m_iXPos = pKeys->GetInt( "x" );
	m_iYPos = pKeys->GetInt( "y" );
	Q_strcpy( m_szMapName, "random" );
	m_iMinDifficulty = pKeys->GetInt( "MinDifficulty", 1 );
	m_iMaxDifficulty = pKeys->GetInt( "MaxDifficulty", 100 );
	m_iCompanyIndex = pKeys->GetInt( "Company", 0 );	
	m_bIsMissionOptional = !!pKeys->GetInt( "Optional", 1 );
	m_iLocationID = pKeys->GetInt( "ID", -1 );		
	m_pszCustomMission = TileGenCopyString( pKeys->GetString( "CustomMission" ) );
	m_szStoryScene = TileGenCopyString( pKeys->GetString( "StoryScene" ) ) ? TileGenCopyString( pKeys->GetString( "StoryScene" ) ) : "spaceport_crashsite";	
	m_szImageName = TileGenCopyString( pKeys->GetString( "ImageName" ) ) ? TileGenCopyString( pKeys->GetString( "ImageName" ) ) : "swarm/MissionPics/PlantMissionpic.vmt";	
	if ( m_pszCustomMission )
	{
		Q_FixSlashes( m_pszCustomMission );
	}

	// pick a difficulty between the bounds
	m_iDifficulty = pStream->RandomInt( m_iMinDifficulty, m_iMaxDifficulty );

	if ( m_pszCustomMission && m_pszCustomMission[0] )
	{
		m_pMissionKV = m_pLocationGrid->GetMissionData( m_pszCustomMission );
	}
	if ( m_pMissionKV )
	{
		// Make a copy so we can modify it
		m_pMissionKV = m_pMissionKV->MakeCopy();
		
		// Fill out mission settings from the mission grid data so it gets into the .layout file
		if ( GetMissionSettings() )
		{
			GetMissionSettings()->SetInt( "Difficulty", m_iDifficulty );
			const char *pMissionName = m_pszCustomMission;
			if ( Q_strnicmp( m_pszCustomMission, "tilegen/new_missions/", Q_strlen( "tilegen/new_missions/" ) ) == 0 )
			{
				pMissionName += Q_strlen( "tilegen/new_missions/" );
			}
			GetMissionSettings()->SetString( "Filename", m_pszCustomMission + Q_strlen( "tilegen/new_missions/" ) );
			GetMissionSettings()->SetInt( "GridLocationID", m_iLocationID );
		}
	}
	
	// if mission doesn't have a valid ID then find one
	if ( m_iLocationID == -1 )
	{
		m_iLocationID = LocationGrid()->GetFreeLocationID();
	}

	KeyValues *pRewards = pKeys->FindKey( "Rewards" );
	if ( pRewards )
	{
		for ( KeyValues *pRewardKey = pRewards->GetFirstSubKey(); pRewardKey; pRewardKey = pRewardKey->GetNextKey() )
		{
			CASW_Reward *pReward = new CASW_Reward();
			if ( pReward->LoadFromKeyValues( pRewardKey, m_iDifficulty ) )
			{
				m_Rewards.AddToTail( pReward );
			}
			else
			{
				delete pReward;
			}
		}
	}
	else
	{
		// TODO: Create some default rewards?
	}
}

bool CASW_Location::IsLocationLocked( CUtlVector<int> &completedMissions )
{
	if ( !GetGroup() )
		return false;

	return GetGroup()->IsGroupLocked( completedMissions );
}

int CASW_Location::GetNumRewards()
{
	return m_Rewards.Count();
}

IASW_Reward* CASW_Location::GetReward( int iRewardIndex )
{
	if ( iRewardIndex < 0 || iRewardIndex >= m_Rewards.Count() )
		return NULL;

	return m_Rewards[ iRewardIndex ];
}

int CASW_Location::GetMoneyReward()
{
	int iAmount = 0;
	for ( int i = 0; i < m_Rewards.Count(); i++ )
	{
		if ( m_Rewards[i]->GetRewardType() == ASW_REWARD_MONEY )
		{
			iAmount += m_Rewards[i]->GetRewardAmount();
		}
	}
	return iAmount;
}

int CASW_Location::GetXPReward()
{
	int iAmount = 0;
	for ( int i = 0; i < m_Rewards.Count(); i++ )
	{
		if ( m_Rewards[i]->GetRewardType() == ASW_REWARD_XP )
		{
			iAmount += m_Rewards[i]->GetRewardAmount();
		}
	}
	return iAmount;
}