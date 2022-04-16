//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui/ISurface.h>
#include "c_baseplayer.h"
#include <vgui_controls/Panel.h>
#include "dod_gamerules.h"
#include "iclientmode.h"
#include "c_dod_objective_resource.h"
#include "c_dod_playerresource.h"
#include "c_dod_player.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "hud_macros.h"
#include <baseviewport.h>	//for IViewPortPanel
#include "spectatorgui.h"
#include "dod_round_timer.h"
#include "vgui_controls/AnimationController.h"
#include "dod_hud_freezepanel.h"

class CHudObjectiveIcons : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudObjectiveIcons, vgui::Panel );

	CHudObjectiveIcons( const char *pName );
	
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void Paint();
	virtual void Init();
	virtual void VidInit();
	virtual void Reset();

	virtual void FireGameEvent( IGameEvent *event );

	void DrawBackgroundBox( int xpos, int ypos, int nBoxWidth, int nBoxHeight, bool bCutCorner );

	virtual bool IsVisible( void );

private:

	vgui::Label	*m_pTimer;
	vgui::Label *m_pTimeAdded;

	CHudTexture *m_pIconDefended;

	int m_iCPTextures[8];
	int m_iCPCappingTextures[8];

	int m_iBackgroundTexture;
	Color m_clrBackground;
	Color m_clrBorder;

	int m_iLastCP;	// the index of the area we were last in

	CHudTexture *m_pC4Icon;
	CHudTexture *m_pExplodedIcon;
	CHudTexture *m_pC4PlantedBG;

	int m_iSecondsAdded;		// how many seconds were added in the last time_added event
	bool bInTimerWarningAnim;
	float m_flDrawTimeAddedUntil;

	CPanelAnimationVar( vgui::HFont, m_hTimerFont, "TimerFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hTimerFontSmall, "TimerFontSmall", "DefaultSmall" );

	CPanelAnimationVar( vgui::HFont, m_hTextFont, "ChatFont", "Default" );

	CPanelAnimationVarAliasType( int, m_nIconSize, "iconsize", "24", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSeparatorWidth, "separator_width", "7", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nCornerCutSize, "CornerCutSize", "5", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nBackgroundOverlap, "BackgroundOverlap", "5", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_iIconStartX, "icon_start_x", "10", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iIconStartY, "icon_start_y", "10", "proportional_int" );
	CPanelAnimationVarAliasType( float, m_flIconExpand, "icon_expand", "0", "proportional_float" );

	CPanelAnimationVar( Color, m_clrTimer, "TimerBG", "255 0 0 128" );

	CPanelAnimationVarAliasType( int, m_nTimeAddedHeight, "time_added_height", "12", "proportional_int" );
	CPanelAnimationVar( float, m_flTimeAddedExpandPercent, "time_added_height_anim", "0.0" );
	CPanelAnimationVar( float, m_flTimeAddedAlpha, "time_added_alpha", "0" );

	CPanelAnimationVar( float, m_flTimeAddedDuration, "time_added_duration", "3.5" );
};

DECLARE_HUDELEMENT( CHudObjectiveIcons );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudObjectiveIcons::CHudObjectiveIcons( const char *pName ) : vgui::Panel( NULL, "HudObjectiveIcons" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( 0 );

	m_pTimer = new vgui::Label( this, "HudObjectivesRoundTimer", " " );
	if ( m_pTimer )
	{
		m_pTimer->SetContentAlignment( Label::a_center );
	}

	m_pTimeAdded = new vgui::Label( this, "HudObjectivesTimeAdded", " " );
	if ( m_pTimeAdded )
	{
		m_pTimeAdded->SetContentAlignment( Label::a_center );
	}

	m_iBackgroundTexture = vgui::surface()->DrawGetTextureId( "vgui/white" );
	if ( m_iBackgroundTexture == -1 )
	{
		m_iBackgroundTexture = vgui::surface()->CreateNewTextureID();
	}
	vgui::surface()->DrawSetTextureFile( m_iBackgroundTexture, "vgui/white", true, true );

	m_iLastCP = -1;

	m_iSecondsAdded = 0;
	bInTimerWarningAnim = false;

	m_flDrawTimeAddedUntil = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::Init( void )
{
	for( int i = 0 ; i < 8 ; i++ )
	{
		m_iCPTextures[i] = vgui::surface()->CreateNewTextureID();
		m_iCPCappingTextures[i] = vgui::surface()->CreateNewTextureID();
	}

	ListenForGameEvent( "dod_timer_time_added" );
	ListenForGameEvent( "dod_timer_flash" );
}

void CHudObjectiveIcons::VidInit( void )
{
	m_flTimeAddedExpandPercent = 0.0;
	m_flTimeAddedAlpha = 0.0;
	m_flDrawTimeAddedUntil = -1;

	CHudElement::VidInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::Reset( void )
{
	m_iLastCP = -1;
	m_flIconExpand = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudObjectiveIcons::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( FStrEq( "dod_timer_time_added", eventname ) )
	{
		// show time added under the timer, flash
		m_iSecondsAdded = event->GetInt( "seconds_added" );

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TimerFlash" ); 

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ShowTimeAdded" ); 

		if ( !m_pTimeAdded->IsVisible() )
		{
			m_pTimeAdded->SetVisible( true );
		}

		wchar_t wText[12];

		int iSecondsToDraw = abs(m_iSecondsAdded);
		bool bNegative = ( m_iSecondsAdded < 0 );

#ifdef WIN32
		_snwprintf( wText, sizeof(wText)/sizeof(wchar_t), L"%s %d:%02d", bNegative ? L"-" : L"+", iSecondsToDraw / 60, iSecondsToDraw % 60 );
#else
		_snwprintf( wText, sizeof(wText)/sizeof(wchar_t), L"%S %d:%02d", bNegative ? L"-" : L"+", iSecondsToDraw / 60, iSecondsToDraw % 60 );
#endif

		m_pTimeAdded->SetText( wText );

		m_flDrawTimeAddedUntil = gpGlobals->curtime + m_flTimeAddedDuration;

	}
	else if ( FStrEq( "dod_timer_flash", eventname ) )
	{
		// generic flash, used for 5, 2, 1 minute warnings
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TimerFlash" ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hTextFont = pScheme->GetFont( "ChatFont" );
	m_hTimerFontSmall = pScheme->GetFont( "TimerFontSmall" );

	m_clrBackground = pScheme->GetColor( "HudPanelBackground", GetFgColor() );
	m_clrBorder = pScheme->GetColor( "HudPanelBorder", GetBgColor() );

	m_pC4Icon = gHUD.GetIcon( "icon_c4" );
	m_pExplodedIcon = gHUD.GetIcon( "icon_c4_exploded" );
	m_pC4PlantedBG = gHUD.GetIcon( "icon_c4_planted_bg" );
	m_pIconDefended = gHUD.GetIcon( "icon_defended" );

	m_pTimer->SetFont( m_hTimerFont );
	m_pTimeAdded->SetFont( m_hTimerFontSmall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::DrawBackgroundBox( int xpos, int ypos, int nBoxWidth, int nBoxHeight, bool bCutCorner )
{
	int nCornerCutSize = bCutCorner ? m_nCornerCutSize : 0;
	vgui::Vertex_t verts[5];

	verts[0].Init( Vector2D( xpos, ypos ) );
	verts[1].Init( Vector2D( xpos + nBoxWidth, ypos ) );
	verts[2].Init( Vector2D( xpos + nBoxWidth + 1, ypos + nBoxHeight - nCornerCutSize + 1 ) );
	verts[3].Init( Vector2D( xpos + nBoxWidth - nCornerCutSize + 1, ypos + nBoxHeight + 1 ) );
	verts[4].Init( Vector2D( xpos, ypos + nBoxHeight ) );

	vgui::surface()->DrawSetTexture( m_iBackgroundTexture );
	vgui::surface()->DrawSetColor( Color( m_clrBackground ) );
	vgui::surface()->DrawTexturedPolygon( 5, verts );

	vgui::Vertex_t borderverts[5];

	borderverts[0].Init( Vector2D( xpos, ypos ) );
	borderverts[1].Init( Vector2D( xpos + nBoxWidth, ypos ) );
	borderverts[2].Init( Vector2D( xpos + nBoxWidth, ypos + nBoxHeight - nCornerCutSize ) );
	borderverts[3].Init( Vector2D( xpos + nBoxWidth - nCornerCutSize, ypos + nBoxHeight ) );
	borderverts[4].Init( Vector2D( xpos, ypos + nBoxHeight ) );

	vgui::surface()->DrawSetColor( Color( m_clrBorder ) );
	vgui::surface()->DrawTexturedPolyLine( borderverts, 5 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudObjectiveIcons::Paint()
{
	int ypos = m_iIconStartY;
	int xpos = m_iIconStartX;
	static Color clrIcon( 255, 255, 255, 255 );

	if( !g_pObjectiveResource )	// MATTTODO: hasn't been transmited yet .. fix ?
	{
		return;
	}

	// Hide the time added if it is time to do so
	if ( m_flDrawTimeAddedUntil > 0 && m_flDrawTimeAddedUntil < gpGlobals->curtime )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HideTimeAdded" ); 

		m_flDrawTimeAddedUntil = -1;
	}

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetColor( clrIcon );
	
	if ( g_pSpectatorGUI && g_pSpectatorGUI->IsVisible() )
	{
		ypos += g_pSpectatorGUI->GetTopBarHeight();
	}

	int num = g_pObjectiveResource->GetNumControlPoints();
	bool bShowTimer = ( g_DODRoundTimer != NULL );

	if ( num <= 0 && !bShowTimer )
	{
		if ( m_pTimer && m_pTimer->IsVisible() )
		{
			m_pTimer->SetVisible( false );
		}

		if ( m_pTimeAdded && m_pTimeAdded->IsVisible() )
		{
			m_pTimeAdded->SetVisible( false );
		}

		return; // nothing to draw yet
	}

	int iLastVisible = 0;

	int k;

	// let's count how many visible capture points we have (for cutting the corner of the last visible one)
	for( k = 0 ; k < num ; k++ )
	{
		if( g_pObjectiveResource->IsCPVisible( k ) )
		{
			iLastVisible = k;
		}
	}

	// do we have a round timer?
	if( bShowTimer )
	{
		if ( m_pTimer )
		{
			if ( !m_pTimer->IsVisible() )
			{
				m_pTimer->SetVisible( true );
			}

			m_pTimer->SetBounds( xpos, ypos, 2 * m_nIconSize, m_nIconSize );
			m_pTimer->SetBgColor( m_clrTimer );

			m_pTimeAdded->SetBounds( xpos, ypos + m_nIconSize, 2 * m_nIconSize, m_nTimeAddedHeight + m_nBackgroundOverlap );
			m_pTimeAdded->SetBgColor( Color(0,0,0,0) );

			int iRoundTime = (int)g_DODRoundTimer->GetTimeRemaining();

			int minutes = iRoundTime / 60;
			int seconds = iRoundTime % 60;

			if( minutes < 0 ) minutes = 0;
			if( minutes > 99 ) minutes = 99;
			if( seconds < 0 ) seconds = 0;

			// cut the corner if this is all we're drawing

			// figure out the height. will change if we're drawing the +1:00 below it
			int boxHeight = 2*m_nBackgroundOverlap + m_nIconSize + (int)( (float)m_nTimeAddedHeight * m_flTimeAddedExpandPercent );

			DrawBackgroundBox( xpos - m_nBackgroundOverlap, ypos - m_nBackgroundOverlap, m_pTimer->GetWide() + 2 * m_nBackgroundOverlap, boxHeight, ( num <= 0 ) ? true : false );

			// set the time
			char szTime[16];
			Q_snprintf( szTime, sizeof( szTime ), "%02d:%02d", minutes, seconds );
			m_pTimer->SetText( szTime );

			m_pTimeAdded->SetAlpha( (int)m_flTimeAddedAlpha );

			xpos += ( m_pTimer->GetWide() + m_nSeparatorWidth + m_nBackgroundOverlap * 2 );
		}
	}
	else
	{
		if ( m_pTimer && m_pTimer->IsVisible() )
		{
			m_pTimer->SetVisible( false );
		}
	}

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	int iCurrentCapAreaIndex = pPlayer->GetCPIndex();

	if ( pPlayer->IsAlive() == false )
	{
		m_iLastCP = -1;
	}

	int index;

	for( index = 0 ; index < num ; index++ )
	{
		if( g_pObjectiveResource->IsCPVisible( index ) )
		{
			int iOwnerIcon = g_pObjectiveResource->GetCPCurrentOwnerIcon( index );

			float uv1 = 0.0f;
			float uv2 = 1.0f;

			float flXPos = (float)xpos;
			float flYPos = (float)ypos;
			float flIconSize = (float)m_nIconSize;

			if ( index == iCurrentCapAreaIndex )
			{
				// animate the current cp
				flXPos -= m_flIconExpand;
				flYPos -= m_flIconExpand;
				flIconSize += m_flIconExpand*2;

				m_iLastCP = iCurrentCapAreaIndex;
			}
			else if ( m_iLastCP == index )
			{
				// as a backup, animate out of the one we just left
				flXPos -= m_flIconExpand;
				flYPos -= m_flIconExpand;
				flIconSize += m_flIconExpand*2;
			}


			int iBombsRequired = g_pObjectiveResource->GetBombsRequired( index );

			if ( iBombsRequired > 0 )
			{
				DrawBackgroundBox( flXPos - m_nBackgroundOverlap, flYPos - m_nBackgroundOverlap, flIconSize + 2 * m_nBackgroundOverlap, flIconSize + 2 * m_nBackgroundOverlap + m_nTimeAddedHeight, ( index != iLastVisible ) ? false : true );
			}
			else
			{
				DrawBackgroundBox( flXPos - m_nBackgroundOverlap, flYPos - m_nBackgroundOverlap, flIconSize + 2 * m_nBackgroundOverlap, flIconSize + 2 * m_nBackgroundOverlap, ( index != iLastVisible ) ? false : true );
			}

			// Draw the background for the icon

			// allow for error
			if ( g_pObjectiveResource->IsBombSetAtPoint( index ) )
			{
				// if bomb timer is > 0, draw swipe
				float flBombTime = g_pObjectiveResource->GetBombTimeForPoint( index );	// round up

				// draw the 'white' version underneath
				int iBlankIcon = g_pObjectiveResource->GetCPTimerCapIcon( index );

				if ( iBlankIcon == 0 )
				{	
					iBlankIcon = g_pObjectiveResource->GetIconForTeam( index, TEAM_UNASSIGNED );
				}

				const char *szMatName = GetMaterialNameFromIndex( iBlankIcon );

				vgui::surface()->DrawSetTextureFile( m_iCPTextures[index], szMatName, true, false );

				Vector2D uv11( uv1, uv1 );
				Vector2D uv21( uv2, uv1 );
				Vector2D uv22( uv2, uv2 );
				Vector2D uv12( uv1, uv2 );

				vgui::Vertex_t vert[4];	
				vert[0].Init( Vector2D( flXPos,					flYPos               ), uv11 );
				vert[1].Init( Vector2D( flXPos + flIconSize,	flYPos               ), uv21 );
				vert[2].Init( Vector2D( flXPos + flIconSize,	flYPos + flIconSize ), uv22 );				
				vert[3].Init( Vector2D( flXPos,					flYPos + flIconSize ), uv12 );

				vgui::surface()->DrawSetColor( Color(255,255,255,255) );	
				vgui::surface()->DrawTexturedPolygon( 4, vert );
			
				// draw the real version in a circular swipe
				float flPercentRemaining = ( flBombTime / DOD_BOMB_TIMER_LENGTH );

				float flHalfWide = (float)flIconSize / 2.0f;
				float flHalfTall = (float)flIconSize / 2.0f;

				const float flCompleteCircle = ( 2.0f * M_PI );
				const float fl90degrees = flCompleteCircle * 0.25f;
				const float fl45degrees = fl90degrees * 0.5f;

				float flEndAngle = flCompleteCircle * flPercentRemaining; // clockwise

				typedef struct 
				{
					Vector2D vecTrailing;
					Vector2D vecLeading;
				} icon_quadrant_t;

				/*
				Quadrants are numbered 0 - 7 counter-clockwise
				_________________
				|	  0	| 7		|
				|		|		|
				|  1	|    6  |
				-----------------
				|  2	|	 5  |
				|		|		|
				|	  3	| 4		|
				-----------------
				*/

				// Encode the leading and trailing edge of each quadrant 
				// in the range 0.0 -> 1.0

				icon_quadrant_t quadrants[8];
				quadrants[0].vecTrailing.Init( 0.5, 0.0 );
				quadrants[0].vecLeading.Init( 0.0, 0.0 );

				quadrants[1].vecTrailing.Init( 0.0, 0.0 );
				quadrants[1].vecLeading.Init( 0.0, 0.5 );

				quadrants[2].vecTrailing.Init( 0.0, 0.5 );
				quadrants[2].vecLeading.Init( 0.0, 1.0 );

				quadrants[3].vecTrailing.Init( 0.0, 1.0 );
				quadrants[3].vecLeading.Init( 0.5, 1.0 );

				quadrants[4].vecTrailing.Init( 0.5, 1.0 );
				quadrants[4].vecLeading.Init( 1.0, 1.0 );

				quadrants[5].vecTrailing.Init( 1.0, 1.0 );
				quadrants[5].vecLeading.Init( 1.0, 0.5 );

				quadrants[6].vecTrailing.Init( 1.0, 0.5 );
				quadrants[6].vecLeading.Init( 1.0, 0.0 );

				quadrants[7].vecTrailing.Init( 1.0, 0.0 );
				quadrants[7].vecLeading.Init( 0.5, 0.0 );

				szMatName = GetMaterialNameFromIndex( iOwnerIcon );

				vgui::surface()->DrawSetTextureFile( m_iCPTextures[index], szMatName, true, false );
				vgui::surface()->DrawSetColor( Color(255,255,255,255) );	

				Vector2D uvMid( 0.5, 0.5 );
				Vector2D vecMid( flXPos + flHalfWide, flYPos + flHalfTall );

				int j;
				for ( j=0;j<=7;j++ )
				{
					float flMinAngle = j * fl45degrees;	

					float flAngle = clamp( flEndAngle - flMinAngle, 0, fl45degrees );

					if ( flAngle <= 0 )
					{
						// past our quadrant, draw nothing
						continue;
					}
					else
					{
						// draw our segment
						vgui::Vertex_t vert[3];	

						// vert 0 is mid ( 0.5, 0.5 )
						vert[0].Init( vecMid, uvMid );

						int xdir = 0, ydir = 0;

						switch( j )
						{
						case 0:  
						case 7:
							//right
							xdir = 1;
							ydir = 0;
							break;

						case 1: 
						case 2:
							//up
							xdir = 0;
							ydir = -1;
							break;

						case 3:
						case 4:
							//left
							xdir = -1;
							ydir = 0;
							break;

						case 5:
						case 6:
							//down
							xdir = 0;
							ydir = 1;
							break;
						}

						Vector2D vec1;
						Vector2D uv1;

						// vert 1 is the variable vert based on leading edge
						vec1.x = flXPos + quadrants[j].vecTrailing.x * flIconSize - xdir * tan(flAngle) * flHalfWide;
						vec1.y = flYPos + quadrants[j].vecTrailing.y * flIconSize - ydir * tan(flAngle) * flHalfTall;

						uv1.x = quadrants[j].vecTrailing.x - xdir * abs( quadrants[j].vecLeading.x - quadrants[j].vecTrailing.x ) * tan(flAngle);
						uv1.y = quadrants[j].vecTrailing.y - ydir * abs( quadrants[j].vecLeading.y - quadrants[j].vecTrailing.y ) * tan(flAngle);

						vert[1].Init( vec1, uv1 );

						// vert 2 is our trailing edge
						vert[2].Init( Vector2D( flXPos + quadrants[j].vecTrailing.x * flIconSize,
							flYPos + quadrants[j].vecTrailing.y * flIconSize ),
							quadrants[j].vecTrailing );

						vgui::surface()->DrawTexturedPolygon( 3, vert );					
					}	
				}

				if ( g_pObjectiveResource->IsBombBeingDefused( index ) )
				{
					float flSize = 0.75;
					int iconX = (int)( flXPos + flIconSize * ( ( 1.0 - flSize ) / 2 ) );
					int iconY = (int)( flYPos + flIconSize * ( ( 1.0 - flSize ) / 2 ) );
					int iconW = (int)( flIconSize * flSize );					

					Color c(255,255,255,255);
					m_pIconDefended->DrawSelf( iconX, iconY, iconW, iconW, c );
				}
			}
			else
			{
				// Draw the owner's icon
				if( iOwnerIcon != 0 )
				{
					const char *szMatName = GetMaterialNameFromIndex( iOwnerIcon );

					vgui::surface()->DrawSetTextureFile( m_iCPTextures[index], szMatName, true, false );
					
					/*
					// re-enable if we want to have animating cp icons
					// todo: framerate
					IVguiMatInfo *pMat = vgui::surface()->DrawGetTextureMatInfoFactory( m_iCPTextures[index] );

					if ( !pMat )
						return;

					int iNumFrames = pMat->GetNumAnimationFrames();

					IVguiMatInfoVar *m_pFrameVar;

					bool bFound = false;
					m_pFrameVar = pMat->FindVarFactory( "$frame", &bFound );

					static int frame = 0;

					if ( bFound )
					{
						frame++;
						m_pFrameVar->SetIntValue( frame % iNumFrames );
					}
					*/

					Vector2D uv11( uv1, uv1 );
					Vector2D uv21( uv2, uv1 );
					Vector2D uv22( uv2, uv2 );
					Vector2D uv12( uv1, uv2 );

					vgui::Vertex_t vert[4];	
					vert[0].Init( Vector2D( flXPos,					flYPos              ), uv11 );
					vert[1].Init( Vector2D( flXPos + flIconSize,	flYPos              ), uv21 );
					vert[2].Init( Vector2D( flXPos + flIconSize,	flYPos + flIconSize ), uv22 );				
					vert[3].Init( Vector2D( flXPos,					flYPos + flIconSize ), uv12 );

					vgui::surface()->DrawSetColor( Color(255,255,255,255) );	
					vgui::surface()->DrawTexturedPolygon( 4, vert );
				}		
			}
			
			// see if there are players in the area
			int iNumAllies = g_pObjectiveResource->GetNumPlayersInArea( index, TEAM_ALLIES );
			int iNumAxis = g_pObjectiveResource->GetNumPlayersInArea( index, TEAM_AXIS );

			int iCappingTeam = g_pObjectiveResource->GetCappingTeam( index );

			// Draw bomb icons under cap points
			if ( iBombsRequired > 0 )
			{
				int iBombsRemaining = g_pObjectiveResource->GetBombsRemaining( index );

				bool bBombPlanted = g_pObjectiveResource->IsBombSetAtPoint( index );

				int yIcon = ypos + flIconSize + YRES(2);

				int iIconHalfWidth = XRES(5);
				int iIconWidth = iIconHalfWidth * 2;

				Color c(255,255,255,255);

				switch( iBombsRequired )
				{
				case 1:
					{
						int xMid = xpos + ( flIconSize * 0.50f );

						switch( iBombsRemaining )
						{
						case 0:
							m_pExplodedIcon->DrawSelf( xMid - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							break;
						case 1:
							if ( bBombPlanted )
							{
								// draw the background behind 1
								int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
								m_pC4PlantedBG->DrawSelf( xMid - iIconWidth, yIcon - iIconHalfWidth, iIconWidth*2, iIconWidth*2, Color( 255,255,255,alpha) );
							}
							m_pC4Icon->DrawSelf( xMid - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							break;
						}
					}
					break;
				case 2:
					{
						int xMid1 = xpos + ( flIconSize * 0.25f );
						int xMid2 = xpos + ( flIconSize * 0.75f );

						switch( iBombsRemaining )
						{
						case 0:
							m_pExplodedIcon->DrawSelf( xMid1 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							m_pExplodedIcon->DrawSelf( xMid2 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							break;
						case 1:
							if ( bBombPlanted )
							{
								// draw the background behind 1
								int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
								m_pC4PlantedBG->DrawSelf( xMid1 - iIconWidth, yIcon - iIconHalfWidth, iIconWidth*2, iIconWidth*2, Color( 255,255,255,alpha) );
							}
							m_pC4Icon->DrawSelf( xMid1 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							m_pExplodedIcon->DrawSelf( xMid2 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							break;
						case 2:
							if ( bBombPlanted )
							{
								// draw the background behind 2
								int alpha = (float)( abs( sin(2*gpGlobals->curtime) ) * 205.0 + 50.0 );
								m_pC4PlantedBG->DrawSelf( xMid2 - iIconWidth, yIcon - iIconHalfWidth, iIconWidth*2, iIconWidth*2, Color( 255,255,255,alpha) );
							}
							m_pC4Icon->DrawSelf( xMid1 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							m_pC4Icon->DrawSelf( xMid2 - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
							break;
						}
					}
					break;
				default:
					{
						// general solution for > 2 bombs

						if ( iBombsRemaining > 0 )
						{
							// draw a bomb icon, then a 'x 3' for how many there are remaining

							if ( bBombPlanted )
							{
								// draw the background behind 2
								int alpha = (float)( abs( sin( 2*gpGlobals->curtime) ) * 205.0 + 50.0 );
								m_pC4PlantedBG->DrawSelf( xpos - iIconHalfWidth, yIcon - iIconHalfWidth, iIconWidth*2, iIconWidth*2, Color( 255,255,255,alpha) );
							}

							m_pC4Icon->DrawSelf( xpos, yIcon, iIconWidth, iIconWidth, c );

							// draw text saying how many bombs there are
							{
								wchar_t wText[6];
								_snwprintf( wText, sizeof(wText)/sizeof(wchar_t), L"x %d", iBombsRemaining );

								vgui::surface()->DrawSetTextColor( g_PR->GetTeamColor( TEAM_SPECTATOR ) );

								// set pos centered under icon
								vgui::surface()->DrawSetTextPos( xpos + ( flIconSize * 0.50f ), yIcon + YRES(2) );

								for ( wchar_t *wch = wText ; *wch != 0 ; wch++ )
								{
									vgui::surface()->DrawUnicodeChar( *wch );
								}
							}

						}
						else
						{
							int xMid = xpos + ( flIconSize * 0.50f );
							m_pExplodedIcon->DrawSelf( xMid - iIconHalfWidth, yIcon, iIconWidth, iIconWidth, c );
						}
					}
					break;
				}
			}

			// see if one team is partially capping,
			// should show a 1/2 under the cap icon
			if ( iCappingTeam == TEAM_UNASSIGNED )
			{
				if ( iNumAllies > 0 && iNumAxis == 0 )
				{
					iCappingTeam = TEAM_ALLIES;
				}
				else if ( iNumAxis > 0 && iNumAllies == 0 )
				{
					iCappingTeam = TEAM_AXIS;
				}

				if ( iCappingTeam == TEAM_UNASSIGNED || iCappingTeam == g_pObjectiveResource->GetOwningTeam( index ) )
				{
					// no team is capping, even partially
					// or the person in the area already owns it
					xpos += ( m_nIconSize + m_nSeparatorWidth + m_nBackgroundOverlap * 2 );
					continue;
				}
			}

			// Draw the number of cappers below the icon
			int numPlayers = g_pObjectiveResource->GetNumPlayersInArea( index, iCappingTeam );
			int requiredPlayers = g_pObjectiveResource->GetRequiredCappers( index, iCappingTeam );

			if ( requiredPlayers > 1 )
			{
				numPlayers = MIN( numPlayers, requiredPlayers );

				wchar_t wText[6];
				_snwprintf( wText, sizeof(wText)/sizeof(wchar_t), L"%d/%d", numPlayers, requiredPlayers );

				vgui::surface()->DrawSetTextColor( g_PR->GetTeamColor( iCappingTeam ) );

				// get string length
				int len = g_pMatSystemSurface->DrawTextLen( m_hTextFont, "2/2" );

				// set pos centered under icon
				vgui::surface()->DrawSetTextPos( xpos + ( m_nIconSize / 2.0f ) - ( len / 2 ), ypos + flIconSize + m_nBackgroundOverlap + YRES( 2 ) ); // + 2 to account for the outlined box

				for ( wchar_t *wch = wText ; *wch != 0 ; wch++ )
				{
					vgui::surface()->DrawUnicodeChar( *wch );
				}
			}

			if ( g_pObjectiveResource->GetCappingTeam( index ) != TEAM_UNASSIGNED )
			{
				int iCapperIcon = g_pObjectiveResource->GetCPCappingIcon( index );

				// Draw the capper's icon
				if( iOwnerIcon != iCapperIcon && iCapperIcon != 0 )
				{
					// axis caps swipe from right to left...allied from left to right
					bool bAxis = ( g_pObjectiveResource->GetCappingTeam(index) == TEAM_AXIS ) ? true : false;

					//swipe!
					float flCapPercentage = g_pObjectiveResource->GetCPCapPercentage(index);
					
					// reversing the direction of the swipe effect
					if ( bAxis )
					{
						flCapPercentage = 1.0f - g_pObjectiveResource->GetCPCapPercentage(index);
					}

					float width = ( flIconSize * flCapPercentage );
					const char *szCappingMatName = GetMaterialNameFromIndex( iCapperIcon );
					
					vgui::surface()->DrawSetTextureFile( m_iCPCappingTextures[index], szCappingMatName, true, false );

					vgui::Vertex_t vert[4];	

					Vector2D uv11( uv1, uv1 );
					Vector2D uv21( flCapPercentage, uv1 );
					Vector2D uv22( flCapPercentage, uv2 );
					Vector2D uv12( uv1, uv2 );

					// reversing the direction of the swipe effect
					if ( bAxis )
					{
						uv11.x = flCapPercentage;
						uv21.x = uv2;
						uv22.x = uv2;
						uv12.x = flCapPercentage;
					}

					Vector2D upperLeft ( flXPos,         flYPos );
					Vector2D upperRight( flXPos + width, flYPos );
					Vector2D lowerRight( flXPos + width, flYPos + flIconSize );
					Vector2D lowerLeft ( flXPos,		 flYPos + flIconSize );

					/// reversing the direction of the swipe effect
					if ( bAxis )
					{
						upperLeft.x  = flXPos + width;
						upperRight.x = flXPos + flIconSize;
						lowerRight.x = flXPos + flIconSize;
						lowerLeft.x  = flXPos + width;
					}
					
					vert[0].Init( upperLeft, uv11 );
					vert[1].Init( upperRight, uv21 );
					vert[2].Init( lowerRight, uv22 );				
					vert[3].Init( lowerLeft, uv12 );
							
					vgui::surface()->DrawSetColor( Color(255,255,255,255) );
					vgui::surface()->DrawTexturedPolygon( 4, vert );
				}
			}
			
			xpos += ( m_nIconSize + m_nSeparatorWidth + m_nBackgroundOverlap * 2 );
		}
	}
}
