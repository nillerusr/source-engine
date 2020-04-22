//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CObjectSentrygun
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_baseobject.h"
#include "c_basetfplayer.h"
#include "ObjectControlPanel.h"
#include "vgui_bitmapbutton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Resupply Station object
//-----------------------------------------------------------------------------
class C_ObjectResupply : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectResupply, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectResupply();

private:
	C_ObjectResupply( const C_ObjectResupply & ); // not defined, not accessible
};


IMPLEMENT_CLIENTCLASS_DT(C_ObjectResupply, DT_ObjectResupply, CObjectResupply)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectResupply::C_ObjectResupply()
{
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CResupplyControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CResupplyControlPanel, CObjectControlPanel );

public:
	CResupplyControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnCommand( const char *command );

protected:
	virtual void OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer );

private:
	void Buy( ResupplyBuyType_t type );

	vgui::Button *m_pBuyAmmoButton;
	vgui::Button *m_pBuyGrenadesButton;
	vgui::Button *m_pBuyHealthButton;
	vgui::Button *m_pBuyAllButton;
};


DECLARE_VGUI_SCREEN_FACTORY( CResupplyControlPanel, "resupply_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CResupplyControlPanel::CResupplyControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CResupplyControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CResupplyControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Grab ahold of certain well-known controls
	m_pBuyAmmoButton = new CBitmapButton( GetActivePanel(), "BuyAmmoButton", "" );
	m_pBuyGrenadesButton = new CBitmapButton( GetActivePanel(), "BuyGrenadesButton", "" );
	m_pBuyHealthButton = new CBitmapButton( GetActivePanel(), "BuyHealthButton", "" );
	m_pBuyAllButton = new CBitmapButton( GetActivePanel(), "BuyAllButton", "" );

	return BaseClass::Init( pKeyValues, pInitData );
}


//-----------------------------------------------------------------------------
// Deactivates buttons we can't afford
//-----------------------------------------------------------------------------
void CResupplyControlPanel::OnTickActive( C_BaseObject *pObj, C_BaseTFPlayer *pLocalPlayer )
{
	BaseClass::OnTickActive( pObj, pLocalPlayer );

	int nBankResources = pLocalPlayer ? pLocalPlayer->GetBankResources() : 0;
	m_pBuyAmmoButton->SetEnabled( nBankResources >= RESUPPLY_AMMO_COST );
	m_pBuyGrenadesButton->SetEnabled( nBankResources >= RESUPPLY_GRENADES_COST );
	m_pBuyHealthButton->SetEnabled( nBankResources >= RESUPPLY_HEALTH_COST );
	m_pBuyAllButton->SetEnabled( nBankResources >= RESUPPLY_ALL_COST );
}


//-----------------------------------------------------------------------------
// Buys stuff
//-----------------------------------------------------------------------------
void CResupplyControlPanel::Buy( ResupplyBuyType_t type )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		char szbuf[48];
		Q_snprintf( szbuf, sizeof( szbuf ), "buy %d", type );
		pObj->SendClientCommand( szbuf );
	}
}


//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CResupplyControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "BuyAmmo", 8))
	{
		Buy(RESUPPLY_BUY_AMMO);
	}
	else if (!Q_strnicmp(command, "BuyHealth", 10))
	{
		Buy(RESUPPLY_BUY_HEALTH);
	}
	else if (!Q_strnicmp(command, "BuyGrenades", 12))
	{
		Buy(RESUPPLY_BUY_GRENADES);
	}
	else if (!Q_strnicmp(command, "BuyAll", 7))
	{
		Buy(RESUPPLY_BUY_ALL);
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}
