//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client Side COrder class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_orders.h"
#include "c_order.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "minimap_trace.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "c_func_resource.h"
#include "tf_shareddefs.h"
#include "c_baseobject.h"
#include "tf_hints.h"
#include "hud.h"
#include "c_basetfplayer.h"
#include "c_tf_hintmanager.h"
#include "clientmode_tfnormal.h"

#define NEW_ORDER_ANIM_DURATION		1.0f
#define NEW_ORDERS_RIGHT			XRES(640-8)
#define NEW_ORDERS_LEFT				XRES(8)
#define NEW_ORDERS_WIDTH			(ORDERS_RIGHT - NEW_ORDERS_LEFT)	//(NEW_ORDERS_RIGHT - ORDERS_LEFT)


class COrderLabel : public vgui::Label
{
public:
							COrderLabel( vgui::Panel *pParent, const char *pPanelName, const char *pText )
								: vgui::Label( pParent, pPanelName, pText )
							{
							}

	virtual void	OnThink()
	{
		BaseClass::OnThink();

		// Resize?
		int x, y, w, h;
		if( m_flAnimCounter < NEW_ORDER_ANIM_DURATION )
		{
			int wantedWidth, dummy;
			GetContentSize( wantedWidth, dummy );
			wantedWidth += 10;
			
			float flPercent = m_flAnimCounter / NEW_ORDER_ANIM_DURATION;
			int newWidth = (int)(NEW_ORDERS_WIDTH + (wantedWidth - NEW_ORDERS_WIDTH) * flPercent);
		
			GetBounds( x, y, w, h );
			
			m_flAnimCounter += gpGlobals->frametime;
			if( m_flAnimCounter >= NEW_ORDER_ANIM_DURATION )
			{
				SetBounds( ORDERS_RIGHT - wantedWidth, y, wantedWidth, h );
			}
			else
			{
				SetBounds( ORDERS_RIGHT - newWidth, y, newWidth, h );
			}
		}
	}

	virtual void	PaintBackground()
	{
		// BaseClass::PaintBackground();

		// Draw our background.
		int x, y, w, h;
		GetBounds( x, y, w, h );

		vgui::surface()->DrawSetColor( Color( 0, 0, 0, 160 ) );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );

		vgui::surface()->DrawSetColor( Color( 63, 63, 63, 255 ) );
		vgui::surface()->DrawOutlinedRect( 0, 0, w, h );
	}

public:
	float	m_flAnimCounter;
};


class CMinimapOrderPanel : public CMinimapTraceBitmapPanel
{
	DECLARE_CLASS( CMinimapOrderPanel, CMinimapTraceBitmapPanel );

public:
	CMinimapOrderPanel( vgui::Panel *parent, const char *panelName ) 
		: BaseClass( parent, "CMinimapOrderPanel" )
	{
	}
	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );
	virtual void OnTick();
	virtual void Paint( );

private:
	C_Order *m_pOrder;
};


DECLARE_MINIMAP_FACTORY( CMinimapOrderPanel, "minimap_order_panel" );

bool CMinimapOrderPanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	m_pOrder = dynamic_cast<C_Order*>(pInitData->m_pEntity);
	if (!m_pOrder)
		return false;

	if (!BaseClass::Init( pKeyValues, pInitData ))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
void CMinimapOrderPanel::OnTick()
{
	// NOTE: Do *not* chain down the the base OnTick; it's going to do 
	// a totally different computation that will conflict with ours
	Assert( m_pOrder );
	if( m_pOrder->GetTarget() <= 0 )
	{
		SetVisible(false);
		return;
	}

	C_BaseEntity *pTarget = ClientEntityList().GetEnt( m_pOrder->GetTarget() );
	if( !pTarget )
	{
		SetVisible(false);
		return;
	}

	SetEntity( pTarget );

	// Now that we're attached to the correct target, compute position!
	BaseClass::OnTick();
}


void CMinimapOrderPanel::Paint( )
{
	Assert( m_pOrder );

	if( m_pOrder->GetTarget() <= 0 )
		return;

	C_BaseEntity *pTarget = ClientEntityList().GetEnt( m_pOrder->GetTarget() );
	if( !pTarget )
		return;

	g_pMatSystemSurface->DisableClipping( true );
	
	static float flStrobeDuration = 0.5;
	float flShade = sin( gpGlobals->curtime * M_PI / flStrobeDuration ) * 0.5f + 0.5f;

	Color color(255*flShade, 0, 0, 255); 
	m_Image.SetColor( color );
	m_Image.Paint();

	g_pMatSystemSurface->DisableClipping( false );
}



enum GetTargetDescriptionType_t
{
	GETDESC_RESOURCEZONE=0,
	GETDESC_OBJECT
};

char* GetTargetDescription( int entindex, GetTargetDescriptionType_t type )
{
	static char szDesc[128];
	szDesc[0]=0;

	// Order target
	if ( entindex )
	{
		C_BaseEntity *pEnt = cl_entitylist->GetEnt( entindex );
		if ( pEnt )
		{
			if( type == GETDESC_RESOURCEZONE )
			{
				C_ResourceZone *pZone = dynamic_cast<C_ResourceZone*>(pEnt);
				if ( pZone )
					Q_strncpy( szDesc, pZone->GetTargetDescription(), sizeof( szDesc ) );
			}
			else if( type == GETDESC_OBJECT )
			{
				C_BaseObject *pObj = dynamic_cast<C_BaseObject*>( pEnt );
				if( pObj )
					Q_strncpy( szDesc, pObj->GetTargetDescription(), sizeof( szDesc ) );
			}
		}
	}

	return szDesc;
}


IMPLEMENT_CLIENTCLASS_DT(C_Order, DT_Order, COrder)
	RecvPropInt( RECVINFO(m_iPriority) ),
	RecvPropInt( RECVINFO(m_iOrderType) ),
	RecvPropInt( RECVINFO(m_iTargetEntIndex) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Order::C_Order( void )
{
	m_nHintID = TF_HINT_UNDEFINED;

	m_pNameLabel = NULL;
	
	CONSTRUCT_MINIMAP_PANEL( "minimap_order", MINIMAP_PERSONAL_ORDERS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Order::~C_Order( void )
{
	RemoveOrder();

	if( C_BaseTFPlayer::GetLocalPlayer() )
		C_BaseTFPlayer::GetLocalPlayer()->RemoveOrderTarget();
	

	if ( m_nHintID != TF_HINT_UNDEFINED )
	{
		DestroyGlobalHint( m_nHintID );
	}
}

void C_Order::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( m_nHintID != TF_HINT_UNDEFINED )
	{
		switch ( m_iOrderType )
		{
		case ORDER_REPAIR:
			m_nHintID = TF_HINT_REPAIROBJECT;
			break;
		default:
			break;
		}
	}

	if ( m_nHintID != TF_HINT_UNDEFINED )
	{
		CreateGlobalHint_Panel( m_pNameLabel, m_nHintID, NULL, index );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Order::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	pPlayer->SetPersonalOrder( this );

	// Update us if we've changed
	if( updateType == DATA_UPDATE_CREATED )
	{
		CreateStatus( GetClientModeNormal()->GetViewport() );
	}
	else
	{
		GetHudOrderList()->RecalculateOrderList();
		UpdateStatus();
	}
	
	if( m_iTargetEntIndex > 0 )
	{
		C_BaseEntity *pTarget = ClientEntityList().GetEnt( m_iTargetEntIndex );
		m_OverlayPanel.Activate( pTarget, "personal_order", true );
	}

	if ( (updateType == DATA_UPDATE_CREATED) && ( m_nHintID != TF_HINT_UNDEFINED ) )
	{
		// Wait for animation to fly all the way in
		SetNextClientThink( gpGlobals->curtime + NEW_ORDER_ANIM_DURATION + 0.25f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Clean up a removed order
//-----------------------------------------------------------------------------
void C_Order::RemoveOrder( void )
{
	m_OverlayPanel.RemoveOverlay();

	DestroyStatus();

	if ( m_pNameLabel )
	{
		delete m_pNameLabel;
		m_pNameLabel = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get a text description of this order
//-----------------------------------------------------------------------------
void C_Order::GetDescription( char *pDest, int bufferSize )
{
	char targetDesc[512];
	GetTargetDescription( targetDesc, sizeof( targetDesc ) );

	switch ( m_iOrderType )
	{
		case ORDER_ATTACK:
			Q_snprintf( pDest, bufferSize, "Attack %s", targetDesc );
			break;

		case ORDER_DEFEND:
			Q_snprintf( pDest, bufferSize, "Defend %s", targetDesc );
			break;

		case ORDER_CAPTURE:
			Q_snprintf( pDest, bufferSize, "Capture %s", targetDesc );
			break;

		default:
			Q_snprintf( pDest, bufferSize, "INVALID" );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get a text description for the target of this order
//-----------------------------------------------------------------------------
void C_Order::GetTargetDescription( char *pDest, int bufferSize )
{
	pDest[0] = 0;

	if ( !m_iTargetEntIndex )
		return;

	C_BaseEntity *pEnt = cl_entitylist->GetEnt( m_iTargetEntIndex );
	if ( !pEnt )
		return;

	C_ResourceZone *pZone = dynamic_cast<C_ResourceZone*>(pEnt);
	if ( pZone )
	{
		Q_strncpy( pDest, pZone->GetTargetDescription(), bufferSize );
	}
	else
	{
		C_BaseObject *pObj = dynamic_cast<C_BaseObject*>( pEnt );
		if( pObj )
			Q_strncpy( pDest, pObj->GetTargetDescription(), bufferSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create all elements needed in a status panel for this order
//-----------------------------------------------------------------------------
void C_Order::CreateStatus( vgui::Panel *pParent )
{
	// If we already have our elements, we're just moving. 
	if ( m_pNameLabel )
	{
		m_pNameLabel->SetParent( pParent );
	}
	else
	{
		m_pNameLabel = new COrderLabel( pParent, "NameLabel", "Temp" );
		m_pNameLabel->SetBounds( ORDERS_LEFT, ORDERS_TOP, NEW_ORDERS_WIDTH, ORDERS_ELEMENT_HEIGHT );
		m_pNameLabel->SetAutoDelete( false );
		m_pNameLabel->SetPaintBackgroundEnabled( true );
		m_pNameLabel->SetFgColor( Color( 0, 0, 0, 255 ) );
		m_pNameLabel->SetContentAlignment( vgui::Label::a_east );
		m_pNameLabel->SetTextInset( -2, 0 );
	}

	UpdateStatus();
}

//-----------------------------------------------------------------------------
// Purpose: Destroy all elements in the status panel for this order
//-----------------------------------------------------------------------------
void C_Order::DestroyStatus( void )
{
	if ( m_pNameLabel )
	{
		delete m_pNameLabel;
		m_pNameLabel = NULL;
	}
} 

//-----------------------------------------------------------------------------
// Purpose: Update all elements in the status panel for this order
//-----------------------------------------------------------------------------
void C_Order::UpdateStatus( void )
{
	if ( m_pNameLabel )
	{
		char desc[512];
		GetDescription( desc, sizeof( desc ) );
		
		m_pNameLabel->SetText( desc );
		m_pNameLabel->m_flAnimCounter = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this order wants a target reticle created around it's target
//-----------------------------------------------------------------------------
bool C_Order::ShouldDrawReticle( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Order::GetPriority( void )
{
	return m_iPriority;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Order::GetType( void )
{
	return m_iOrderType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Order::GetTarget( void )
{
	return m_iTargetEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this order is a personal one for this player
//-----------------------------------------------------------------------------
bool C_Order::IsPersonalOrder( void )
{
	return (GetPriority() == 0);
}

