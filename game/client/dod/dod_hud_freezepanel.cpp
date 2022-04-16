//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_hud_freezepanel.h"
#include "vgui_controls/AnimationController.h"
#include "iclientmode.h"
#include "c_dod_player.h"
#include "c_dod_playerresource.h"
#include <vgui_controls/Label.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "fmtstr.h"
#include "dod_gamerules.h"
#include "view.h"
#include "ivieweffects.h"
#include "viewrender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT_DEPTH( CDODFreezePanel, 1 );

#define CALLOUT_WIDE		(XRES(100))
#define CALLOUT_TALL		(XRES(50))

extern float g_flFreezeFlash;

ConVar cl_dod_freezecam( "cl_dod_freezecam", "1", FCVAR_ARCHIVE, "Client option to not show freeze camera on death" );

#define FREEZECAM_SCREENSHOT_STRING "is looking good!"

bool IsTakingAFreezecamScreenshot( void )
{
	// Don't draw in freezecam, or when the game's not running
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	bool bInFreezeCam = ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM );

	if ( bInFreezeCam == true && engine->IsTakingScreenshot() )
		return true;

	CDODFreezePanel *pPanel = GET_HUDELEMENT( CDODFreezePanel );
	if ( pPanel )
	{
		if ( pPanel->IsHoldingAfterScreenShot() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODFreezePanel::CDODFreezePanel( const char *pElementName )
: EditablePanel( NULL, "FreezePanel" ), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetScheme( "ClientScheme" );

	m_iKillerIndex = 0;
	m_iShowNemesisPanel = SHOW_NO_NEMESIS;
	m_iYBase = -1;
	m_flShowCalloutsAt = 0;

	m_iBasePanelOriginalX = -1;
	m_iBasePanelOriginalY = -1;

	RegisterForRenderGroup( "winpanel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::Reset()
{
	Hide();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::Init()
{
	// listen for events
	ListenForGameEvent( "show_freezepanel" );
	ListenForGameEvent( "hide_freezepanel" );
	ListenForGameEvent( "freezecam_started" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "dod_win_panel" );

	Hide();

	CHudElement::Init();
}

//-----------------------------------------------------------------------------
// Purpose: Applies scheme settings
//-----------------------------------------------------------------------------
void CDODFreezePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/FreezePanel_Basic.res" );

	m_pBasePanel = dynamic_cast<EditablePanel *>( FindChildByName("FreezePanelBase") );

	Assert( m_pBasePanel );

	if ( m_pBasePanel )
	{
		m_pFreezeLabel = dynamic_cast<Label *>( m_pBasePanel->FindChildByName("FreezeLabel") );
		m_pFreezePanelBG = m_pBasePanel->FindChildByName( "FreezePanelBG" );
		m_pNemesisSubPanel = dynamic_cast<EditablePanel *>( m_pBasePanel->FindChildByName( "NemesisSubPanel" ) );
		m_pHealthStatus = dynamic_cast<CDoDHudHealth *>( m_pBasePanel->FindChildByName( "PlayerStatusHealth" ) );
		m_pAvatar = dynamic_cast<CAvatarImagePanel *>( m_pBasePanel->FindChildByName("AvatarImage") );

		if ( m_pAvatar )
		{
			m_pAvatar->SetShouldScaleImage( true );
			m_pAvatar->SetShouldDrawFriendIcon( false );
		}
	}

	m_pScreenshotPanel = dynamic_cast<EditablePanel *>( FindChildByName( "ScreenshotPanel" ) );
	Assert( m_pScreenshotPanel );

	// Move killer panels when the win panel is up
	int xp,yp;
	GetPos( xp, yp );
	m_iYBase = yp;

	int w, h;
	m_pBasePanel->GetBounds( m_iBasePanelOriginalX, m_iBasePanelOriginalY, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::FireGameEvent( IGameEvent * event )
{
	if ( !cl_dod_freezecam.GetBool() )
	{
		if ( IsVisible() )
		{
			Hide();
		}
		return;
	}

	const char *pEventName = event->GetName();

	if ( Q_strcmp( "player_death", pEventName ) == 0 )
	{
		// see if the local player died
		int iPlayerIndexVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
		if ( pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex() )
		{
			// the local player is dead, see if this is a new nemesis or a revenge
			if ( event->GetInt( "dominated" ) > 0 )
			{
				m_iShowNemesisPanel = SHOW_NEW_NEMESIS;
			}
			else if ( event->GetInt( "revenge" ) > 0 )
			{
				m_iShowNemesisPanel = SHOW_REVENGE;
			}
			else
			{
				m_iShowNemesisPanel = SHOW_NO_NEMESIS;
			}
		}		
	}
	else if ( Q_strcmp( "hide_freezepanel", pEventName ) == 0 )
	{
		Hide();
	}
	else if ( Q_strcmp( "freezecam_started", pEventName ) == 0 )
	{
		ShowCalloutsIn( 1.0 );
		ShowSnapshotPanelIn( 1.0 );
	}
	else if ( Q_strcmp( "dod_win_panel", pEventName ) == 0 )
	{
		Hide();
	}
	else if ( Q_strcmp( "show_freezepanel", pEventName ) == 0 )
	{
		C_DOD_PlayerResource *tf_PR = dynamic_cast<C_DOD_PlayerResource *>(g_PR);
		if ( !tf_PR )
		{
			m_pNemesisSubPanel->SetDialogVariable( "nemesisname", NULL );
			return;
		}

		Show();

		ShowSnapshotPanel( false );
		m_bHoldingAfterScreenshot = false;

		if ( m_iBasePanelOriginalX > -1 && m_iBasePanelOriginalY > -1 )
		{
			m_pBasePanel->SetPos( m_iBasePanelOriginalX, m_iBasePanelOriginalY );
		}

		// Get the entity who killed us
		m_iKillerIndex = event->GetInt( "killer" );
		C_BaseEntity *pKiller =  ClientEntityList().GetBaseEntity( m_iKillerIndex );

		int xp,yp;
		GetPos( xp, yp );
		SetPos( xp, m_iYBase );

		bool bShowHealth = pKiller && pKiller->IsPlayer();
		m_pHealthStatus->SetVisible( bShowHealth );
		if ( bShowHealth )
		{
			m_pHealthStatus->SetHealthDelegatePlayer( ToDODPlayer(pKiller) );
		}

		if ( pKiller )
		{
			if ( pKiller->IsPlayer() )
			{
				C_DODPlayer *pVictim = C_DODPlayer::GetLocalDODPlayer();
				C_DODPlayer *pDODKiller = ToDODPlayer( pKiller );

				//If this was just a regular kill but this guy is our nemesis then just show it.
				if ( pVictim && pDODKiller && pDODKiller->m_Shared.IsPlayerDominated( pVictim->entindex() ) )
				{
					if ( !pKiller->IsAlive() )
					{
						m_pFreezeLabel->SetText( "#FreezePanel_Nemesis_Dead" );
					}
					else
					{
						m_pFreezeLabel->SetText( "#FreezePanel_Nemesis" );
					}
				}
				else
				{
					if ( !pKiller->IsAlive() )
					{
						m_pFreezeLabel->SetText( "#FreezePanel_Killer_Dead" );
					}
					else
					{
						m_pFreezeLabel->SetText( "#FreezePanel_Killer" );
					}
				}

				m_pBasePanel->SetDialogVariable( "killername", g_PR->GetPlayerName( m_iKillerIndex ) );

				if ( m_pAvatar )
				{
					m_pAvatar->SetPlayer( (C_BasePlayer*)pKiller );
				}
			}
			else if ( m_pFreezeLabel )
			{
				if ( !pKiller->IsAlive() )
				{
					m_pFreezeLabel->SetText( "#FreezePanel_Killer_Dead" );
				}
				else
				{
					m_pFreezeLabel->SetText( "#FreezePanel_Killer" );
				}
			}
		}

		// see if we should show nemesis panel
		const wchar_t *pchNemesisText = NULL;
		switch ( m_iShowNemesisPanel )
		{
		case SHOW_NO_NEMESIS:
			{
				C_DODPlayer *pVictim = C_DODPlayer::GetLocalDODPlayer();
				C_DODPlayer *pTFKiller = ToDODPlayer( pKiller );

				//If this was just a regular kill but this guy is our nemesis then just show it.
				if ( pTFKiller && pTFKiller->m_Shared.IsPlayerDominated( pVictim->entindex() ) )
				{					
					pchNemesisText = g_pVGuiLocalize->Find( "#FreezePanel_FreezeNemesis" );
				}
			}
			break;
		case SHOW_NEW_NEMESIS:
			{
				C_DODPlayer *pVictim = C_DODPlayer::GetLocalDODPlayer();
				C_DODPlayer *pTFKiller = ToDODPlayer( pKiller );
				// check to see if killer is still the nemesis of victim; victim may have managed to kill him after victim's
				// death by grenade or some such, extracting revenge and clearing nemesis condition
				if ( pTFKiller && pTFKiller->m_Shared.IsPlayerDominated( pVictim->entindex() ) )
				{					
					pchNemesisText = g_pVGuiLocalize->Find( "#FreezePanel_NewNemesis" );
				}			
			}
			break;
		case SHOW_REVENGE:
			pchNemesisText = g_pVGuiLocalize->Find( "#FreezePanel_GotRevenge" );
			break;
		default:
			Assert( false );	// invalid value
			break;
		}
		m_pNemesisSubPanel->SetDialogVariable( "nemesisname", pchNemesisText );

		ShowNemesisPanel( NULL != pchNemesisText );
		m_iShowNemesisPanel = SHOW_NO_NEMESIS;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::ShowCalloutsIn( float flTime )
{
	m_flShowCalloutsAt = gpGlobals->curtime + flTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDODFreezePanelCallout *CDODFreezePanel::TestAndAddCallout( Vector &origin, Vector &vMins, Vector &vMaxs, CUtlVector<Vector> *vecCalloutsTL, 
														 CUtlVector<Vector> *vecCalloutsBR, Vector &vecFreezeTL, Vector &vecFreezeBR, Vector &vecStatTL, Vector &vecStatBR, int *iX, int *iY )
{
	// This is the offset from the topleft of the callout to the arrow tip
	const int iXOffset = XRES(25);
	const int iYOffset = YRES(50);

	//if ( engine->IsBoxInViewCluster( vMins + origin, vMaxs + origin) && !engine->CullBox( vMins + origin, vMaxs + origin ) )
	{
		if ( GetVectorInHudSpace( origin, *iX, *iY ) )		// TODO: GetVectorInHudSpace or GetVectorInScreenSpace?
		{
			*iX -= iXOffset;
			*iY -= iYOffset;
			int iRight = *iX + CALLOUT_WIDE;
			int iBottom = *iY + CALLOUT_TALL;
			if ( *iX > 0 && *iY > 0 && (iRight < ScreenWidth()) && (iBottom < (ScreenHeight()-YRES(40))) )
			{
				// Make sure it wouldn't be over the top of the freezepanel or statpanel
				Vector vecCalloutTL( *iX, *iY, 0 );
				Vector vecCalloutBR( iRight, iBottom, 1 );
				if ( !QuickBoxIntersectTest( vecCalloutTL, vecCalloutBR, vecFreezeTL, vecFreezeBR ) &&
					!QuickBoxIntersectTest( vecCalloutTL, vecCalloutBR, vecStatTL, vecStatBR ) )
				{
					// Make sure it doesn't intersect any other callouts
					bool bClear = true;
					for ( int iCall = 0; iCall < vecCalloutsTL->Count(); iCall++ )
					{
						if ( QuickBoxIntersectTest( vecCalloutTL, vecCalloutBR, vecCalloutsTL->Element(iCall), vecCalloutsBR->Element(iCall) ) )
						{
							bClear = false;
							break;
						}
					}

					if ( bClear )
					{
						// Verify that we have LOS to the gib
						trace_t	tr;
						UTIL_TraceLine( origin, MainViewOrigin(), MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr );
						bClear = ( tr.fraction >= 1.0f );
					}

					if ( bClear )
					{
						CDODFreezePanelCallout *pCallout = new CDODFreezePanelCallout( g_pClientMode->GetViewport(), "FreezePanelCallout" );
						m_pCalloutPanels.AddToTail( vgui::SETUP_PANEL(pCallout) );
						vecCalloutsTL->AddToTail( vecCalloutTL );
						vecCalloutsBR->AddToTail( vecCalloutBR );
						pCallout->SetVisible( true );
						pCallout->SetBounds( *iX, *iY, CALLOUT_WIDE, CALLOUT_TALL );
						return pCallout;
					}
				}
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::UpdateCallout( void )
{
	CDODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pPlayer )
		return;

	// Abort early if we have no ragdoll
	IRagdoll *pRagdoll = pPlayer->GetRepresentativeRagdoll();
	if ( !pRagdoll )
		return;

	if ( m_pFreezePanelBG == NULL )
		return;

	// Precalc the vectors of the freezepanel & statpanel
	int iX, iY;
	m_pFreezePanelBG->GetPos( iX, iY );
	Vector vecFreezeTL( iX, iY, 0 );
	Vector vecFreezeBR( iX + m_pFreezePanelBG->GetWide(), iY + m_pFreezePanelBG->GetTall(), 1 );

	CUtlVector<Vector> vecCalloutsTL;
	CUtlVector<Vector> vecCalloutsBR;

	Vector vecStatTL(0,0,0);
	Vector vecStatBR(0,0,1);

	Vector vMins, vMaxs;

	if ( pRagdoll )
	{
		Vector origin = pRagdoll->GetRagdollOrigin();
		pRagdoll->GetRagdollBounds( vMins, vMaxs );

		// Try and add the callout
		//CDODFreezePanelCallout *pCallout = 
			
		TestAndAddCallout( origin, vMins, vMaxs, &vecCalloutsTL, &vecCalloutsBR, 
			vecFreezeTL, vecFreezeBR, vecStatTL, vecStatBR, &iX, &iY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::Show()
{
	m_flShowCalloutsAt = 0;
	SetVisible( true );

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "freezepanel" );
	if ( iRenderGroup >= 0 )
	{
		gHUD.LockRenderGroup( iRenderGroup );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::Hide()
{
	SetVisible( false );
	m_bHoldingAfterScreenshot = false;

	// Delete all our callout panels
	for ( int i = m_pCalloutPanels.Count()-1; i >= 0; i-- )
	{
		m_pCalloutPanels[i]->MarkForDeletion();
	}
	m_pCalloutPanels.RemoveAll();

	int iRenderGroup = gHUD.LookupRenderGroupIndexByName( "winpanel" );
	if ( iRenderGroup >= 0 )
	{
		gHUD.UnlockRenderGroup( iRenderGroup );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDODFreezePanel::ShouldDraw( void )
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	return ( IsVisible() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::OnThink( void )
{
	BaseClass::OnThink();

	if ( m_flShowCalloutsAt && m_flShowCalloutsAt < gpGlobals->curtime )
	{
		if ( ShouldDraw() )
		{
			UpdateCallout();
		}
		m_flShowCalloutsAt = 0;
	}

	if ( m_flShowSnapshotReminderAt && m_flShowSnapshotReminderAt < gpGlobals->curtime )
	{
		if ( ShouldDraw() )
		{
			ShowSnapshotPanel( true );
		}
		m_flShowSnapshotReminderAt = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::ShowSnapshotPanelIn( float flTime )
{
	m_flShowSnapshotReminderAt = gpGlobals->curtime + flTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODFreezePanel::ShowSnapshotPanel( bool bShow )
{
	if ( !m_pScreenshotPanel )
		return;

	const char *key = engine->Key_LookupBinding( "screenshot" );

	if ( key == NULL || FStrEq( key, "(null)" ) )
	{
		bShow = false;
		key = " ";
	}

	if ( bShow )
	{
		char szKey[16];
		Q_snprintf( szKey, sizeof(szKey), "%s", key );
		wchar_t wKey[16];
		wchar_t wLabel[256];

		g_pVGuiLocalize->ConvertANSIToUnicode(szKey, wKey, sizeof(wKey));
		g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find("#TF_freezecam_snapshot" ), 1, wKey );

		m_pScreenshotPanel->SetDialogVariable( "text", wLabel );

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudSnapShotReminderIn" );
	}

	m_pScreenshotPanel->SetVisible( bShow );
}

int	CDODFreezePanel::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( ShouldDraw() && pszCurrentBinding )
	{
		if ( FStrEq( pszCurrentBinding, "screenshot" ) || FStrEq( pszCurrentBinding, "jpeg" ) )
		{
			// move the target id to the corner
			if ( m_pBasePanel )
			{
				int w, h;
				m_pBasePanel->GetSize( w, h );
				m_pBasePanel->SetPos( ScreenWidth() - w, ScreenHeight() - h );
			}

			// Get the local player.
			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			if ( pPlayer )
			{
				//Do effects
				g_flFreezeFlash = gpGlobals->curtime + 0.75f;
				pPlayer->EmitSound( "Camera.SnapShot" );

				//Extend Freezecam by a couple more seconds.
				engine->ClientCmd( "extendfreeze" );
				view->FreezeFrame( 3.0f );

				//Hide the reminder panel
				m_flShowSnapshotReminderAt = 0;
				ShowSnapshotPanel( false );

				m_bHoldingAfterScreenshot = true;

				//Set the screenshot name
				if ( m_iKillerIndex <= MAX_PLAYERS )
				{
					const char *pszKillerName = g_PR->GetPlayerName( m_iKillerIndex );

					if ( pszKillerName )
					{
						ConVarRef cl_screenshotname( "cl_screenshotname" );

						if ( cl_screenshotname.IsValid() )
						{
							char szScreenShotName[512];

							Q_snprintf( szScreenShotName, sizeof( szScreenShotName ), "%s %s", pszKillerName, FREEZECAM_SCREENSHOT_STRING );

							cl_screenshotname.SetValue( szScreenShotName );
						}
					}
				}
			}
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Shows or hides the nemesis part of the panel
//-----------------------------------------------------------------------------
void CDODFreezePanel::ShowNemesisPanel( bool bShow )
{
	m_pNemesisSubPanel->SetVisible( bShow );

	if ( bShow )
	{
		vgui::Label *pLabel = dynamic_cast< vgui::Label *>( m_pNemesisSubPanel->FindChildByName( "NemesisLabel" ) );
		vgui::Panel *pBG = m_pNemesisSubPanel->FindChildByName( "NemesisPanelBG" );
		vgui::ImagePanel *pIcon = dynamic_cast< vgui::ImagePanel *>( m_pNemesisSubPanel->FindChildByName( "NemesisIcon" ) );

		// check that our Nemesis panel and resize it to the length of the string (the right side is pinned and doesn't move)
		if ( pLabel && pBG && pIcon )
		{
			int wide, tall;
			pLabel->GetContentSize( wide, tall );

			int nDiff = wide - pLabel->GetWide();

			if ( nDiff != 0 )
			{
				int x, y, w, t;

				// move the icon
				pIcon->GetBounds( x, y, w, t );
				pIcon->SetBounds( x - nDiff, y, w, t );

				// move/resize the label
				pLabel->GetBounds( x, y, w, t );
				pLabel->SetBounds( x - nDiff, y, w + nDiff, t );

				// move/resize the background
				pBG->GetBounds( x, y, w, t );
				pBG->SetBounds( x - nDiff, y, w + nDiff, t );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDODFreezePanelCallout::CDODFreezePanelCallout( Panel *parent, const char *name ) : EditablePanel(parent,name)
{
}

//-----------------------------------------------------------------------------
// Purpose: Applies scheme settings
//-----------------------------------------------------------------------------
void CDODFreezePanelCallout::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/FreezePanelCallout.res" );
}