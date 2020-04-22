//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Order window
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_orders.h"
#include "hud.h"
#include "c_basetfplayer.h"
#include "clientmode_tfnormal.h"
#include "VGUI_BasePanel.h"
#include <vgui/IScheme.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLabel::SetSelected( bool bSelected )
{
	m_bSelected = bSelected;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudOrderList *GetHudOrderList( void )
{
	return GET_HUDELEMENT( CHudOrderList );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudOrder::CHudOrder( int x,int y,int wide,int tall ) : vgui::Panel( NULL, "CHudOrder")
{
	SetBounds( x, y, wide, tall );
	SetAutoDelete( false );
	m_pOrder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudOrder::~CHudOrder( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrder::Init( void )
{
	SetSize( ORDERS_ELEMENT_WIDTH, ORDERS_ELEMENT_HEIGHT );
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CHudOrder::Paint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrder::SetOrder( C_Order *pOrder )
{
	// If we had an order, tell it to clean up
	if ( m_pOrder )
	{
		m_pOrder->DestroyStatus();
	}

	m_pOrder = pOrder;

	// Tell the order to create it's elements
	if ( m_pOrder )
	{
		m_pOrder->CreateStatus( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Order *CHudOrder::GetOrder( void )
{
	return m_pOrder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrder::UpdateOrder( void )
{
	if ( m_pOrder == NULL )
		return;
}

//-----------------------------------------------------------------------------
// Purpose: Our order has been deleted.
//-----------------------------------------------------------------------------
void CHudOrder::OrderRemoved( void )
{
	m_pOrder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrder::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	Panel::ApplySchemeSettings(pScheme);

	if ( m_pOrder && m_pOrder->IsPersonalOrder() )
	{
		SetBgColor( GetSchemeColor("HudStatusSelectedBgColor", pScheme) );
	}
	else
	{
		SetBgColor( GetSchemeColor("HudStatusBgColor", pScheme) );
	}
}

//================================================================================================================
// LARGE STATUS PANEL.
//================================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
CHudOrderList::CHudOrderList( const char *pElementName ) :
	CHudElement( pElementName ), vgui::Panel( NULL, "HudOrderList" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetPaintBackgroundEnabled( false );
	for (int i = 0; i < MAX_HUD_ORDERS; i++)
	{
		m_pOrderPanels[i] = NULL;
		m_pOrderLabels[i] = NULL;
	}

	SetHiddenBits( HIDEHUD_MISCSTATUS | HIDEHUD_PLAYERDEAD );
}

DECLARE_HUDELEMENT( CHudOrderList );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudOrderList::~CHudOrderList( void )
{
	for (int i = 0; i < MAX_HUD_ORDERS; i++)
	{
		if ( m_pOrderPanels[i] )
		{
			delete m_pOrderPanels[i];
			delete m_pOrderLabels[i];
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrderList::LevelInit( void )
{
	SetPos( ORDERS_LEFT, ORDERS_TOP );
	SetSize( ORDERS_WIDTH, ORDERS_HEIGHT );

	for ( int i = 0; i < MAX_HUD_ORDERS; i++ )
	{
		m_pOrderPanels[i] = new CHudOrder( ORDERS_ELEMENT_LEFT - ORDERS_LEFT, ORDERS_ELEMENT_HEIGHT * i, ORDERS_ELEMENT_WIDTH, ORDERS_ELEMENT_HEIGHT );
		m_pOrderPanels[i]->Init();

		m_pOrderLabels[i] = new CHudLabel( NULL, "orderlabels", " " );
		m_pOrderLabels[i]->SetBounds( 0, ORDERS_ELEMENT_HEIGHT * i, ORDERS_ELEMENT_LEFT - ORDERS_LEFT, ORDERS_ELEMENT_HEIGHT );
		m_pOrderLabels[i]->SetContentAlignment( vgui::Label::a_northwest );
		m_pOrderLabels[i]->SetTextInset( 4, 0 );

		// Start all of them hidden
		m_pOrderPanels[i]->SetParent( (vgui::Panel *)NULL );
		m_pOrderLabels[i]->SetParent( (vgui::Panel *)NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrderList::LevelShutdown( void )
{
	for ( int i = 0; i < MAX_HUD_ORDERS; i++ )
	{
		delete m_pOrderPanels[i];
		m_pOrderPanels[i] = NULL;
		delete m_pOrderLabels[i];
		m_pOrderLabels[i] = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrderList::OnThink( void )
{
	// Call update for all active objects
	for (int i = 0; i < MAX_HUD_ORDERS; i++)
	{
		if ( m_pOrderPanels[i] && m_pOrderPanels[i]->GetOrder() )
		{
			m_pOrderPanels[i]->UpdateOrder();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudOrderList::Paint( void )
{
//	vgui::surface()->DrawSetColor( 255,0,0, 64 );
//	vgui::surface()->DrawOutlinedRect( 0,0, GetWide(), GetTall() );
}

//-----------------------------------------------------------------------------
// Purpose: Insert the specified order into the slot
//-----------------------------------------------------------------------------
void CHudOrderList::InsertOrder( C_Order *pOrder, int iSlot )
{
	// Personal Orders don't have selection keys
	if ( pOrder->IsPersonalOrder() )
	{
		m_pOrderLabels[iSlot]->SetText("");
	}
	else
	{
		// Check the key binding
		char binding[64];
		Q_snprintf(binding, sizeof( binding ), "order %d", iSlot+1);
		const char *pBinding = engine->Key_LookupBinding( binding );
		if ( pBinding && strcmp(pBinding,"") )
		{
			m_pOrderLabels[iSlot]->SetText( pBinding );
		}
	}

	// Only insert it if it's not there already
	if ( m_pOrderPanels[iSlot]->GetOrder() != pOrder )
	{
		m_pOrderPanels[iSlot]->SetOrder( pOrder );
		m_pOrderPanels[iSlot]->SetParent( this );
		m_pOrderLabels[iSlot]->SetParent( this );
		m_pOrderLabels[iSlot]->SetSelected( pOrder->IsPersonalOrder() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: We've received a new order, or a modified old one. Figure out what to do.
//-----------------------------------------------------------------------------
void CHudOrderList::RecalculateOrderList( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();

	// Does this player have a personal order?
	C_Order *pOrder = pPlayer->PersonalOrder();
	if( pOrder )
	{
		// Highlight the selected order (and the personal order always)
		m_pOrderPanels[0]->SetBgColor( Color( 0,0,0, 192) );
		m_pOrderLabels[0]->SetBgColor( Color( 0,0,0, 192) );
	}
}

