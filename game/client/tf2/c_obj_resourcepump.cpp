//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_obj_resourcepump.h"
#include "commanderoverlay.h"
#include "vgui_healthbar.h"
#include "ObjectControlPanel.h"
#include "tf_shareddefs.h"
#include "vgui_bitmapbutton.h"
#include "C_Func_Resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_RECV_TABLE_NOBASE( C_ObjectResourcePump, DT_ResourcePumpTeamOnlyVars )
	RecvPropInt( RECVINFO(m_iPumpLevel) ),
	RecvPropEHandle( RECVINFO(m_hResourceZone) ),
END_RECV_TABLE()


IMPLEMENT_CLIENTCLASS_DT(C_ObjectResourcePump, DT_ResourcePump, CObjectResourcePump)
	RecvPropDataTable( "teamonly", 0, 0, &REFERENCE_RECV_TABLE( DT_ResourcePumpTeamOnlyVars ) )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectResourcePump::C_ObjectResourcePump()
{
	m_iPumpLevel = 1;
	m_pResourceBar = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectResourcePump::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "resource_pump", !bDormant );
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CResourcePumpControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CResourcePumpControlPanel, CObjectControlPanel );

public:
	CResourcePumpControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

	void		 Upgrade( void );

private:
	vgui::Button *m_pUpgradeButton;
	vgui::Label *m_pResourcesLabel;
};


DECLARE_VGUI_SCREEN_FACTORY( CResourcePumpControlPanel, "resourcepump_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CResourcePumpControlPanel::CResourcePumpControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CResourcePumpControlPanel" ) 
{
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CResourcePumpControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pUpgradeButton = new CBitmapButton( this, "UpgradeButton", "Upgrade" );
	m_pResourcesLabel = new vgui::Label( this, "ResourcesLabel", "Resources: 0" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	// ROBIN: Removed upgrading for now
	m_pUpgradeButton->SetVisible( false );

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CResourcePumpControlPanel::OnTick()
{
	BaseClass::OnTick();
	
	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_ObjectResourcePump*>(pObj) );
	C_ObjectResourcePump *pPump = static_cast<C_ObjectResourcePump*>(pObj);

	char buf[256];
	int iPumpLevel = pPump->GetLevel();
	int iCost = CalculateObjectUpgrade( OBJ_RESOURCEPUMP, iPumpLevel );
	if ( iCost )
	{
		Q_snprintf( buf, sizeof( buf ), "Upgrade to Level %d\nCost: %d", iPumpLevel+1, iCost );
	}
	else
	{
		Q_snprintf( buf, sizeof( buf ), "Level %d", iPumpLevel );
	}

	m_pUpgradeButton->SetText( buf );

	C_ResourceZone *pResourceZone = pPump->GetResourceZone();
	if (pResourceZone)
	{
		Q_snprintf( buf, sizeof( buf ), "Resources: %d", pResourceZone->m_nResourcesLeft );
		m_pResourcesLabel->SetText( buf );
	}
	else
	{
		m_pResourcesLabel->SetText( "Resources: 0" );
	}
}

//-----------------------------------------------------------------------------
// Dismantles the object 
//-----------------------------------------------------------------------------
void CResourcePumpControlPanel::Upgrade( void )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		pObj->SendClientCommand( "upgrade" );
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CResourcePumpControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Upgrade", 7))
	{
		Upgrade();
		return;
	}

	BaseClass::OnCommand(command);
}

