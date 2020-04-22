//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_vguiscreen.h"
#include "clientmode_tfbase.h"
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include "vgui_bitmapbutton.h"
#include "c_info_act.h"
#include "tf_shareddefs.h"
#include "c_basetfplayer.h"

int g_ValidVehicles[] =
{
	OBJ_WAGON,
	OBJ_BATTERING_RAM,
	OBJ_VEHICLE_TANK,
	OBJ_VEHICLE_TELEPORT_STATION,
	OBJ_WALKER_STRIDER,
	OBJ_WALKER_MINI_STRIDER

	// If you add a new vehicle here, you have to add a button for it to the screen_vehicle_bay.res file.
	// The button's name must be VehicleButton%d, where %d is it's index into this array.
	// Then add the build%d command to OnCommand at the bottom of this file.
};

#define NUM_VEHICLES	ARRAYSIZE(g_ValidVehicles)

//-----------------------------------------------------------------------------
// Vgui screen handling vehicle selection in vehicle bays
//-----------------------------------------------------------------------------
class CVehicleBayVGuiScreen : public CVGuiScreenPanel
{
	DECLARE_CLASS( CVehicleBayVGuiScreen, CVGuiScreenPanel );

public:
	CVehicleBayVGuiScreen( vgui::Panel *parent, const char *panelName );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

private:
	vgui::Button *m_pVehicleButtons[ NUM_VEHICLES ];
};


//-----------------------------------------------------------------------------
// Standard VGUI panel for objects 
//-----------------------------------------------------------------------------
DECLARE_VGUI_SCREEN_FACTORY( CVehicleBayVGuiScreen, "vehicle_bay_screen" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleBayVGuiScreen::CVehicleBayVGuiScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName, g_hVGuiObjectScheme ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleBayVGuiScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Create our vehicle buttons
	for ( int i = 0; i < NUM_VEHICLES; i++ )
	{
		char ch[128];
		Q_snprintf( ch, sizeof(ch), "VehicleButton%d", i );
		m_pVehicleButtons[i] = new CBitmapButton( this, ch, "Name" );
	}

	// Load all of the controls in
	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	return true;
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CVehicleBayVGuiScreen::OnTick()
{
	BaseClass::OnTick();

	if (!GetEntity())
		return;

	C_BaseTFPlayer *pLocalPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	int nBankResources = pLocalPlayer ? pLocalPlayer->GetBankResources() : 0;

	// Set the vehicles costs
	for ( int i = 0; i < NUM_VEHICLES; i++ )
	{
		if ( !m_pVehicleButtons[i] )
			continue;

		char buf[128];
		int iCost = CalculateObjectCost( g_ValidVehicles[i], pLocalPlayer->GetNumObjects( g_ValidVehicles[i] ), pLocalPlayer->GetTeamNumber() );
		Q_snprintf( buf, sizeof( buf ), "%s : %d", GetObjectInfo( g_ValidVehicles[i] )->m_pStatusName, iCost );
		m_pVehicleButtons[i]->SetText( buf );
		// Can't build if the game hasn't started
		if ( CurrentActIsAWaitingAct() )
		{
			m_pVehicleButtons[i]->SetEnabled( false );
		}
		else
		{
			m_pVehicleButtons[i]->SetEnabled( nBankResources >= iCost );
		}
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CVehicleBayVGuiScreen::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "build", 5))
	{
		int iButton;
		int nCount = sscanf( command, "build%d", &iButton );
		if (nCount == 1)
		{
			char szbuf[64];
			Q_snprintf( szbuf, sizeof( szbuf ), "buildvehicle %d %d", GetEntity()->entindex(), g_ValidVehicles[iButton] );
  			engine->ClientCmd(szbuf);
			return;
		}
	}

	BaseClass::OnCommand(command);
}
