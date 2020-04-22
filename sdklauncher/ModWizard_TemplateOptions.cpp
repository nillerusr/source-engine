//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "ModWizard_TemplateOptions.h"
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui/IInput.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/WizardPanel.h>
#include <vgui_controls/CheckButton.h>
#include "sdklauncher_main.h"
#include <ctype.h>
#include "CreateModWizard.h"
#include "ModWizard_GetModInfo.h"
#include "ModWizard_CopyFiles.h"


using namespace vgui;

// If the option is disabled, throw this on the front of it. 
#define OPTION_DISABLED_PREFIX	"//"

// The option strings for the replace
static const char *g_szOptions[] =
{
	"#define SDK_USE_TEAMS",				//TPOPTION_TEAMS,
	"#define SDK_USE_PLAYERCLASSES",		//TPOPTION_CLASSES,
	"#define SDK_USE_STAMINA",				//TPOPTION_STAMINA,
	"#define SDK_USE_SPRINTING",			//TPOPTION_SPRINTING,
	"#define SDK_USE_PRONE",				//TPOPTION_PRONE,
	"#define SDK_SHOOT_WHILE_SPRINTING",	//TPOPTION_SHOOTSPRINTING,
	"#define SDK_SHOOT_ON_LADDERS",			//TPOPTION_SHOOTLADDERS,
	"#define SDK_SHOOT_WHILE_JUMPING",		//TPOPTION_SHOOTJUMPING,
};
char *VarArgs( PRINTF_FORMAT_STRING const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	Q_vsnprintf(string, sizeof(string), format,argptr);
	va_end (argptr);

	return string;	
}

CModWizardSubPanel_TemplateOptions::CModWizardSubPanel_TemplateOptions( Panel *parent, const char *panelName )
: BaseClass( parent, panelName )
{
	m_pOptionTeams = new CheckButton(this, "OptionTeamsButton", "" );
	m_pOptionClasses = new CheckButton( this, "OptionClassesButton", "" );
	m_pOptionStamina = new CheckButton( this, "OptionStaminaButton", "" );
	m_pOptionSprinting = new CheckButton( this, "OptionSprintingButton", "" );
	m_pOptionProne = new CheckButton(this, "OptionProneButton", "" );
	m_pOptionShootSprinting = new CheckButton( this, "OptionShootSprintingButton", "" );
	m_pOptionShootOnLadders = new CheckButton( this, "OptionShootLaddersButton", "" );
	m_pOptionShootJumping = new CheckButton( this, "OptionShootJumpingButton", "" );

	LoadControlSettings( "ModWizardSubPanel_TemplateOptions.res");

	// Sets everything off
	SetDefaultOptions();

}
void CModWizardSubPanel_TemplateOptions::SetDefaultOptions()
{
	// default everything to off.
	for ( int i = 0;i < TPOPTIONS_TOTAL; i++ )
		m_bOptions[i] = false;

	if ( m_pOptionTeams )
		m_pOptionTeams->SetSelected( false );
	if ( m_pOptionClasses )
		m_pOptionClasses->SetSelected( false );
	if ( m_pOptionStamina )
		m_pOptionStamina->SetSelected( false );
	if ( m_pOptionSprinting )
		m_pOptionSprinting->SetSelected( false );
	if ( m_pOptionProne )
		m_pOptionProne->SetSelected( false );
	if ( m_pOptionShootSprinting )
		m_pOptionShootSprinting->SetSelected( false );
	if ( m_pOptionShootOnLadders )
		m_pOptionShootOnLadders->SetSelected( false );
	if ( m_pOptionShootJumping )
		m_pOptionShootJumping->SetSelected( false );
}


char *CModWizardSubPanel_TemplateOptions::GetOption( int optionType )
{
	// Option is enabled, just pass the string back unaltered.
	if ( true == m_bOptions[optionType] )
	{
		return (char*)g_szOptions[optionType];
	}
	else
	{
		// Option is disabled, tack the comment '//' infront of the string, must strdup this
		return strdup(VarArgs( "//%s", g_szOptions[optionType] ));
	}
}

// Update all the options based on the check boxes.
void CModWizardSubPanel_TemplateOptions::UpdateOptions()
{
	if ( m_pOptionTeams && m_pOptionTeams->IsSelected() )
		m_bOptions[TPOPTION_TEAMS] = true;

	if ( m_pOptionClasses && m_pOptionClasses->IsSelected() )
		m_bOptions[TPOPTION_CLASSES] = true;

	if ( m_pOptionStamina && m_pOptionStamina->IsSelected() )
		m_bOptions[TPOPTION_STAMINA] = true;

	// if sprinting is enabled, so is stamina no matter what.
	if ( m_pOptionSprinting && m_pOptionSprinting->IsSelected() )
	{
		m_bOptions[TPOPTION_SPRINTING] = true;
		m_bOptions[TPOPTION_STAMINA] = true;
	}

	if ( m_pOptionProne && m_pOptionProne->IsSelected() )
		m_bOptions[TPOPTION_PRONE] = true;

	if ( m_pOptionShootSprinting && m_pOptionShootSprinting->IsSelected() )
		m_bOptions[TPOPTION_SHOOTSPRINTING] = true;


	if ( m_pOptionShootOnLadders && m_pOptionShootOnLadders->IsSelected() )
		m_bOptions[TPOPTION_SHOOTLADDERS] = true;

	if ( m_pOptionShootJumping && m_pOptionShootJumping->IsSelected() )
		m_bOptions[TPOPTION_SHOOTJUMPING] = true;
}

WizardSubPanel *CModWizardSubPanel_TemplateOptions::GetNextSubPanel()
{
	return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_CopyFiles"));
}

void CModWizardSubPanel_TemplateOptions::PerformLayout()
{
	BaseClass::PerformLayout();
	GetWizardPanel()->SetFinishButtonEnabled(false);
}

void CModWizardSubPanel_TemplateOptions::OnDisplayAsNext()
{
	GetWizardPanel()->SetTitle( "#ModWizard_TemplateOptions_Title", true );
}


bool CModWizardSubPanel_TemplateOptions::OnNextButton()
{

	// Update all the options
	UpdateOptions();
	// nothing else needs to be done here

	return true;
}

void CModWizardSubPanel_TemplateOptions::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}