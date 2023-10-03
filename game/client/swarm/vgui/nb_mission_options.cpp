#include "cbase.h"
#include "nb_mission_options.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Button.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_player.h"
#include "nb_header_footer.h"
#include "nb_button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Mission_Options::CNB_Mission_Options( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pDifficultyUpButton = new vgui::Button( this, "DifficultyUpButton", "", this, "DifficultyUpButton" );
	m_pDifficultyDownButton = new vgui::Button( this, "DifficultyDownButton", "", this, "DifficultyDownButton" );
	m_pDifficultyTitle = new vgui::Label( this, "DifficultyTitle", "" );
	m_pSkillLevelLabel = new vgui::Label( this, "SkillLevelLabel", "" );
	m_pSkillDescriptionLabel = new vgui::Label( this, "SkillDescriptionLabel", "" );
	m_pStyleTitle = new vgui::Label( this, "StyleTitle", "" );
	m_pStyleLabel = new vgui::Label( this, "StyleLabel", "" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );
	
	m_pHeaderFooter->SetTitle( "#nb_mission_settings" );

	m_pGameStyle[0] = new vgui::Button( this, "GameStyleButton0", "", this, "GameStyleButton0" );
	m_pGameStyle[1] = new vgui::Button( this, "GameStyleButton1", "", this, "GameStyleButton1" );
	m_pGameStyle[2] = new vgui::Button( this, "GameStyleButton2", "", this, "GameStyleButton2" );
	m_pGameStyle[3] = new vgui::Button( this, "GameStyleButton3", "", this, "GameStyleButton3" );

	m_iLastSkillLevel = -1;
}

CNB_Mission_Options::~CNB_Mission_Options()
{

}

void CNB_Mission_Options::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_mission_options.res" );
}

void CNB_Mission_Options::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Mission_Options::OnThink()
{
	BaseClass::OnThink();

	if (!ASWGameRules())
		return;

	// check the label reflects the actual difficulty of the game rules
	if (m_iLastSkillLevel != ASWGameRules()->GetSkillLevel())
	{
		m_iLastSkillLevel = ASWGameRules()->GetSkillLevel();
		if (m_iLastSkillLevel == 4)
			m_pSkillLevelLabel->SetText("#asw_difficulty_insane");
		else if (m_iLastSkillLevel == 3)
			m_pSkillLevelLabel->SetText("#asw_difficulty_hard");
		else if (m_iLastSkillLevel == 1)
			m_pSkillLevelLabel->SetText("#asw_difficulty_easy");
		else if (m_iLastSkillLevel == 5)
			m_pSkillLevelLabel->SetText("#asw_difficulty_imba");
		else 
			m_pSkillLevelLabel->SetText("#asw_difficulty_normal");

		// update description label
		switch (m_iLastSkillLevel)
		{
		case 1: m_pSkillDescriptionLabel->SetText("#asw_difficulty_chooser_easyd"); break;
		case 2: m_pSkillDescriptionLabel->SetText("#asw_difficulty_chooser_normald"); break;
		case 3: m_pSkillDescriptionLabel->SetText("#asw_difficulty_chooser_hardd"); break;
		case 4: m_pSkillDescriptionLabel->SetText("#asw_difficulty_chooser_insaned"); break;
		case 5: m_pSkillDescriptionLabel->SetText("#asw_difficulty_chooser_imbad"); break;
		default: m_pSkillDescriptionLabel->SetText("???"); break;
		}

	}

	if (!ASWGameRules() || !ASWGameResource())
		return;

	int iLeaderIndex = ASWGameResource()->GetLeaderEntIndex();
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	bool bLeader = (pPlayer && (pPlayer->entindex() == iLeaderIndex));
	m_pDifficultyUpButton->SetVisible( bLeader );
	m_pDifficultyDownButton->SetVisible( bLeader );

	if ( ASWGameRules()->IsCarnageMode() )
	{
		m_pStyleLabel->SetText( "#asw_difficulty_carnage" );
	}
	else if ( ASWGameRules()->IsUberMode() )
	{
		m_pStyleLabel->SetText( "#asw_difficulty_uber" );
	}
	else if ( ASWGameRules()->IsHardcoreMode() )
	{
		m_pStyleLabel->SetText( "#asw_difficulty_hardcore" );
	}
	else
	{
		m_pStyleLabel->SetText( "#asw_difficulty_normal" );
	}

	// update visible gamemodes
	m_pGameStyle[0]->SetVisible(true);
// 	m_pGameStyle[1]->SetVisible((ASWGameRules()->GetUnlockedModes() & ASW_SM_CARNAGE) > 0);
// 	m_pGameStyle[2]->SetVisible((ASWGameRules()->GetUnlockedModes() & ASW_SM_UBER) > 0);
// 	m_pGameStyle[3]->SetVisible((ASWGameRules()->GetUnlockedModes() & ASW_SM_HARDCORE) > 0);	

	if ( bLeader )
	{
		m_pGameStyle[0]->SetEnabled(true);
		m_pGameStyle[1]->SetEnabled((ASWGameRules()->GetUnlockedModes() & ASW_SM_CARNAGE) > 0);
		m_pGameStyle[2]->SetEnabled((ASWGameRules()->GetUnlockedModes() & ASW_SM_UBER) > 0);
		m_pGameStyle[3]->SetEnabled((ASWGameRules()->GetUnlockedModes() & ASW_SM_HARDCORE) > 0);
	}
	else
	{
		for ( int i = 0; i < ASW_NUM_GAME_STYLES; i++ )
		{
			m_pGameStyle[1]->SetEnabled( false );
		}
	}
}

void CNB_Mission_Options::OnCommand(char const* command)
{
	if (!stricmp(command, "DifficultyUpButton"))
	{
		if (C_ASW_Player::GetLocalASWPlayer())
		{
			C_ASW_Player::GetLocalASWPlayer()->RequestSkillUp();
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
		}
	}
	else if (!stricmp(command, "DifficultyDownButton"))
	{
		if (C_ASW_Player::GetLocalASWPlayer())
		{
			C_ASW_Player::GetLocalASWPlayer()->RequestSkillDown();
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
		}
	}
	else if (!stricmp(command, "GameStyleButton0"))		// normal
	{		
		Msg("activating normal mode\n");
		engine->ClientCmd("cl_carnage 0; cl_uber 0; cl_hardcore 0");
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
	}
	else if (!stricmp(command, "GameStyleButton1"))	// carnage
	{
		Msg("activating carnage mode\n");
		engine->ClientCmd("cl_carnage 1; cl_uber 0; cl_hardcore 0");
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
	}
	else if (!stricmp(command, "GameStyleButton2"))	// uber
	{		
		Msg("activating uber mode\n");
		engine->ClientCmd("cl_carnage 0; cl_uber 1; cl_hardcore 0");
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
	}
	else if (!stricmp(command, "GameStyleButton3"))	// hardcore
	{		
		Msg("activating hardcore mode\n");
		engine->ClientCmd("cl_carnage 0; cl_uber 0; cl_hardcore 1");
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
	}
	else if ( !Q_stricmp( command, "BackButton" ) )
	{
		MarkForDeletion();
		return;
	}
}