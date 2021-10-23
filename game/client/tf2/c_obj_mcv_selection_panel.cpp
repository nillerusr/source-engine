//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_baseobject.h"
#include "ObjectControlPanel.h"
#include "hud_minimap.h"
#include "vgui_bitmapimage.h"
#include "c_vehicle_teleport_station.h"
#include <vgui/MouseCode.h>


class C_ObjMCVSelectionPanel;
CUtlLinkedList<C_ObjMCVSelectionPanel*,int> g_SelectionPanels;


// ------------------------------------------------------------------------------------------------ //
// C_ObjMCVSelectionPanel
// ------------------------------------------------------------------------------------------------ //

class C_ObjMCVSelectionPanel : public C_BaseObject
{
public:
	
	DECLARE_CLASS( C_ObjMCVSelectionPanel, C_BaseObject );
	DECLARE_CLIENTCLASS();

	C_ObjMCVSelectionPanel();
	~C_ObjMCVSelectionPanel();

	typedef CHandle<C_VehicleTeleportStation> VehicleTeleportStationHandle;
	CUtlVector<VehicleTeleportStationHandle> m_DeployedTeleportStations;
	

private:
	static C_ObjMCVSelectionPanel *s_pSelectionPanel;

	friend void RecvProxy_TeleportStationCount( void *pStruct, int objectID, int currentArrayLength );
	friend void RecvProxy_TeleportStationElement( const CRecvProxyData *pData, void *pStruct, void *pOut );

	C_ObjMCVSelectionPanel( const C_ObjMCVSelectionPanel & );
};


void RecvProxy_TeleportStationCount( void *pStruct, int objectID, int currentArrayLength )
{
	C_ObjMCVSelectionPanel *pPanel = (C_ObjMCVSelectionPanel*)pStruct;
	if ( pPanel->m_DeployedTeleportStations.Count() != currentArrayLength )
	{
		pPanel->m_DeployedTeleportStations.SetSize( currentArrayLength );
	}
}


void RecvProxy_TeleportStationElement( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ObjMCVSelectionPanel *pPanel = (C_ObjMCVSelectionPanel*)pStruct;

	Assert( pData->m_iElement < pPanel->m_DeployedTeleportStations.Count() );
	
	RecvProxy_IntToEHandle( pData, pStruct, &pPanel->m_DeployedTeleportStations[pData->m_iElement] );
}


IMPLEMENT_CLIENTCLASS_DT( C_ObjMCVSelectionPanel, DT_MCVSelectionPanel, CObjMCVSelectionPanel )
	RecvPropVirtualArray( 
		RecvProxy_TeleportStationCount,
		32,
		RecvPropEHandle( "teleport_station_element", 0, 0, RecvProxy_TeleportStationElement ),
		"teleport_stations" )
END_RECV_TABLE()


C_ObjMCVSelectionPanel::C_ObjMCVSelectionPanel()
{
	g_SelectionPanels.AddToTail( this );
}

C_ObjMCVSelectionPanel::~C_ObjMCVSelectionPanel()
{
	g_SelectionPanels.FindAndRemove( this );
}



//-----------------------------------------------------------------------------
// CMCVMinimapPanel
//-----------------------------------------------------------------------------

class CMCVMinimapPanel : public CMinimapPanel
{
public:

	DECLARE_CLASS( CMCVMinimapPanel, CMinimapPanel );

	CMCVMinimapPanel( vgui::Panel *pParent, const char *pElementName );
	virtual ~CMCVMinimapPanel();

	virtual void Paint();
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnCursorMoved( int x, int y );


private:
	BitmapImage m_MCVImage;
	BitmapImage m_SelectedMCVImage;
	int m_LastX, m_LastY;		   
};


CMCVMinimapPanel::CMCVMinimapPanel( vgui::Panel *pParent, const char *pElementName )
	: CMinimapPanel( pElementName )
{
	SetParent( pParent );
	m_MCVImage.Init( GetVPanel(), "hud/minimap/icon_mcv_unselected" );
	m_SelectedMCVImage.Init( GetVPanel(), "hud/minimap/icon_mcv_selected" );
	m_LastX = m_LastY = 0;
}


CMCVMinimapPanel::~CMCVMinimapPanel()
{
}


void CMCVMinimapPanel::Paint()
{
	// Draw the minimap.
	BaseClass::Paint();

	// Now draw the MCVs.
	if ( g_SelectionPanels.Count() > 0 )
	{
		C_ObjMCVSelectionPanel *pPanel = g_SelectionPanels[ g_SelectionPanels.Head() ];
		C_BaseEntity *pSelectedMCV = C_BaseTFPlayer::GetLocalPlayer()->GetSelectedMCV();

		for ( int i=0; i < pPanel->m_DeployedTeleportStations.Count(); i++ )
		{
			C_VehicleTeleportStation *pStation = pPanel->m_DeployedTeleportStations[i];
			if ( pStation )
			{
				float x, y;
				if ( WorldToMinimap( MINIMAP_CLAMP, pStation->GetAbsOrigin(), x, y ) )
				{
					int size = 20;
					
					if ( pStation == pSelectedMCV )
						m_SelectedMCVImage.DoPaint( x-size/2, y-size/2, size, size );
					else
						m_MCVImage.DoPaint( x-size/2, y-size/2, size, size );
				}				
			}
		}
	}
}



void CMCVMinimapPanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	if ( code != vgui::MOUSE_LEFT )
		return;
	
	// Now draw the MCVs.
	if ( g_SelectionPanels.Count() > 0 )
	{
		C_ObjMCVSelectionPanel *pPanel = g_SelectionPanels[ g_SelectionPanels.Head() ];
		
		// Find the closest MCV to their mouse press.
		int iClosest = -1;
		float flClosest = 1e24;
		Vector2D curMousePos( m_LastX, m_LastY );

		for ( int i=0; i < pPanel->m_DeployedTeleportStations.Count(); i++ )
		{
			C_VehicleTeleportStation *pStation = pPanel->m_DeployedTeleportStations[i];
			if ( pStation )
			{
				Vector2D mcvPos;
				if ( WorldToMinimap( MINIMAP_CLAMP, pStation->GetAbsOrigin(), mcvPos.x, mcvPos.y ) )
				{
					float flTestDist = mcvPos.DistTo( curMousePos );
					if ( flTestDist < flClosest )
					{
						flClosest = flTestDist;
						iClosest = i;
					}
				}
			}
		}

		if ( iClosest != -1 && flClosest < 10 )
		{
			C_VehicleTeleportStation *pClosest = pPanel->m_DeployedTeleportStations[iClosest];

			char str[512];
			Q_snprintf( str, sizeof( str ), "SelectMCV %d", pClosest->entindex() );
			pPanel->SendClientCommand( str );
		}
	}				
}


void CMCVMinimapPanel::OnCursorMoved( int x, int y )
{
	BaseClass::OnCursorMoved( x, y );

	m_LastX = x;
	m_LastY = y;
}



// ------------------------------------------------------------------------------------------------ //
// CMCVSelectionPanel
// ------------------------------------------------------------------------------------------------ //

class CMCVSelectionPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CMCVSelectionPanel, CObjectControlPanel );


public:
	
	CMCVSelectionPanel( vgui::Panel *parent, const char *panelName );
	virtual ~CMCVSelectionPanel();
	
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnCommand( const char *command );


private:

	CMCVMinimapPanel *m_pMinimapPanel;
};


DECLARE_VGUI_SCREEN_FACTORY( CMCVSelectionPanel, "mcv_selection_panel" );


CMCVSelectionPanel::CMCVSelectionPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CMCVSelectionPanel" )
{
	m_pMinimapPanel = new CMCVMinimapPanel( this, "MinimapPanel" );
	m_pMinimapPanel->SetZPos( 10 );
	m_pMinimapPanel->Init( NULL );
}


CMCVSelectionPanel::~CMCVSelectionPanel()
{
	delete m_pMinimapPanel;
}

 
bool CMCVSelectionPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	if ( !BaseClass::Init( pKeyValues, pInitData ) )
		return false;
	
	m_pMinimapPanel->LevelInit( engine->GetLevelName() );
	m_pMinimapPanel->SetVisible( true );

	return true;
}


void CMCVSelectionPanel::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}



