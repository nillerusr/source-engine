#include "cbase.h"
#include "C_ASW_Computer_Area.h"
#include "c_asw_marine.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "c_asw_door.h"
#include "asw_marine_profile.h"
#include "c_asw_hack.h"
#include "asw_util_shared.h"
#include "igameevents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Computer_Area, DT_ASW_Computer_Area, CASW_Computer_Area )
	RecvPropInt			(RECVINFO(m_iHackLevel)),
	RecvPropFloat		(RECVINFO(m_fDownloadTime)),
	RecvPropBool		(RECVINFO(m_bIsLocked)),
	RecvPropBool(RECVINFO(m_bIsInUse)),
	RecvPropBool(RECVINFO(m_bWaitingForInput)),
	RecvPropFloat(RECVINFO(m_fHackProgress)),	

	RecvPropInt		(RECVINFO(m_bIsLocked)),
	RecvPropFloat		(RECVINFO(m_fHackProgress)),

	RecvPropEHandle( RECVINFO( m_hSecurityCam1 ) ),
	RecvPropEHandle( RECVINFO( m_hTurret1 ) ),

	RecvPropString( RECVINFO( m_MailFile ) ),
	RecvPropString( RECVINFO( m_NewsFile ) ),
	RecvPropString( RECVINFO( m_StocksSeed ) ),
	RecvPropString( RECVINFO( m_WeatherSeed ) ),
	RecvPropString( RECVINFO( m_PlantFile ) ),
	RecvPropString( RECVINFO( m_PDAName ) ),

	RecvPropString( RECVINFO( m_SecurityCamLabel1 ) ),
	RecvPropString( RECVINFO( m_SecurityCamLabel2 ) ),
	RecvPropString( RECVINFO( m_SecurityCamLabel3 ) ),
	RecvPropString( RECVINFO( m_TurretLabel1 ) ),
	RecvPropString( RECVINFO( m_TurretLabel2 ) ),
	RecvPropString( RECVINFO( m_TurretLabel3 ) ),

	RecvPropString( RECVINFO( m_DownloadObjectiveName ) ),
	RecvPropBool( RECVINFO(m_bDownloadedDocs) ),

	RecvPropBool		(RECVINFO(m_bSecurityCam1Locked)),	
	RecvPropBool		(RECVINFO(m_bTurret1Locked)),	
	RecvPropBool		(RECVINFO(m_bMailFileLocked)),
	RecvPropBool		(RECVINFO(m_bNewsFileLocked)),
	RecvPropBool		(RECVINFO(m_bStocksFileLocked)),
	RecvPropBool		(RECVINFO(m_bWeatherFileLocked)),
	RecvPropBool		(RECVINFO(m_bPlantFileLocked)),
END_RECV_TABLE()

bool C_ASW_Computer_Area::s_bLoadedLockedIconTexture = false;
int  C_ASW_Computer_Area::s_nLockedIconTextureID = -1;
bool C_ASW_Computer_Area::s_bLoadedOpenIconTexture = false;
int  C_ASW_Computer_Area::s_nOpenIconTextureID = -1;
bool C_ASW_Computer_Area::s_bLoadedCloseIconTexture = false;
int  C_ASW_Computer_Area::s_nCloseIconTextureID = -1;
bool C_ASW_Computer_Area::s_bLoadedUseIconTexture = false;
int  C_ASW_Computer_Area::s_nUseIconTextureID = -1;
bool C_ASW_Computer_Area::s_bLoadedHackIconTexture = false;
int  C_ASW_Computer_Area::s_nHackIconTextureID = -1;
bool C_ASW_Computer_Area::s_bLoadedUseIconPDA = false;
int  C_ASW_Computer_Area::s_nUseIconPDA = -1;

C_ASW_Computer_Area::C_ASW_Computer_Area()
{
	m_bOldWaitingForInput = false;

	m_iActiveCam = 1;	// should be set serverside and networked down depending on which cam we're using?
	m_fLastPositiveSoundTime = 0;
}

C_ASW_Door* C_ASW_Computer_Area::GetDoor()
{
	return dynamic_cast<C_ASW_Door*>(GetUseTargetHandle().Get());
}

// use icon textures

int C_ASW_Computer_Area::GetLockedIconTextureID()
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
int C_ASW_Computer_Area::GetOpenIconTextureID()
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
int C_ASW_Computer_Area::GetCloseIconTextureID()
{
	if (!s_bLoadedCloseIconTexture)
	{
		// load the portrait textures
		s_nCloseIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nCloseIconTextureID, "vgui/swarm/UseIcons/PanelNoPower", true, false);
		s_bLoadedCloseIconTexture = true;
	}

	return s_nCloseIconTextureID;
}
int C_ASW_Computer_Area::GetUseIconTextureID()
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
int C_ASW_Computer_Area::GetHackIconTextureID()
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

int C_ASW_Computer_Area::GetUseIconPDATextureID()
{
	if (!s_bLoadedUseIconPDA)
	{
		// load the portrait textures
		s_nUseIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nUseIconTextureID, "vgui/swarm/UseIcons/UseIconPDA", true, false);
		s_bLoadedUseIconPDA = true;
	}

	return s_nUseIconTextureID;
}

int C_ASW_Computer_Area::GetNumMenuOptions()
{
	int n=0;

	if (m_DownloadObjectiveName.Get()[0] != 0 && GetHackProgress() < 1.0f) n++;
	if (m_MailFile.Get()[0] != 0) n++;
	if (m_NewsFile.Get()[0] != 0) n++;
	if (m_StocksSeed.Get()[0] != 0) n++;
	if (m_WeatherSeed.Get()[0] != 0) n++;
	if (m_PlantFile.Get()[0] != 0) n++;

	if (m_hSecurityCam1.Get() != NULL) n++;	
	if (m_hTurret1.Get() != NULL) n++;
	
	if (n > 6)	// clamp it to 6 options, since that's all our UI supports
		n = 6;

	return n;
}

bool C_ASW_Computer_Area::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	CASW_Marine_Profile *pProfile = pUser->GetMarineProfile();
	bool bTech = pProfile->CanHack();

	action.UseIconRed = 255;
	action.UseIconGreen = 255;
	action.UseIconBlue = 255;
	action.bShowUseKey = true;
	action.iInventorySlot = -1;
	if (pUser->m_hCurrentHack.Get())
	{
		// if we're a tech and we're on the 'access denied' screen, then change use icon to be an 'override'
		if (bTech && pUser->m_hCurrentHack->CanOverrideHack())
		{
			action.iUseIconTexture = GetHackIconTextureID();
			TryLocalize( "#asw_override_security", action.wszText, sizeof( action.wszText ) );
			action.UseTarget = this;
			if (IsLocked())
				action.fProgress = GetTumblerProgress(pUser);
			else
				action.fProgress = GetHackProgress();
		}
		else
		{
			action.iUseIconTexture = GetHackIconTextureID();
			TryLocalize(  "#asw_log_off", action.wszText, sizeof( action.wszText ) );
			action.UseTarget = this;
			if (IsLocked())
				action.fProgress = GetTumblerProgress(pUser);
			else
				action.fProgress = GetHackProgress();
		}
	}
	else
	{
		if (IsLocked())
		{					
			if (bTech)
			{
				action.iUseIconTexture = GetHackIconTextureID();
				TryLocalize( "#asw_hack_comp", action.wszText, sizeof( action.wszText ) );
				action.UseTarget = this;
				action.fProgress = GetHackProgress();
			}
			else
			{
				action.iUseIconTexture = GetLockedIconTextureID();
				TryLocalize( "#asw_requires_tech", action.wszText, sizeof( action.wszText ) );
				action.UseTarget = this;
				action.fProgress = GetHackProgress();
			}		
		}	
		else
		{
			action.iUseIconTexture = GetUseIconTextureID();
			if ( IsPDA() )
			{
				action.iUseIconTexture = GetUseIconPDATextureID();
				TryLocalize( GetUseIconPDAText(), action.wszText, sizeof( action.wszText ) );
			}
			else
			{
				TryLocalize( GetUseIconText(), action.wszText, sizeof( action.wszText ) );
			}
			action.UseTarget = this;
			action.fProgress = -1;			
		}
	}
	return true;
}

bool C_ASW_Computer_Area::IsPDA()
{
	return m_PDAName.Get()[0] != 0;
}

float C_ASW_Computer_Area::GetTumblerProgress(C_ASW_Marine *pUser)
{
	if (!pUser || !pUser->m_hCurrentHack.Get())
		return 0;

	return pUser->m_hCurrentHack->GetTumblerProgress();
}

void C_ASW_Computer_Area::PlayPositiveSound(C_ASW_Player *pHackingPlayer)
{	
	if (gpGlobals->curtime > m_fLastPositiveSoundTime + 0.6f)
	{
		m_fLastPositiveSoundTime = gpGlobals->curtime;
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, entindex(), "ASWComputer.NumberAligned" );
	}
}

void C_ASW_Computer_Area::PlayNegativeSound(C_ASW_Player *pHackingPlayer)
{
	// none atm
}