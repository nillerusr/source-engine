
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#ifdef _WIN32
#include "winerror.h"
#endif
#include "tf_hud_statpanel.h"
#include "tf_hud_winpanel.h"
#include <vgui/IVGui.h>
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_tf_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "tf2base/c_tf_player.h"
#include "tf2base/c_tf_team.h"
#include "tf2base/tf_steamstats.h"
#include "filesystem.h"
#include "dmxloader/dmxloader.h"
#include "fmtstr.h"
#include "tf_statsummary.h"
#include "usermessages.h"
#include "hud_macros.h"
#include "ixboxsystem.h"
#include "achievementmgr.h"
#include "tf_hud_freezepanel.h"
#include "tf_gamestats_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CTFStatPanel, 1 );
DECLARE_HUD_MESSAGE( CTFStatPanel, PlayerStatsUpdate );

BEGIN_DMXELEMENT_UNPACK( RoundStats_t )
	DMXELEMENT_UNPACK_FIELD( "iNumShotsHit", "0", int, m_iStat[TFSTAT_SHOTS_HIT] )
	DMXELEMENT_UNPACK_FIELD( "iNumShotsFired", "0", int, m_iStat[TFSTAT_SHOTS_FIRED] )
	DMXELEMENT_UNPACK_FIELD( "iNumberOfKills", "0", int, m_iStat[TFSTAT_KILLS] )
	DMXELEMENT_UNPACK_FIELD( "iNumDeaths", "0", int, m_iStat[TFSTAT_DEATHS] )
	DMXELEMENT_UNPACK_FIELD( "iDamageDealt", "0", int, m_iStat[TFSTAT_DAMAGE] )
	DMXELEMENT_UNPACK_FIELD( "iPlayTime", "0", int, m_iStat[TFSTAT_PLAYTIME] )
	DMXELEMENT_UNPACK_FIELD( "iPointCaptures", "0", int, m_iStat[TFSTAT_CAPTURES] )
	DMXELEMENT_UNPACK_FIELD( "iPointDefenses", "0", int, m_iStat[TFSTAT_DEFENSES] )
	DMXELEMENT_UNPACK_FIELD( "iDominations", "0", int, m_iStat[TFSTAT_DOMINATIONS] )
	DMXELEMENT_UNPACK_FIELD( "iRevenge", "0", int, m_iStat[TFSTAT_REVENGE] )
	DMXELEMENT_UNPACK_FIELD( "iPointsScored", "0", int, m_iStat[TFSTAT_POINTSSCORED] )
	DMXELEMENT_UNPACK_FIELD( "iBuildingsDestroyed", "0", int, m_iStat[TFSTAT_BUILDINGSDESTROYED] )
	DMXELEMENT_UNPACK_FIELD( "iHeadshots", "0", int, m_iStat[TFSTAT_HEADSHOTS] )
	DMXELEMENT_UNPACK_FIELD( "iHealthPointsHealed", "0", int, m_iStat[TFSTAT_HEALING] )
	DMXELEMENT_UNPACK_FIELD( "iNumInvulnerable", "0", int, m_iStat[TFSTAT_INVULNS] )
	DMXELEMENT_UNPACK_FIELD( "iKillAssists", "0", int, m_iStat[TFSTAT_KILLASSISTS] )
	DMXELEMENT_UNPACK_FIELD( "iBackstabs", "0", int, m_iStat[TFSTAT_BACKSTABS] )
	DMXELEMENT_UNPACK_FIELD( "iHealthPointsLeached", "0", int, m_iStat[TFSTAT_HEALTHLEACHED] )
	DMXELEMENT_UNPACK_FIELD( "iBuildingsBuilt", "0", int, m_iStat[TFSTAT_BUILDINGSBUILT] )
	DMXELEMENT_UNPACK_FIELD( "iSentryKills", "0", int, m_iStat[TFSTAT_MAXSENTRYKILLS] )
	DMXELEMENT_UNPACK_FIELD( "iNumTeleports", "0", int, m_iStat[TFSTAT_TELEPORTS] )
END_DMXELEMENT_UNPACK( RoundStats_t, s_RoundStatsUnpack )

BEGIN_DMXELEMENT_UNPACK( ClassStats_t )
	DMXELEMENT_UNPACK_FIELD( "iPlayerClass", "0", int, iPlayerClass )
	DMXELEMENT_UNPACK_FIELD( "iNumberOfRounds", "0", int, iNumberOfRounds )
	// RoundStats_t		accumulated;
	// RoundStats_t		max;
END_DMXELEMENT_UNPACK( ClassStats_t, s_ClassStatsUnpack )

// priority order of stats to display record for; earlier position in list is highest
TFStatType_t g_statPriority[] = { TFSTAT_HEADSHOTS, TFSTAT_BACKSTABS, TFSTAT_MAXSENTRYKILLS, TFSTAT_HEALING, TFSTAT_KILLS, TFSTAT_KILLASSISTS,  
	TFSTAT_DAMAGE, TFSTAT_DOMINATIONS, TFSTAT_INVULNS, TFSTAT_BUILDINGSDESTROYED, TFSTAT_CAPTURES, TFSTAT_DEFENSES, TFSTAT_REVENGE, TFSTAT_TELEPORTS, TFSTAT_BUILDINGSBUILT, 
	TFSTAT_HEALTHLEACHED, TFSTAT_POINTSSCORED, TFSTAT_PLAYTIME };
// stat types that we don't display records for, kept in this list just so we can assert all stats appear in one list or the other
TFStatType_t g_statUnused[] = { TFSTAT_DEATHS, TFSTAT_UNDEFINED, TFSTAT_SHOTS_FIRED, TFSTAT_SHOTS_HIT };

// localization keys for stat panel text, must be in same order as TFStatType_t
const char *g_szLocalizedRecordText[] =
{
	"",
	"[shots hit]",
	"[shots fired]",
	"#StatPanel_Kills",	
	"[deaths]",
	"#StatPanel_DamageDealt",
	"#StatPanel_Captures",
	"#StatPanel_Defenses",
	"#StatPanel_Dominations",
	"#StatPanel_Revenge",
	"#StatPanel_PointsScored",
	"#StatPanel_BuildingsDestroyed",
	"#StatPanel_Headshots",
	"#StatPanel_PlayTime",
	"#StatPanel_Healing",
	"#StatPanel_Invulnerable",
	"#StatPanel_KillAssists",
	"#StatPanel_Backstabs",
	"#StatPanel_HealthLeached",
	"#StatPanel_BuildingsBuilt",
	"#StatPanel_SentryKills",		
	"#StatPanel_Teleports"
};


static CTFStatPanel *statPanel = NULL;
extern CAchievementMgr g_AchievementMgrTF;

//-----------------------------------------------------------------------------
// Purpose: Returns the static stats panel
//-----------------------------------------------------------------------------
CTFStatPanel *GetStatPanel()
{
	return statPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFStatPanel::CTFStatPanel( const char *pElementName )
	: EditablePanel( NULL, "StatPanel" ), CHudElement( pElementName )
{
	// Assert that all defined stats are in our prioritized list or explicitly unused
	Assert( ARRAYSIZE( g_statPriority ) + ARRAYSIZE( g_statUnused ) == TFSTAT_MAX );

	ResetDisplayedStat();
	m_bStatsChanged = false;
	m_bLocalFileTrusted = false;
	m_flTimeLastSpawn = 0;
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	m_bShouldBeVisible = false;
	SetScheme( "ClientScheme" );
	statPanel = this;
	m_bNeedToCalcMaxs = false;

	m_pClassImage = new CTFClassImage( this, "StatPanelClassImage" );
	m_iClassCurrentLife = TF_CLASS_UNDEFINED;
	m_iTeamCurrentLife = TEAM_UNASSIGNED;

	// Read stats from disk.  (Definitive stat store for X360; for PC, whatever we get from Steam is authoritative.)
	ReadStats();

	RegisterForRenderGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFStatPanel::~CTFStatPanel()
{
	if ( statPanel == this )
		statPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: called when level is shutting down
//-----------------------------------------------------------------------------
void CTFStatPanel::LevelShutdown()
{
	// write out stats if they've changed
	CalcMaxsAndRecords();
	WriteStats();
	UpdateStatSummaryPanel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::Reset()
{
	if ( gpGlobals->curtime > m_flTimeHide )
	{
		Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets which stat is being displayed
//-----------------------------------------------------------------------------
void CTFStatPanel::ResetDisplayedStat()
{
	m_iCurStatValue = 0;
	m_iCurStatTeam = TEAM_UNASSIGNED;
	m_statRecord = TFSTAT_UNDEFINED;
	m_recordBreakType = RECORDBREAK_NONE;
	m_iCurStatClass = TF_CLASS_UNDEFINED;
	m_flTimeHide = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::Init()
{
	// listen for events
	HOOK_HUD_MESSAGE( CTFStatPanel, PlayerStatsUpdate );
	ListenForGameEvent( "player_spawn" );
	
	Hide();

	CHudElement::Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::UpdateStats( int iMsgType )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return;

	// don't count stats if cheats on, commentary mode, etc
	if ( !g_AchievementMgrTF.CheckAchievementsEnabled() )
		return;

	ClassStats_t &classStats = GetClassStats( m_iClassCurrentLife );
	
	if ( iMsgType == STATMSG_PLAYERDEATH || iMsgType == STATMSG_PLAYERRESPAWN )
	{
		// if the player just died, accumulate current life into total and check for maxs and records
		classStats.AccumulateRound( m_RoundStatsCurrentLife );
		classStats.accumulated.m_iStat[TFSTAT_MAXSENTRYKILLS] = 0;	// sentry kills is a max value rather than a count, meaningless to accumulate
		CalcMaxsAndRecords();

		// reset current life stats
		m_iClassCurrentLife = TF_CLASS_UNDEFINED;
		m_iTeamCurrentLife = TEAM_UNASSIGNED;
		m_RoundStatsCurrentLife.Reset();
	}
	
	m_bStatsChanged = true;

	if ( m_statRecord > TFSTAT_UNDEFINED )
	{
		bool bAlive = ( iMsgType != STATMSG_PLAYERDEATH );
		if ( !bAlive || ( gpGlobals->curtime - m_flTimeLastSpawn < 3.0 ) )
		{
			// show the panel now if dead or very recently spawned
			vgui::ivgui()->AddTickSignal( GetVPanel(), 1000 );
			ShowStatPanel( m_iCurStatClass, m_iCurStatTeam, m_iCurStatValue, m_statRecord, m_recordBreakType, bAlive );
			m_flTimeHide = gpGlobals->curtime + ( bAlive ? 12.0f : 20.0f );
			m_statRecord = TFSTAT_UNDEFINED;
		}
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		event->SetBool( "forceupload", false );
		gameeventmanager->FireEventClientSide( event );
	}

	UpdateStatSummaryPanel();
}

//-----------------------------------------------------------------------------
// Purpose: Sees if current life stats have broken records & keep track of max values
//-----------------------------------------------------------------------------
void CTFStatPanel::CalcMaxsAndRecords()
{
	if ( m_iClassCurrentLife == TF_CLASS_UNDEFINED )
		return;

	ClassStats_t &classStats = GetClassStats( m_iClassCurrentLife );

	ResetDisplayedStat();
	m_iCurStatClass = m_iClassCurrentLife;
	m_iCurStatTeam = m_iTeamCurrentLife;
		
	// run through all stats we keep records for, update the max value, and if a record is set,
	// remember the highest priority record
	for ( int i= ARRAYSIZE( g_statPriority )-1; i >= 0; i-- )
	{
		TFStatType_t statType = g_statPriority[i];
		int iCur = m_RoundStatsCurrentLife.m_iStat[statType];
		int iMax = classStats.max.m_iStat[statType];
		if ( iCur > iMax )
		{
			// Record was set, remember what stat set a record.
			classStats.max.m_iStat[statType] = iCur;
			m_iCurStatValue = iCur;
			m_statRecord = statType;
			m_recordBreakType = RECORDBREAK_BEST;
		}
		else if ( ( iCur > 0 ) && ( m_recordBreakType <= RECORDBREAK_TIE ) && ( iCur == iMax ) )
		{
			// if we haven't broken a record and we tied this one, display it
			m_iCurStatValue = iCur;
			m_statRecord = statType;
			m_recordBreakType = RECORDBREAK_TIE;
		}
		else if ( ( iCur > 0 ) && ( m_recordBreakType <= RECORDBREAK_CLOSE ) && ( iCur >= (int) ( (float) iMax * 0.8f ) ) )
		{
			// if we haven't broken a record or tied a record but we came close to this one, display it
			m_iCurStatValue = iCur;
			m_statRecord = statType;
			m_recordBreakType = RECORDBREAK_CLOSE;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::TestStatPanel( TFStatType_t statType, RecordBreakType_t recordType )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return;
	
	m_iCurStatClass = pPlayer->GetPlayerClass()->GetClassIndex();
	ClassStats_t &classStats = GetClassStats( m_iCurStatClass );;
	m_iCurStatValue = classStats.max.m_iStat[statType];
	m_iCurStatTeam = pPlayer->GetTeamNumber();

	ShowStatPanel( m_iCurStatClass, m_iCurStatTeam, m_iCurStatValue, statType, recordType, false );
}

//-----------------------------------------------------------------------------
// Purpose: Writes stat file.  Used as primary storage for X360.  For PC,
//			Steam is authoritative but we write stat file for debugging (although
//			we never read it).
//-----------------------------------------------------------------------------
void CTFStatPanel::WriteStats( void )
{
	if ( !m_bStatsChanged )
		return;

	MEM_ALLOC_CREDIT();

	DECLARE_DMX_CONTEXT();
	CDmxElement *pPlayerStats = CreateDmxElement( "PlayerStats" );
	CDmxElementModifyScope modify( pPlayerStats );

	// get Steam ID.  If not logged into Steam, use 0
	int iSteamID = 0;
	/*
	if ( SteamUser() )
	{
		CSteamID steamID = SteamUser()->GetSteamID();
		iSteamID = steamID.GetAccountID();
	}	*/
	// Calc CRC of all data to make the local data file somewhat tamper-resistant
	int iCRC = CalcCRC( iSteamID );

	pPlayerStats->SetValue( "iVersion", static_cast<int>( PLAYERSTATS_FILE_VERSION ) );
	pPlayerStats->SetValue( "SteamID", iSteamID );	
	pPlayerStats->SetValue( "iTimestamp", iCRC );	// store the CRC with a non-obvious name

	CDmxAttribute *pClassStatsList = pPlayerStats->AddAttribute( "aClassStats" );
	CUtlVector< CDmxElement* >& classStats = pClassStatsList->GetArrayForEdit<CDmxElement*>();

	modify.Release();

	for( int i = 0; i < m_aClassStats.Count(); i++ )
	{
		const ClassStats_t &stat = m_aClassStats[ i ];

		// strip out any garbage class data
		if ( ( stat.iPlayerClass > TF_LAST_NORMAL_CLASS ) || ( stat.iPlayerClass < TF_FIRST_NORMAL_CLASS ) )
			continue;

		CDmxElement *pClass = CreateDmxElement( "ClassStats_t" );
		classStats.AddToTail( pClass );

		CDmxElementModifyScope modifyClass( pClass );

		pClass->SetValue( "comment: classname", g_aPlayerClassNames_NonLocalized[ stat.iPlayerClass ] );
		pClass->AddAttributesFromStructure( &stat, s_ClassStatsUnpack );
		
		CDmxElement *pAccumulated = CreateDmxElement( "RoundStats_t" );
		pAccumulated->AddAttributesFromStructure( &stat.accumulated, s_RoundStatsUnpack );
		pClass->SetValue( "accumulated", pAccumulated );

		CDmxElement *pMax = CreateDmxElement( "RoundStats_t" );
		pMax->AddAttributesFromStructure( &stat.max, s_RoundStatsUnpack );
		pClass->SetValue( "max", pMax );
	}

	if ( IsX360() )
	{
#ifdef _X360
		if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
			return;
#endif
	}

	char szFilename[_MAX_PATH];

	if ( IsX360() )
		Q_snprintf( szFilename, sizeof( szFilename ), "cfg:/tf2_playerstats.dmx" );
	else
		Q_snprintf( szFilename, sizeof( szFilename ), "tf2_playerstats.dmx" );

	{
		MEM_ALLOC_CREDIT();
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( SerializeDMX( buf, pPlayerStats, szFilename ) )
		{
			filesystem->WriteFile( szFilename, "MOD", buf );
		}
	}

	CleanupDMX( pPlayerStats );

	if ( IsX360() )
	{
		xboxsystem->FinishContainerWrites();
	}

	m_bStatsChanged = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFStatPanel::ReadStats( void )
{
	CDmxElement *pPlayerStats;

	DECLARE_DMX_CONTEXT();

	if ( IsX360() )
	{
#ifdef _X360
		if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
			return false;
#endif
	}

	char	szFilename[_MAX_PATH];

	if ( IsX360() )
	{
		Q_snprintf( szFilename, sizeof( szFilename ), "cfg:/tf2_playerstats.dmx" );
	}
	else
	{
		Q_snprintf( szFilename, sizeof( szFilename ), "tf2_playerstats.dmx" );
	}

	MEM_ALLOC_CREDIT();

	bool bOk = UnserializeDMX( szFilename, "MOD", true, &pPlayerStats );

	if ( !bOk )
		return false;

	int iVersion = pPlayerStats->GetValue< int >( "iVersion" );
	if ( iVersion > PLAYERSTATS_FILE_VERSION )
	{
		// file is beyond our comprehension
		return false;
	}

	int iSteamID = pPlayerStats->GetValue<int>( "SteamID" );
	int iCRCFile = pPlayerStats->GetValue<int>( "iTimestamp" );	
	
	const CUtlVector< CDmxElement* > &aClassStatsList = pPlayerStats->GetArray< CDmxElement * >( "aClassStats" );
	int iCount = aClassStatsList.Count();
	m_aClassStats.SetCount( iCount );
	for( int i = 0; i < m_aClassStats.Count(); i++ )
	{
		CDmxElement *pClass = aClassStatsList[ i ];
		ClassStats_t &stat = m_aClassStats[ i ];

		pClass->UnpackIntoStructure(&stat, sizeof(stat), s_ClassStatsUnpack);

		CDmxElement *pAccumulated = pClass->GetValue< CDmxElement * >( "accumulated" );
		pAccumulated->UnpackIntoStructure(&stat.accumulated, sizeof(stat.accumulated), s_RoundStatsUnpack);

		CDmxElement *pMax = pClass->GetValue< CDmxElement * >( "max" );
		pMax->UnpackIntoStructure( &stat.max, sizeof(stat.max), s_RoundStatsUnpack );
	}

	CleanupDMX( pPlayerStats );

	UpdateStatSummaryPanel();

	// check file CRC and steam ID to see if we think this file has not been tampered with
	int iCRC = CalcCRC( iSteamID );
	// does file CRC match CRC generated from file data, and is there a Steam ID in the file
	if ( ( iCRC == iCRCFile ) && ( iSteamID > 0 ) ) 
	{		
		// does the file Steam ID match current Steam ID (so you can't hand around files)
		/*CSteamID steamID = SteamUser()->GetSteamID();
		if ( steamID.GetAccountID() == (uint32) iSteamID )
		{
			m_bLocalFileTrusted = true;
		}*/
	}

	m_bStatsChanged = false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Calcs CRC of all stat data
//-----------------------------------------------------------------------------
int CTFStatPanel::CalcCRC( int iSteamID )
{
	CRC32_t crc;
	CRC32_Init( &crc );
	
	// make a CRC of stat data
	CRC32_ProcessBuffer( &crc, &iSteamID, sizeof( iSteamID ) );

	for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass <= TF_LAST_NORMAL_CLASS; iClass++ )
	{
		// add each class' data to the CRC
		ClassStats_t &classStats = GetClassStats( iClass );
		CRC32_ProcessBuffer( &crc, &classStats, sizeof( classStats ) );
		// since the class data structure is highly guessable from the file, add one other thing to make the CRC hard to hack w/o code disassembly
		int iObfuscate = iClass * iClass;
		CRC32_ProcessBuffer( &crc, &iObfuscate, sizeof( iObfuscate ) );	
	}

	CRC32_Final( &crc );

	return (int) ( crc & 0x7FFFFFFF );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::ShowStatPanel( int iClass, int iTeam, int iCurStatValue, TFStatType_t statType, RecordBreakType_t recordBreakType,
								 bool bAlive )
{
	ClassStats_t &classStats = GetClassStats( iClass );
	vgui::Label *pLabel = dynamic_cast<Label *>( FindChildByName( "summaryLabel" ) );
	if ( !pLabel )
		return;

	const char *pRecordTextSuffix[RECORDBREAK_MAX] = { "", "close", "tie", "best" };

	const char *pLocalizedTitle = bAlive ? "#StatPanel_Title_Alive" : "#StatPanel_Title_Dead";
	SetDialogVariable( "title", g_pVGuiLocalize->Find( pLocalizedTitle ) );
	SetDialogVariable( "stattextlarge", "" );
	SetDialogVariable( "stattextsmall", "" );
	if ( recordBreakType == RECORDBREAK_CLOSE )
	{		
		// if we are displaying that the player got close to a record, show current & best values
		char szCur[32],szBest[32];
		wchar_t wzCur[32],wzBest[32];
		GetStatValueAsString( iCurStatValue, statType, szCur, ARRAYSIZE( szCur ) );
		GetStatValueAsString( classStats.max.m_iStat[statType], statType, szBest, ARRAYSIZE( szBest ) );
		g_pVGuiLocalize->ConvertANSIToUnicode( szCur, wzCur, sizeof( wzCur ) );
		g_pVGuiLocalize->ConvertANSIToUnicode( szBest, wzBest, sizeof( wzBest ) );
		wchar_t *wzFormat = g_pVGuiLocalize->Find( "#StatPanel_Format_Close" );
		wchar_t wzText[256];
		g_pVGuiLocalize->ConstructString( wzText, sizeof( wzText ), wzFormat, 2, wzCur, wzBest );
		SetDialogVariable( "stattextsmall", wzText );
	}
	else
	{
		// player broke or tied a record, just show current value
		char szValue[32];
		GetStatValueAsString( iCurStatValue, statType, szValue, ARRAYSIZE( szValue ) );
		SetDialogVariable( "stattextlarge", szValue );
	}

	SetDialogVariable( "statdesc", g_pVGuiLocalize->Find( CFmtStr( "%s_%s", g_szLocalizedRecordText[statType], 
		pRecordTextSuffix[recordBreakType] ) ) );

	// Set the class name. We can't use a dialog variable because it's a string that's already
	// been set using a dialog variable, and apparently we don't support nested dialog variables.
	wchar_t szOriginalSummary[ 256 ];
	wchar_t szSummary[ 256 ];

	// This is the field that "statdesc" completed for us
	pLabel->GetText( szOriginalSummary, sizeof( szOriginalSummary ) );
	const wchar_t *pszPlayerClass = L"undefined";

	if ( ( iClass >= TF_FIRST_NORMAL_CLASS ) && ( iClass <= TF_LAST_NORMAL_CLASS ) )
	{
		pszPlayerClass = g_pVGuiLocalize->Find( g_aPlayerClassNames[ iClass ] );
	}

	g_pVGuiLocalize->ConstructString( szSummary, sizeof( szSummary ), szOriginalSummary, 1, pszPlayerClass );

	pLabel->SetText( szSummary );

	if ( m_pClassImage )
	{
		m_pClassImage->SetClass( iTeam, iClass, 0 );
	}

	Show();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "player_spawn", pEventName ) == 0 )
	{
		Msg( "got player_spawn event\n" );
		int iUserID = event->GetInt( "userid" );
		if ( !C_TFPlayer::GetLocalTFPlayer() || ( C_TFPlayer::GetLocalTFPlayer()->GetUserID() != iUserID ) )
			return;

		// hide panel if we're currently showing it
		Hide();

		m_flTimeLastSpawn = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/StatPanel_Base.res" );

	vgui::Panel *pStatBox = FindChildByName("StatBox");
	if ( pStatBox )
	{
		// Dirty hack: Make the statbox update now, and then change its bgColor.
		// When it then gets ApplySchemeSetting called shortly after this, it doesn't
		// reapply the scheme because its dirty-scheme flag has been removed.
		pStatBox->ApplySchemeSettings( pScheme );
		pStatBox->SetBgColor( GetSchemeColor("TransparentLightBlack", pScheme) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::OnTick()
{
	// see if it's time to hide the panel
	if ( m_flTimeHide > 0 && gpGlobals->curtime > m_flTimeHide )
	{
		Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::Show()
{
	m_bShouldBeVisible = true;

	HideLowerPriorityHudElementsInGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::Hide()
{
	m_bShouldBeVisible = false;
	if ( m_flTimeHide > 0 )
	{
		m_flTimeHide = 0;
		vgui::ivgui()->RemoveTickSignal( GetVPanel() );
	}

	UnhideLowerPriorityHudElementsInGroup( "mid" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFStatPanel::ShouldDraw( void )
{
	if ( !m_bShouldBeVisible )
		return false;

	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: called when the local player object is being destroyed
//-----------------------------------------------------------------------------
void CTFStatPanel::OnLocalPlayerRemove( C_TFPlayer *pPlayer )
{
	// this handles the case of map change/server shutdown while player is still alive -- accumulate values for this life.
	if ( pPlayer->IsAlive() && g_AchievementMgrTF.CheckAchievementsEnabled() )
	{
		m_RoundStatsCurrentLife.m_iStat[TFSTAT_PLAYTIME] = gpGlobals->curtime - m_flTimeCurrentLifeStart;
		ClassStats_t &classStats = GetClassStats( m_iClassCurrentLife );
		classStats.AccumulateRound( m_RoundStatsCurrentLife );
		classStats.accumulated.m_iStat[TFSTAT_MAXSENTRYKILLS] = 0;	// sentry kills is a max value rather than a count, meaningless to accumulate
		m_bStatsChanged = true;
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::ClearStatsInMemory( void )
{
	m_aClassStats.RemoveAll();
	m_bStatsChanged = true;
	ResetDisplayedStat();
	UpdateStatSummaryPanel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFStatPanel::ResetStats( void )
{
	ClearStatsInMemory();

	WriteStats();
	g_TFSteamStats.UploadStats();
}

//-----------------------------------------------------------------------------
// Purpose: returns class stat struct for specified class
//-----------------------------------------------------------------------------
ClassStats_t &CTFStatPanel::GetClassStats( int iClass )
{
	Assert( statPanel );
	Assert( iClass >= TF_FIRST_NORMAL_CLASS );
	Assert( iClass <= TF_LAST_NORMAL_CLASS );
	int i;
	for( i = 0; i < statPanel->m_aClassStats.Count(); i++ )
	{
		if ( statPanel->m_aClassStats[i].iPlayerClass == iClass )
			return statPanel->m_aClassStats[i];
	}

	ClassStats_t stats;
	stats.iPlayerClass = iClass;
	statPanel->m_aClassStats.AddToTail( stats );
	return statPanel->m_aClassStats[statPanel->m_aClassStats.Count()-1];
}

//-----------------------------------------------------------------------------
// Purpose: Updates the stat summary panel w/current stats
//-----------------------------------------------------------------------------
void CTFStatPanel::UpdateStatSummaryPanel()
{
	GStatsSummaryPanel()->SetStats( m_aClassStats );
}

//-----------------------------------------------------------------------------
// Purpose: Renders stat value as string
//-----------------------------------------------------------------------------
void CTFStatPanel::GetStatValueAsString( int iValue, TFStatType_t statType, char *value, int valuelen )
{
	if ( TFSTAT_PLAYTIME == statType )
	{
		// Format time as a time string
		Q_strncpy( value, FormatSeconds( iValue ), valuelen );
	}
	else
	{
		// all other stats are just displayed as #'s
		Q_snprintf( value, valuelen, "%d", iValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when we get a stat update for the local player
//-----------------------------------------------------------------------------
void CTFStatPanel::MsgFunc_PlayerStatsUpdate( bf_read &msg )
{
	// get the fixed-size information
	int iClass = msg.ReadByte();
	int iMsgType = msg.ReadByte();
	int iSendBits = msg.ReadLong();

	bool bAlive = true;
	bool bSpawned = false;

	switch ( iMsgType )
	{
	case STATMSG_RESET:
		m_RoundStatsCurrentGame.Reset();
		m_RoundStatsLifeStart.Reset();
		return;
	case STATMSG_PLAYERSPAWN:
	case STATMSG_PLAYERRESPAWN:
		bSpawned = true;
		break;
	case STATMSG_PLAYERDEATH:
		bAlive = false;
		break;
	case STATMSG_UPDATE:
		break;
	default:
		Assert( false );
	}

	Assert( iClass >= TF_FIRST_NORMAL_CLASS && iClass <= TF_LAST_NORMAL_CLASS );
	if ( iClass < TF_FIRST_NORMAL_CLASS || iClass > TF_LAST_NORMAL_CLASS )
		return;
	
	m_iClassCurrentLife = iClass;
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pPlayer )
	{
		m_iTeamCurrentLife = pPlayer->GetTeamNumber();
	}

//	Msg( "Stat update: (msg %d) ", iMsgType );
	// the bitfield indicates which stats are contained in the message.  Set the stats appropriately.
	int iStat = TFSTAT_FIRST;
	while ( iSendBits > 0 )
	{
		if ( iSendBits & 1 )
		{	
			int iVal = msg.ReadLong();
//			Msg( "#%d=%d ", iStat, iVal );
			m_RoundStatsCurrentGame.m_iStat[iStat] = iVal;
		}
		iSendBits >>= 1;
		iStat++;
	}
//	Msg( "\n" );
	// Calculate stat values for current life.  Take current game stats and subtract what the values were at the start of this life
	for ( iStat = TFSTAT_FIRST; iStat < TFSTAT_MAX; iStat++ )
	{
		if ( iStat == TFSTAT_MAXSENTRYKILLS )
		{
			// max sentry kills is special, it is a max value.  Always use absolute value, do not use delta from earlier value.
			m_RoundStatsCurrentLife.m_iStat[TFSTAT_MAXSENTRYKILLS] = m_RoundStatsCurrentGame.m_iStat[TFSTAT_MAXSENTRYKILLS];
			continue;
		}
		int iDelta = m_RoundStatsCurrentGame.m_iStat[iStat] - m_RoundStatsLifeStart.m_iStat[iStat];
		Assert( iDelta >= 0 );
		m_RoundStatsCurrentLife.m_iStat[iStat] = iDelta;
	}

	if ( iMsgType == STATMSG_PLAYERDEATH || iMsgType == STATMSG_PLAYERRESPAWN )
	{
		m_RoundStatsCurrentLife.m_iStat[TFSTAT_PLAYTIME] = gpGlobals->curtime - m_flTimeCurrentLifeStart;
	}

	if ( bSpawned )
	{
		// if the player just spawned, use current stats as baseline to calculate stats for next life
		m_RoundStatsLifeStart = m_RoundStatsCurrentGame;
		m_flTimeCurrentLifeStart = gpGlobals->curtime;
	}

	// sanity check: the message should contain exactly the # of bytes we expect based on the bit field
	Assert( !msg.IsOverflowed() );
	Assert( 0 == msg.GetNumBytesLeft() );
	// if byte count isn't correct, bail out and don't use this data, rather than risk polluting player stats with garbage
	if ( msg.IsOverflowed() || ( 0 != msg.GetNumBytesLeft() ) )
		return;

	UpdateStats( iMsgType );
}

/**********************************************************************************/

void TestStatPanel( const CCommand &args )
{
	int iPanelType;

	if( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  teststatpanel < panel type > < optional record type >\n" );
		ConMsg( "Usable panel types are %d to %d. Record types are %d to %d\n", TFSTAT_UNDEFINED + 1, TFSTAT_MAX - 1, RECORDBREAK_NONE+1, RECORDBREAK_MAX-1 );
		return;
	}

	if ( statPanel )
	{
		iPanelType = atoi( args.Arg( 1 ) );
		int iRecordType = RECORDBREAK_BEST;

		if ( args.ArgC() >= 3 )
		{
			iRecordType = atoi( args.Arg( 2 ) );
		}

		if ( ( iPanelType <= TFSTAT_UNDEFINED ) || ( iPanelType >= TFSTAT_MAX ) || (iRecordType <= RECORDBREAK_NONE) || (iRecordType >= RECORDBREAK_MAX) )
		{
			ConMsg( "Usage:  teststatpanel < panel type > < optional record type >\n" );
			ConMsg( "Usable panel types are %d to %d. Record types are %d to %d\n", TFSTAT_UNDEFINED + 1, TFSTAT_MAX - 1, RECORDBREAK_NONE+1, RECORDBREAK_MAX-1 );
			return;
		}

		statPanel->TestStatPanel( (TFStatType_t) iPanelType, (RecordBreakType_t)iRecordType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void HideStatPanel()
{
	if ( statPanel )
	{
		statPanel->Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ResetPlayerStats()
{
	if ( statPanel )
	{
		statPanel->ResetStats();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RefreshPlayerStats()
{
	if ( statPanel )
	{
		if ( !statPanel->ReadStats() )
		{
			// Read failed, need to clear everything
			statPanel->ClearStatsInMemory();
		}
	}
}

ConCommand teststatpanel( "teststatpanel", TestStatPanel, "", FCVAR_DEVELOPMENTONLY );
ConCommand hidestatpanel( "hidestatpanel", HideStatPanel, "", FCVAR_DEVELOPMENTONLY );
ConCommand resetplayerstats( "resetplayerstats", ResetPlayerStats, "" );
ConCommand refreshplayerstats( "refreshplayerstats", RefreshPlayerStats, "", FCVAR_DEVELOPMENTONLY );
