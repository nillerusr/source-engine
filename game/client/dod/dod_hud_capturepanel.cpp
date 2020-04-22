//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISurface.h>
#include "c_dod_player.h"
#include "clientmode_dod.h"

#include "c_dod_objective_resource.h"
#include "c_dod_playerresource.h"

#include "dod_hud_capturepanel.h"

DECLARE_HUDELEMENT( CDoDHudCapturePanel );

ConVar hud_capturepanel( "hud_capturepanel", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Set to 0 to not draw the HUD capture panel" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDCapturePanelProgressBar::CDoDCapturePanelProgressBar( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
	m_flPercent = 0.0f;

	m_iTexture = vgui::surface()->DrawGetTextureId( "vgui/progress_bar" );
	if ( m_iTexture == -1 ) // we didn't find it, so create a new one
	{
		m_iTexture = vgui::surface()->CreateNewTextureID();	
	}

	vgui::surface()->DrawSetTextureFile( m_iTexture, "vgui/progress_bar", true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCapturePanelProgressBar::Paint()
{
	int wide, tall;
	GetSize( wide, tall );

	float uv1 = 0.0f, uv2 = 1.0f;
	Vector2D uv11( uv1, uv1 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );
	Vector2D uv12( uv1, uv2 );

	vgui::Vertex_t verts[4];	
	verts[0].Init( Vector2D( 0, 0 ), uv11 );
	verts[1].Init( Vector2D( wide, 0 ), uv21 );
	verts[2].Init( Vector2D( wide, tall ), uv22 );
	verts[3].Init( Vector2D( 0, tall ), uv12  );

	// first, just draw the whole thing inactive.
	vgui::surface()->DrawSetTexture( m_iTexture );
	vgui::surface()->DrawSetColor( m_clrInActive );
	vgui::surface()->DrawTexturedPolygon( 4, verts );

	// now, let's calculate the "active" part of the progress bar
	vgui::surface()->DrawSetColor( m_clrActive );

	// we're going to do this using quadrants
	//  -------------------------
	//  |           |           |
	//  |           |           |
	//  |     4     |     1     |
	//  |           |           |
	//  |           |           |
	//  -------------------------
	//  |           |           |
	//  |           |           |
	//  |     3     |     2     |
	//  |           |           |
	//  |           |           |
	//  -------------------------

	float flCompleteCircle = ( 2.0f * M_PI );
	float fl90degrees = flCompleteCircle / 4.0f;
	
	float flEndAngle = flCompleteCircle * ( 1.0f - m_flPercent ); // count DOWN (counter-clockwise)
	//	float flEndAngle = flCompleteCircle * m_flPercent; // count UP (clockwise)
	
	float flHalfWide = (float)wide / 2.0f;
	float flHalfTall = (float)tall / 2.0f;

	if ( flEndAngle >= fl90degrees * 3.0f ) // >= 270 degrees
	{
		// draw the first and second quadrants
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 1.0f );
		uv12.Init( 0.5, 1.0f );
		
		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, tall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, tall ), uv12  );

		vgui::surface()->DrawTexturedPolygon( 4, verts );

		// draw the third quadrant
		uv11.Init( 0.0f, 0.5f );
		uv21.Init( 0.5f, 0.5f );
		uv22.Init( 0.5f, 1.0f );
		uv12.Init( 0.0f, 1.0f );

		verts[0].Init( Vector2D( 0.0f, flHalfTall ), uv11 );
		verts[1].Init( Vector2D( flHalfWide, flHalfTall ), uv21 );
		verts[2].Init( Vector2D( flHalfWide, tall ), uv22 );
		verts[3].Init( Vector2D( 0.0f, tall ), uv12  );

		vgui::surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial fourth quadrant
		if ( flEndAngle > fl90degrees * 3.5f ) // > 315 degrees
		{
			uv11.Init( 0.0f, 0.0f );
			uv21.Init( 0.5f - ( tan(fl90degrees * 4.0f - flEndAngle) * 0.5 ), 0.0f );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.0f, 0.5f );

			verts[0].Init( Vector2D( 0.0f, 0.0f ), uv11 );
			verts[1].Init( Vector2D( flHalfWide - ( tan(fl90degrees * 4.0f - flEndAngle) * flHalfTall ), 0.0f ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall ), uv12 );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 315 degrees
		{
			uv11.Init( 0.0f, 0.5f );
			uv21.Init( 0.0f, 0.5f - ( tan(flEndAngle - fl90degrees * 3.0f) * 0.5 ) );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.0f, 0.5f );

			verts[0].Init( Vector2D( 0.0f, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( 0.0f, flHalfTall - ( tan(flEndAngle - fl90degrees * 3.0f) * flHalfWide ) ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else if ( flEndAngle >= fl90degrees * 2.0f ) // >= 180 degrees
	{
		// draw the first and second quadrants
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 1.0f );
		uv12.Init( 0.5, 1.0f );

		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, tall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, tall ), uv12  );

		vgui::surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial third quadrant
		if ( flEndAngle > fl90degrees * 2.5f ) // > 225 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 0.5f, 1.0f );
			uv22.Init( 0.0f, 1.0f );
			uv12.Init( 0.0f, 0.5f + ( tan(fl90degrees * 3.0f - flEndAngle) * 0.5 ) );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( flHalfWide, tall ), uv21 );
			verts[2].Init( Vector2D( 0.0f, tall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall + ( tan(fl90degrees * 3.0f - flEndAngle) * flHalfWide ) ), uv12 );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 225 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 0.5f, 1.0f );
			uv22.Init( 0.5f - ( tan( flEndAngle - fl90degrees * 2.0f) * 0.5 ), 1.0f );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( flHalfWide, tall ), uv21 );
			verts[2].Init( Vector2D( flHalfWide - ( tan(flEndAngle - fl90degrees * 2.0f) * flHalfTall ), tall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else if ( flEndAngle >= fl90degrees ) // >= 90 degrees
	{
		// draw the first quadrant
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 0.5f );
		uv12.Init( 0.5f, 0.5f );
	
		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, flHalfTall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

		vgui::surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial second quadrant
		if ( flEndAngle > fl90degrees * 1.5f ) // > 135 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 1.0f, 0.5f );
			uv22.Init( 1.0f, 1.0f );
			uv12.Init( 0.5f + ( tan(fl90degrees * 2.0f - flEndAngle) * 0.5f ), 1.0f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( wide, flHalfTall ), uv21 );
			verts[2].Init( Vector2D( wide, tall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide + ( tan(fl90degrees * 2.0f - flEndAngle) * flHalfTall ), tall ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 135 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 1.0f, 0.5f );
			uv22.Init( 1.0f, 0.5f + ( tan(flEndAngle - fl90degrees) * 0.5f ) );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( wide, flHalfTall ), uv21 );
			verts[2].Init( Vector2D( wide, flHalfTall + ( tan(flEndAngle - fl90degrees) * flHalfWide ) ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else // > 0 degrees
	{
		if ( flEndAngle > fl90degrees / 2.0f ) // > 45 degrees
		{
			uv11.Init( 0.5f, 0.0f );
			uv21.Init( 1.0f, 0.0f );
			uv22.Init( 1.0f, 0.5f - ( tan(fl90degrees - flEndAngle) * 0.5 ) );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
			verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
			verts[2].Init( Vector2D( wide, flHalfTall - ( tan(fl90degrees - flEndAngle) * flHalfWide ) ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 45 degrees
		{
			uv11.Init( 0.5f, 0.0f );
			uv21.Init( 0.5 + ( tan(flEndAngle) * 0.5 ), 0.0f );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.5f, 0.0f );

			verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
			verts[1].Init( Vector2D( flHalfWide + ( tan(flEndAngle) * flHalfTall ), 0.0f ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, 0.0f ), uv12  );

			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDCapturePanelIcon::CDoDCapturePanelIcon( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
	m_bActive = false;

	m_iTexture = vgui::surface()->DrawGetTextureId( "vgui/capture_icon" );
	if ( m_iTexture == -1 ) // we didn't find it, so create a new one
	{
		m_iTexture = vgui::surface()->CreateNewTextureID();	
	}

	vgui::surface()->DrawSetTextureFile( m_iTexture, "vgui/capture_icon", true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDCapturePanelIcon::Paint()
{
	int wide, tall;
	GetSize( wide, tall );

	float uv1 = 0.0f, uv2 = 1.0f;
	Vector2D uv11( uv1, uv1 );
	Vector2D uv12( uv1, uv2 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );

	vgui::Vertex_t verts[4];	
	verts[0].Init( Vector2D( 0, 0 ), uv11 );
	verts[1].Init( Vector2D( wide, 0 ), uv21 );
	verts[2].Init( Vector2D( wide, tall ), uv22 );
	verts[3].Init( Vector2D( 0, tall ), uv12  );

	// just draw the whole thing
	vgui::surface()->DrawSetTexture( m_iTexture );
	vgui::surface()->DrawSetColor( m_bActive ? m_clrActive : m_clrInActive );
	vgui::surface()->DrawTexturedPolygon( 4, verts );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDoDHudCapturePanel::CDoDHudCapturePanel( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudCapturePanel" ) 
{
	SetParent( g_pClientMode->GetViewport() );

	m_pBackground = new vgui::Panel( this, "CapturePanelBackground" );
	m_pProgressBar = new CDoDCapturePanelProgressBar( this, "CapturePanelProgressBar" );

	for ( int i = 0 ; i < 5 ; i++ )
	{
		CDoDCapturePanelIcon *pPanel;
		char szName[64];

		Q_snprintf( szName, sizeof( szName ), "CapturePanelPlayerIcon%d", i + 1 );
		pPanel = new CDoDCapturePanelIcon( this, szName );

		m_PlayerIcons.AddToTail( pPanel );
	}

	m_pAlliesFlag = new vgui::ImagePanel( this, "CapturePanelAlliesFlag" );
	m_pAxisFlag = new vgui::ImagePanel( this, "CapturePanelAxisFlag" );
	m_pNeutralFlag = new vgui::ImagePanel( this, "CapturePanelNeutralFlag" );

	m_pMessage = new vgui::Label( this, "CapturePanelMessage", " " );

	// load control settings...
	LoadControlSettings( "resource/UI/HudCapturePanel.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudCapturePanel::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/HudCapturePanel.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudCapturePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( m_pBackground )
	{
		m_pBackground->SetBgColor( GetSchemeColor( "HintMessageBg", pScheme ) );
		m_pBackground->SetPaintBackgroundType( 2 );
	}

	SetFgColor( GetSchemeColor( "HudProgressBar.Active", pScheme ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudCapturePanel::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer )
	{
		int iCPIndex = pPlayer->GetCPIndex();
		bool bInCapZone = ( iCPIndex >= 0 );

		// turn off the panel and children if the player is dead or not in a cap zone
		if ( !pPlayer->IsAlive() )
		{
			if ( IsVisible() )
			{
				SetVisible( false );
			}
			return;
		}

		if ( !bInCapZone  || !hud_capturepanel.GetBool() )
		{
			// see if the player progress bar wants to draw

			// then return, we're not in an area cap
			if ( pPlayer->m_iProgressBarDuration > 0 && pPlayer )
			{
				if ( !IsVisible() )
				{
					SetVisible( true );
				}

				// hide the other stuff
				m_pAlliesFlag->SetVisible( false );
				m_pAxisFlag->SetVisible( false );
				m_pNeutralFlag->SetVisible( false );

				m_pMessage->SetVisible( false );

				int i;
				for ( i = 0 ; i < m_PlayerIcons.Count() ; i++ )
				{
					m_PlayerIcons[i]->SetVisible( false );
				}

				// turn on the progress bar, we're capping
				if ( m_pProgressBar )
				{
					if ( !m_pProgressBar->IsVisible() )
					{
						m_pProgressBar->SetVisible( true );
					}

					float flPercent = (pPlayer->m_flSimulationTime - pPlayer->m_flProgressBarStartTime) / (float)pPlayer->m_iProgressBarDuration;
					flPercent = clamp( flPercent, 0, 1 );

					m_pProgressBar->SetPercentage( flPercent );
				}
			}
			else
			{
				if ( IsVisible() )
				{
					SetVisible( false );
				}
			}
			return;
		}

		int nOwningTeam = g_pObjectiveResource->GetOwningTeam( iCPIndex );
		int nPlayerTeam = pPlayer->GetTeamNumber();

		int nNumTeammates = g_pObjectiveResource->GetNumPlayersInArea( iCPIndex, nPlayerTeam == TEAM_ALLIES ? TEAM_ALLIES : TEAM_AXIS );
		int nRequiredTeammates = g_pObjectiveResource->GetRequiredCappers( iCPIndex, nPlayerTeam == TEAM_ALLIES ? TEAM_ALLIES : TEAM_AXIS );

		int nNumEnemies = g_pObjectiveResource->GetNumPlayersInArea( iCPIndex, nPlayerTeam == TEAM_ALLIES ? TEAM_AXIS : TEAM_ALLIES );
		int nRequiredEnemies = g_pObjectiveResource->GetRequiredCappers( iCPIndex, nPlayerTeam == TEAM_ALLIES ? TEAM_AXIS : TEAM_ALLIES );

		int iCappingTeam = g_pObjectiveResource->GetCappingTeam( iCPIndex );
		
		// if we already own this capture point and there are no enemies in the area
		if ( nOwningTeam == nPlayerTeam && nNumEnemies < nRequiredEnemies )
		{
			// don't need to do anything
			if ( IsVisible() )
			{
				SetVisible( false );
			}
			return;		
		}

		// okay, turn on the capture point panel
		if ( !IsVisible() )
		{
			SetVisible( true );
		}

		// set the correct flag image
		switch( nOwningTeam )
		{
		case TEAM_ALLIES:
			if ( m_pAlliesFlag && !m_pAlliesFlag->IsVisible() )
				m_pAlliesFlag->SetVisible( true );
			if ( m_pAxisFlag && m_pAxisFlag->IsVisible() )
				m_pAxisFlag->SetVisible( false );
			if ( m_pNeutralFlag && m_pNeutralFlag->IsVisible() )
				m_pNeutralFlag->SetVisible( false );
			break;
		case TEAM_AXIS:
			if ( m_pAlliesFlag && m_pAlliesFlag->IsVisible() )
				m_pAlliesFlag->SetVisible( false );
			if ( m_pAxisFlag && !m_pAxisFlag->IsVisible() )
				m_pAxisFlag->SetVisible( true );
			if ( m_pNeutralFlag && m_pNeutralFlag->IsVisible() )
				m_pNeutralFlag->SetVisible( false );
			break;
		default:
			if ( m_pAlliesFlag && m_pAlliesFlag->IsVisible() )
				m_pAlliesFlag->SetVisible( false );
			if ( m_pAxisFlag && m_pAxisFlag->IsVisible() )
				m_pAxisFlag->SetVisible( false );
			if ( m_pNeutralFlag && !m_pNeutralFlag->IsVisible() )
				m_pNeutralFlag->SetVisible( true );
			break;
		}

		// arrange the player icons
		int i = 0;
		for ( i = 0 ; i < m_PlayerIcons.Count() ; i++ )
		{
			CDoDCapturePanelIcon *pPanel = m_PlayerIcons[i];

			if ( !pPanel )
			{
				continue;
			}

			if ( i < nRequiredTeammates )
			{
				if ( i < nNumTeammates )
				{
					pPanel->SetActive( true );

					if ( !pPanel->IsVisible() )
						pPanel->SetVisible( true );
				}
				else
				{
					pPanel->SetActive( false );

					if ( !pPanel->IsVisible() )
						pPanel->SetVisible( true );
				}
			}
			else
			{
				if ( pPanel->IsVisible() )
					pPanel->SetVisible( false );
			}
		}

		int wide = 0, tall = 0, iconWide = 0, iconTall = 0;
		GetSize( wide, tall );

		vgui::ImagePanel *pPanel = m_PlayerIcons[0];
		if ( pPanel )
			pPanel->GetSize( iconWide, iconTall );
		
        int width = ( nRequiredTeammates * iconWide ) + ( ( nRequiredTeammates - 1 ) * m_nSpaceBetweenIcons );
		int xpos = wide / 2.0 - width / 2.0;

		// rearrange the player icon panels
		for ( i = 0 ; i < nRequiredTeammates ; i++ )
		{
			CDoDCapturePanelIcon *pPanel = m_PlayerIcons[i];
 
			if ( pPanel )
			{
				int x, y, w, t;
				pPanel->GetBounds( x, y, w, t );
				pPanel->SetBounds( xpos, y, w, t );
			}

			xpos += iconWide + m_nSpaceBetweenIcons;
		}

		// are we capping an area?
		if ( iCappingTeam == TEAM_UNASSIGNED || iCappingTeam != nPlayerTeam )
		{
			// turn off the progress bar, we're not capping
			if ( m_pProgressBar && m_pProgressBar->IsVisible() )
			{
				m_pProgressBar->SetVisible( false );
			}

			// turn on the message
			if ( m_pMessage )
			{
				m_pMessage->SetFgColor( GetFgColor() );

				if ( !m_pMessage->IsVisible() )
				{
					m_pMessage->SetVisible( true );
				}

				if ( nNumTeammates >= nRequiredTeammates && nNumEnemies > 0 )
				{
					m_pMessage->SetText( "#Dod_Capture_Blocked" );
				}
				else if ( nNumEnemies > 0 /*nNumEnemies >= nRequiredEnemies*/ )
				{
					m_pMessage->SetText( "#Dod_Blocking_Capture" );
				}
				else
				{
					m_pMessage->SetText( "#Dod_Waiting_for_teammate" );
				}
		
				if ( m_pBackground )
				{
					// do we need to resize our background?
					int textW, textH, bgX, bgY, bgW, bgH;
					m_pMessage->GetContentSize( textW, textH );
					m_pBackground->GetBounds( bgX, bgY, bgW, bgH );

					if ( bgW < textW )
					{
						m_pBackground->SetBounds( bgX + ( bgW / 2.0 ) - ( ( textW + XRES(3) ) / 2.0 ), bgY, textW + XRES(3), bgH );
					}
				}
			}
		}
		else
		{
			// turn on the progress bar, we're capping
			if ( m_pProgressBar )
			{
				if ( !m_pProgressBar->IsVisible() )
				{
					m_pProgressBar->SetVisible( true );
				}

				m_pProgressBar->SetPercentage( g_pObjectiveResource->GetCPCapPercentage( pPlayer->GetCPIndex() ) );
			}

			// turn off the message
			if ( m_pMessage && m_pMessage->IsVisible() )
			{
				m_pMessage->SetVisible( false );
			}
		}
	}
}


