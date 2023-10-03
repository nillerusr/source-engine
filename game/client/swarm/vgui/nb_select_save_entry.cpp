#include "cbase.h"
#include "nb_select_save_entry.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_bitmapbutton.h"
#include "KeyValues.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "filesystem.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Save_Entry::CNB_Select_Save_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pImage = new CBitmapButton( this, "Image", "" );
	m_pName = new vgui::Label( this, "Name", "" );
	m_pCampaignName = new vgui::Label( this, "CampaignName", "" );
	m_pDate = new vgui::Label( this, "Date", "" );
	m_pMissionsCompleted = new vgui::Label( this, "MissionsCompleted", "" );
	m_pPlayers = new vgui::Label( this, "Players", "" );
	// == MANAGED_MEMBER_CREATION_END ==
}

CNB_Select_Save_Entry::~CNB_Select_Save_Entry()
{

}

void CNB_Select_Save_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_save_entry.res" );
}

void CNB_Select_Save_Entry::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Select_Save_Entry::OnThink()
{
	BaseClass::OnThink();
}






