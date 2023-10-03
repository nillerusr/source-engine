#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui/ILocalize.h>
#include <filesystem.h>
#include <keyvalues.h>
#include "hud_numericdisplay.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "iasw_client_usable_entity.h"
#include "asw_shareddefs.h"
#include "precache_register.h"
#include "tier0/vprof.h"
#include "c_asw_hack.h"
#include "asw_hud_use_icon.h"
#include <vgui_controls/Label.h>
#include "asw_hudelement.h"
#include "asw_hud_use_area.h"
#include "clientmode_asw.h"
#include "asw_gamerules.h"

#include "ConVar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheUseIcons )
//PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconProgressBar" )

PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/PanelLocked" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/PanelUnlocked" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/PanelNoPower" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconDoorPartlySealed" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconDoorFullySealed" )

PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeAmmoDrop" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeRifleAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeAutogunAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeShotgunAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeVindicatorAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeFlamerAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakePistolAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeMiningLaserAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakePDWAmmo" )
PRECACHE( MATERIAL, "VGUI/swarm/UseIcons/UseIconTakeRailgunAmmo" )


PRECACHE_REGISTER_END()

extern ConVar asw_draw_hud;
extern ConVar asw_debug_hud;
extern int g_asw_iGUIWindowsOpen;

using namespace vgui;

CASWHudCustomPaintPanel::CASWHudCustomPaintPanel( vgui::Panel* pParent, const char *pElementName ) : vgui::Panel(pParent, pElementName)
{
	m_hUsable = NULL;
	m_pHudParent = dynamic_cast<CASWHudUseArea*>(pParent);
}

void CASWHudCustomPaintPanel::Paint()
{
	if (m_hUsable.Get() && m_pHudParent)
	{
		IASW_Client_Usable_Entity* pUsable = dynamic_cast<IASW_Client_Usable_Entity*>(m_hUsable.Get());
		if (pUsable)
		{
			int ix = 0;
			int iy = 0;
			if (m_pHudParent->m_pUseIcon)
			{
				m_pHudParent->m_pUseIcon->GetPos(ix, iy);				
				ix += m_pHudParent->m_pUseIcon->m_iImageX;
				iy += m_pHudParent->m_pUseIcon->m_iImageY;
			}
			pUsable->CustomPaint( ix, iy, m_pHudParent->GetUseIconAlpha(), m_pHudParent->m_pUseIcon );
		}
	}
}

void CASWHudCustomPaintPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetParent()->GetSize( w, h );
	SetBounds( 0, 0, w, h );
}

DECLARE_HUDELEMENT( CASWHudUseArea );

CASWHudUseArea::CASWHudUseArea( const char *pElementName ) : vgui::Panel(GetClientMode()->GetViewport(), "ASWHudUseArea"), CASW_HudElement( pElementName )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	LoadUseTextures();
	m_pUseIcon = new CASW_HUD_Use_Icon(this, "UseIcon");
	m_pCustomPaintPanel = new CASWHudCustomPaintPanel(this, "CustomPaint");
	ListenForGameEvent( "gameui_hidden" );
}

bool CASWHudUseArea::ShouldDraw()
{	
	// don't show use icon while chatting
	vgui::Panel *pChatInput = GetClientModeASW()->GetMessagePanel();
	if ( pChatInput && pChatInput->GetAlpha() > 0 )
		return false;

	if ( ASWGameRules() && ASWGameRules()->GetMarineDeathCamInterp() > 0.0f )
		return false;

	 return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw();
}

void CASWHudUseArea::LevelInit()
{
	CASW_HudElement::LevelInit();

	if (m_pUseIcon)
	{
		m_pUseIcon->ClearUseAction();
		m_pUseIcon->FindUseKeyBind();
	}
}

void CASWHudUseArea::LevelShutdown()
{
	if (m_pUseIcon)
		m_pUseIcon->ClearUseAction();
}

CASWHudUseArea::~CASWHudUseArea()
{
	
}

void CASWHudUseArea::Init()
{
	Reset();
}

void CASWHudUseArea::Reset()
{	

}

void CASWHudUseArea::VidInit()
{
	Reset();
}

void CASWHudUseArea::ApplySchemeSettings(IScheme *pScheme)
{
	//Msg("CASWHudUseArea::ApplySchemeSettings\n");
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,0));
	SetFgColor(Color(255,255,255,255));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudUseArea::FireGameEvent( IGameEvent *event )
{
	const char * type = event->GetName();

	if ( Q_strcmp(type, "gameui_hidden") == 0 )
	{
		// update keybinds shown on the HUD
		engine->ClientCmd("asw_update_binds");
	}
}

void CASWHudUseArea::Paint()
{
	VPROF_BUDGET( "CASWHudUseArea::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	GetSize(m_iFrameWidth,m_iFrameHeight);
	m_iFrameWidth = ScreenWidth();
	BaseClass::Paint();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;
	int r, g, b;
	r = g = b = 255;
	if (asw_debug_hud.GetBool())
	{
		char cbuffer[20];
		Q_snprintf(cbuffer, sizeof(cbuffer), "Usable = %d", pPlayer->m_iUseEntities);		
		g_pMatSystemSurface->DrawColoredText(m_hUseAreaFont, 0, 0, r, g, 
				b, 200, &cbuffer[0]);
	}
	
	for (int k=0; k<3; k++)
	{
		pPlayer->UseIconTarget[k] = NULL;
	}

	m_iNumUseIcons = 0;
	pPlayer->FindUseEntities();

	// don't add any icons if our marine is dead
	if (!pPlayer->GetMarine() || !pPlayer->GetMarine()->IsAlive())
	{
		pPlayer->UseIconTarget[0] = NULL;
		if (m_pUseIcon)
			m_pUseIcon->ClearUseAction();
		return;
	}

	int iNumToDraw = MIN(1, pPlayer->m_iUseEntities);
	int iDrew = 0;
	for (int i=0; i<iNumToDraw; i++)
	{	
		if (AddUseIconsFor(pPlayer->GetUseEntity(i)))
			iDrew++;
		
		if (asw_debug_hud.GetBool())
		{
			char buffer[20];
			C_BaseEntity *pEnt = pPlayer->GetUseEntity(i);
			Q_snprintf(buffer, sizeof(buffer), "Use:%d", pEnt->entindex());
			//int wide = g_pMatSystemSurface->DrawTextLen(m_hUseAreaFont, &buffer[0]);
			int tall = vgui::surface()->GetFontTall( m_hUseAreaFont );
			float xPos		= 0;
			float yPos		= (i+1) * tall;
			
			// actual text
			g_pMatSystemSurface->DrawColoredText(m_hUseAreaFont, xPos, yPos, r, g, 
				b, 200, &buffer[0]);
		}
	}
	if (iDrew < 1)
	{
		if (m_pUseIcon)
			m_pUseIcon->ClearUseAction();
	}
}

bool CASWHudUseArea::AddUseIconsFor(C_BaseEntity* pEnt)
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return false;
	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return false;

	if (g_asw_iGUIWindowsOpen > 0)	// don't show use icons while an info message is up
		return false;

	IASW_Client_Usable_Entity* pUsable = dynamic_cast<IASW_Client_Usable_Entity*>(pEnt);
	if (pUsable)
	{
		ASWUseAction action;
		if (!pUsable->GetUseAction(action, pMarine))
			return false;

		if (m_pUseIcon)
			m_pUseIcon->SetUseAction(action);

		pPlayer->UseIconTarget[0] = action.UseTarget;

		if (m_pUseIcon && m_pUseIcon->m_pUseText && m_pUseIcon->m_CurrentAction == action)
		{
			if (m_pCustomPaintPanel)
				m_pCustomPaintPanel->m_hUsable = pUsable->GetEntity();
		}
		return true;
	}	
	return false;
}

void CASWHudUseArea::AddUseIcon(int iUseIconTexture, const char *pText,
									 EHANDLE UseTarget, float fProgress)
{
	int icon_width = m_iFrameWidth * 0.225f;
	int x = icon_width * m_iNumUseIcons * 1.4f;
	int y_offset = icon_width * 0.4f;
	if ( iUseIconTexture != -1 )
	{
		surface()->DrawSetColor(Color(255,255,255,255));
		surface()->DrawSetTexture(iUseIconTexture);
		surface()->DrawTexturedRect(x, y_offset,
									x + icon_width, y_offset + icon_width);
	}
	// if a progress bar was specified, draw that texture and blank out the bar part
	if (fProgress != -1)
	{
		surface()->DrawSetColor(Color(255,255,255,255));
		surface()->DrawSetTexture(iUseIconTexture); //m_iProgressBarTexture); - removed this..
		surface()->DrawTexturedRect(x + icon_width, y_offset,
									x + icon_width + (icon_width * 0.5f), y_offset + icon_width);
		// blank out our percent
		fProgress = 1.0f - fProgress;
		int blank_x = icon_width * 0.1225;	// 0.0625;
		int blank_y = fProgress * icon_width * 0.6946875; // 0.6796875;
		int bar_offset_y = icon_width * 0.0815625;	// 0.1015625;
		int bar_offset_x = icon_width * 0.125; // 0.125;
		surface()->DrawSetColor(Color(0,0,0,255));
		surface()->DrawTexturedRect(x + icon_width + bar_offset_x, y_offset + bar_offset_y,
			x + icon_width + bar_offset_x + blank_x, y_offset + bar_offset_y + blank_y);
	}
	int r, g, b;
	r = g = b = 255;	
	int y = 0;
	if (m_iNumUseIcons % 2 == 0)
	{
		int font_tall = vgui::surface()->GetFontTall( m_hUseAreaFont );
		y += font_tall;
	}
	char cbuffer[64];
	Q_snprintf(cbuffer, sizeof(cbuffer), "%s", pText);
	// drop shadow
	g_pMatSystemSurface->DrawColoredText(m_hUseAreaFont, x+1, y+1, 0, 0, 
		0, 200, cbuffer);
	// actual text
	g_pMatSystemSurface->DrawColoredText(m_hUseAreaFont, x, y, r, g, 
		b, 200, cbuffer);
	
	if (m_iNumUseIcons < 3)
	{
		C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
		if ( local )
		{
			local->UseIconTarget[m_iNumUseIcons] = UseTarget;
		}
	}

	m_iNumUseIcons++;
}

void CASWHudUseArea::LoadUseTextures()
{
	//m_iProgressBarTexture = vgui::surface()->CreateNewTextureID();
	//vgui::surface()->DrawSetTextureFile( m_iProgressBarTexture, "vgui/swarm/UseIcons/UseIconProgressBar", true, false);
}

int CASWHudUseArea::GetUseIconAlpha()
{
	if (m_pUseIcon && m_pUseIcon->m_pUseText)
		return m_pUseIcon->m_pUseText->GetAlpha();

	return 0;
}

void asw_update_binds_f()
{
	CASWHudUseArea *pUseArea = GET_HUDELEMENT( CASWHudUseArea );
	if ( pUseArea && pUseArea->m_pUseIcon )
	{
		pUseArea->m_pUseIcon->FindUseKeyBind();
	}
}

static ConCommand asw_update_binds("asw_update_binds", asw_update_binds_f, "Makes ASI HUD elements update the detected key for various actions (e.g. the use key on use icons)", 0);