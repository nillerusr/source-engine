#include "cbase.h"
#include "ForceReadyPanel.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextImage.h>
#include "vgui_controls/frame.h"
#include "iclientmode.h"
#include "vgui_controls\Label.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "controller_focus.h"
#include "WrappedLabel.h"
#include "ImageButton.h"
#include "BriefingTooltip.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "asw_gamerules.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

vgui::DHANDLE<vgui::Panel> g_hPopupDialog;

ForceReadyPanel::ForceReadyPanel(vgui::Panel *parent, const char *name, const char *message, int iForceReadyType) : vgui::Panel(parent, name)
{	
	SetProportional( true );
	if (g_hPopupDialog.Get())
	{
		g_hPopupDialog->MarkForDeletion();
		g_hPopupDialog->SetVisible(false);
		g_hPopupDialog = NULL;
	}
	g_hPopupDialog = this;
	if (g_hBriefingTooltip.Get())
		g_hBriefingTooltip->SetTooltipsEnabled(false);

	m_pGradientBar = new CNB_Gradient_Bar( this, "GradientBar" );
	m_iForceReadyType = iForceReadyType;

	m_pMessage = new vgui::WrappedLabel(this, "Message", message);

	wchar_t wszReadyText[ 64 ];
	g_pVGuiLocalize->ConstructString( wszReadyText, sizeof( wszReadyText ), g_pVGuiLocalize->Find( "#asw_force_ready_delayed" ), 1, L"30" );

	m_pForceButton = new CNB_Button(this, "ForceButton", wszReadyText );
	m_pForceButton->AddActionSignalTarget(this);	
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "ForceButton");
	m_pForceButton->SetCommand(msg);

	m_pCancelButton = new CNB_Button(this, "CancelButton", "#asw_force_wait");
	m_pCancelButton->AddActionSignalTarget(this);	
	KeyValues *msg2 = new KeyValues("Command");	
	msg2->SetString("command", "CancelButton");
	m_pCancelButton->SetCommand(msg2);

	m_pForceButton->SetContentAlignment(vgui::Label::a_center);
	m_pForceButton->SetEnabled(true);

	m_pCancelButton->SetContentAlignment(vgui::Label::a_center);
	m_pCancelButton->SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pForceButton, false, true);
		GetControllerFocus()->AddToFocusList(m_pCancelButton, false, true);
		GetControllerFocus()->SetFocusPanel(m_pCancelButton, false);
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );	
}

ForceReadyPanel::~ForceReadyPanel()
{
	if (g_hBriefingTooltip.Get())
		g_hBriefingTooltip->SetTooltipsEnabled(true);

	if (g_hPopupDialog.Get() == this)
	{
		g_hPopupDialog = NULL;
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pForceButton);
		GetControllerFocus()->RemoveFromFocusList(m_pCancelButton);

		//GetControllerFocus()->SetFocusPanel(m_pCancelButton, false);
	}
}

void ForceReadyPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pMessage->SetContentAlignment(vgui::Label::a_northwest);
	m_pMessage->SetFont( pScheme->GetFont( "Default", true ) );
	m_pMessage->SetFgColor(pScheme->GetColor("LightBlue", Color(255,255,255,255)));	

	m_pForceButton->SetFont( pScheme->GetFont( "DefaultMedium", IsProportional() ) );
	m_pForceButton->SetAllCaps( true );
	m_pCancelButton->SetFont( pScheme->GetFont( "DefaultMedium", IsProportional() ) );
	m_pCancelButton->SetAllCaps( true );

	SetBgColor(Color(0,0,0,220));
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
}

void ForceReadyPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds(0, 0, ScreenWidth(), ScreenHeight());

	int text_wide = ScreenWidth() * 0.6f;
	m_pMessage->SetBounds(0, 0, text_wide, ScreenHeight());
	m_pMessage->InvalidateLayout(true);

	int w, t;
	m_pMessage->GetTextImage()->GetContentSize(w, t);
	int text_left = ScreenWidth() * 0.5f - w * 0.5f;
	int text_top = ScreenHeight() * 0.5f - t * 0.5f;
	m_pMessage->SetBounds(text_left, text_top, text_wide, ScreenHeight());

	int button_padding = YRES( 5 );
	int top_padding = YRES( 20 );
	int bottom_padding = YRES( 5 );
	int button_wide = YRES( 117 );
	int button_height = YRES( 27 );
	int button_top = text_top + t + YRES( 15 );
	m_pGradientBar->SetBounds( 0, text_top - top_padding, ScreenWidth() + 1, ( button_top + bottom_padding + button_height ) - ( text_top - top_padding ) );
	
	m_pForceButton->SetBounds(ScreenWidth() * 0.5f - (button_wide + button_padding), button_top, button_wide, button_height);
	m_pCancelButton->SetBounds(ScreenWidth() * 0.5f + button_padding, button_top, button_wide, button_height);
}

void ForceReadyPanel::OnThink()
{
	BaseClass::OnThink();

	int nTimeTillForceReady = clamp( static_cast<int>( ( ASWGameRules()->m_fBriefingStartedTime + 30.0f ) - gpGlobals->curtime ), 0, 30 );

	if ( nTimeTillForceReady == 0 )
	{
		m_pForceButton->SetText( "#asw_force_ready" );
		m_pForceButton->SetEnabled( true );
	}
	else
	{
		wchar_t wszNum[ 6 ];
		V_snwprintf( wszNum, sizeof( wszNum ), L"%i", nTimeTillForceReady );

		wchar_t wszReadyText[ 64 ];
		g_pVGuiLocalize->ConstructString( wszReadyText, sizeof( wszReadyText ), g_pVGuiLocalize->Find( "#asw_force_ready_delayed" ), 1, wszNum );

		m_pForceButton->SetText( wszReadyText );
		m_pForceButton->SetEnabled( false );
	}
}

void ForceReadyPanel::OnCommand(char const* command)
{
	//Msg("BriefingOptionsPanel row got command: %s\n", command);
	if (!strcmp(command, "CancelButton"))
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
		MarkForDeletion();
		SetVisible(false);
		return;
	}
	else if (!strcmp(command, "ForceButton"))
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );

		char buffer[24];
		Q_snprintf(buffer, sizeof(buffer), "cl_forceready %d", m_iForceReadyType);
		engine->ClientCmd(buffer);		

		MarkForDeletion();
		SetVisible(false);
		return;
	}
	BaseClass::OnCommand(command);
}