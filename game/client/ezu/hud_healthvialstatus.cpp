//=============================================================================//
//
// Purpose: A HUD element which displays active HealthVials.
//  This is a feature for Entropy : Zero Uprising Episode 2
//	Copied from Blixibon's SLAM status HUD element (hud_slamstatus.cpp)
// 
// Author: 1upD
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_basehlplayer.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudHealthVialStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHealthVialStatus, vgui::Panel );

public:
	CHudHealthVialStatus( const char *pElementName );
	~CHudHealthVialStatus();
	virtual void Init( void );
	virtual void Reset( void );
	virtual void OnThink( void );
	bool ShouldDraw();

	void MsgFunc_HealthVialConsumed( bf_read &msg );

protected:
	virtual void Paint();

private:
	CPanelAnimationVar( vgui::HFont, m_hIconFont, "IconFont", "HudNumbers" );
	CPanelAnimationVarAliasType( float, m_flIconInsetX, "IconInsetX", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconInsetY, "IconInsetY", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconGap, "IconGap", "20", "proportional_float" );

	CPanelAnimationVar( Color, m_HealthVialIconColor, "HealthVialIconColor", "255 220 0 160" );
	CPanelAnimationVar( Color, m_LastHealthVialColor, "LastHealthVialColor", "255 220 0 0" );
	
	int m_iNumHealthVials;
	bool m_bHealthVialAdded;
	bool m_bHealthVialConsumed;

	int m_iNumHealthVialsDiff;

	CPanelAnimationVarAliasType( int, m_iDefaultX, "xpos_default", "r120", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iDefaultWide, "wide_default", "104", "proportional_int" );
};	


DECLARE_HUDELEMENT( CHudHealthVialStatus );
DECLARE_HUD_MESSAGE( CHudHealthVialStatus, HealthVialConsumed );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHealthVialStatus::CHudHealthVialStatus( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudHealthVialStatus" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

CHudHealthVialStatus::~CHudHealthVialStatus()
{
	/*
	if (vgui::surface())
	{
		if (m_textureID_HealthVial != -1)
		{
			vgui::surface()->DestroyTextureID( m_textureID_HealthVial );
			m_textureID_HealthVial = -1;
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealthVialStatus::Init( void )
{
	HOOK_HUD_MESSAGE( CHudHealthVialStatus, HealthVialConsumed );
	m_iNumHealthVials = 0;
	m_bHealthVialConsumed = false;
	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealthVialStatus::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudHealthVialStatus::ShouldDraw( void )
{
	bool bNeedsDraw = false;

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	bNeedsDraw = ( pPlayer->GetAmmoCount("item_healthvial") > 0 ||
					 pPlayer->GetAmmoCount("item_healthvial") != m_iNumHealthVials || 
					m_iNumHealthVials > 0 ||
					m_LastHealthVialColor[3] > 0 );
		
	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: updates hud icons
//-----------------------------------------------------------------------------
void CHudHealthVialStatus::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int HealthVials = pPlayer->GetAmmoCount("item_healthvial");

	// Only update if we've changed vars
	if ( HealthVials == m_iNumHealthVials )
		return;

	// update status display
	if ( HealthVials > 0 )
	{
		// we have HealthVials, show the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialStatusShow" );
	}
	else if (m_LastHealthVialColor[3] <= 0)
	{
		// no HealthVials, hide the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialStatusHide" );
	}

	if ( HealthVials > m_iNumHealthVials )
	{
		//Msg( "HealthVial status thinking (Player's: %i/%i) (Ours: %i/%i)\n",
		//	HealthVials, tripmines,
		//	m_iNumHealthVials, m_iNumTripmines );

		// someone is added
		// reset the last icon color and animate
		m_LastHealthVialColor = m_HealthVialIconColor;
		m_LastHealthVialColor[3] = 0;
		m_bHealthVialAdded = true;
		m_iNumHealthVialsDiff = (m_iNumHealthVials - HealthVials);
		DevMsg( "Sequence: HealthVialAdded\n" );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialAdded" ); 
	}
	else if (HealthVials < m_iNumHealthVials)
	{
		//Msg( "HealthVial status thinking (Player's: %i/%i) (Ours: %i/%i)\n",
		//	HealthVials, tripmines,
		//	m_iNumHealthVials, m_iNumTripmines );

		// someone has left
		// reset the last icon color and animate
		m_LastHealthVialColor = m_HealthVialIconColor;
		m_bHealthVialAdded = false;
		m_iNumHealthVialsDiff = (m_iNumHealthVials - HealthVials);

		if (m_bHealthVialConsumed)
		{
			DevMsg( "Sequence: HealthVialConsumed\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialConsumed" );

			m_bHealthVialConsumed = false;
		}
		else
		{
			DevMsg( "Sequence: HealthVialLeft\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialLeft" ); 
		}
	}
	else
	{
		m_iNumHealthVialsDiff = 0;
	}

	m_iNumHealthVials = HealthVials;

	if (m_bHealthVialAdded || m_LastHealthVialColor[3] <= 0)
	{
		// Expand the menu
		switch (m_iNumHealthVials)
		{
			case 0:
			case 1:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeOne" );
				break;

			case 2:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeTwo" );
				break;

			case 3:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeThree" );
				break;

			case 4:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeFour" );
				break;

			case 5:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeFive" );
				break;

			case 6:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeSix" );
				break;

			case 7:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeSeven" );
				break;

			default:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeMax" );
			case 8:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVialElementSizeEight" );
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudHealthVialStatus::MsgFunc_HealthVialConsumed( bf_read &msg )
{
	//Msg( "HealthVial exploded\n" );
	m_bHealthVialConsumed = true;
}

//-----------------------------------------------------------------------------
// Purpose: draws the power bar
//-----------------------------------------------------------------------------
void CHudHealthVialStatus::Paint()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	/*
	if ( m_textureID_HealthVial == -1 )
	{
		m_textureID_HealthVial = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_textureID_HealthVial, "vgui/icons/HealthVial_HealthVial", true, false );
	}

	if ( m_textureID_Tripmine == -1 )
	{
		m_textureID_Tripmine = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_textureID_Tripmine, "vgui/icons/HealthVial_tripmine", true, false );
	}
	*/

	// draw the suit power bar
	//surface()->DrawSetTextColor( m_HealthVialIconColor );
	surface()->DrawSetTextFont( m_hIconFont );

	int total = m_iNumHealthVials;

	if (m_bHealthVialAdded)
	{
		if (m_iNumHealthVialsDiff < 0)
			m_iNumHealthVialsDiff = -m_iNumHealthVialsDiff;
	}
	else if (m_LastHealthVialColor[3])
	{
		// Add to the total to represent our lost HealthVials
		total += (m_iNumHealthVialsDiff);
	}

	bool bShowHighlight = (m_bHealthVialAdded || m_LastHealthVialColor[3]);
	int iHealthVialHighlight = (total - m_iNumHealthVialsDiff);

	//Msg( "HealthVial Paint: %s - (%i/%i), (%i/%i)\n", bShowHighlight ? "Showing highlight" : "Not showing highlight",
	//	m_iNumHealthVials, m_iNumHealthVialsDiff,
	//	m_iNumTripmines, m_iNumTripminesDiff );

	int xpos = m_flIconInsetX, ypos = m_flIconInsetY;
	//int iconSize = m_flIconGap - 2;
	for (int i = 0; i < total; i++)
	{
		//surface()->DrawSetColor( m_HealthVialIconColor );
		surface()->DrawSetTextColor( m_HealthVialIconColor );

		surface()->DrawSetTextPos(xpos, ypos);


		if (bShowHighlight)
		{
			if (m_iNumHealthVialsDiff != 0 && i >= iHealthVialHighlight)
			{
				//surface()->DrawSetColor( m_LastHealthVialColor );
				surface()->DrawSetTextColor( m_LastHealthVialColor );
			}
		}

		surface()->DrawUnicodeChar('S');
		//surface()->DrawSetTexture( m_textureID_HealthVial );
		//surface()->DrawTexturedRect( xpos, ypos, xpos + iconSize, ypos + iconSize );

		xpos += m_flIconGap;
	}
}


