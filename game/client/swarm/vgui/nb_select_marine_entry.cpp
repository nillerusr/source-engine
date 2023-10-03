#include "cbase.h"
#include "nb_select_marine_entry.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "asw_briefing.h"
#include "asw_marine_profile.h"
#include "nb_horiz_list.h"
#include "nb_select_marine_panel.h"
#include "vgui_bitmapbutton.h"
#include "nb_main_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Marine_Entry::CNB_Select_Marine_Entry( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pClassImage = new vgui::ImagePanel( this, "ClassImage" );
	m_pClassLabel = new vgui::Label( this, "ClassLabel", "" );
	m_pNameLabel = new vgui::Label( this, "NameLabel", "" );
	m_pPlayerNameLabel = new vgui::Label( this, "PlayerNameLabel", "" );
	// == MANAGED_MEMBER_CREATION_END ==

	//m_pPortraitImage = new CNB_Button( this, "PortraitImage", "" );
	m_pPortraitImage = new CBitmapButton( this, "PortraitImage", "" );
	m_nProfileIndex = -1;
	m_nPreferredLobbySlot = -1;

	m_pPortraitImage->AddActionSignalTarget( this );
	m_pPortraitImage->SetCommand( "AcceptButton" );
	//m_pPortraitImage->SetDoublePressedCommand( "AcceptButton" );

	m_szLastImageName[0] = 0;
	m_bSpendingSkillPointsMode = false;
}

CNB_Select_Marine_Entry::~CNB_Select_Marine_Entry()
{

}

void CNB_Select_Marine_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_szLastImageName[0] = 0;

	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_marine_entry.res" );
}

void CNB_Select_Marine_Entry::PerformLayout()
{
	BaseClass::PerformLayout();	
}

void CNB_Select_Marine_Entry::OnThink()
{
	BaseClass::OnThink();

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( m_nProfileIndex );
	if ( !pProfile )
		return;

	ASW_Marine_Class nMarineClass = pProfile->GetMarineClass();
	switch( nMarineClass )
	{
	case MARINE_CLASS_NCO: m_pClassLabel->SetText( "#marine_class_officer" ); m_pClassImage->SetImage( "swarm/ClassIcons/NCOClassIcon" ); break;
	case MARINE_CLASS_SPECIAL_WEAPONS: m_pClassLabel->SetText( "#marine_class_sw" ); m_pClassImage->SetImage( "swarm/ClassIcons/SpecialWeaponsClassIcon" ); break;
	case MARINE_CLASS_MEDIC: m_pClassLabel->SetText( "#marine_class_medic" ); m_pClassImage->SetImage( "swarm/ClassIcons/MedicClassIcon" ); break;
	case MARINE_CLASS_TECH: m_pClassLabel->SetText( "#marine_class_tech" ); m_pClassImage->SetImage( "swarm/ClassIcons/TechClassIcon" ); break;
	default: m_pClassLabel->SetText( "" ); break;
	}
	m_pNameLabel->SetText( pProfile->GetShortName() );

	// assumes parent's parent is the horiz list!
	//CNB_Horiz_List *pList = dynamic_cast<CNB_Horiz_List*>( GetParent()->GetParent() );
	//bool bHighlight = ( pList && pList->GetHighlightedEntry() == this );

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	color32 grey;
	grey.r = 128;
	grey.g = 128;
	grey.b = 128;
	grey.a = 128;

	char imagename[255];
	Q_snprintf( imagename, sizeof(imagename), "vgui/briefing/face_%s", pProfile->m_PortraitName );
	if ( Q_strcmp( m_szLastImageName, imagename ) )
	{
		Q_snprintf( m_szLastImageName, sizeof( m_szLastImageName ), "%s", imagename );

		m_pPortraitImage->SetImage( CBitmapButton::BUTTON_ENABLED, imagename, white );
		m_pPortraitImage->SetImage( CBitmapButton::BUTTON_DISABLED, imagename, grey );
		Q_snprintf( imagename, sizeof(imagename), "vgui/briefing/face_%s_lit", pProfile->m_PortraitName );
		m_pPortraitImage->SetImage( CBitmapButton::BUTTON_PRESSED, imagename, white );		
		m_pPortraitImage->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, imagename, white );
	}
	bool bEnabled = !m_bSpendingSkillPointsMode;
	if ( Briefing()->IsProfileSelected( m_nProfileIndex ) )
	{
		if ( m_nPreferredLobbySlot == -1 )
		{
			bEnabled = false;
		}
		else
		{
			CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfile( m_nPreferredLobbySlot );
			if ( !pProfile || pProfile->m_ProfileIndex != m_nProfileIndex )
			{
				bEnabled = false;
			}
		}
	}
	
	m_pPortraitImage->SetEnabled( bEnabled );
	m_pPlayerNameLabel->SetVisible( !m_bSpendingSkillPointsMode );
	m_pPlayerNameLabel->SetText( Briefing()->GetPlayerNameForMarineProfile( m_nProfileIndex ) );
}

void CNB_Select_Marine_Entry::OnCommand( const char *command )
{
	if ( !m_bSpendingSkillPointsMode )
	{
		if ( !Q_stricmp( command, "ChangeMarine" ) )
		{
			CNB_Select_Marine_Panel* pParent = dynamic_cast<CNB_Select_Marine_Panel*>( GetParent() );
			pParent->SetHighlight( m_nProfileIndex );
			return;
		}
		else if ( !Q_stricmp( command, "AcceptButton" ) )
		{
			CNB_Select_Marine_Panel* pParent = dynamic_cast<CNB_Select_Marine_Panel*>( GetParent() );
			pParent->OnCommand( command );
			return;
		}
	}
	BaseClass::OnCommand( command );
}