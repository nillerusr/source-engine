#include "cbase.h"
#include "nb_campaign_mission_details.h"
#include "vgui_controls/Label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Campaign_Mission_Details::CNB_Campaign_Mission_Details( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pBackgroundInner = new vgui::Panel( this, "BackgroundInner" );
	m_pTitleBG = new vgui::Panel( this, "TitleBG" );
	m_pTitleBGBottom = new vgui::Panel( this, "TitleBGBottom" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pMissionName = new vgui::Label( this, "MissionName", "" );
	m_pMissionDescription = new vgui::Label( this, "MissionDescription", "" );
	// == MANAGED_MEMBER_CREATION_END ==
}

CNB_Campaign_Mission_Details::~CNB_Campaign_Mission_Details()
{

}

void CNB_Campaign_Mission_Details::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_campaign_mission_details.res" );
}

void CNB_Campaign_Mission_Details::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Campaign_Mission_Details::OnThink()
{
	BaseClass::OnThink();

	if ( m_pCurrentMission )
	{
		m_pMissionName->SetVisible( true );
		m_pMissionDescription->SetVisible( true );

		m_pMissionName->SetText( STRING( m_pCurrentMission->m_LocationDescription ) );
		m_pMissionDescription->SetText( STRING( m_pCurrentMission->m_Briefing ) );
	}
	else
	{
		m_pMissionName->SetVisible( false );
		m_pMissionDescription->SetVisible( false );
	}
}

void CNB_Campaign_Mission_Details::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}
