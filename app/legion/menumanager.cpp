//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The menu manager  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "menumanager.h"
#include "vgui_controls/panel.h"
#include "vgui_controls/frame.h"
#include "uimanager.h"


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CMenuManager s_MenuManager;
extern CMenuManager *g_pMenuManager = &s_MenuManager;


//-----------------------------------------------------------------------------
// Static members.
// NOTE: Do *not* set this to 0; it could cause us to lose some registered
// menus since that list is set up during construction
//-----------------------------------------------------------------------------
IMenuFactory *CMenuManager::m_pFirstFactory;


//-----------------------------------------------------------------------------
// Call to register methods which can construct menus w/ particular names
//-----------------------------------------------------------------------------
IMenuFactory *CMenuManager::RegisterMenu( IMenuFactory *pMenuFactory )
{
	// NOTE: This method is expected to be called during global constructor
	// time, so it must not require any global constructors to be called to work
	IMenuFactory *pPrevFactory = m_pFirstFactory;
	m_pFirstFactory = pMenuFactory;
	return pPrevFactory;
}

	
//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CMenuManager::Init()
{
	// Build a dictionary of all registered menus
	IMenuFactory *pFactory;
	for ( pFactory = m_pFirstFactory; pFactory; pFactory = pFactory->GetNextFactory() )
	{
		m_MenuFactories.Insert( pFactory->GetMenuName(), pFactory );
	}

	m_bPopRequested = false;
	m_bPopAllRequested = false;
	m_pPushRequested = NULL;

	return true;
}

void CMenuManager::Shutdown()
{
	CleanUpAllMenus();
}

	
//-----------------------------------------------------------------------------
// Push, pop menus
//-----------------------------------------------------------------------------
void CMenuManager::PushMenu( const char *pMenuName )
{
	AssertMsg( !m_pPushRequested, "Can't request to push two menus in a single frame!" );

	MenuFactoryIndex_t i = m_MenuFactories.Find( pMenuName );
	if ( i == m_MenuFactories.InvalidIndex() )
	{
		Warning( "Tried to push unknown menu %s\n", pMenuName );
		return;
	}
	m_pPushRequested = m_MenuFactories[i];
}

void CMenuManager::PopMenu( )
{
	AssertMsg( !m_bPopRequested, "Can't request to pop two menus in a single frame!" );
	AssertMsg( !m_pPushRequested, "Can't request to pop after requesting to push a menu in a single frame!" );
	m_bPopRequested = true;
}

void CMenuManager::PopAllMenus( )
{
	AssertMsg( !m_pPushRequested, "Can't request to pop after requesting to push a menu in a single frame!" );
	m_bPopAllRequested = true;
}


//-----------------------------------------------------------------------------
// Request a menu to switch to
//-----------------------------------------------------------------------------
void CMenuManager::SwitchToMenu( const char *pMenuName )
{
	AssertMsg( !m_bPopRequested, "Can't request to pop two menus in a single frame!" );
	AssertMsg( !m_pPushRequested, "Can't request to push two menus in a single frame!" );

	MenuFactoryIndex_t i = m_MenuFactories.Find( pMenuName );
	if ( i == m_MenuFactories.InvalidIndex() )
	{
		Warning( "Tried to switch to unknown menu %s\n", pMenuName );
		return;
	}
	m_bPopRequested = true;
	m_pPushRequested = m_MenuFactories[i];
}


//-----------------------------------------------------------------------------
// Returns the name of the topmost panel
//-----------------------------------------------------------------------------
const char *CMenuManager::GetTopmostPanelName()
{
	if ( !m_nActiveMenu.Count() )
		return NULL;
	return m_nActiveMenu.Top()->GetName();
}

	
//-----------------------------------------------------------------------------
// Request a menu to switch to
//-----------------------------------------------------------------------------
void CMenuManager::Update( )
{
	if ( m_bPopAllRequested )
	{
		CleanUpAllMenus();
		m_bPopAllRequested = false;
		return;
	}

	if ( m_bPopRequested )
	{
		AssertMsg( m_nActiveMenu.Count(), "Tried to pop a menu when no menus are active" );
		vgui::Panel *pTop = m_nActiveMenu.Top();
		pTop->MarkForDeletion();
		m_nActiveMenu.Pop();

		// Mark the new active menu as visible, attach it to hierarchy.
		if ( m_nActiveMenu.Count() > 0 )
		{
			vgui::Panel *pTop = m_nActiveMenu.Top();
			pTop->SetVisible( true );
			pTop->SetParent( g_pUIManager->GetRootPanel( UI_ROOT_MENU ) );
		}
		else
		{
			g_pUIManager->EnablePanel( UI_ROOT_MENU, false );
		}
		m_bPopRequested = false;
	}

	if ( m_pPushRequested )
	{
		// Mark the previous menu as not visible, detach it from hierarchy.
		if ( m_nActiveMenu.Count() > 0 )
		{
			vgui::Panel *pTop = m_nActiveMenu.Top();
			pTop->SetVisible( false );
			pTop->SetParent( (vgui::Panel*)NULL );
		}
		else
		{
			g_pUIManager->EnablePanel( UI_ROOT_MENU, true );
		}
		vgui::Panel *pMenu = m_pPushRequested->CreateMenu( g_pUIManager->GetRootPanel( UI_ROOT_MENU ) );
		m_nActiveMenu.Push( pMenu );
		static_cast<vgui::Frame*>( pMenu )->Activate();
		m_pPushRequested = NULL;
	}
}

	
//-----------------------------------------------------------------------------
// Cleans up all menus
//-----------------------------------------------------------------------------
void CMenuManager::CleanUpAllMenus()
{
	while ( m_nActiveMenu.Count() )
	{
		vgui::Panel *pTop = m_nActiveMenu.Top();
		pTop->MarkForDeletion();
		m_nActiveMenu.Pop();
	}
	g_pUIManager->EnablePanel( UI_ROOT_MENU, false );
}

