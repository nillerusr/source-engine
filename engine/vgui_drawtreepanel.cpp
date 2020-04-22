//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "client_pch.h"

#include <vgui/IClientPanel.h>
#include <vgui_controls/TreeView.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Button.h>
#include <vgui/IInput.h>
#include "../vgui2/src/VPanel.h"
#include "convar.h"
#include "tier0/vprof.h"
#include "vgui_baseui_interface.h"
#include "vgui_helpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool g_bForceRefresh = true;

void ChangeCallback_RefreshDrawTree( IConVar *var, const char *pOldString, float flValue )
{
	g_bForceRefresh = true;
}

// Bunch of vgui debugging stuff
static ConVar vgui_drawtree( "vgui_drawtree", "0", FCVAR_CHEAT, "Draws the vgui panel hiearchy to the specified depth level." );
static ConVar vgui_drawtree_visible( "vgui_drawtree_visible", "1", 0, "Draw the visible panels.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_hidden( "vgui_drawtree_hidden", "0", 0, "Draw the hidden panels.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_popupsonly( "vgui_drawtree_popupsonly", "0", 0, "Draws the vgui popup list in hierarchy(1) or most recently used(2) order.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_freeze( "vgui_drawtree_freeze", "0", 0, "Set to 1 to stop updating the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_panelptr( "vgui_drawtree_panelptr", "0", 0, "Show the panel pointer values in the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_panelalpha( "vgui_drawtree_panelalpha", "0", 0, "Show the panel alpha values in the vgui_drawtree view.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_render_order( "vgui_drawtree_render_order", "0", 0, "List the vgui_drawtree panels in render order.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_bounds( "vgui_drawtree_bounds", "0", 0, "Show panel bounds.", ChangeCallback_RefreshDrawTree );
static ConVar vgui_drawtree_draw_selected( "vgui_drawtree_draw_selected", "0", 0, "Highlight the selected panel", ChangeCallback_RefreshDrawTree );
extern ConVar vgui_drawfocus;


void vgui_drawtree_on_f()
{
	vgui_drawtree.SetValue( 1 );
}
void vgui_drawtree_off_f()
{
	vgui_drawtree.SetValue( 0 );
}
ConCommand vgui_drawtree_on( "+vgui_drawtree", vgui_drawtree_on_f );
ConCommand vgui_drawtree_off( "-vgui_drawtree", vgui_drawtree_off_f );


extern CUtlVector< vgui::VPANEL > g_FocusPanelList;
extern vgui::VPanelHandle g_DrawTreeSelectedPanel;


class CVGuiTree : public vgui::TreeView
{
public:
	typedef vgui::TreeView BaseClass;	


	CVGuiTree( vgui::Panel *parent, const char *pName ) : 
		BaseClass( parent, pName )
	{
		
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		SetFont( pScheme->GetFont( "ConsoleText", false ) );
		//SetBgColor( Color( 0, 0, 0, 175 ) );
		SetPaintBackgroundEnabled( false );
	}
};


class CDrawTreeFrame : public vgui::Frame
{
public:
	DECLARE_CLASS_SIMPLE( CDrawTreeFrame, vgui::Frame );
	
	CDrawTreeFrame( vgui::Panel *parent, const char *pName ) : 
		BaseClass( parent, pName )
	{
		// Init the frame.
		SetTitle( "VGUI Hierarchy", false );
		SetMenuButtonVisible( false );
		
		// Create the tree control itself.
		m_pTree = vgui::SETUP_PANEL( new CVGuiTree( this, "Tree view" ) );
		m_pTree->SetVisible( true );

		// Init the buttons.
		m_pShowVisible = vgui::SETUP_PANEL( new CConVarCheckButton( this, "show visible", "Show Visible" ) );
		m_pShowVisible->SetVisible( true );
		m_pShowVisible->SetConVar( &vgui_drawtree_visible );

		m_pShowHidden = vgui::SETUP_PANEL( new CConVarCheckButton( this, "show hidden", "Show Hidden" ) );
		m_pShowHidden->SetVisible( true );
		m_pShowHidden->SetConVar( &vgui_drawtree_hidden );

		m_pPopupsOnly = vgui::SETUP_PANEL( new CConVarCheckButton( this, "popups only", "Popups Only" ) );
		m_pPopupsOnly->SetVisible( true );
		m_pPopupsOnly->SetConVar( &vgui_drawtree_popupsonly );

		m_pDrawFocus = vgui::SETUP_PANEL( new CConVarCheckButton( this, "draw focus", "Highlight MouseOver" ) );
		m_pDrawFocus->SetVisible( true );
		m_pDrawFocus->SetConVar( &vgui_drawfocus );

		m_pFreeze = vgui::SETUP_PANEL( new CConVarCheckButton( this, "freeze option", "Freeze" ) );
		m_pFreeze->SetVisible( true );
		m_pFreeze->SetConVar( &vgui_drawtree_freeze );

		m_pShowPanelPtr = vgui::SETUP_PANEL( new CConVarCheckButton( this, "panel ptr option", "Show Addresses" ) );
		m_pShowPanelPtr->SetVisible( true );
		m_pShowPanelPtr->SetConVar( &vgui_drawtree_panelptr );

		m_pShowPanelAlpha = vgui::SETUP_PANEL( new CConVarCheckButton( this, "panel alpha option", "Show Alpha" ) );
		m_pShowPanelAlpha->SetVisible( true );
		m_pShowPanelAlpha->SetConVar( &vgui_drawtree_panelalpha );

		m_pRenderOrder = vgui::SETUP_PANEL( new CConVarCheckButton( this, "render order option", "In Render Order" ) );
		m_pRenderOrder->SetVisible( true );
		m_pRenderOrder->SetConVar( &vgui_drawtree_render_order );

		m_pShowBounds = vgui::SETUP_PANEL( new CConVarCheckButton( this, "show panel bounds", "Show Panel Bounds" ) );
		m_pShowBounds->SetVisible( true );
		m_pShowBounds->SetConVar( &vgui_drawtree_bounds );

		m_pHighlightSelected = vgui::SETUP_PANEL( new CConVarCheckButton( this, "highlight selected", "Highlight Selected" ) );
		m_pHighlightSelected->SetVisible( true );
		m_pHighlightSelected->SetConVar( &vgui_drawtree_draw_selected );

		m_pPerformLayoutBtn = vgui::SETUP_PANEL( new vgui::Button( this, "performlayout", "Perform Layout (Highlighted)", this, "performlayout" ) );
		m_pPerformLayoutBtn->SetVisible( true );
		
		m_pReloadSchemeBtn = vgui::SETUP_PANEL( new vgui::Button( this, "reloadscheme", "Reload Scheme (Highlighted)", this, "reloadscheme" ) );
		m_pReloadSchemeBtn->SetVisible( true );
		
		int r,g,b,a;
		GetBgColor().GetColor( r, g, b, a );
		a = 128;
		SetBgColor( Color( r, g, b, a ) );
	}


	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int x, y, w, t;
		GetClientArea( x, y, w, t );
		
		int yOffset = y;

		// Align the check boxes.
		m_pShowVisible->SetPos( x, yOffset );
		m_pShowVisible->SetWide( w/2 );
		yOffset += m_pShowVisible->GetTall();

		m_pShowHidden->SetPos( x, yOffset );
		m_pShowHidden->SetWide( w/2 );
		yOffset += m_pShowHidden->GetTall();

		m_pPopupsOnly->SetPos( x, yOffset );
		m_pPopupsOnly->SetWide( w/2 );
		yOffset += m_pPopupsOnly->GetTall();

		m_pDrawFocus->SetPos( x, yOffset );
		m_pDrawFocus->SetWide( w/2 );
		yOffset += m_pDrawFocus->GetTall();

		m_pShowBounds->SetPos( x, yOffset );
		m_pShowBounds->SetWide( w/2 );
		yOffset += m_pShowBounds->GetTall();

		// Next column..
		yOffset = y;

		m_pFreeze->SetPos( x + w/2, yOffset );
		m_pFreeze->SetWide( w/2 );
		yOffset += m_pFreeze->GetTall();

		m_pShowPanelPtr->SetPos( x + w/2, yOffset );
		m_pShowPanelPtr->SetWide( w/2 );
		yOffset += m_pShowPanelPtr->GetTall();

		m_pShowPanelAlpha->SetPos( x + w/2, yOffset );
		m_pShowPanelAlpha->SetWide( w/2 );
		yOffset += m_pShowPanelAlpha->GetTall();
			   
		m_pRenderOrder->SetPos( x + w/2, yOffset );
		m_pRenderOrder->SetWide( w/2 );
		yOffset += m_pRenderOrder->GetTall();

		m_pHighlightSelected->SetPos( x + w/2, yOffset );
		m_pHighlightSelected->SetWide( w/2 );
		yOffset += m_pHighlightSelected->GetTall();

		// buttons

		m_pPerformLayoutBtn->SetPos( x , yOffset );
		m_pPerformLayoutBtn->SizeToContents();
		m_pPerformLayoutBtn->SetWide( w );
		yOffset += m_pPerformLayoutBtn->GetTall();

		m_pReloadSchemeBtn->SetPos( x , yOffset );
		m_pReloadSchemeBtn->SizeToContents();
		m_pReloadSchemeBtn->SetWide( w );
		yOffset += m_pReloadSchemeBtn->GetTall();

		// the tree control
		m_pTree->SetBounds( x, yOffset, w, t - (yOffset - y) );
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_stricmp( command, "performlayout" ) )
		{
			if ( g_DrawTreeSelectedPanel )
			{
				vgui::ipanel()->SendMessage( g_DrawTreeSelectedPanel, new KeyValues("Command", "command", "performlayout"), GetVPanel() );
			}
		}
		else if ( !Q_stricmp( command, "reloadscheme" ) )
		{
			if ( g_DrawTreeSelectedPanel )
			{
				vgui::ipanel()->SendMessage( g_DrawTreeSelectedPanel, new KeyValues("Command", "command", "reloadscheme"), GetVPanel() );
			}
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	MESSAGE_FUNC( OnItemSelected, "TreeViewItemSelected" )
	{
		RecalculateSelectedHighlight();
	}

	void RecalculateSelectedHighlight( void )
	{
		Assert( m_pTree );

		if ( !vgui_drawtree_draw_selected.GetBool() || m_pTree->GetSelectedItemCount() != 1 )
		{
			// clear the selection
			g_DrawTreeSelectedPanel = 0;
		}
		else
		{
			CUtlVector< int > list;
			m_pTree->GetSelectedItems( list );

			Assert( list.Count() == 1 );

			KeyValues *data = m_pTree->GetItemData( list.Element(0) );

			if ( data )
			{
				g_DrawTreeSelectedPanel = (data) ? (vgui::VPANEL)data->GetInt( "PanelPtr", 0 ) : 0;
			}
			else
			{
				g_DrawTreeSelectedPanel = 0;
			}
		}
	}

	virtual void OnClose()
	{
		vgui_drawtree.SetValue( 0 );
		g_DrawTreeSelectedPanel = 0;

		// fixme - g_DrawTreeSelectedPanel has a potential crash if you hilight a panel, and then spam hud_reloadscheme
		// you will sometimes end up on a different panel or on garbage.
	}

	virtual void OnThink() OVERRIDE
	{
		BaseClass::OnThink();

		vgui::VPANEL focus = 0;
		if ( vgui::input() )
		{
			focus = vgui::input()->GetFocus();
		}

		MoveToFront();

		if ( vgui::input() && focus )
		{
			OnRequestFocus(focus, NULL);
		}
	}

public:
	CVGuiTree *m_pTree;	
	CConVarCheckButton *m_pShowVisible;
	CConVarCheckButton *m_pShowHidden;
	CConVarCheckButton *m_pPopupsOnly;
	CConVarCheckButton *m_pFreeze;
	CConVarCheckButton *m_pShowPanelPtr;
	CConVarCheckButton *m_pShowPanelAlpha;
	CConVarCheckButton *m_pRenderOrder;
	CConVarCheckButton *m_pDrawFocus;
	CConVarCheckButton *m_pShowBounds;
	CConVarCheckButton *m_pHighlightSelected;
	vgui::Button	   *m_pPerformLayoutBtn;
	vgui::Button	   *m_pReloadSchemeBtn;
};


CDrawTreeFrame *g_pDrawTreeFrame = 0;


void VGui_RecursivePrintTree( 
	vgui::VPANEL current, 
	KeyValues *pCurrentParent,
	int popupDepthCounter )
{
	if ( !current )
		return;

	vgui::IPanel *ipanel = vgui::ipanel();

	if ( !vgui_drawtree_visible.GetInt() && ipanel->IsVisible( current ) )
		return;
	else if ( !vgui_drawtree_hidden.GetInt() && !ipanel->IsVisible( current ) )
		return;
	else if ( popupDepthCounter <= 0 && ipanel->IsPopup( current ) )
		return;	
	
	KeyValues *pNewParent = pCurrentParent;
	KeyValues *pVal = pCurrentParent->CreateNewKey();
		
	// Bind data to pVal.
	char name[1024];
	const char *pInputName = ipanel->GetName( current );
	if ( pInputName && pInputName[0] != 0 )
		Q_snprintf( name, sizeof( name ), "%s", pInputName );
	else
		Q_snprintf( name, sizeof( name ), "%s", "(no name)" );

	if ( ipanel->IsMouseInputEnabled( current ) )
	{
		Q_strncat( name, ", +m", sizeof( name ), COPY_ALL_CHARACTERS );
	}

	if ( ipanel->IsKeyBoardInputEnabled( current ) )
	{
		Q_strncat( name, ", +k", sizeof( name ), COPY_ALL_CHARACTERS );
	}

	if ( vgui_drawtree_bounds.GetBool() )
	{
		int x, y, w, h;
		vgui::ipanel()->GetPos( current, x, y );
		vgui::ipanel()->GetSize( current, w, h );
		char b[ 128 ];
		Q_snprintf( b, sizeof( b ), "[%-4i %-4i %-4i %-4i]", x, y, w, h );

		Q_strncat( name, ", ", sizeof( name ), COPY_ALL_CHARACTERS );
		Q_strncat( name, b, sizeof( name ), COPY_ALL_CHARACTERS );
	}

	char str[1024];
	if ( vgui_drawtree_panelptr.GetInt() )
		Q_snprintf( str, sizeof( str ), "%s - [0x%x]", name, current );
	else if (vgui_drawtree_panelalpha.GetInt() )
	{
		KeyValues *kv = new KeyValues("alpha");
		vgui::ipanel()->RequestInfo(current, kv);
		Q_snprintf( str, sizeof( str ), "%s - [%d]", name, kv->GetInt("alpha") );
		kv->deleteThis();
	}
	else
		Q_snprintf( str, sizeof( str ), "%s", name );

	pVal->SetString( "Text", str );
	pVal->SetInt( "PanelPtr", current );

	pNewParent = pVal;

	
	// Don't recurse past the tree itself because the tree control uses a panel for each
	// panel inside itself, and it'll infinitely create panels.
	if ( current == g_pDrawTreeFrame->m_pTree->GetVPanel() )
		return;


	int count = ipanel->GetChildCount( current );
	for ( int i = 0; i < count ; i++ )
	{
		vgui::VPANEL panel = ipanel->GetChild( current, i );
		VGui_RecursivePrintTree( panel, pNewParent, popupDepthCounter-1 );
	}
}


bool UpdateItemState(
	vgui::TreeView *pTree, 
	int iChildItemId, 
	KeyValues *pSub )
{
	bool bRet = false;
	vgui::IPanel *ipanel = vgui::ipanel();

	KeyValues *pItemData = pTree->GetItemData( iChildItemId );
	if ( pItemData->GetInt( "PanelPtr" ) != pSub->GetInt( "PanelPtr" ) ||
		Q_stricmp( pItemData->GetString( "Text" ), pSub->GetString( "Text" ) ) != 0 )
	{
		pTree->ModifyItem( iChildItemId, pSub );
		bRet = true;
	}

	// Ok, this is a new panel.
	vgui::VPANEL vPanel = pSub->GetInt( "PanelPtr" );

	int iBaseColor[3] = { 255, 255, 255 };
	if ( ipanel->IsPopup( vPanel ) )
	{
		iBaseColor[0] = 255;	iBaseColor[1] = 255;	iBaseColor[2] = 0;
	}

	if ( g_FocusPanelList.Find( vPanel ) != -1 )
	{
		iBaseColor[0] = 0;		iBaseColor[1] = 255;	iBaseColor[2] = 0;
		pTree->ExpandItem( iChildItemId, true );
	}

	if ( !ipanel->IsVisible( vPanel ) )
	{
		iBaseColor[0] >>= 1;	iBaseColor[1] >>= 1;	iBaseColor[2] >>= 1;
	}

	pTree->SetItemFgColor( iChildItemId, Color( iBaseColor[0], iBaseColor[1], iBaseColor[2], 255 ) );
	return bRet;
}


void IncrementalUpdateTree( 
	vgui::TreeView *pTree, 
	KeyValues *pValues )
{
	if ( !g_bForceRefresh && vgui_drawtree_freeze.GetInt() )
		return;

	g_bForceRefresh = false;

	bool bInvalidateLayout = IncrementalUpdateTree( pTree, pValues, UpdateItemState, -1 );
	
	pTree->ExpandItem( pTree->GetRootItemIndex(), true );

	if ( g_pDrawTreeFrame )
		g_pDrawTreeFrame->RecalculateSelectedHighlight();
	
	if ( bInvalidateLayout )
		pTree->InvalidateLayout();
}


bool WillPanelBeVisible( vgui::VPANEL hPanel )
{
	while ( hPanel )
	{
		if ( !vgui::ipanel()->IsVisible( hPanel ) )
			return false;

		hPanel = vgui::ipanel()->GetParent( hPanel );
	}
	return true;
}


void VGui_AddPopupsToKeyValues( KeyValues *pCurrentParent )
{
	// 'twould be nice if we could get the Panel* from the VPANEL, but we can't.
	int count = vgui::surface()->GetPopupCount();
	for ( int i=0; i < count; i++ )
	{
		vgui::VPANEL vPopup = vgui::surface()->GetPopup( i );
		if ( vgui_drawtree_hidden.GetInt() || WillPanelBeVisible( vPopup ) )
		{
			VGui_RecursivePrintTree( 
				vPopup, 
				pCurrentParent,
				1 );
		}
	}
}


void VGui_FillKeyValues( KeyValues *pCurrentParent )
{
	if ( !EngineVGui()->IsInitialized() )
		return;

	// Figure out the root panel to start at. 
	// If they specified a name for a root panel, then use that one.
	vgui::VPANEL hBase = vgui::surface()->GetEmbeddedPanel();

	if ( vgui_drawtree_popupsonly.GetInt() )
	{
		VGui_AddPopupsToKeyValues( pCurrentParent );
	}
	else if ( vgui_drawtree_render_order.GetInt() )
	{
		VGui_RecursivePrintTree( 
			hBase, 
			pCurrentParent,
			0 );

		VGui_AddPopupsToKeyValues( pCurrentParent );
	}
	else
	{
		VGui_RecursivePrintTree( 
			hBase, 
			pCurrentParent,
			99999 );
	}
}


void VGui_DrawHierarchy( void )
{
	VPROF( "VGui_DrawHierarchy" );

	if ( IsX360() )
		return;

	if ( vgui_drawtree.GetInt() <= 0 )
	{
		g_pDrawTreeFrame->SetVisible( false );
		return;
	}

	g_pDrawTreeFrame->SetVisible( true );

	// Now reconstruct the tree control.
	KeyValues *pRoot = new KeyValues("");
	pRoot->SetString( "Text", "<shouldn't see this>" );
	
	VGui_FillKeyValues( pRoot );

	// Now incrementally update the tree control so we can preserve which nodes are open.
	IncrementalUpdateTree( g_pDrawTreeFrame->m_pTree, pRoot );

	// Delete the keyvalues.
	pRoot->deleteThis();
}


void VGui_CreateDrawTreePanel( vgui::Panel *parent )
{
	int widths = 300;
	
	g_pDrawTreeFrame = vgui::SETUP_PANEL( new CDrawTreeFrame( parent, "DrawTreeFrame" ) );
	g_pDrawTreeFrame->SetVisible( false );
	g_pDrawTreeFrame->SetBounds( parent->GetWide() - widths, 0, widths, parent->GetTall() - 10 );
	
	g_pDrawTreeFrame->MakePopup( false, false );
	g_pDrawTreeFrame->SetKeyBoardInputEnabled( true );
	g_pDrawTreeFrame->SetMouseInputEnabled( true );
}


void VGui_MoveDrawTreePanelToFront()
{
	if ( g_pDrawTreeFrame )
		g_pDrawTreeFrame->MoveToFront();
}


void VGui_UpdateDrawTreePanel()
{
	VGui_DrawHierarchy();
}


void vgui_drawtree_clear_f()
{
	if ( g_pDrawTreeFrame && g_pDrawTreeFrame->m_pTree )
		g_pDrawTreeFrame->m_pTree->RemoveAll();
}

ConCommand vgui_drawtree_clear( "vgui_drawtree_clear", vgui_drawtree_clear_f );
