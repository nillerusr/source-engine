#include "cbase.h"
#include "holdout_resupply_frame.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "asw_holdout_mode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

vgui::DHANDLE<Holdout_Resupply_Frame> g_hResupplyFrame;

bool Holdout_Resupply_Frame::HasResupplyFrameOpen()
{
	return ( g_hResupplyFrame.Get() != NULL );
}

Holdout_Resupply_Frame::Holdout_Resupply_Frame( vgui::Panel *parent, const char *name ) : vgui::Frame( parent, name )
{	
	if ( g_hResupplyFrame.Get() )
	{
		g_hResupplyFrame->MarkForDeletion();
	}

	g_hResupplyFrame = this;
	SetScheme( vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew") );
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetTitle( "Resupply", true );
	SetMoveable( false );
	SetSizeable( false );
	SetMenuButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetMinimizeToSysTrayButtonVisible( false );
	SetCloseButtonVisible( false );
	SetTitleBarVisible( false );

	m_pBackground = new vgui::EditablePanel( this, "Background" );
	m_pTitleLabel = new vgui::Label( this, "TitleLabel", "" );
	m_pCountdownLabel = new vgui::Label( this, "CountdownLabel", "" );
	m_pCountdownTimeLabel = new vgui::Label( this, "CountdownTimeLabel", "" );
	m_pOkayButton = new vgui::Button( this, "OkayButton", "", this, "okay_button" );

	RequestFocus();
	SetVisible(true);
	SetEnabled(true);
	// ASWTODO: this causes an assert, but without it, F1 doesn't work
	SetKeyBoardInputEnabled(false);
	SetZPos( 200 );
}

Holdout_Resupply_Frame::~Holdout_Resupply_Frame()
{
	if ( g_hResupplyFrame.Get() == this )
	{
		g_hResupplyFrame = NULL;
	}
}

void Holdout_Resupply_Frame::PerformLayout()
{
	SetSize( ScreenWidth() + 1, ScreenHeight() + 1 );
	SetPos( 0,0 );

	BaseClass::PerformLayout();
}

void Holdout_Resupply_Frame::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HoldoutResupplyFrame.res" );
}

void Holdout_Resupply_Frame::OnThink()
{
	BaseClass::OnThink();

	if ( !ASWHoldoutMode() )
		return;

	float flTimeLeft = ASWHoldoutMode()->GetResupplyEndTime() - gpGlobals->curtime;
	flTimeLeft = MAX( 0, flTimeLeft );

	wchar_t wzValue[15];
	_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", (int) flTimeLeft );

	m_pCountdownTimeLabel->SetText( wzValue );
}

void Holdout_Resupply_Frame::OnCommand( const char* command )
{
	if ( !Q_strcmp( command, "okay_button" ) )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );

		MarkForDeletion();
	}
}