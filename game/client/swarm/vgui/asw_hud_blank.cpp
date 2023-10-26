
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "asw_hudelement.h"
#include "hud_numericdisplay.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"

#include "ConVar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_HEALTH -1

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CASWHudHealth : public CASW_HudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CASWHudHealth, CHudNumericDisplay );

public:
	CASWHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
			void MsgFunc_Damage( bf_read &msg );

	//virtual void DrawTexturedBox(int x, int y, int wide, int tall, Color color, float normalizedAlpha );
	virtual void ApplySchemeSettings(IScheme *pScheme);

private:
	// old variables
	int		m_iHealth;
	
	int		m_bitsDamage;

	//int m_nSwarmBackgroundID;
};	

DECLARE_HUDELEMENT( CASWHudHealth );
DECLARE_HUD_MESSAGE( CASWHudHealth, Damage );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASWHudHealth::CASWHudHealth( const char *pElementName ) : CASW_HudElement( pElementName ), CHudNumericDisplay(NULL, "ASWHudHealth")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	//SetPaintBackgroundType(1);	// single texture
	//m_nSwarmBackgroundID = vgui::surface()->CreateNewTextureID();
	//vgui::surface()->DrawSetTextureFile( m_nSwarmBackgroundID, "vgui/swarm/HUD/ASWHUDNameHealth" , true, false);
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudHealth::Init()
{
	HOOK_HUD_MESSAGE( CASWHudHealth, Damage );
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudHealth::Reset()
{
	m_iHealth		= INIT_HEALTH;
	m_bitsDamage	= 0;

	SetLabelText(L"BASTILLE");
	/*wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_HEALTH");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"HEALTH");
	}*/
	SetDisplayValue(m_iHealth);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudHealth::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudHealth::OnThink()
{
	int newHealth = 0;
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{			
			newHealth = MAX( marine->GetHealth(), 0 );
			if (marine->GetMarineResource())
			{
				CASW_Marine_Profile* profile = local->GetMarine()->GetMarineResource()->GetProfile();
				if (profile)
				{
					char lower[32];
					strcpy(lower, profile->m_ShortName);
					strupr(lower);
					wchar_t szconverted[32];
					const wchar_t* label = &szconverted[0];
					g_pVGuiLocalize->ConvertANSIToUnicode( lower, szconverted, sizeof(szconverted)  );
					SetLabelText(label);
					
				}
			}
		}
		else
		{
			newHealth = 0;
			SetLabelText(L" ");
		}		
	}

	// Only update the fade if we've changed health
	if ( newHealth == m_iHealth )
	{
		return;
	}

	m_iHealth = newHealth;

	if ( m_iHealth >= 20 )
	{
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedAbove20");
	}
	else if ( m_iHealth > 0 )
	{
		GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedBelow20");
	}

	SetDisplayValue(m_iHealth);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASWHudHealth::MsgFunc_Damage( bf_read &msg )
{

	int armor = msg.ReadByte();	// armor
	int damageTaken = msg.ReadByte();	// health
	long bitsDamage = msg.ReadLong(); // damage bits
	bitsDamage; // variable still sent but not used

	Vector vecFrom;

	vecFrom.x = msg.ReadBitCoord();
	vecFrom.y = msg.ReadBitCoord();
	vecFrom.z = msg.ReadBitCoord();

	// Actually took damage?
	if ( damageTaken > 0 || armor > 0 )
	{
		if ( damageTaken > 0 )
		{
			// start the animation
			GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("HealthDamageTaken");

			// see if our health is low
			if ( m_iHealth > 0 && m_iHealth < 20 )
			{
				GetClientMode()->GetViewportAnimationController()->StartAnimationSequence("HealthLow");
			}
		}
	}
}
/*
void CASWHudHealth::DrawTexturedBox(int x, int y, int wide, int tall, Color color, float normalizedAlpha )
{
	if ( m_nSwarmBackgroundID == -1 )
		return;

	color[3] *= normalizedAlpha;

	surface()->DrawSetColor( color );
	surface()->DrawSetTexture(m_nSwarmBackgroundID);
	surface()->DrawTexturedRect(x, y, x + wide, y + tall);
}*/

void CASWHudHealth::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(255,255,255,255));
}