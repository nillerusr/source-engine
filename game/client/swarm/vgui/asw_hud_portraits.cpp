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

using namespace vgui;

#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "asw_gamerules.h"
#include "asw_hud_marine_portrait.h"

#include "vguimatsurface/imatsystemsurface.h"

#include "ConVar.h"
#include "tier0/vprof.h"

#include "asw_hud_portraits.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_HEALTH -1

extern ConVar asw_draw_hud;
ConVar asw_portraits_border("asw_portraits_border", "8", 0, "Black border around HUD portraits");
ConVar asw_portraits_border_shrink("asw_portraits_border_shrink", "0.5f", 0, "How much of the border is applied to the actual size of the portrait.");
ConVar asw_portraits_hide( "asw_portraits_hide", "0", FCVAR_NONE );

//DECLARE_HUDELEMENT( CASWHudPortraits );

CASWHudPortraits::CASWHudPortraits( const char *pElementName ) : CASW_HudElement( pElementName ), CHudNumericDisplay(NULL, "ASWHudPortraits")
{
	SetHiddenBits( HIDEHUD_REMOTE_TURRET );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_iNumMyMarines = 0;
	m_iNumOtherMarines = 0;
	m_FrameWidth = 0;
	m_CurrentY = 0;
	m_iCurrentMarine = 0;
	m_nInfestedTexture = -1;
	m_nFiringTexture = -1;
	m_nWhiteTexture = -1;
	m_iFiringMod = 0;
	m_fFiringFade = 0;
	m_fInfestedFade = 0;
	// JOYPAD REMOVED
	//m_bLastJoypadMode = engine->ASW_IsJoypadMode();
	m_bLastJoypadMode = false;

	// bar locations on our texture
	m_PortraitBar[PB_HEALTH].CornerX = 104;
	m_PortraitBar[PB_HEALTH].CornerY = 13;
	m_PortraitBar[PB_HEALTH].Width = 17;
	m_PortraitBar[PB_HEALTH].Height = 90;

	m_PortraitBar[PB_AMMO].CornerX = 87;
	m_PortraitBar[PB_AMMO].CornerY = 13;
	m_PortraitBar[PB_AMMO].Width = 12;
	m_PortraitBar[PB_AMMO].Height = 90;

	m_PortraitBar[PB_CLIPS].CornerX = 74;
	m_PortraitBar[PB_CLIPS].CornerY = 13;
	m_PortraitBar[PB_CLIPS].Width = 9;
	m_PortraitBar[PB_CLIPS].Height = 90;

	// create the portrait panels
	for (int i=0;i<ASW_MAX_PORTRAITS;i++)
	{
		m_pPortraits[i] = new CASW_HUD_Marine_Portrait(this, "Portrait", this);
		m_pPortraits[i]->SetAlpha(0);
		m_pPortraits[i]->SetVisible(false);	// portraits start out invisible, become visible when given a marine resource
	}
}


void CASWHudPortraits::Init()
{	
	Reset();
}

void CASWHudPortraits::Reset()
{
	SetLabelText(L"Portaits");
	for (int i=0;i<ASW_MAX_PORTRAITS;i++)
	{
		if (m_pPortraits[i])
			m_pPortraits[i]->SetMarineResource(NULL, i, false);
	}
}

void CASWHudPortraits::VidInit()
{
	Reset();
}

void CASWHudPortraits::UpdatePortraits(bool bResize)
{
	int iPortraitIndex = 0;

	// inform each portrait about the marine it should be displaying
	for (int i=0;i<m_iNumMyMarines;i++)
	{
		m_pPortraits[iPortraitIndex]->SetMarineResource(m_hMyMarine[i], iPortraitIndex, true, bResize);	// causes it to be positioned accordingly
		iPortraitIndex++;
		if (iPortraitIndex >= ASW_MAX_PORTRAITS)
			break;
	}
	if (iPortraitIndex < ASW_MAX_PORTRAITS)
	{
		for (int i=0;i<m_iNumOtherMarines;i++)
		{		
			m_pPortraits[iPortraitIndex]->SetMarineResource(m_hOtherMarine[i], iPortraitIndex, false, bResize);		// causes it to be positioned accordingly		
			iPortraitIndex++;
			if (iPortraitIndex >= ASW_MAX_PORTRAITS)
				break;
		}
	}
	// hide all subsequent ones
	for (int i=iPortraitIndex;i<ASW_MAX_PORTRAITS;i++)
	{
		m_pPortraits[i]->SetMarineResource(NULL, iPortraitIndex, false);
	}
}

void CASWHudPortraits::OnThink()
{
	VPROF_BUDGET( "CASWHudPortraits::OnThink", VPROF_BUDGETGROUP_ASW_CLIENT );
	
	int iNumMyMarines = 0;
	int iNumOtherMarines = 0;
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();	
	if ( local )
	{
		C_ASW_Game_Resource* pGameResource = ASWGameResource();
		if (pGameResource)
		{
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
				if (pMR)
				{
					if (pMR->GetCommander() == local)
					{
						m_hMyMarine[iNumMyMarines] = pMR;

						iNumMyMarines++;
						if (pMR->GetMarineEntity() == local->GetMarine())
							m_hCurrentlySelectedMarineResource = pMR;
					}
					else if (!(ASWGameRules() && ASWGameRules()->IsTutorialMap()))	// in tutorial, don't show marines not under your command
					{
						m_hOtherMarine[iNumOtherMarines] = pMR;
						iNumOtherMarines++;
					}
				}
			}
			// clear out future slots
			for (int i=iNumMyMarines;i<ASW_MAX_MARINE_RESOURCES;i++)
			{
				m_hMyMarine[i] == NULL;
			}
			for (int i=iNumOtherMarines;i<ASW_MAX_MARINE_RESOURCES;i++)
			{
				m_hOtherMarine[i] == NULL;
			}
		}	
	}	
	// JOYPAD REMOVED
	//bool bJoypadModeChanged = (m_bLastJoypadMode != engine->ASW_IsJoypadMode());
	//m_bLastJoypadMode = engine->ASW_IsJoypadMode();
	bool bJoypadModeChanged = m_bLastJoypadMode = false;
	bool bResize = ((m_iNumMyMarines != iNumMyMarines) || (m_iNumOtherMarines != iNumOtherMarines) || bJoypadModeChanged);
	m_iNumMyMarines = iNumMyMarines;
	m_iNumOtherMarines = iNumOtherMarines;

	UpdatePortraits(bResize);
}

void CASWHudPortraits::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,0));
}

void CASWHudPortraits::Paint()
{
	VPROF_BUDGET( "CASWHudPortraits::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	int tall;

	GetSize(m_FrameWidth,tall);
	m_CurrentY = 0;
	m_iCurrentMarine = 0;
}

void CASWHudPortraits::OnPortraitSizeChanged( CASW_HUD_Marine_Portrait *pPortrait )
{
	InvalidateLayout();
}

void CASWHudPortraits::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_bHorizontalLayout )
	{
		int border = 1;
		int panel_padding = 1;
		int x = 0;

		// count portraits
		int iNumPortraits = 0;
		for (int i=0;i<ASW_MAX_PORTRAITS;i++)
		{
			if ( !m_pPortraits[i] )
				continue;

			iNumPortraits++;
		}

		// first place your own portrait
		int iMyPortrait = -1;
		for (int i=0;i<ASW_MAX_PORTRAITS;i++)
		{
			if ( !m_pPortraits[i] )
				continue;

			if ( m_pPortraits[i]->m_bLocalMarine )
			{
				border = 10.0f * m_pPortraits[i]->m_fScale;
				panel_padding = 11 * m_pPortraits[i]->m_fScale;
				x = border;
				int w, h;
				m_pPortraits[i]->GetSize( w, h );
				m_pPortraits[i]->SetPos( x, GetTall() - ( h + border ) );
				iMyPortrait = i;
				break;
			}
		}

		// now place other portraits in reverse order from the right edge
		x = 0;
		for (int i=ASW_MAX_PORTRAITS-1; i>iMyPortrait; i--)
		{
			if ( !m_pPortraits[i] || !m_pPortraits[i]->m_hMarineResource.Get() )
				continue;

			if ( x == 0 )
			{
				border = 10.0f * m_pPortraits[i]->m_fScale;
				panel_padding = 11 * m_pPortraits[i]->m_fScale;

				x = border;
			}

			int w, h;
			m_pPortraits[i]->GetSize( w, h );
			m_pPortraits[i]->SetPos( GetWide() - ( x + w ), GetTall() - ( h + border ) );
			//Msg("putting portrait %d at %d it has width %d my width %d\n", i, GetWide() - ( x + w ), w, GetWide());
			x += w + panel_padding;
		}
	}
	else
	{
		int border = 1;
		int panel_padding = 1;
		int y = 0;

		for (int i=0;i<ASW_MAX_PORTRAITS;i++)
		{
			if ( !m_pPortraits[i] )
				continue;

			if ( y == 0 )
			{
				border = 10.0f * m_pPortraits[i]->m_fScale;
				panel_padding = 11 * m_pPortraits[i]->m_fScale;

				y = border;
			}

			int w, h;
			m_pPortraits[i]->GetSize( w, h );
			m_pPortraits[i]->SetPos( GetWide() - (w + border), y );
			y += h + panel_padding;
		}
	}
}

bool CASWHudPortraits::ShouldDraw( void )
{
	return CASW_HudElement::ShouldDraw() && asw_draw_hud.GetBool() && !asw_portraits_hide.GetBool();
}