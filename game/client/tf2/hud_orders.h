//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Order window
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDERS_H
#define ORDERS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include "c_order.h"
#include "hudelement.h"

class CHudLabel;

#define MAX_HUD_ORDERS		1		// Number of orders to show in the HUD

//-----------------------------------------------------------------------------
// Purpose: Panel used to display the status for an order
//-----------------------------------------------------------------------------
class CHudOrder : public vgui::Panel
{
public:
	CHudOrder( int x,int y,int wide,int tall );
	~CHudOrder( void );

	virtual void Init( void );
	virtual void Paint( void );

	// Order handling
	virtual void SetOrder( C_Order *pOrder );
	virtual C_Order *GetOrder( void );
	virtual void UpdateOrder( void );
	virtual void OrderRemoved( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	C_Order *m_pOrder;		// Order that this panel holds
};

//-----------------------------------------------------------------------------
// Purpose: Panel that holds all the individual order panels
//-----------------------------------------------------------------------------
class CHudOrderList : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudOrderList, vgui::Panel );
public:
	DECLARE_MULTIPLY_INHERITED();

	CHudOrderList( const char *pElementName );
	~CHudOrderList( void );

	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void OnThink( void );
	virtual void Paint( void );

	// Order handling
	void RecalculateOrderList( void );
	void InsertOrder( C_Order *pOrder, int iSlot );

private:
	CHudOrder			*m_pOrderPanels[ MAX_HUD_ORDERS ];
	CHudLabel			*m_pOrderLabels[ MAX_HUD_ORDERS ];
};

extern CHudOrderList *GetHudOrderList( void );

// Position & Size
// X
#define ORDERS_LEFT				(ScreenWidth() - 5 - XRES(112))
#define ORDERS_RIGHT			(ScreenWidth() - 5 )
#define ORDERS_WIDTH			(ORDERS_RIGHT - ORDERS_LEFT)
#define ORDERS_ELEMENT_LEFT		XRES(32)
#define ORDERS_ELEMENT_WIDTH	(ORDERS_RIGHT - ORDERS_ELEMENT_LEFT)
// Y
#define ORDERS_BOTTOM			YRES(412)
#define ORDERS_ELEMENT_HEIGHT	YRES(32)
#define ORDERS_TOP				(ORDERS_BOTTOM - (ORDERS_ELEMENT_HEIGHT * MAX_HUD_ORDERS))
#define ORDERS_HEIGHT			(ORDERS_BOTTOM - ORDERS_TOP)

#endif // ORDER_H
