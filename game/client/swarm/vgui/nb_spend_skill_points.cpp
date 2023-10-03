#include "cbase.h"
#include "nb_spend_skill_points.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "nb_select_marine_entry.h"
#include "asw_marine_profile.h"
#include "nb_skill_panel.h"
#include "asw_briefing.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "vgui_bitmapbutton.h"
#include "c_user_message_register.h"
#include "skillanimpanel.h"
#include "clientmode_shared.h"
#include "nb_main_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Spend_Skill_Points::CNB_Spend_Skill_Points( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pSkillBackground = new CNB_Gradient_Bar( this, "SkillBackground" );
	m_pMarineBackground = new CNB_Gradient_Bar( this, "MarineBackground" );
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pMarineNameLabel = new vgui::Label( this, "MarineNameLabel", "" );
	m_pBioTitle = new vgui::Label( this, "BioTitle", "" );
	m_pBioLabel = new vgui::Label( this, "BioLabel", "" );
	m_pSkillTitle = new vgui::Label( this, "SkillTitle", "" );
	m_pSkillDescription = new vgui::Label( this, "SkillDescription", "" );		
	m_pAcceptButton = new vgui::Button( this, "AcceptButton", "", this, "AcceptButton" );
	m_pSelectedMarine = new vgui::ImagePanel( this, "SelectedMarine" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pSpareSkillPointsTitle = new vgui::Label( this, "SpareSkillPointsTitle", "" );
	m_pSpareSkillPointsLabel = new vgui::Label( this, "SpareSkillPointsLabel", "" );
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );

	m_pUndoButton = new CBitmapButton( this, "UndoButton", "" );
	m_pUndoButton->AddActionSignalTarget( this );
	m_pUndoButton->SetCommand( "SkillUndo" );

	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );

	m_pSkillPanel[ 0 ] = new CNB_Skill_Panel_Spending( this, "SkillPanel0" );
	m_pSkillPanel[ 1 ] = new CNB_Skill_Panel_Spending( this, "SkillPanel1" );
	m_pSkillPanel[ 2 ] = new CNB_Skill_Panel_Spending( this, "SkillPanel2" );
	m_pSkillPanel[ 3 ] = new CNB_Skill_Panel_Spending( this, "SkillPanel3" );
	m_pSkillPanel[ 4 ] = new CNB_Skill_Panel_Spending( this, "SkillPanel4" );

	m_pSkillAnimPanel = new SkillAnimPanel( this, "SkillAnimPanel" );
	m_pSkillAnimPanel->SetZPos( 50 );

	m_nProfileIndex = -1;
}

CNB_Spend_Skill_Points::~CNB_Spend_Skill_Points()
{

}

void CNB_Spend_Skill_Points::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_spend_skill_points.res" );

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	color32 dull;
	dull.r = 192;
	dull.g = 192;
	dull.b = 192;
	dull.a = 255;

	m_pUndoButton->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/Computer/TumblerSwitchDirection", white );
	m_pUndoButton->SetImage( CBitmapButton::BUTTON_DISABLED, "vgui/swarm/Computer/TumblerSwitchDirection", dull );
	m_pUndoButton->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/swarm/Computer/TumblerSwitchDirection", white );		
	m_pUndoButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/swarm/Computer/TumblerSwitchDirection_over", white );
}

void CNB_Spend_Skill_Points::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Spend_Skill_Points::Init()
{
	
}

void CNB_Spend_Skill_Points::OnThink()
{
	BaseClass::OnThink();

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( m_nProfileIndex );
	if ( pProfile )
	{
		char imagename[255];
		Q_snprintf( imagename, sizeof(imagename), "briefing/face_%s", pProfile->m_PortraitName );
		m_pSelectedMarine->SetImage( imagename );

		m_pMarineNameLabel->SetText( pProfile->GetShortName() );
		m_pBioLabel->SetText( pProfile->m_Bio );
		int nMouseOverSkill = -1;
		for ( int i = 0; i < NUM_SKILL_PANELS; i++ )
		{
			m_pSkillPanel[ i ]->SetSkillDetails( m_nProfileIndex, i, Briefing()->GetProfileSkillPoints( m_nProfileIndex, i ) );
			m_pSkillPanel[ i ]->m_bSpendPointsMode = true;
			if ( m_pSkillPanel[ i ]->IsCursorOver() )
			{
				nMouseOverSkill = i;
			}
		}	
		if ( nMouseOverSkill != -1 && MarineSkills() )
		{
			ASW_Skill nSkillIndex = pProfile->GetSkillMapping( nMouseOverSkill );		// translate from skill slot to skill index

			m_pSkillTitle->SetText( MarineSkills()->GetSkillName( nSkillIndex ) );
			m_pSkillDescription->SetText( MarineSkills()->GetSkillDescription( nSkillIndex ) );
		}
		m_pSpareSkillPointsLabel->SetText( VarArgs( "%d", Briefing()->GetProfileSkillPoints( m_nProfileIndex, ASW_SKILL_SLOT_SPARE ) ) );
	}
}

void CNB_Spend_Skill_Points::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		CNB_Main_Panel::RemoveFromSpendQueue( m_nProfileIndex );
		MarkForDeletion();
		Briefing()->SetChangingWeaponSlot( 0 );

		CNB_Main_Panel *pMainPanel = dynamic_cast<CNB_Main_Panel*>( GetParent() );
		if ( pMainPanel )
		{
			pMainPanel->OnFinishedSpendingSkillPoints();
		}
		return;
	}
	else if ( !Q_stricmp( command, "SkillUndo" ) )
	{
		char buffer[16];
		Q_snprintf( buffer, sizeof(buffer), "cl_undoskill %d", m_nProfileIndex );
		engine->ClientCmd(buffer);
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button4" );
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Spend_Skill_Points::DoSpendPointAnimation( int nProfileIndex, int nSkillSlot )
{
	if ( nProfileIndex != m_nProfileIndex || nSkillSlot < 0 || nSkillSlot >= NUM_SKILL_PANELS )
		return;

	m_pSkillAnimPanel->AddParticlesAroundPanel( m_pSkillPanel[ nSkillSlot ]->m_pSkillButton );
}

void __MsgFunc_ASWSkillSpent( bf_read &msg )
{
	int iRosterIndex = msg.ReadByte();
	int nSkillSlot = msg.ReadByte();
	
	CNB_Spend_Skill_Points *pSpendPanel = dynamic_cast<CNB_Spend_Skill_Points*>( GetClientMode()->GetViewport()->FindChildByName( "Spend_Skill_Points", true ) );
	if ( !pSpendPanel )
	{
		//Msg( "ASWSkillSpent: Couldn't find CNB_Spend_Skill_Points\n" );
		return;
	}
	pSpendPanel->DoSpendPointAnimation( iRosterIndex, nSkillSlot );
}
USER_MESSAGE_REGISTER( ASWSkillSpent );