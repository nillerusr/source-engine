//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The manager that deals with menus
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "tier1/utldict.h"
#include "tier1/utlstack.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Interface used to create menus
//-----------------------------------------------------------------------------
abstract_class IMenuFactory
{
public:
	// Returns the name of the menu it will create
	virtual const char *GetMenuName() = 0;

	// Creates the menu
	virtual vgui::Panel *CreateMenu( vgui::Panel *pParent ) = 0;
	
	// Used to build a list during construction
	virtual IMenuFactory *GetNextFactory( ) = 0;

protected:
	virtual ~IMenuFactory() {}
};


//-----------------------------------------------------------------------------
// Menu managemer
//-----------------------------------------------------------------------------
class CMenuManager : public CGameManager<>
{
public:
	typedef vgui::Panel* (*MenuFactory_t)( vgui::Panel *pParent );

	// Inherited from IGameManager
	virtual bool Init();
	virtual void Update( );
	virtual void Shutdown();

	// Push, pop menus
	void PushMenu( const char *pMenuName );
	void PopMenu( );
	void PopAllMenus( );

	// Pop the top menu, push specified menu
	void SwitchToMenu( const char *pMenuName );

	// Returns the name of the topmost panel
	const char *GetTopmostPanelName();

	// Call to register methods which can construct menus w/ particular ids
	// NOTE: This method is not expected to be called directly. Use the REGISTER_MENU macro instead
	// It returns the previous head of the list of factories
	static IMenuFactory* RegisterMenu( IMenuFactory *pMenuFactory );

private:
	void CleanUpAllMenus();

	typedef unsigned char MenuFactoryIndex_t;
	CUtlDict< IMenuFactory *, MenuFactoryIndex_t > m_MenuFactories;
	CUtlStack< vgui::Panel * > m_nActiveMenu;
	bool m_bPopRequested;
	bool m_bPopAllRequested;
	IMenuFactory *m_pPushRequested;
	static IMenuFactory *m_pFirstFactory;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CMenuManager *g_pMenuManager;


//-----------------------------------------------------------------------------
// Macro used to register menus with the menu manager
// For example, add the line REGISTER_MENU( "MainMenu", CMainMenu );
// into the class defining the main menu
//-----------------------------------------------------------------------------
template < class T >
class CMenuFactory : public IMenuFactory
{
public:
	CMenuFactory( const char *pMenuName ) : m_pMenuName( pMenuName )
	{
		m_pNextFactory = CMenuManager::RegisterMenu( this );
	}

	// Returns the name of the menu it will create
	virtual const char *GetMenuName()
	{
		return m_pMenuName;
	}

	// Creates the menu
	virtual vgui::Panel *CreateMenu( vgui::Panel *pParent )
	{
		return new T( pParent, m_pMenuName );
	}

	// Used to build a list during construction
	virtual IMenuFactory *GetNextFactory( )
	{
		return m_pNextFactory;
	}

private:
	const char* m_pMenuName;
	IMenuFactory *m_pNextFactory;
};

#define REGISTER_MENU( _name, _className )	\
	static CMenuFactory< _className > s_Factory ## _className( _name )


#endif // MENUMANAGER_H

