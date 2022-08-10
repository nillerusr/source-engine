//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "iclientmode.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IImage.h>
#include <vgui_controls/Label.h>

#include "c_playerresource.h"
#include "teamplay_round_timer.h"
#include "utlvector.h"
#include "entity_capture_flag.h"
#include "c_tf_player.h"
#include "c_team.h"
#include "c_tf_team.h"
#include "c_team_objectiveresource.h"
#include "tf_hud_objectivestatus.h"
#include "tf_spectatorgui.h"
#include "teamplayroundbased_gamerules.h"
#include "tf_gamerules.h"
#include "tf_hud_freezepanel.h"

using namespace vgui;

CUtlVector<int> g_Flags;
CUtlVector<int> g_CaptureZones;

DECLARE_BUILD_FACTORY( CTFArrowPanel );
DECLARE_BUILD_FACTORY( CTFFlagStatus );

extern ConVar tf_flag_caps_per_round;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFArrowPanel::CTFArrowPanel( Panel *parent, const char *name ) : CTFImagePanel( parent, name )
{
	m_RedMaterial.Init( "hud/objectives_flagpanel_compass_red", TEXTURE_GROUP_VGUI ); 
	m_BlueMaterial.Init( "hud/objectives_flagpanel_compass_blue", TEXTURE_GROUP_VGUI ); 
	m_NeutralMaterial.Init( "hud/objectives_flagpanel_compass_grey", TEXTURE_GROUP_VGUI ); 

	m_RedMaterialNoArrow.Init( "hud/objectives_flagpanel_compass_red_noArrow", TEXTURE_GROUP_VGUI ); 
	m_BlueMaterialNoArrow.Init( "hud/objectives_flagpanel_compass_blue_noArrow", TEXTURE_GROUP_VGUI ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFArrowPanel::GetAngleRotation( void )
{
	float flRetVal = 0.0f;

	C_TFPlayer *pPlayer = ToTFPlayer( C_BasePlayer::GetLocalPlayer() );
	C_BaseEntity *pEnt = m_hEntity.Get();

	if ( pPlayer && pEnt )
	{
		QAngle vangles;
		Vector eyeOrigin;
		float zNear, zFar, fov;

		pPlayer->CalcView( eyeOrigin, vangles, zNear, zFar, fov );

		Vector vecFlag = pEnt->WorldSpaceCenter() - eyeOrigin;
		vecFlag.z = 0;
		vecFlag.NormalizeInPlace();

		Vector forward, right, up;
		AngleVectors( vangles, &forward, &right, &up );
		forward.z = 0;
		right.z = 0;
		forward.NormalizeInPlace();
		right.NormalizeInPlace();

		float dot = DotProduct( vecFlag, forward );
		float angleBetween = acos( dot );

		dot = DotProduct( vecFlag, right );

		if ( dot < 0.0f )
		{
			angleBetween *= -1;
		}

		flRetVal = RAD2DEG( angleBetween );
	}

	return flRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFArrowPanel::Paint()
{
	if ( !m_hEntity.Get() )
		return;

	C_BaseEntity *pEnt = m_hEntity.Get();
	IMaterial *pMaterial = m_NeutralMaterial;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// figure out what material we need to use
	if ( pEnt->GetTeamNumber() == TF_TEAM_RED )
	{
		pMaterial = m_RedMaterial;

		if ( pLocalPlayer && ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) )
		{
			// is our target a player?
			C_BaseEntity *pTargetEnt = pLocalPlayer->GetObserverTarget();
			if ( pTargetEnt && pTargetEnt->IsPlayer() )
			{
				// does our target have the flag and are they carrying the flag we're currently drawing?
				C_TFPlayer *pTarget = static_cast< C_TFPlayer* >( pTargetEnt );
				if ( pTarget->HasTheFlag() && ( pTarget->GetItem() == pEnt ) )
				{
					pMaterial = m_RedMaterialNoArrow;
				}
			}
		}
	}
	else if ( pEnt->GetTeamNumber() == TF_TEAM_BLUE )
	{
		pMaterial = m_BlueMaterial;

		if ( pLocalPlayer && ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) )
		{
			// is our target a player?
			C_BaseEntity *pTargetEnt = pLocalPlayer->GetObserverTarget();
			if ( pTargetEnt && pTargetEnt->IsPlayer() )
			{
				// does our target have the flag and are they carrying the flag we're currently drawing?
				C_TFPlayer *pTarget = static_cast< C_TFPlayer* >( pTargetEnt );
				if ( pTarget->HasTheFlag() && ( pTarget->GetItem() == pEnt ) )
				{
					pMaterial = m_BlueMaterialNoArrow;
				}
			}
		}
	}

	int x = 0;
	int y = 0;
	ipanel()->GetAbsPos( GetVPanel(), x, y );
	int nWidth = GetWide();
	int nHeight = GetTall();

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix(); 

	VMatrix panelRotation;
	panelRotation.Identity();
	MatrixBuildRotationAboutAxis( panelRotation, Vector( 0, 0, 1 ), GetAngleRotation() );
//	MatrixRotate( panelRotation, Vector( 1, 0, 0 ), 5 );
	panelRotation.SetTranslation( Vector( x + nWidth/2, y + nHeight/2, 0 ) );
	pRenderContext->LoadMatrix( panelRotation );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.TexCoord2f( 0, 0, 0 );
	meshBuilder.Position3f( -nWidth/2, -nHeight/2, 0 );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 0 );
	meshBuilder.Position3f( nWidth/2, -nHeight/2, 0 );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 1, 1 );
	meshBuilder.Position3f( nWidth/2, nHeight/2, 0 );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.TexCoord2f( 0, 0, 1 );
	meshBuilder.Position3f( -nWidth/2, nHeight/2, 0 );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();

	pMesh->Draw();
	pRenderContext->PopMatrix();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFArrowPanel::IsVisible( void )
{
	if( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFlagStatus::CTFFlagStatus( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pArrow = NULL;
	m_pStatusIcon = NULL;
	m_pBriefcase = NULL;
	m_hEntity = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlagStatus::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// load control settings...
	LoadControlSettings( "resource/UI/FlagStatus.res" );

	m_pArrow = dynamic_cast<CTFArrowPanel *>( FindChildByName( "Arrow" ) );
	m_pStatusIcon = dynamic_cast<CTFImagePanel *>( FindChildByName( "StatusIcon" ) );
	m_pBriefcase = dynamic_cast<CTFImagePanel *>( FindChildByName( "Briefcase" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlagStatus::IsVisible( void )
{
	if( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlagStatus::UpdateStatus( void )
{
	if ( m_hEntity.Get() )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag *>( m_hEntity.Get() );
	
		if ( pFlag )
		{
			const char *pszImage = "../hud/objectives_flagpanel_ico_flag_home";

			if ( pFlag->IsDropped() )
			{
				pszImage = "../hud/objectives_flagpanel_ico_flag_dropped";
			}
			else if ( pFlag->IsStolen() )
			{
				pszImage = "../hud/objectives_flagpanel_ico_flag_moving";
			}

			if ( m_pStatusIcon )
			{
				m_pStatusIcon->SetImage( pszImage );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudFlagObjectives::CTFHudFlagObjectives( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pCarriedImage = NULL;
	m_pPlayingTo = NULL;
	m_bFlagAnimationPlayed = false;
	m_bCarryingFlag = false;
	m_pSpecCarriedImage = NULL;

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	ListenForGameEvent( "flagstatus_update" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFHudFlagObjectives::IsVisible( void )
{
	if( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// load control settings...
	LoadControlSettings( "resource/UI/HudObjectiveFlagPanel.res" );

	m_pCarriedImage = dynamic_cast<CTFImagePanel *>( FindChildByName( "CarriedImage" ) );
	m_pPlayingTo = dynamic_cast<CTFLabel *>( FindChildByName( "PlayingTo" ) );
	m_pPlayingToBG = dynamic_cast<CTFImagePanel *>( FindChildByName( "PlayingToBG" ) );

	m_pRedFlag = dynamic_cast<CTFFlagStatus *>( FindChildByName( "RedFlag" ) );
	m_pBlueFlag = dynamic_cast<CTFFlagStatus *>( FindChildByName( "BlueFlag" ) );

	m_pCapturePoint = dynamic_cast<CTFArrowPanel *>( FindChildByName( "CaptureFlag" ) );

	m_pSpecCarriedImage = dynamic_cast<ImagePanel *>( FindChildByName( "SpecCarriedImage" ) );

	// outline is always on, so we need to init the alpha to 0
	CTFImagePanel *pOutline = dynamic_cast<CTFImagePanel *>( FindChildByName( "OutlineImage" ) );
	if ( pOutline )
	{
		pOutline->SetAlpha( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "FlagOutlineHide" );

	if ( m_pCarriedImage && m_pCarriedImage->IsVisible() )
	{
		m_pCarriedImage->SetVisible( false );
	}

	if ( m_pBlueFlag && !m_pBlueFlag->IsVisible() )
	{
		m_pBlueFlag->SetVisible( true );
	}

	if ( m_pRedFlag && !m_pRedFlag->IsVisible() )
	{
		m_pRedFlag->SetVisible( true );
	}

	if ( m_pSpecCarriedImage && m_pSpecCarriedImage->IsVisible() )
	{
		m_pSpecCarriedImage->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::SetPlayingToLabelVisible( bool bVisible )
{
	if ( m_pPlayingTo && m_pPlayingToBG )
	{
		if ( m_pPlayingTo->IsVisible() != bVisible )
		{
			m_pPlayingTo->SetVisible( bVisible );
		}

		if ( m_pPlayingToBG->IsVisible() != bVisible )
		{
			m_pPlayingToBG->SetVisible( bVisible );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::OnTick()
{
	// iterate through the flags to set their position in our HUD
	for ( int i = 0; i < g_Flags.Count(); i++ )
	{
		CCaptureFlag *pFlag = dynamic_cast< CCaptureFlag* >( ClientEntityList().GetEnt( g_Flags[i] ) );

		if ( pFlag )
		{
			if ( !pFlag->IsDisabled() )
			{
				if ( m_pRedFlag && pFlag->GetTeamNumber() == TF_TEAM_RED )
				{
					m_pRedFlag->SetEntity( pFlag );
				}
				else if ( m_pBlueFlag && pFlag->GetTeamNumber() == TF_TEAM_BLUE )
				{
					m_pBlueFlag->SetEntity( pFlag );
				}
			}
		}
		else
		{
			// this isn't a valid index for a flag
			g_Flags.Remove( i );
		}
	}

	// are we playing captures for rounds?
	if ( tf_flag_caps_per_round.GetInt() > 0 )
	{
		C_TFTeam *pTeam = GetGlobalTFTeam( TF_TEAM_BLUE );
		if ( pTeam )
		{
			SetDialogVariable( "bluescore", pTeam->GetFlagCaptures() );
		}

		pTeam = GetGlobalTFTeam( TF_TEAM_RED );
		if ( pTeam )
		{
			SetDialogVariable( "redscore", pTeam->GetFlagCaptures() );
		}

		SetPlayingToLabelVisible( true );
		SetDialogVariable( "rounds", tf_flag_caps_per_round.GetInt() );
	}
	else // we're just playing straight score
	{
		C_TFTeam *pTeam = GetGlobalTFTeam( TF_TEAM_BLUE );
		if ( pTeam )
		{
			SetDialogVariable( "bluescore", pTeam->Get_Score() );
		}

		pTeam = GetGlobalTFTeam( TF_TEAM_RED );
		if ( pTeam )
		{
			SetDialogVariable( "redscore", pTeam->Get_Score() );
		}

		SetPlayingToLabelVisible( false );
	}

	// check the local player to see if they're spectating, OBS_MODE_IN_EYE, and the target entity is carrying the flag
	bool bSpecCarriedImage = false;
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pPlayer && ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) )
	{
		// does our target have the flag?
		C_BaseEntity *pEnt = pPlayer->GetObserverTarget();
		if ( pEnt && pEnt->IsPlayer() )
		{
			C_TFPlayer *pTarget = static_cast< C_TFPlayer* >( pEnt );
			if ( pTarget->HasTheFlag() )
			{
				bSpecCarriedImage = true;
				if ( pTarget->GetTeamNumber() == TF_TEAM_RED )
				{
					if ( m_pSpecCarriedImage )
					{
						m_pSpecCarriedImage->SetImage( "../hud/objectives_flagpanel_carried_blue" );
					}
				}
				else
				{
					if ( m_pSpecCarriedImage )
					{
						m_pSpecCarriedImage->SetImage( "../hud/objectives_flagpanel_carried_red" );
					}
				}
			}
		}
	}

	if ( bSpecCarriedImage )
	{
		if ( m_pSpecCarriedImage && !m_pSpecCarriedImage->IsVisible() )
		{
			m_pSpecCarriedImage->SetVisible( true );
		}
	}
	else
	{
		if ( m_pSpecCarriedImage && m_pSpecCarriedImage->IsVisible() )
		{
			m_pSpecCarriedImage->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::UpdateStatus( void )
{
	C_TFPlayer *pLocalPlayer = ToTFPlayer( C_BasePlayer::GetLocalPlayer() );

	// are we carrying a flag?
	CCaptureFlag *pPlayerFlag = NULL;
	if ( pLocalPlayer && pLocalPlayer->HasItem() && ( pLocalPlayer->GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG ) )
	{
		pPlayerFlag = dynamic_cast<CCaptureFlag*>( pLocalPlayer->GetItem() );
	}

	if ( pPlayerFlag )
	{
		m_bCarryingFlag = true;

		// make sure the panels are on, set the initial alpha values, 
		// set the color of the flag we're carrying, and start the animations
		if ( m_pCarriedImage && !m_bFlagAnimationPlayed )
		{
			m_bFlagAnimationPlayed = true;

			if ( m_pBlueFlag && m_pBlueFlag->IsVisible() )
			{
				m_pBlueFlag->SetVisible( false );
			}

			if ( m_pRedFlag && m_pRedFlag->IsVisible() )
			{
				m_pRedFlag->SetVisible( false );
			}

			if ( !m_pCarriedImage->IsVisible() )
			{
				m_pCarriedImage->SetVisible( true );
			}

			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "FlagOutline" );

			if ( m_pCapturePoint )
			{
				if ( !m_pCapturePoint->IsVisible() )
				{
					m_pCapturePoint->SetVisible( true );
				}

				if ( pLocalPlayer )
				{
					// go through all the capture zones and find ours
					for ( int i = 0; i < g_CaptureZones.Count(); i++ )
					{
						C_BaseEntity *pZone = ClientEntityList().GetEnt( g_CaptureZones[i] );

						if ( pZone )
						{
							if ( pZone->GetTeamNumber() == pLocalPlayer->GetTeamNumber() )
							{
								m_pCapturePoint->SetEntity( pZone );
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// were we carrying the flag?
		if ( m_bCarryingFlag )
		{
			m_bCarryingFlag = false;
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "FlagOutline" );
		}

		m_bFlagAnimationPlayed = false;

		if ( m_pCarriedImage && m_pCarriedImage->IsVisible() )
		{
			m_pCarriedImage->SetVisible( false );
		}

		if ( m_pCapturePoint && m_pCapturePoint->IsVisible() )
		{
			m_pCapturePoint->SetVisible( false );
		}

		if ( m_pBlueFlag )
		{
			if ( !m_pBlueFlag->IsVisible() )
			{
				m_pBlueFlag->SetVisible( true );
			}
			
			m_pBlueFlag->UpdateStatus();
		}

		if ( m_pRedFlag )
		{
			if ( !m_pRedFlag->IsVisible() )
			{
				m_pRedFlag->SetVisible( true );
			}

			m_pRedFlag->UpdateStatus();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudFlagObjectives::FireGameEvent( IGameEvent *event )
{
	const char *eventName = event->GetName();

	if ( !Q_strcmp( eventName, "flagstatus_update" ) )
	{
		UpdateStatus();
	}
}
