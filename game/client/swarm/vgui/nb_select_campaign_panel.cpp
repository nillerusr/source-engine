#include "cbase.h"
#include "nb_select_campaign_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "nb_select_campaign_entry.h"
#include "nb_horiz_list.h"
#include "nb_header_footer.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "nb_button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Campaign_Panel::CNB_Select_Campaign_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHorizList = new CNB_Horiz_List( this, "HorizList" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );

	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
}

CNB_Select_Campaign_Panel::~CNB_Select_Campaign_Panel()
{

}

void CNB_Select_Campaign_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_campaign_panel.res" );

	m_pHorizList->m_pBackgroundImage->SetImage( "briefing/select_marine_list_bg" );
	m_pHorizList->m_pForegroundImage->SetImage( "briefing/horiz_list_fg" );
}

void CNB_Select_Campaign_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Select_Campaign_Panel::OnThink()
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

	int nCampaigns = pSource->GetNumCampaigns();
	for ( int i = 0; i < nCampaigns; i++ )
	{
		if ( m_pHorizList->m_Entries.Count() < i + 1 )
		{
			CNB_Select_Campaign_Entry *pEntry = new CNB_Select_Campaign_Entry( NULL, "Select_Campaign_Entry" );
			m_pHorizList->AddEntry( pEntry );
		}
		CNB_Select_Campaign_Entry *pEntry = dynamic_cast<CNB_Select_Campaign_Entry*>( m_pHorizList->m_Entries[i].Get() );
		if ( pEntry )
		{
			pEntry->m_nCampaignIndex = i;
		}
	}

	// empty out remaining slots
	for ( int i = nCampaigns; i < m_pHorizList->m_Entries.Count(); i++ )
	{
		CNB_Select_Campaign_Entry *pEntry = dynamic_cast<CNB_Select_Campaign_Entry*>( m_pHorizList->m_Entries[i].Get() );
		if ( pEntry )
		{
			pEntry->m_nCampaignIndex = -1;
		}
	}
}


void CNB_Select_Campaign_Panel::OnCommand( const char *command )
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

void CNB_Select_Campaign_Panel::CampaignSelected( ASW_Mission_Chooser_Mission *pMission )
{
	if ( !pMission || !pMission->m_szMissionName || pMission->m_szMissionName[0] == 0 )
		return;

	// pass selected mission name up to vgamesettings
	char buffer[ 256 ];
	Q_snprintf( buffer, sizeof( buffer ), "cmd_campaign_selected_%s", pMission->m_szMissionName );
	GetParent()->OnCommand( buffer );

	MarkForDeletion();
}