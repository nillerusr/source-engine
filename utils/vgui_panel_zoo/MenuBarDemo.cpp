//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/MenuBar.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h> 
#include "vgui/ISurface.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// A MenuBar
//-----------------------------------------------------------------------------

class MenuBarDemo: public DemoPage
{
	public:
		MenuBarDemo(Panel *parent, const char *name);
		~MenuBarDemo();
		
	private:
		MenuBar *m_pMenuBar;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MenuBarDemo::MenuBarDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pMenuBar = new MenuBar(this, "AMenuBar");
	m_pMenuBar->SetPos(0, 20);

	// Make a couple menus and attach them in.

	// A menu 
	MenuButton *pMenuButton = new MenuButton(this, "FileMenuButton", "&File");
	Menu *pMenu = new Menu(pMenuButton, "AMenu");
	pMenu->AddMenuItem("&New",  new KeyValues ("NewFile"), this);
	pMenu->AddMenuItem("&Open",  new KeyValues ("OpenFile"), this);
	pMenu->AddMenuItem("&Save",  new KeyValues ("SaveFile"), this);
	pMenuButton->SetMenu(pMenu);

	m_pMenuBar->AddButton(pMenuButton);

	// A menu 
	pMenuButton = new MenuButton(this, "EditMenuButton", "&Edit");
	pMenu = new Menu(pMenuButton, "AMenu");
	pMenu->AddMenuItem("&Undo",  new KeyValues ("Undo"), this);
	pMenu->AddMenuItem("&Find",  new KeyValues ("Find"), this);
	pMenu->AddMenuItem("Select&All",  new KeyValues ("SelectAll"), this);
	pMenuButton->SetMenu(pMenu);

	m_pMenuBar->AddButton(pMenuButton);

	// A menu 
	pMenuButton = new MenuButton(this, "ViewMenuButton", "&View");
	pMenu = new Menu(pMenuButton, "AMenu");
	pMenu->AddMenuItem("&FullScreen",  new KeyValues ("FullScreen"), this);
	pMenu->AddMenuItem("&SplitScreen",  new KeyValues ("SplitScreen"), this);
	pMenu->AddMenuItem("&Properties",  new KeyValues ("Properties"), this);
	pMenu->AddMenuItem("&Output",  new KeyValues ("Output"), this);
	pMenuButton->SetMenu(pMenu);

	m_pMenuBar->AddButton(pMenuButton);

	// A menu 
	pMenuButton = new MenuButton(this, "Big", "&HugeMenu");
	pMenu = new Menu(pMenuButton, "HugeMenu");
	int items = 150;
	for ( int i = 0 ; i < items; ++i )
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "Item %03d", i + 1 );

        int idx = pMenu->AddMenuItem( sz,  new KeyValues ( sz ), this);

		if ( !(i % 4 ) )
		{
			char binding[ 256 ];
			Q_snprintf( binding, sizeof( binding ), "Ctrl+%c", 'A' + ( rand() % 26 ) );
			pMenu->SetCurrentKeyBinding( idx, binding );
		}

		if ( !(i % 7 ) )
		{
			pMenu->AddSeparator();
		}
	}
	pMenuButton->SetMenu(pMenu);

	m_pMenuBar->AddButton(pMenuButton);

	pMenuButton = new MenuButton(this, "Big", "&HalfHuge");
	pMenu = new Menu(pMenuButton, "HalfHuge");
	int htotal = 0;
	int itemHeight = pMenu->GetMenuItemHeight();

	int workX, workY, workWide, workTall;
	surface()->GetWorkspaceBounds(workX, workY, workWide, workTall);

	int i = 0;
	while ( htotal < ( workTall / 2 ) )
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "Item %03d", i + 1 );

        int idx = pMenu->AddMenuItem( sz,  new KeyValues ( sz ), this);

		if ( !(i % 4 ) )
		{
			char binding[ 256 ];
			Q_snprintf( binding, sizeof( binding ), "Ctrl+%c", 'A' + ( rand() % 26 ) );
			pMenu->SetCurrentKeyBinding( idx, binding );
		}

		if ( !(i % 7 ) )
		{
			pMenu->AddSeparator();
			htotal += 3;
		}
		++i;
		htotal += itemHeight;
	}
	pMenuButton->SetMenu(pMenu);

	m_pMenuBar->AddButton(pMenuButton);

	int bwide, btall;
	pMenuButton->GetSize( bwide, btall);
	int wide, tall;
	GetParent()->GetSize(wide, tall);
	m_pMenuBar->SetSize(wide - 2, btall + 8);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MenuBarDemo::~MenuBarDemo()
{
}


Panel* MenuBarDemo_Create(Panel *parent)
{
	return new MenuBarDemo(parent, "MenuBarDemo");
}


