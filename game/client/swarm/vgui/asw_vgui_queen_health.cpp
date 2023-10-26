#include "cbase.h"
#include "asw_vgui_queen_health.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "iasw_client_vehicle.h"
#include "iclientmode.h"
#include "asw_gamerules.h"
#include "iinput.h"
#include "hud.h"
#include "c_asw_objective_kill_queen.h"
#include "c_asw_queen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_hud_alpha;

CASW_VGUI_Queen_Health_Panel::CASW_VGUI_Queen_Health_Panel( vgui::Panel *pParent, const char *pElementName, C_ASW_Queen *pQueen )
:	vgui::Panel( pParent, pElementName )
{	
	m_hQueen = pQueen;

	m_pBackdrop = new vgui::Panel(this, "Backdrop");
	for (int i=0;i<ASW_QUEEN_HEALTH_BARS;i++)
	{
		m_pHealthBar[i] = new vgui::Panel(this, "HealthBar");
	}
}

CASW_VGUI_Queen_Health_Panel::~CASW_VGUI_Queen_Health_Panel()
{
	
}

void CASW_VGUI_Queen_Health_Panel::PerformLayout()
{
	int x = ScreenWidth() * 0.25f;
	int y = ScreenHeight() * 0.02f;
	int w = ScreenWidth() * 0.6f;
	int h = ScreenHeight() * 0.04f;

	SetBounds(x, y, w, h);
		
	//PositionAroundQueen();
	UpdateBars();
}

void CASW_VGUI_Queen_Health_Panel::UpdateBars()
{	
	if (GetQueen() && GetQueen()->GetHealth() > 0 && !GetQueen()->IsDormant())
	{
		int w = GetWide();
		int h = GetTall();

		m_pBackdrop->SetBounds(0, 0, w, h);

		int padding = ScreenWidth() * 0.005f;
		int bar_width = (w - padding) / ASW_QUEEN_HEALTH_BARS;
		float health_per_bar = float(GetQueen()->GetMaxHealth()) / ASW_QUEEN_HEALTH_BARS;
		float queen_health = GetQueen()->GetHealth();
		static float flasthealth = 0;
		if (flasthealth != queen_health)
		{
			//Msg("Queen health changed.  health=%f / %d\n", queen_health, GetQueen()->GetMaxHealth());
			flasthealth = queen_health;
		}
		
		for (int i=0;i<ASW_QUEEN_HEALTH_BARS;i++)
		{
			float f = (queen_health > (health_per_bar * (i+1)) ) ? 1.0f :		// queen's health is more than this bar section, so full bar
						(queen_health - (health_per_bar * i)) / health_per_bar;		// find the health remainder into this bar and divide by the health per bar to get how full this bar should be
			m_pHealthBar[i]->SetBounds((bar_width * i + padding), padding, f * (bar_width - padding), h - (padding * 2));
		}
	}
}

void CASW_VGUI_Queen_Health_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetAlpha(255);
	SetMouseInputEnabled(false);

	m_pBackdrop->SetPaintBackgroundType(0);
	m_pBackdrop->SetPaintBackgroundEnabled(true);
	m_pBackdrop->SetBgColor(Color(0, 0, 0, 64));
	m_pBackdrop->SetVisible(false);

	for (int i=0;i<ASW_QUEEN_HEALTH_BARS;i++)
	{
		m_pHealthBar[i]->SetPaintBackgroundType(0);
		m_pHealthBar[i]->SetPaintBackgroundEnabled(true);
		m_pHealthBar[i]->SetBgColor(Color(255, 32, 0, 64));
		m_pHealthBar[i]->SetVisible(false);
	}
}

void CASW_VGUI_Queen_Health_Panel::OnThink()
{
	if (GetQueen() && GetQueen()->GetHealth() > 0 && GetQueen()->GetHealth() < GetQueen()->GetMaxHealth())
	{
		if (!m_pBackdrop->IsVisible())
		{
			m_pBackdrop->SetVisible(true);
			m_pBackdrop->SetAlpha(0);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBackdrop, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			for (int i=0;i<ASW_QUEEN_HEALTH_BARS;i++)
			{
				m_pHealthBar[i]->SetVisible(true);
				m_pHealthBar[i]->SetAlpha(0);
				vgui::GetAnimationController()->RunAnimationCommand(m_pHealthBar[i], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
		}		
		UpdateBars();
	}
	else
	{
		if (m_pBackdrop->IsVisible())
		{
			m_pBackdrop->SetVisible(false);
			for (int i=0;i<ASW_QUEEN_HEALTH_BARS;i++)
			{
				m_pHealthBar[i]->SetVisible(false);			
			}
		}
	}
}

void CASW_VGUI_Queen_Health_Panel::Paint()
{	
	BaseClass::Paint();	
}

void CASW_VGUI_Queen_Health_Panel::PaintBackground()
{	
	BaseClass::PaintBackground();
}


void CASW_VGUI_Queen_Health_Panel::PositionAroundQueen()
{	
	C_BaseEntity *pEnt = GetQueen();
	if (!pEnt)
		return;

	// find the volume this entity takes up
	Vector mins, maxs;	
	pEnt->GetRenderBoundsWorldspace(mins,maxs);


	// pull out all 8 corners of this volume
	Vector worldPos[8];
	Vector screenPos[8];
	worldPos[0] = mins;
	worldPos[1] = mins; worldPos[1].x = maxs.x;
	worldPos[2] = mins; worldPos[2].y = maxs.y;
	worldPos[3] = mins; worldPos[3].z = maxs.z;
	worldPos[4] = mins;
	worldPos[5] = maxs; worldPos[5].x = mins.x;
	worldPos[6] = maxs; worldPos[6].y = mins.y;
	worldPos[7] = maxs; worldPos[7].z = mins.z;

	// convert them to screen space
	for (int k=0;k<8;k++)
	{
		debugoverlay->ScreenPosition( worldPos[k], screenPos[k] );
	}

	// find the rectangle bounding all screen space points
	Vector topLeft = screenPos[0];
	Vector bottomRight = screenPos[0];
	for (int k=0;k<8;k++)
	{
		topLeft.x = MIN(screenPos[k].x, topLeft.x);
		topLeft.y = MIN(screenPos[k].y, topLeft.y);
		bottomRight.x = MAX(screenPos[k].x, bottomRight.x);
		bottomRight.y = MAX(screenPos[k].y, bottomRight.y);
	}
	int BracketSize = 0;	// todo: set by screen res?

	// pad it a bit
	topLeft.x -= BracketSize * 2;
	topLeft.y -= BracketSize * 2;
	bottomRight.x += BracketSize * 2;
	bottomRight.y += BracketSize * 2;

	int w = ScreenWidth() * 0.6f;
	int h = ScreenHeight() * 0.02f;
	SetBounds( (bottomRight.x + topLeft.x) * 0.5f - w * 0.5f, topLeft.y, w, h);
}