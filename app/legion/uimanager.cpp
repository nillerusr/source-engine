//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the UI 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "uimanager.h"
#include "legion.h"
#include "appframework/vguimatsysapp.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui_controls/controls.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/AnimationController.h"
#include "filesystem.h"
#include "tier3/tier3.h"
#include "vgui_controls/consoledialog.h"
#include "inputmanager.h"


//-----------------------------------------------------------------------------
// Console dialog for use in legion
//-----------------------------------------------------------------------------
class CLegionConsoleDialog : public vgui::CConsoleDialog
{
	DECLARE_CLASS_SIMPLE( CLegionConsoleDialog, vgui::CConsoleDialog );

public:
	CLegionConsoleDialog( vgui::Panel *pParent, const char *pName );
	virtual ~CLegionConsoleDialog();

	virtual void OnClose();
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );
};


CLegionConsoleDialog::CLegionConsoleDialog( vgui::Panel *pParent, const char *pName ) : BaseClass ( pParent, pName )
{
	AddActionSignalTarget( this );
}

CLegionConsoleDialog::~CLegionConsoleDialog()
{
	g_pUIManager->HideConsole( );
}


//-----------------------------------------------------------------------------
// A command was sent by the console
//-----------------------------------------------------------------------------
void CLegionConsoleDialog::OnCommandSubmitted( const char *pCommand )
{
	g_pInputManager->AddCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Deals with close
//-----------------------------------------------------------------------------
void CLegionConsoleDialog::OnClose()
{	 
	g_pUIManager->HideConsole( );
}



//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CUIManager s_UIManager;
extern CUIManager *g_pUIManager = &s_UIManager;


static const char *s_pRootPanelNames[UI_ROOT_PANEL_COUNT] = 
{
	"RootGamePanel",
	"RootMenuPanel",
	"RootToolsPanel",
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CUIManager::CUIManager()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CUIManager::Init()
{
	COMPILE_TIME_ASSERT( sizeof(s_pRootPanelNames) / sizeof(const char*) == UI_ROOT_PANEL_COUNT );

	// load the base localization file
	if (! vgui::scheme()->LoadSchemeFromFile("resource/legion.res", "Legion" ) )
		return false;

	vgui::filesystem()->AddSearchPath( "platform", "PLATFORM" );
	vgui::localize()->AddFile( vgui::filesystem(), "Resource/vgui_%language%.txt" );

	// start vgui
	g_pVGui->Start();

	// Run a frame to get the embedded panel to be the right size
	g_pVGui->RunFrame();

	int w, h;
	m_hEmbeddedPanel = g_pVGuiSurface->GetEmbeddedPanel();
	vgui::ipanel()->GetSize( m_hEmbeddedPanel, w, h );

	// add our root panels
	for ( int i = 0; i < UI_ROOT_PANEL_COUNT; ++i )
	{
		m_pRootPanels[i] = new vgui::EditablePanel( NULL, s_pRootPanelNames[i] );
		m_pRootPanels[i]->SetParent( m_hEmbeddedPanel );
		m_pRootPanels[i]->SetZPos( i );
		m_pRootPanels[i]->SetBounds( 0, 0, w, h );
		m_pRootPanels[i]->SetPaintBorderEnabled( false );
		m_pRootPanels[i]->SetPaintBackgroundEnabled( false );
		m_pRootPanels[i]->SetPaintEnabled( false );
		m_pRootPanels[i]->SetKeyBoardInputEnabled( i != UI_ROOT_GAME );
		m_pRootPanels[i]->SetMouseInputEnabled( i != UI_ROOT_GAME );
		m_pRootPanels[i]->SetVisible( false );
		m_pRootPanels[i]->SetCursor( vgui::dc_crosshair );
		m_pRootPanels[i]->SetAutoResize( vgui::Panel::PIN_TOPLEFT, vgui::Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	}

	m_hConsole = NULL;

	vgui::surface()->Invalidate( m_hEmbeddedPanel );
	return true;
}

void CUIManager::Shutdown()
{
	g_pVGui->Stop();
}


//-----------------------------------------------------------------------------
// Sets particular root panels to be visible
//-----------------------------------------------------------------------------
void CUIManager::EnablePanel( UIRootPanel_t id, bool bEnable )
{
	m_pRootPanels[id]->SetVisible( bEnable );
}

	
//-----------------------------------------------------------------------------
// Toggles the console
//-----------------------------------------------------------------------------
void CUIManager::ToggleConsole( const CCommand &args )
{
	if ( !m_hConsole.Get() )
	{
		m_hConsole = new CLegionConsoleDialog( m_pRootPanels[UI_ROOT_TOOLS], "Console" );

		// set the console to taking up most of the right-half of the screen
		int swide, stall;
		vgui::surface()->GetScreenSize(swide, stall);
		int offset = vgui::scheme()->GetProportionalScaledValue(16);

		m_hConsole->SetBounds(
			swide / 2 - (offset * 4),
			offset,
			(swide / 2) + (offset * 3),
			stall - (offset * 8));

		m_hConsole->SetVisible( false );
	}

	bool bMakeVisible = !m_hConsole->IsVisible();
	EnablePanel( UI_ROOT_TOOLS, bMakeVisible );
	if ( bMakeVisible )
	{
		m_hConsole->Activate();
	}
	else
	{
		m_hConsole->Hide();
	}
}


//-----------------------------------------------------------------------------
// Hides the console
//-----------------------------------------------------------------------------
void CUIManager::HideConsole()
{
	EnablePanel( UI_ROOT_TOOLS, false );
	if ( m_hConsole.Get() )
	{
		m_hConsole->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CUIManager::Update( )
{
	vgui::GetAnimationController()->UpdateAnimations( IGameManager::CurrentTime() );
	g_pVGui->RunFrame();
	if ( !g_pVGui->IsRunning() )
	{
		IGameManager::Stop();
	}
}


//-----------------------------------------------------------------------------
// Attempt to process an input event, return true if it sholdn't be chained to the rest of the game
//-----------------------------------------------------------------------------
bool CUIManager::ProcessInputEvent( const InputEvent_t& event )
{
	return g_pMatSystemSurface->HandleInputEvent( event );
}


//-----------------------------------------------------------------------------
// Draws the UI
//-----------------------------------------------------------------------------
void CUIManager::DrawUI()
{
	g_pVGuiSurface->PaintTraverseEx( m_hEmbeddedPanel, true );
}

	
//-----------------------------------------------------------------------------
// Push, pop menus
//-----------------------------------------------------------------------------
vgui::Panel *CUIManager::GetRootPanel( UIRootPanel_t id )
{
	return m_pRootPanels[id];
}

