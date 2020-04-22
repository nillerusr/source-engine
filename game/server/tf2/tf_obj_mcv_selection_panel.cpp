//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_obj.h"
#include "tf_shareddefs.h"
#include "vguiscreen.h"
#include "tf_vehicle_teleport_station.h"


#define MCV_SELECTION_MODEL			"models/objects/obj_resupply.mdl"
#define MCV_SELECTION_SCREEN_NAME	"screen_mcv_selection_panel"  


class CObjMCVSelectionPanel : public CBaseObject
{
public:

	DECLARE_CLASS( CObjMCVSelectionPanel, CBaseObject );
	DECLARE_SERVERCLASS();

	CObjMCVSelectionPanel();
	~CObjMCVSelectionPanel();


public:

	virtual void Spawn();
	virtual void Precache();
	virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual bool ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg );

	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
};


// This holds all the allocated CObjMCVSelectionPanels.
CUtlLinkedList<CObjMCVSelectionPanel*,int> g_MCVSelectionPanels;


LINK_ENTITY_TO_CLASS( obj_mcv_selection_panel, CObjMCVSelectionPanel );


int SendProxy_TeleportStationCount( const void *pStruct, int objectID )
{
	return CVehicleTeleportStation::GetNumDeployedTeleportStations();
}


void SendProxy_TeleportStationElement( const SendProp *pProp, const void *pStructBase, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	// Get the EHANDLE.
	EHANDLE hEnt;
	hEnt = CVehicleTeleportStation::GetDeployedTeleportStation( iElement );
	
	// Use the standard ehandle-encoding SendProxy to encode it.
	SendProxy_EHandleToInt( pProp, pStructBase, &hEnt, pOut, iElement, objectID );
}


void SignalChangeInMCVSelectionPanels()
{
}


IMPLEMENT_SERVERCLASS_ST( CObjMCVSelectionPanel, DT_MCVSelectionPanel )
	SendPropVirtualArray( 
		SendProxy_TeleportStationCount,
		32, // max # elements we'd ever send
		SendPropEHandle( "teleport_station_element", 0, 0, 0, SendProxy_TeleportStationElement ),
		"teleport_stations" )
END_SEND_TABLE()


CObjMCVSelectionPanel::CObjMCVSelectionPanel()
{
	g_MCVSelectionPanels.AddToTail( this );
}


CObjMCVSelectionPanel::~CObjMCVSelectionPanel()
{
	g_MCVSelectionPanels.FindAndRemove( this );
}


void CObjMCVSelectionPanel::Spawn()
{
	SetModel( MCV_SELECTION_MODEL );
	m_takedamage = DAMAGE_NO;
	SetType( OBJ_MCV_SELECTION_PANEL );

	BaseClass::Spawn();
}


void CObjMCVSelectionPanel::Precache()
{
	PrecacheModel( MCV_SELECTION_MODEL );
	PrecacheVGuiScreen( MCV_SELECTION_SCREEN_NAME );

	BaseClass::Precache();
}


void CObjMCVSelectionPanel::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = MCV_SELECTION_SCREEN_NAME;
}


bool CObjMCVSelectionPanel::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	if ( stricmp( pCmd, "SelectMCV" ) == 0 )
	{
		int mcvID = atoi( pArg->Argv( 1 ) );
		pPlayer->SetSelectedMCV( dynamic_cast< CVehicleTeleportStation* >( CBaseEntity::Instance( mcvID ) ) );
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}


void CObjMCVSelectionPanel::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	BaseClass::SetTransmit( pInfo, bAlways );

	// Force deployed MCVs to be sent to the client too so the client can draw their position on its vgui screen.
	int count = CVehicleTeleportStation::GetNumDeployedTeleportStations();
	for ( int i=0; i < count; i++ )
	{
		CVehicleTeleportStation::GetDeployedTeleportStation( i )->SetTransmit( pInfo, bAlways );
	}
}

