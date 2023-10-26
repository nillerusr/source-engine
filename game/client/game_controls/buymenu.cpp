//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "BuyMenu.h"

#include "BuySubMenu.h"
using namespace vgui;

#include "mouseoverpanelbutton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBuyMenu::CBuyMenu(IViewPort *pViewPort) : WizardPanel( NULL, PANEL_BUY )
{
	SetScheme("ClientScheme");
	SetTitle( "#Cstrike_Buy_Menu", true);

	SetMoveable(false);
	SetSizeable(false);
	SetProportional(true);

	// hide the system buttons
	SetTitleBarVisible( false );

	SetAutoDelete( false ); // we reuse this panel, don't let WizardPanel delete us
	
	LoadControlSettings( "Resource/UI/BuyMenu.res" );
	ShowButtons( false );

	m_pViewPort = pViewPort;

	m_pMainMenu = new CBuySubMenu( this, "mainmenu" );
	m_pMainMenu->LoadControlSettings( "Resource/UI/MainBuyMenu.res" );
	m_pMainMenu->SetVisible( false );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBuyMenu::~CBuyMenu()
{
	if ( m_pMainMenu )
		m_pMainMenu->DeleteSubPanels();	//?
}

//-----------------------------------------------------------------------------
// Purpose: shows/hides the buy menu
//-----------------------------------------------------------------------------
void CBuyMenu::ShowPanel(bool bShow)
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( bShow )
	{
		Update();

		Run( m_pMainMenu );

		SetMouseInputEnabled( true );
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}

	m_pViewPort->ShowBackGround( bShow );
}


void CBuyMenu::Update()
{
	//Don't need to do anything, but do need to implement this function as base is pure virtual
	NULL;
}
void CBuyMenu::OnClose()
{
	BaseClass::OnClose();
	ResetHistory();
}
