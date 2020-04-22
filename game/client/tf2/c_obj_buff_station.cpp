//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The client-side version of the portable power generator
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_baseobject.h"
#include "tf_shareddefs.h"
#include "C_BaseTFPlayer.h"
#include "ObjectControlPanel.h"
#include "vgui_bitmapbutton.h"
	    
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
//
// Portable Power Generator Class
//
class C_ObjectBuffStation : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectBuffStation, C_BaseObject );

public:

	DECLARE_CLIENTCLASS();

	C_ObjectBuffStation( void );

	virtual void	Release( void );
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	// Since we have material proxies to show building amount, don't offset origin
	virtual bool	OffsetObjectOrigin( Vector& origin )
	{
		return false;
	}

	int PlayerSocketsLeft() const { return ( BUFF_STATION_MAX_PLAYERS - m_nPlayerCount ); }
	int ObjectSocketsLeft() const { return ( BUFF_STATION_MAX_OBJECTS - m_nObjectCount ); }

	// Check if the local player is attached
	bool IsLocalPlayerAttached( void );

private:
	typedef CHandle<C_BaseTFPlayer> CPlayerHandle;
	int				m_nPlayerCount;					
	CPlayerHandle	m_hPlayers[BUFF_STATION_MAX_PLAYERS];
	CPlayerHandle	m_hOldPlayers[BUFF_STATION_MAX_PLAYERS];

	typedef CHandle<C_BaseObject> CObjectHandle;
	int				m_nObjectCount;
	CObjectHandle	m_hObjects[BUFF_STATION_MAX_OBJECTS];

private:
	C_ObjectBuffStation( const C_ObjectBuffStation & ); // not defined, not accessible
};


IMPLEMENT_CLIENTCLASS_DT( C_ObjectBuffStation, DT_ObjectBuffStation, CObjectBuffStation )
	RecvPropInt( RECVINFO( m_nPlayerCount ) ),
	RecvPropArray( RecvPropEHandle( RECVINFO( m_hPlayers[0]) ), m_hPlayers ),
	RecvPropInt( RECVINFO( m_nObjectCount ) ),
	RecvPropArray( RecvPropEHandle( RECVINFO( m_hObjects[0]) ), m_hObjects ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectBuffStation::C_ObjectBuffStation( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Check if the local player is attached
//-----------------------------------------------------------------------------
bool C_ObjectBuffStation::IsLocalPlayerAttached( void )
{
	C_BaseTFPlayer *pLocalPlayer = C_BaseTFPlayer::GetLocalPlayer();
	for ( int iPlayer = 0; iPlayer < m_nPlayerCount; ++iPlayer )
	{
		if ( m_hPlayers[iPlayer].Get() == pLocalPlayer )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectBuffStation::Release( void )
{
	// Remove any sounds for players attached
	for ( int i = 0; i < BUFF_STATION_MAX_PLAYERS; i++ )
	{
		if ( m_hPlayers[i] )
		{
			// Stop the startup, in case it's still going
			StopSound( m_hPlayers[i]->entindex(), "ObjectPortablePowerGenerator.Startup" );

			// Start the shutdown sound
			CPASAttenuationFilter filter( m_hPlayers[i], "ObjectPortablePowerGenerator.Shutdown" );
			EmitSound( filter, m_hPlayers[i]->entindex(), "ObjectPortablePowerGenerator.Shutdown" );
		}
	}

	BaseClass::Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectBuffStation::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	for ( int i = 0; i < BUFF_STATION_MAX_PLAYERS; i++ )
	{
		m_hOldPlayers[i] = m_hPlayers[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectBuffStation::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Did a player connect / disconnect?
	for ( int i = 0; i < BUFF_STATION_MAX_PLAYERS; i++ )
	{
		// Something's changed
		if ( m_hOldPlayers[i] != m_hPlayers[i] )
		{
			// Disconnected?
			if ( m_hOldPlayers[i] )
			{
				// Stop the startup, in case it's still going
				StopSound( m_hOldPlayers[i]->entindex(), "ObjectPortablePowerGenerator.Startup" );

				// Start the shutdown sound
				CPASAttenuationFilter filter( m_hOldPlayers[i], "ObjectPortablePowerGenerator.Shutdown" );
				EmitSound( filter, m_hOldPlayers[i]->entindex(), "ObjectPortablePowerGenerator.Shutdown" );
			}

			if ( m_hPlayers[i] )
			{
				// Start "buff" sound.
				CPASAttenuationFilter filter( m_hPlayers[i], "ObjectPortablePowerGenerator.Startup" );
				EmitSound( filter, m_hPlayers[i]->entindex(), "ObjectPortablePowerGenerator.Startup" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CBuffStationControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CBuffStationControlPanel, CObjectControlPanel );

public:

	CBuffStationControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

	void ConnectToStation( void );

private:
	vgui::Label		*m_pSocketsLabel;
	vgui::Button	*m_pConnectButton;
};


DECLARE_VGUI_SCREEN_FACTORY( CBuffStationControlPanel, "buffstation_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CBuffStationControlPanel::CBuffStationControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CBuffStationControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CBuffStationControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pSocketsLabel = new vgui::Label( this, "SocketReadout", "" );
	m_pConnectButton = new CBitmapButton( this, "ConnectButton", "Connect" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CBuffStationControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_ObjectBuffStation*>(pObj) );
	C_ObjectBuffStation *pStation = static_cast<C_ObjectBuffStation*>(pObj);

	char buf[256];
	int nSocketsLeft = pStation->PlayerSocketsLeft();
	if (nSocketsLeft > 0)
	{
		Q_snprintf( buf, sizeof( buf ), "%d sockets left", pStation->PlayerSocketsLeft() );
	}
	else
	{
		Q_strncpy( buf, "No sockets left", sizeof( buf ) );
	}

	m_pSocketsLabel->SetText( buf );

	// Make sure the connect/disconnect button is correct
	if ( pStation->IsLocalPlayerAttached() )
	{
		m_pConnectButton->SetText( "Disconnect from Station" );
	}
	else
	{
		m_pConnectButton->SetText( "Connect To Station" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Connect/Disconnect button
//-----------------------------------------------------------------------------
void CBuffStationControlPanel::ConnectToStation( void )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		pObj->SendClientCommand( "toggle_connect" );
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CBuffStationControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Connect", 7))
	{
		ConnectToStation();
		return;
	}

	BaseClass::OnCommand(command);
}

