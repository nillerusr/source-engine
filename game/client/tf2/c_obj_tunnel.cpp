//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "c_obj_mapdefined.h"
#include "minimap_trace.h"
#include <KeyValues.h>
#include "VGuiMatSurface/IMatSystemSurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectTunnel : public C_ObjectMapDefined
{
	DECLARE_CLASS( C_ObjectTunnel, C_ObjectMapDefined );
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLIENTCLASS();

	C_ObjectTunnel();

private:
	C_ObjectTunnel( const C_ObjectTunnel& src );
};

LINK_ENTITY_TO_CLASS( obj_tunnel, C_ObjectTunnel );
BEGIN_PREDICTION_DATA( C_ObjectTunnel )
END_PREDICTION_DATA();


IMPLEMENT_CLIENTCLASS_DT(C_ObjectTunnel, DT_ObjectTunnel, CObjectTunnel)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectTunnel::C_ObjectTunnel()
{
	CONSTRUCT_MINIMAP_PANEL( "obj_tunnel", MINIMAP_OBJECTS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMinimapObjectTunnelPanel : public CMinimapTraceTeamBitmapPanel
{
	DECLARE_CLASS( CMinimapObjectTunnelPanel, CMinimapTraceTeamBitmapPanel );

public:
	CMinimapObjectTunnelPanel( vgui::Panel *parent, const char *panelName ) 
	 : BaseClass( parent, "CMinimapObjectTunnelPanel" )
	{
	}

	virtual bool Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData );
	virtual void Paint();

private:
	enum
	{
		STATE_ENABLED = 0,
		STATE_DISABLED,

		NUM_STATES
	};

	CTeamBitmapImage m_TeamImage[ NUM_STATES ];
};

//-----------------------------------------------------------------------------
//
// A standard minimap renderable that displays a bitmap that changes when team changes
//
//-----------------------------------------------------------------------------
DECLARE_MINIMAP_FACTORY( CMinimapObjectTunnelPanel, "minimap_obj_tunnel_panel" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pKeyValues - 
//			pInitData - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMinimapObjectTunnelPanel::Init( KeyValues* pKeyValues, MinimapInitData_t* pInitData )
{
	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	// Load viewcone material
	KeyValues *enabled = pKeyValues->FindKey( "EnabledImage" );
	if ( enabled )
	{
		if ( !m_TeamImage[ STATE_ENABLED ].Init( this, enabled, pInitData->m_pEntity ) )
			return false;
	}

	KeyValues *disabled = pKeyValues->FindKey( "DisabledImage" );
	if ( disabled )
	{
		if ( !m_TeamImage[ STATE_DISABLED ].Init( this, disabled, pInitData->m_pEntity ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMinimapObjectTunnelPanel::Paint()
{
	// Draw the view cone
	C_BaseEntity *pEntity = GetEntity();
	Assert( pEntity );

	if ( gHUD.IsHidden( HIDEHUD_MISCSTATUS ) )
		return;

	if ( !pEntity->IsBaseObject() )
		return;

	C_BaseObject *obj = static_cast< C_BaseObject * >( pEntity );
	Assert( obj );

	bool enabled = !obj->IsDisabled();
	int image = enabled ? STATE_ENABLED : STATE_DISABLED;

	if (!m_bClipToMap)
	{
		g_pMatSystemSurface->DisableClipping( true );
	}
	m_TeamImage[ image ].SetAlpha( ComputePanelAlpha() );
	m_TeamImage[ image ].Paint();
	g_pMatSystemSurface->DisableClipping( false );
}