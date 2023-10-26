#include "cbase.h"
#include "c_asw_button_area.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_asw_door.h"
#include "asw_marine_profile.h"
#include "asw_util_shared.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Button_Area, DT_ASW_Button_Area, CASW_Button_Area )
	RecvPropInt			(RECVINFO(m_iHackLevel)),
	RecvPropBool		(RECVINFO(m_bIsLocked)),
	RecvPropBool		(RECVINFO(m_bIsDoorButton)),
	RecvPropBool(RECVINFO(m_bIsInUse)),
	RecvPropFloat(RECVINFO(m_fHackProgress)),	
	RecvPropBool(RECVINFO(m_bNoPower)),
	RecvPropBool(RECVINFO(m_bWaitingForInput)),
	RecvPropString( RECVINFO( m_NoPowerMessage ) ),
END_RECV_TABLE()

bool C_ASW_Button_Area::s_bLoadedLockedIconTexture = false;
int  C_ASW_Button_Area::s_nLockedIconTextureID = -1;
bool C_ASW_Button_Area::s_bLoadedOpenIconTexture = false;
int  C_ASW_Button_Area::s_nOpenIconTextureID = -1;
bool C_ASW_Button_Area::s_bLoadedCloseIconTexture = false;
int  C_ASW_Button_Area::s_nCloseIconTextureID = -1;
bool C_ASW_Button_Area::s_bLoadedUseIconTexture = false;
int  C_ASW_Button_Area::s_nUseIconTextureID = -1;
bool C_ASW_Button_Area::s_bLoadedHackIconTexture = false;
int  C_ASW_Button_Area::s_nHackIconTextureID = -1;
bool C_ASW_Button_Area::s_bLoadedNoPowerIconTexture = false;
int  C_ASW_Button_Area::s_nNoPowerIconTextureID = -1;

C_ASW_Button_Area::C_ASW_Button_Area()
{
	m_bOldWaitingForInput = false;
}

C_ASW_Door* C_ASW_Button_Area::GetDoor()
{
	return dynamic_cast<C_ASW_Door*>(GetUseTargetHandle().Get());
}

// use icon textures

int C_ASW_Button_Area::GetLockedIconTextureID()
{
	if (!s_bLoadedLockedIconTexture)
	{
		// load the portrait textures
		s_nLockedIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nLockedIconTextureID, "vgui/swarm/UseIcons/PanelLocked", true, false);
		s_bLoadedLockedIconTexture = true;
	}

	return s_nLockedIconTextureID;
}
int C_ASW_Button_Area::GetOpenIconTextureID()
{
	if (!s_bLoadedOpenIconTexture)
	{
		// load the portrait textures
		s_nOpenIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nOpenIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedOpenIconTexture = true;
	}

	return s_nOpenIconTextureID;
}
int C_ASW_Button_Area::GetCloseIconTextureID()
{
	if (!s_bLoadedCloseIconTexture)
	{
		// load the portrait textures
		s_nCloseIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nCloseIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedCloseIconTexture = true;
	}

	return s_nCloseIconTextureID;
}
int C_ASW_Button_Area::GetUseIconTextureID()
{
	if (!s_bLoadedUseIconTexture)
	{
		// load the portrait textures
		s_nUseIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nUseIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedUseIconTexture = true;
	}

	return s_nUseIconTextureID;
}
int C_ASW_Button_Area::GetHackIconTextureID()
{
	if (!s_bLoadedHackIconTexture)
	{
		// load the portrait textures
		s_nHackIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nHackIconTextureID, "vgui/swarm/UseIcons/PanelLocked", true, false);
		s_bLoadedHackIconTexture = true;
	}

	return s_nHackIconTextureID;
}

int C_ASW_Button_Area::GetNoPowerIconTextureID()
{
	if (!s_bLoadedNoPowerIconTexture)
	{
		// load the portrait textures
		s_nNoPowerIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nNoPowerIconTextureID, "vgui/swarm/UseIcons/PanelNoPower", true, false);
		s_bLoadedNoPowerIconTexture = true;
	}

	return s_nNoPowerIconTextureID;
}

bool C_ASW_Button_Area::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.UseIconRed = 255;
	action.UseIconGreen = 255;
	action.UseIconBlue = 255;
	action.bShowUseKey = true;
	action.iInventorySlot = -1;
	if (!HasPower())
	{
		action.iUseIconTexture = GetNoPowerIconTextureID();
		TryLocalize( GetNoPowerText(), action.wszText, sizeof( action.wszText ) );
		action.UseTarget = this;
		action.fProgress = GetHackProgress();
		action.bShowUseKey = false;
		return true;
	}
	if (IsLocked())
	{
		CASW_Marine_Profile *pProfile = pUser->GetMarineProfile();

		if ( pProfile->CanHack() )
		{
			action.iUseIconTexture = GetHackIconTextureID();
			TryLocalize( GetHackIconText(pUser), action.wszText, sizeof( action.wszText ) );
			action.UseTarget = this;
			action.fProgress = GetHackProgress();
		}
		else
		{
			action.iUseIconTexture = GetLockedIconTextureID();
			TryLocalize( GetLockedIconText(), action.wszText, sizeof( action.wszText ) );
			action.UseTarget = this;
			action.fProgress = GetHackProgress();
		}	
	}

	else
	{
		action.iUseIconTexture = GetUseIconTextureID();
		TryLocalize( GetUseIconText(), action.wszText, sizeof( action.wszText ) );
		action.UseTarget = this;
		action.fProgress = -1;
	}
	return true;
}

const char* C_ASW_Button_Area::GetNoPowerText()
{
	const char *szCustom = GetNoPowerMessage();
	if (!szCustom || Q_strlen(szCustom) <= 0)
		return "#asw_no_power";

	return szCustom;
}

const char* C_ASW_Button_Area::GetHackIconText(C_ASW_Marine *pUser)
{
	if (m_bIsInUse)
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->m_hUsingEntity.Get() == this)
		{
			return "#asw_exit_panel";
		}
	}
	return "#asw_hack_panel";
}