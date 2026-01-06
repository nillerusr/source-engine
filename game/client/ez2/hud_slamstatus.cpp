//================= Copyright LOLOLOLOL, All rights reserved. ==================//
//
// Purpose: A HUD element which displays active SLAMs. Based on the squad status indicator code.
// 
// Author: Blixibon
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
class CHudSLAMStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudSLAMStatus, vgui::Panel );

public:
	CHudSLAMStatus( const char *pElementName );
	~CHudSLAMStatus();
	virtual void Init( void );
	virtual void Reset( void );
	virtual void OnThink( void );
	bool ShouldDraw();

	void MsgFunc_SLAMExploded( bf_read &msg );

protected:
	virtual void Paint();

private:
	CPanelAnimationVar( vgui::HFont, m_hIconFont, "IconFont", "HudNumbers" );
	CPanelAnimationVarAliasType( float, m_flIconInsetX, "IconInsetX", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconInsetY, "IconInsetY", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconGap, "IconGap", "20", "proportional_float" );

	CPanelAnimationVar( Color, m_SLAMIconColor, "SLAMIconColor", "255 220 0 160" );
	CPanelAnimationVar( Color, m_LastSLAMColor, "LastSLAMColor", "255 220 0 0" );
	
	int m_iNumSatchels;
	int m_iNumTripmines;
	int m_iNumDetonatables;
	bool m_bSLAMAdded;
	bool m_bSLAMExploded;
	bool m_bSLAMExplodedFromNonPlayer;

	int m_iNumSatchelsDiff;
	int m_iNumTripminesDiff;
	int m_iNumDetonatablesDiff;

	//int m_textureID_Satchel;
	//int m_textureID_Tripmine;

	CPanelAnimationVarAliasType( int, m_iDefaultX, "xpos_default", "r120", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iDefaultWide, "wide_default", "104", "proportional_int" );
};	


DECLARE_HUDELEMENT( CHudSLAMStatus );
DECLARE_HUD_MESSAGE( CHudSLAMStatus, SLAMExploded );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudSLAMStatus::CHudSLAMStatus( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudSLAMStatus" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

CHudSLAMStatus::~CHudSLAMStatus()
{
	/*
	if (vgui::surface())
	{
		if (m_textureID_Satchel != -1)
		{
			vgui::surface()->DestroyTextureID( m_textureID_Satchel );
			m_textureID_Satchel = -1;
		}

		if (m_textureID_Tripmine != -1)
		{
			vgui::surface()->DestroyTextureID( m_textureID_Tripmine );
			m_textureID_Tripmine = -1;
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSLAMStatus::Init( void )
{
	HOOK_HUD_MESSAGE( CHudSLAMStatus, SLAMExploded );
	m_iNumSatchels = 0;
	m_iNumTripmines = 0;
	m_iNumDetonatables = 0;
	m_bSLAMExploded = false;
	m_bSLAMExplodedFromNonPlayer = false;
	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSLAMStatus::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudSLAMStatus::ShouldDraw( void )
{
	bool bNeedsDraw = false;

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	bNeedsDraw = ( (pPlayer->m_HL2Local.m_iSatchelCount > 0 || pPlayer->m_HL2Local.m_iTripmineCount > 0 || pPlayer->m_HL2Local.m_iDetonatableCount > 0) ||
					( pPlayer->m_HL2Local.m_iSatchelCount != m_iNumSatchels || pPlayer->m_HL2Local.m_iSatchelCount != m_iNumTripmines || pPlayer->m_HL2Local.m_iDetonatableCount != m_iNumDetonatables ) ||
					( m_iNumSatchels > 0 || m_iNumTripmines > 0 || m_iNumDetonatables > 0 ) ||
					( m_LastSLAMColor[3] > 0 ) );
		
	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: updates hud icons
//-----------------------------------------------------------------------------
void CHudSLAMStatus::OnThink( void )
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	int satchels = pPlayer->m_HL2Local.m_iSatchelCount;
	int tripmines = pPlayer->m_HL2Local.m_iTripmineCount;
	int detonatables = pPlayer->m_HL2Local.m_iDetonatableCount;

	// Only update if we've changed vars
	if ( satchels == m_iNumSatchels && tripmines == m_iNumTripmines && detonatables == m_iNumDetonatables )
		return;

	// update status display
	if ( satchels > 0 || tripmines > 0 || detonatables > 0 )
	{
		// we have SLAMs, show the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMStatusShow" );
	}
	else if (m_LastSLAMColor[3] <= 0)
	{
		// no SLAMs, hide the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMStatusHide" );
	}

	if ( satchels > m_iNumSatchels || tripmines > m_iNumTripmines || detonatables > m_iNumDetonatables )
	{
		//Msg( "SLAM status thinking (Player's: %i/%i) (Ours: %i/%i)\n",
		//	satchels, tripmines,
		//	m_iNumSatchels, m_iNumTripmines );

		// someone is added
		// reset the last icon color and animate
		m_LastSLAMColor = m_SLAMIconColor;
		m_LastSLAMColor[3] = 0;
		m_bSLAMAdded = true;
		m_iNumSatchelsDiff = (m_iNumSatchels - satchels);
		m_iNumTripminesDiff = (m_iNumTripmines - tripmines);
		m_iNumDetonatablesDiff = (m_iNumDetonatables - detonatables);
		Msg( "Sequence: SLAMAdded\n" );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMAdded" ); 
	}
	else if (satchels < m_iNumSatchels || tripmines < m_iNumTripmines || detonatables > m_iNumDetonatables)
	{
		//Msg( "SLAM status thinking (Player's: %i/%i) (Ours: %i/%i)\n",
		//	satchels, tripmines,
		//	m_iNumSatchels, m_iNumTripmines );

		// someone has left
		// reset the last icon color and animate
		m_LastSLAMColor = m_SLAMIconColor;
		m_bSLAMAdded = false;
		m_iNumSatchelsDiff = (m_iNumSatchels - satchels);
		m_iNumTripminesDiff = (m_iNumTripmines - tripmines);
		m_iNumDetonatablesDiff = (m_iNumDetonatables - detonatables);
		if (m_bSLAMExploded)
		{
			if (m_bSLAMExplodedFromNonPlayer)
			{
				Msg( "Sequence: SLAMDied\n" );
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMDied" );
				m_bSLAMExplodedFromNonPlayer = false;
			}
			else
			{
				Msg( "Sequence: SLAMExploded\n" );
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMExploded" );
			}

			m_bSLAMExploded = false;
		}
		else
		{
			Msg( "Sequence: SLAMLeft\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMLeft" ); 
		}
	}
	else
	{
		m_iNumSatchelsDiff = 0;
		m_iNumTripminesDiff = 0;
		m_iNumDetonatablesDiff = 0;
	}

	m_iNumSatchels = satchels;
	m_iNumTripmines = tripmines;
	m_iNumDetonatables = detonatables;

	if (m_bSLAMAdded || m_LastSLAMColor[3] <= 0)
	{
		// Expand the menu
		int total = m_iNumSatchels + m_iNumTripmines + m_iNumDetonatables;
		switch (total)
		{
			case 0:
			case 1:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeOne" );
				break;

			case 2:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeTwo" );
				break;

			case 3:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeThree" );
				break;

			case 4:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeFour" );
				break;

			case 5:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeFive" );
				break;

			case 6:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeSix" );
				break;

			case 7:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeSeven" );
				break;

			default:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeMax" );
			case 8:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SLAMElementSizeEight" );
				break;
		}
	}

	// Method 1: hudanimations.txt creator (currently used)
	/*
	int add = 24; // m_flIconGap
	for (int i = 0; i <= 8; i++)
	{
		const char *pszName = "0";
		switch (i)
		{
			case 1:
				pszName = "SLAMElementSizeOne";
				break;

			case 2:
				pszName = "SLAMElementSizeTwo";
				break;

			case 3:
				pszName = "SLAMElementSizeThree";
				break;

			case 4:
				pszName = "SLAMElementSizeFour";
				break;

			case 5:
				pszName = "SLAMElementSizeFive";
				break;

			case 6:
				pszName = "SLAMElementSizeSix";
				break;

			case 7:
				pszName = "SLAMElementSizeSeven";
				break;

			case 8:
				pszName = "SLAMElementSizeEight";
				break;
		}

		Msg( "\n%s\n{\n	Animate HudSLAMStatus		XPos	\"r%i\"	Linear	0.0		0.3\n	Animate HudSLAMStatus		Wide	\"%i\"	Deaccel	0.0		0.3\n}\n",
			pszName, (add * i+1) + 20, (add * i+1) + 4 );
	}
	*/

	// Method 2: Dynamic expansion (not currently used)
	/*
	int totalChange = -(m_iNumSatchelsDiff + m_iNumTripminesDiff + m_iNumDetonatablesDiff);
	//if (totalChange != 0)
	{
		int expand = (48 * totalChange) + m_flIconInsetX;

		int x, y, wide, tall;
		GetBounds( x, y, wide, tall );

		Msg( "Expanding to x %i, width %i\n", x - expand, wide + expand );

		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "XPos", Color( x - expand, 0, 0, 0 ), 0.0f, 0.3f, AnimationController::INTERPOLATOR_LINEAR );
		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "Wide", Color( wide + expand, 0, 0, 0 ), 0.0f, 0.3f, AnimationController::INTERPOLATOR_DEACCEL );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudSLAMStatus::MsgFunc_SLAMExploded( bf_read &msg )
{
	//Msg( "SLAM exploded\n" );

	m_bSLAMExploded = true;

	if (msg.ReadByte() == 1)
	{
		m_bSLAMExplodedFromNonPlayer = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the power bar
//-----------------------------------------------------------------------------
void CHudSLAMStatus::Paint()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	/*
	if ( m_textureID_Satchel == -1 )
	{
		m_textureID_Satchel = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_textureID_Satchel, "vgui/icons/slam_satchel", true, false );
	}

	if ( m_textureID_Tripmine == -1 )
	{
		m_textureID_Tripmine = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_textureID_Tripmine, "vgui/icons/slam_tripmine", true, false );
	}
	*/

	// draw the suit power bar
	//surface()->DrawSetTextColor( m_SLAMIconColor );
	surface()->DrawSetTextFont( m_hIconFont );

	int total = (m_iNumSatchels + m_iNumTripmines + m_iNumDetonatables);
	int numTripmines = m_iNumTripmines;
	int numSatchels = m_iNumSatchels;
	int numDetonatables = m_iNumDetonatables;

	if (m_bSLAMAdded)
	{
		if (m_iNumSatchelsDiff < 0)
			m_iNumSatchelsDiff = -m_iNumSatchelsDiff;
		if (m_iNumTripminesDiff < 0)
			m_iNumTripminesDiff = -m_iNumTripminesDiff;
		if (m_iNumDetonatablesDiff < 0)
			m_iNumDetonatablesDiff = -m_iNumDetonatablesDiff;
	}
	else if (m_LastSLAMColor[3])
	{
		// Add to the total to represent our lost SLAMs
		total += (m_iNumSatchelsDiff + m_iNumTripminesDiff + m_iNumDetonatablesDiff);
		numTripmines += m_iNumTripminesDiff;
		numSatchels += m_iNumSatchelsDiff;
		numDetonatables += m_iNumDetonatablesDiff;
	}

	bool bShowHighlight = (m_bSLAMAdded || m_LastSLAMColor[3]);
	int iTripmineHighlight = (numTripmines - m_iNumTripminesDiff);
	int iSatchelHighlight = (numSatchels - m_iNumSatchelsDiff);
	int iDetonatableHighlight = (numDetonatables - m_iNumDetonatablesDiff);

	//Msg( "SLAM Paint: %s - (%i/%i), (%i/%i)\n", bShowHighlight ? "Showing highlight" : "Not showing highlight",
	//	m_iNumSatchels, m_iNumSatchelsDiff,
	//	m_iNumTripmines, m_iNumTripminesDiff );

	int xpos = m_flIconInsetX, ypos = m_flIconInsetY;
	//int iconSize = m_flIconGap - 2;
	for (int i = 0; i < total; i++)
	{
		//surface()->DrawSetColor( m_SLAMIconColor );
		surface()->DrawSetTextColor( m_SLAMIconColor );

		surface()->DrawSetTextPos(xpos, ypos);

		if (i < numTripmines)
		{
			if (bShowHighlight)
			{
				if (m_iNumTripminesDiff != 0 && i >= iTripmineHighlight)
				{
					//Msg( "Tripmine using last SLAM color\n" );
					//surface()->DrawSetColor( m_LastSLAMColor );
					surface()->DrawSetTextColor( m_LastSLAMColor );
				}
				else
				{
					//Msg( "Tripmine not using last SLAM color (diff = %i, i = %i, highlight = %i\n", m_iNumTripminesDiff, i, iTripmineHighlight );
				}
			}

			surface()->DrawUnicodeChar('T');
			//surface()->DrawSetTexture( m_textureID_Tripmine );
			//surface()->DrawTexturedRect( xpos, ypos, xpos + iconSize, ypos + iconSize );
		}
		else if (i < numTripmines + numSatchels)
		{
			if (bShowHighlight)
			{
				if (m_iNumSatchelsDiff != 0 && i >= iSatchelHighlight)
				{
					//surface()->DrawSetColor( m_LastSLAMColor );
					surface()->DrawSetTextColor( m_LastSLAMColor );
				}
			}

			surface()->DrawUnicodeChar('S');
			//surface()->DrawSetTexture( m_textureID_Satchel );
			//surface()->DrawTexturedRect( xpos, ypos, xpos + iconSize, ypos + iconSize );
		}
		else
		{
			if (bShowHighlight)
			{
				if (m_iNumDetonatablesDiff != 0 && i >= iDetonatableHighlight)
				{
					//surface()->DrawSetColor( m_LastSLAMColor );
					surface()->DrawSetTextColor( m_LastSLAMColor );
				}
			}

			surface()->DrawUnicodeChar('U');
			//surface()->DrawSetTexture( m_textureID_Satchel );
			//surface()->DrawTexturedRect( xpos, ypos, xpos + iconSize, ypos + iconSize );
		}
		xpos += m_flIconGap;
	}
}


