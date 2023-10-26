#include "cbase.h"
#include "nb_select_mission_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "nb_select_mission_entry.h"
#include "nb_horiz_list.h"
#include "nb_header_footer.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "nb_button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Mission_Panel::CNB_Select_Mission_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pHorizList = new CNB_Horiz_List( this, "HorizList" );	
	// == MANAGED_MEMBER_CREATION_END ==
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );
	
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( false );
	m_szCampaignFilter[0] = 0;
	m_nLastCount = -1;
}

CNB_Select_Mission_Panel::~CNB_Select_Mission_Panel()
{

}

void CNB_Select_Mission_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_mission_panel.res" );

	m_pHorizList->m_pBackgroundImage->SetImage( "briefing/select_marine_list_bg" );
	m_pHorizList->m_pForegroundImage->SetImage( "briefing/horiz_list_fg" );
}

void CNB_Select_Mission_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

bool CampaignContainsMission( KeyValues *pCampaignKeys, const char *szMissionName )
{
	char szMissionStripped[256];
	Q_StripExtension( szMissionName, szMissionStripped, sizeof( szMissionStripped ) );
	for ( KeyValues *pMissionKey = pCampaignKeys->GetFirstSubKey(); pMissionKey; pMissionKey = pMissionKey->GetNextKey() )
	{
		if ( !Q_stricmp( pMissionKey->GetName(), "MISSION" ) )
		{
			const char *szMapName = pMissionKey->GetString( "MapName", "none" );
			if ( !Q_stricmp( szMapName, szMissionStripped ) )
				return true;
		}
	}
	return false;
}

int GetMissionIndex( IASW_Mission_Chooser_Source *pSource, const char *szMapName )
{
	int nMissions = pSource->GetNumMissions( true );
	for ( int i = 0; i < nMissions; i++ )
	{
		ASW_Mission_Chooser_Mission* pMission = pSource->GetMission( i, true );
		char pTemp[64];
		Q_StripExtension( pMission->m_szMissionName, pTemp, sizeof(pTemp) );
		if ( !Q_stricmp( pTemp, szMapName ) )
		{
			return i;
		}
	}
	return -1;
}

void CNB_Select_Mission_Panel::OnThink()
{
	BaseClass::OnThink();

	IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;

	// TODO: If voting, then use:
	//IASW_Mission_Chooser_Source *pSource = GetVotingMissionSource();

	if ( !pSource )
	{
		//Warning( "Unable to select a mission as couldn't find an IASW_Mission_Chooser_Source\n" );
		return;
	}

	pSource->Think();

	KeyValues *pCampaignDetails = NULL;
	if ( m_szCampaignFilter[0] )
	{
		pCampaignDetails = pSource->GetCampaignDetails( m_szCampaignFilter );
	}
	int nCount = 0;
	if ( pCampaignDetails )
	{
		bool bSkippedFirst = false;
		for ( KeyValues *pMission = pCampaignDetails->GetFirstSubKey(); pMission; pMission = pMission->GetNextKey() )
		{
			if ( !Q_stricmp( pMission->GetName(), "MISSION" ) )
			{
				if ( !bSkippedFirst )
				{
					bSkippedFirst = true;
				}
				else
				{
					const char *szMapName = pMission->GetString( "MapName", "asi-jac1-landingbay01" );
					int nMissionIndex = GetMissionIndex( pSource, szMapName );
					if ( nMissionIndex == -1 )
						continue;
					
					if ( m_pHorizList->m_Entries.Count() < nCount + 1 )
					{
						CNB_Select_Mission_Entry *pEntry = new CNB_Select_Mission_Entry( NULL, "Select_Mission_Entry" );
						m_pHorizList->AddEntry( pEntry );
					}
					CNB_Select_Mission_Entry *pEntry = dynamic_cast<CNB_Select_Mission_Entry*>( m_pHorizList->m_Entries[nCount].Get() );
					if ( pEntry )
					{
						pEntry->m_nMissionIndex = nMissionIndex;
					}
					nCount++;
				}
			}
		}
	}
	else
	{
		int nMissions = pSource->GetNumMissions( true );
		for ( int i = 0; i < nMissions; i++ )
		{
			
			//ASW_Mission_Chooser_Mission* pMission = pSource->GetMission( i, true );
			//if ( pMission && pCampaignKeys && !CampaignContainsMission( pCampaignKeys, pMission->m_szMissionName ) )
				//continue;
			if ( m_pHorizList->m_Entries.Count() < nCount + 1 )
			{
				CNB_Select_Mission_Entry *pEntry = new CNB_Select_Mission_Entry( NULL, "Select_Mission_Entry" );
				m_pHorizList->AddEntry( pEntry );
			}
			CNB_Select_Mission_Entry *pEntry = dynamic_cast<CNB_Select_Mission_Entry*>( m_pHorizList->m_Entries[nCount].Get() );
			if ( pEntry )
			{
				pEntry->m_nMissionIndex = i;
			}
			nCount++;
		}
	}

	// empty out remaining slots
	for ( int i = nCount; i < m_pHorizList->m_Entries.Count(); i++ )
	{
		CNB_Select_Mission_Entry *pEntry = dynamic_cast<CNB_Select_Mission_Entry*>( m_pHorizList->m_Entries[i].Get() );
		if ( pEntry )
		{
			pEntry->m_nMissionIndex = -1;
		}
	}
	if ( nCount != m_nLastCount )
	{
		m_nLastCount = nCount;
		InvalidateLayout( true, true );
	}
}

void CNB_Select_Mission_Panel::InitList()
{
	if ( m_szCampaignFilter[0] )
	{
		m_pTitle->SetText( "#nb_select_starting_mission" );
	}
	else
	{
		m_pTitle->SetText( "#nb_select_mission" );
	}
}

void CNB_Select_Mission_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		MarkForDeletion();
		return;
	}
	else if ( !Q_stricmp( command, "AcceptButton" ) )
	{

		GetParent()->OnCommand( command );
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Select_Mission_Panel::MissionSelected( ASW_Mission_Chooser_Mission *pMission )
{
	if ( !pMission || !pMission->m_szMissionName || pMission->m_szMissionName[0] == 0 )
		return;

	// pass selected mission name up to vgamesettings
	char buffer[ 256 ];
	Q_snprintf( buffer, sizeof( buffer ), "cmd_mission_selected_%s", pMission->m_szMissionName );
	GetParent()->OnCommand( buffer );

	MarkForDeletion();
}

void CNB_Select_Mission_Panel::SelectMissionsFromCampaign( const char *szCampaignName )
{
	if ( !szCampaignName )
		return;

	Q_snprintf( m_szCampaignFilter, sizeof( m_szCampaignFilter ), "%s", szCampaignName );
}