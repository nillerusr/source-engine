//========= StargateTC ============================================================//
//
// sg_teammenu.cpp - combined team + class selection panel.
// See sg_teammenu.h. Buttons are built in code (no .res dependency) and laid out
// in PerformLayout, so the panel is self-contained.
//=================================================================================//
#include "cbase.h"
#include "sg_teammenu.h"
#include <cdll_client_int.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// StargateTC team numbering (matches server: 2 = Tau'ri, 3 = Goa'uld).
#define SG_TEAM_AUTO       0
#define SG_TEAM_SPECTATOR  1
#define SG_TEAM_TAURI      2
#define SG_TEAM_GOAULD     3

struct SgClassDef_t { const char *cmd; const char *label; };

// Class rosters recovered from the GoldSrc panel (FUN_1002bec0 / Titles.txt).
static const SgClassDef_t s_TauClasses[] = {
	{ "oneill",  "O'Neill" },
	{ "carter",  "Carter" },
	{ "jackson", "Jackson" },
	{ "tealc",   "Teal'c" },
	{ "soldat",  "Soldier" },
};
static const SgClassDef_t s_GoaClasses[] = {
	{ "dieu",    "System Lord" },
	{ "anubis",  "Anubis Guard" },
	{ "jaffa",   "Jaffa" },
	{ "prima",   "First Prime" },
	{ "esclave", "Horus Guard" },
};

static void SgStyleButton( Button *pButton, const Color &fg, const Color &bg )
{
	if ( !pButton )
		return;

	pButton->SetButtonBorderEnabled( true );
	pButton->SetDefaultColor( fg, bg );
	pButton->SetArmedColor( Color( 255, 255, 255, 255 ), Color( 45, 94, 110, 245 ) );
	pButton->SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 23, 58, 72, 255 ) );
	pButton->SetSelectedColor( fg, Color( 29, 68, 82, 245 ) );
}

//-----------------------------------------------------------------------------
CSgTeamMenu::CSgTeamMenu( IViewPort *pViewPort ) : Frame( NULL, PANEL_TEAM )
{
	m_pViewPort = pViewPort;

	SetScheme( "ClientScheme" );
	SetTitle( "", true );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	SetProportional( false );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );

	m_pTitle = new Label( this, "Title", "Identify yourself" );
	m_pTauHeader = new Label( this, "TauHeader", "Tau'ri" );
	m_pGoaHeader = new Label( this, "GoaHeader", "Goa'uld" );

	for ( int i = 0; i < ARRAYSIZE( s_TauClasses ); i++ )
		m_TauButtons.AddToTail( AddClassButton( SG_TEAM_TAURI, s_TauClasses[i].cmd, s_TauClasses[i].label ) );
	for ( int i = 0; i < ARRAYSIZE( s_GoaClasses ); i++ )
		m_GoaButtons.AddToTail( AddClassButton( SG_TEAM_GOAULD, s_GoaClasses[i].cmd, s_GoaClasses[i].label ) );

	m_BottomButtons.AddToTail( AddTeamButton( SG_TEAM_AUTO,      "Auto-Assign", "auto" ) );
	m_BottomButtons.AddToTail( AddTeamButton( SG_TEAM_SPECTATOR, "Spectate",    "spec" ) );
	m_BottomButtons.AddToTail( AddTeamButton( -1,                "Cancel",      "cancel" ) );

	SetVisible( false );
}

CSgTeamMenu::~CSgTeamMenu()
{
}

//-----------------------------------------------------------------------------
vgui::Button *CSgTeamMenu::AddClassButton( int teamId, const char *cls, const char *label )
{
	Button *b = new Button( this, cls, label );
	KeyValues *kv = new KeyValues( "JoinClass" );
	kv->SetInt( "team", teamId );
	kv->SetString( "cls", cls );
	b->SetCommand( kv );
	b->AddActionSignalTarget( this );
	return b;
}

vgui::Button *CSgTeamMenu::AddTeamButton( int teamId, const char *label, const char *key )
{
	Button *b = new Button( this, key, label );
	KeyValues *kv = new KeyValues( "JoinTeam" );
	kv->SetInt( "team", teamId );
	b->SetCommand( kv );
	b->AddActionSignalTarget( this );
	return b;
}

//-----------------------------------------------------------------------------
void CSgTeamMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBgColor( Color( 5, 9, 12, 238 ) );

	m_pTitle->SetFgColor( Color( 235, 243, 235, 255 ) );
	m_pTauHeader->SetFgColor( Color( 90, 220, 130, 255 ) );
	m_pGoaHeader->SetFgColor( Color( 235, 88, 78, 255 ) );

	for ( int i = 0; i < m_TauButtons.Count(); i++ )
		SgStyleButton( m_TauButtons[i], Color( 210, 255, 220, 255 ), Color( 16, 46, 30, 235 ) );
	for ( int i = 0; i < m_GoaButtons.Count(); i++ )
		SgStyleButton( m_GoaButtons[i], Color( 255, 220, 210, 255 ), Color( 54, 25, 22, 235 ) );
	for ( int i = 0; i < m_BottomButtons.Count(); i++ )
		SgStyleButton( m_BottomButtons[i], Color( 230, 235, 230, 255 ), Color( 32, 39, 43, 235 ) );
}

//-----------------------------------------------------------------------------
void CSgTeamMenu::PaintBackground( void )
{
	int w, h;
	GetSize( w, h );

	surface()->DrawSetColor( 5, 9, 12, 238 );
	surface()->DrawFilledRect( 0, 0, w, h );

	surface()->DrawSetColor( 76, 155, 118, 255 );
	surface()->DrawOutlinedRect( 0, 0, w, h );
	surface()->DrawSetColor( 18, 57, 36, 210 );
	surface()->DrawFilledRect( 22, 44, w / 2 - 10, h - 58 );
	surface()->DrawSetColor( 76, 32, 28, 210 );
	surface()->DrawFilledRect( w / 2 + 10, 44, w - 22, h - 58 );
	surface()->DrawSetColor( 28, 37, 40, 235 );
	surface()->DrawFilledRect( 22, h - 54, w - 22, h - 16 );
}

//-----------------------------------------------------------------------------
void CSgTeamMenu::PerformLayout( void )
{
	BaseClass::PerformLayout();

	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	const int w = 460, h = 360;
	const int x = ( sw - w ) / 2, y = ( sh - h ) / 2;
	SetBounds( x, y, w, h );

	m_pTitle->SetBounds( 0, 12, w, 24 );
	m_pTitle->SetContentAlignment( Label::a_center );

	const int colW = 180, btnH = 26, gap = 6;
	const int leftX = 30, rightX = w - 30 - colW;
	int hy = 48;
	m_pTauHeader->SetBounds( leftX, hy, colW, 22 );
	m_pTauHeader->SetContentAlignment( Label::a_center );
	m_pGoaHeader->SetBounds( rightX, hy, colW, 22 );
	m_pGoaHeader->SetContentAlignment( Label::a_center );

	int by = hy + 28;
	for ( int i = 0; i < m_TauButtons.Count(); i++ )
		m_TauButtons[i]->SetBounds( leftX, by + i * ( btnH + gap ), colW, btnH );
	for ( int i = 0; i < m_GoaButtons.Count(); i++ )
		m_GoaButtons[i]->SetBounds( rightX, by + i * ( btnH + gap ), colW, btnH );

	// bottom row: auto / spectate / cancel
	int bottomY = h - 40;
	int n = m_BottomButtons.Count();
	int bw = 130, totalW = n * bw + ( n - 1 ) * gap;
	int bx = ( w - totalW ) / 2;
	for ( int i = 0; i < n; i++ )
		m_BottomButtons[i]->SetBounds( bx + i * ( bw + gap ), bottomY, bw, btnH );
}

//-----------------------------------------------------------------------------
void CSgTeamMenu::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( bShow )
	{
		Activate();
		SetVisible( true );
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
		InvalidateLayout();
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}

	if ( m_pViewPort )
		m_pViewPort->ShowBackGround( bShow );
}

//-----------------------------------------------------------------------------
void CSgTeamMenu::OnJoinClass( KeyValues *data )
{
	int team = data->GetInt( "team" );
	const char *cls = data->GetString( "cls" );
	char cmd[ 128 ];
	Q_snprintf( cmd, sizeof( cmd ), "jointeam %d; joinclass %s\n", team, cls );
	engine->ClientCmd( cmd );
	ShowPanel( false );
}

void CSgTeamMenu::OnJoinTeam( KeyValues *data )
{
	int team = data->GetInt( "team" );
	if ( team < 0 )   // Cancel
	{
		ShowPanel( false );
		return;
	}
	char cmd[ 96 ];
	if ( team == SG_TEAM_AUTO )
		Q_snprintf( cmd, sizeof( cmd ), "jointeam 0; joinclass auto\n" );
	else
		Q_snprintf( cmd, sizeof( cmd ), "jointeam %d\n", team );
	engine->ClientCmd( cmd );
	ShowPanel( false );
}

void CSgTeamMenu::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		ShowPanel( false );
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Console commands to open the menu (also bind a key, e.g. bind m chooseteam).
//-----------------------------------------------------------------------------
CON_COMMAND( chooseteam, "Open the StargateTC team/class selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
}

CON_COMMAND( changeclass, "Open the StargateTC class selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
}

CON_COMMAND( showmenu, "Open the StargateTC team/class selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
}

CON_COMMAND( showequip, "Open the StargateTC equipment/class selection menu" )
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowPanel( PANEL_TEAM, true );
}
