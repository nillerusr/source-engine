#include "cbase.h"
#include "c_asw_dummy_vehicle.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Dummy_Vehicle, DT_ASW_Dummy_Vehicle, CASW_Dummy_Vehicle)
	RecvPropEHandle( RECVINFO( m_hDriver ) ),
END_RECV_TABLE()

C_ASW_Dummy_Vehicle::C_ASW_Dummy_Vehicle()
{

}


C_ASW_Dummy_Vehicle::~C_ASW_Dummy_Vehicle()
{

}


// implement driver interface
C_ASW_Marine* C_ASW_Dummy_Vehicle::ASWGetDriver()
{
	return dynamic_cast<C_ASW_Marine*>(m_hDriver.Get());
}

// implement client vehicle interface
bool C_ASW_Dummy_Vehicle::s_bLoadedDriveIconTexture = false;
int  C_ASW_Dummy_Vehicle::s_nDriveIconTextureID = -1;
bool C_ASW_Dummy_Vehicle::s_bLoadedRideIconTexture = false;
int  C_ASW_Dummy_Vehicle::s_nRideIconTextureID = -1;

int C_ASW_Dummy_Vehicle::GetDriveIconTexture()
{
	if (!s_bLoadedDriveIconTexture)
	{
		// load the portrait textures
		s_nDriveIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nDriveIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedDriveIconTexture = true;
	}

	return s_nDriveIconTextureID;
}
int C_ASW_Dummy_Vehicle::GetRideIconTexture()
{
	if (!s_bLoadedRideIconTexture)
	{
		// load the portrait textures
		s_nRideIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nRideIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedRideIconTexture = true;
	}

	return s_nRideIconTextureID;
}

bool C_ASW_Dummy_Vehicle::MarineInVehicle()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	return (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->IsInVehicle());
}

const char* C_ASW_Dummy_Vehicle::GetDriveIconText()
{
	if (MarineInVehicle())
		return "Exit Vehicle";

	return "Drive";
}

const char* C_ASW_Dummy_Vehicle::GetRideIconText()
{
	if (MarineInVehicle())
		return "Exit Vehicle";

	return "Passenger";
}

void C_ASW_Dummy_Vehicle::ClientThink()
{
	if (!ASWGetDriver() || !ASWGetDriver()->GetClientsideVehicle())
	{
		UpdateVisibility();
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

bool C_ASW_Dummy_Vehicle::ShouldDraw()
{
	// don't draw this if the client has a clientside vehicle to use instead
	if (gpGlobals->maxClients > 1)
	{
		if (ASWGetDriver() && ASWGetDriver()->GetClientsideVehicle())
		{
			SetNextClientThink( CLIENT_THINK_ALWAYS );	// we need to check if we should show ourselves again
			return false;
		}
	}

	return BaseClass::ShouldDraw();
}

ShadowType_t C_ASW_Dummy_Vehicle::ShadowCastType()
{
	// don't draw this if the client has a clientside vehicle to use instead
	if (gpGlobals->maxClients > 1)
	{
		if (ASWGetDriver() && ASWGetDriver()->GetClientsideVehicle())
		{			
			return SHADOWS_NONE;
		}
	}

	return BaseClass::ShadowCastType();
}

bool C_ASW_Dummy_Vehicle::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_Dummy_Vehicle::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.iUseIconTexture = GetDriveIconTexture();
	TryLocalize( GetDriveIconText(), action.wszText, sizeof( action.wszText ) );
	action.UseTarget = GetEntity();
	action.fProgress = -1;
	action.UseIconRed = 255;
	action.UseIconGreen = 255;
	action.UseIconBlue = 255;
	action.bShowUseKey = true;
	action.iInventorySlot = -1;
	return true;
}