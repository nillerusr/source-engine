//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client Side COrder class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_H
#define C_ORDER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include "CommanderOverlay.h"
#include "hud_minimap.h"

class COrderLabel;


//-----------------------------------------------------------------------------
// Purpose: Datatable container class for orders
//-----------------------------------------------------------------------------
class C_Order : public C_BaseEntity
{
	DECLARE_CLASS( C_Order, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_Order( void );
	~C_Order( void );

	virtual void	ClientThink( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	RemoveOrder( void );
	virtual void	GetDescription( char *pDest, int bufferSize );
	virtual void	GetTargetDescription( char *pDest, int bufferSize );

	// Status drawing
	virtual void	CreateStatus( vgui::Panel *pParent );
	virtual void	DestroyStatus( void );
	virtual void	UpdateStatus( void );
	virtual bool	ShouldDrawReticle( void );

	// Data access
	int				GetPriority( void );
	int				GetTarget( void );
	int				GetType( void );
	bool			IsPersonalOrder( void );

protected:
	// Received via datatable
	int				m_iPriority;
	int				m_iOrderType;
	int				m_iTargetEntIndex;

	// Used in status drawing
	COrderLabel		*m_pNameLabel;

	// Animating panel to show the new order.
	float			m_flNewOrderHighlightTimer;

	// Hook up the overlay on the tactical map.
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL();

protected:
	int				m_nHintID;
};


#endif // C_ORDER_H
