//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cbase.h"

#ifdef SAXXYMAINMENU_ENABLED

#include "tf_hud_saxxycontest.h"
#include "econ_item_inventory.h"
#include "econ/confirm_dialog.h"
#include "econ/econ_controls.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ComboBox.h"
#include "ienginevgui.h"
#include "replaybrowsermainpanel.h"
#include "gc_clientsystem.h"

#include "rtime.h"
#include "tf_gcmessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_BUILD_FACTORY( CSaxxyAwardsPanel );

static void SaxxyAwards_ShowSubmitForm();

//-----------------------------------------------------------------------------

int	CSaxxyAwardsPanel::sm_nShowCounter = -1;

//-----------------------------------------------------------------------------

#define FOR_EACH_FLASH( i_ ) 	for ( int i_ = 0; i_ < MAX_FLASHES; ++i_ )
#define FOR_EACH_GLOW( i_ ) 	for ( int i_ = 0; i_ < MAX_GLOWS; ++i_ )
#define FOR_EACH_CLAP( i_ ) 	for ( int i_ = 0; i_ < MAX_CLAPS; ++i_ )

//-----------------------------------------------------------------------------

#define NUM_FLASH_SOUNDS		11
#define NUM_FLASH_MATERIALS		3
#define FREAKOUT_THRESHOLD		0.7f

//-----------------------------------------------------------------------------

inline float SCurve( float t )
{
	t = clamp( t, 0.0f, 1.0f );
	return t * t * (3 - 2*t);
}

inline float LerpScale( float flIn, float flInMin, float flInMax, float flOutMin, float flOutMax )
{
	float flDenom = flInMax - flInMin;
	if ( flDenom == 0.0f )
		return 0.0f;

	float t = clamp( ( flIn - flInMin ) / flDenom, 0.0f, 1.0f );
	return Lerp( t, flOutMin, flOutMax );
}

//-----------------------------------------------------------------------------

CSaxxyAwardsPanel::CSaxxyAwardsPanel( Panel *pParent, const char *pName )
:	EditablePanel( pParent, pName ),
	m_pCameraFlashKv( NULL )
{
	ivgui()->AddTickSignal( GetVPanel(), 17 );

	Init();

	++sm_nShowCounter;
}

CSaxxyAwardsPanel::~CSaxxyAwardsPanel()
{
	ivgui()->RemoveTickSignal( GetVPanel() );
}

void CSaxxyAwardsPanel::Init()
{
	m_flShowTime = GetCurrentTime();

	m_vDialogsParent = (VPANEL)-1;
	m_nNumTargetFlashes = 0;
	m_flLastTickTime = 0.0f;
	m_flEarliestNextFlashTime = 0.0f;
	m_flGlowFade = 0.0f;
	m_flNextPanelTestTime = 0.0f;
	m_pBackgroundPanel = NULL;
	m_pSaxxyModelPanel = NULL;
	m_pStageBgPanel = NULL;
	m_pSubmitButton = NULL;
	m_pInfoLabel = NULL;
	m_pCurtainPanel = NULL;
	m_pContestOverLabel = NULL;
	m_pSpotlightPanel = NULL;

	m_flCurtainStartAnimTime = -1.0f;

	FOR_EACH_CLAP( i )
	{
		m_aClapPlayTimes[i] = 0.0f;
	}

	for ( int i = 0; i < 2; ++i )
	{
		m_aFilteredMousePos[i][0] = ScreenWidth() / 2;
		m_aFilteredMousePos[i][1] = ScreenHeight() / 2;
	}

	FOR_EACH_FLASH( i )
	{
		CFmtStr fmtPanelName( "FlashPanel_%i", i );
		FlashInfo_t *pCurFlashInfo = &m_aFlashes[ i ];
		pCurFlashInfo->m_pPanel = new ImagePanel( NULL, fmtPanelName.Access() );
		pCurFlashInfo->m_bInUse = false;
	}

	m_angModelRot.Init();
	m_vSaxxyDefaultPos.Init();
}

void CSaxxyAwardsPanel::ApplySettings( KeyValues *pInResourceData )
{
	BaseClass::ApplySettings( pInResourceData );

	// Delete old KeyValues
	if ( m_pCameraFlashKv )
	{
		m_pCameraFlashKv->deleteThis();
		m_pCameraFlashKv = NULL;
	}

	// Copy camera flash keyvalues for creating instances
	KeyValues *pCameraFlash = pInResourceData->FindKey( "CameraFlashSettings" );
	Assert( pCameraFlash );
	if ( pCameraFlash )
	{
		m_pCameraFlashKv = pCameraFlash->MakeCopy();
	}
}

bool CSaxxyAwardsPanel::CreateFlash()
{
	if ( GetActiveFlashCount() == MAX_FLASHES )
		return false;

	const int iUnusedSlot = GetUnusedFlashSlot();		Assert( iUnusedSlot >= 0 && iUnusedSlot < MAX_FLASHES );

	if ( iUnusedSlot < 0 )
		return false;	// Should never happen, since GetActiveFlashCount() didn't return 0

	FlashInfo_t *pFlashInfo = &m_aFlashes[ iUnusedSlot ];
	ImagePanel *pPanel = pFlashInfo->m_pPanel;

	// Apply base settings
	if ( !m_pCameraFlashKv )
		return false;

	pPanel->ApplySettings( m_pCameraFlashKv );

	// Set the image to a random image
	const int iImage = 1 + rand() % NUM_FLASH_MATERIALS;
	CFmtStr fmtImageName(  "../models/effects/camera_flash/camera_flash_%02i", iImage );
	pPanel->SetImage( fmtImageName.Access() );

	pFlashInfo->m_flStartTime = GetCurrentTime();
	pFlashInfo->m_flLifeLength = FRand( m_flFlashLifeLengthMin, m_flFlashLifeLengthMax );
	pFlashInfo->m_bInUse = true;

	// Put the flash at a random position in the "flash bounds"
	pFlashInfo->m_nCenterX = (int)FRand( m_nFlashBoundsX, m_nFlashBoundsX + m_nFlashBoundsW );
	pFlashInfo->m_nCenterY = (int)FRand( m_nFlashBoundsY, m_nFlashBoundsY + m_nFlashBoundsH );

	pFlashInfo->m_nMinSize = (int)Lerp( FRand( 0.0f, 1.0f ), m_nFlashStartSizeMin, m_nFlashStartSizeMax );
	pFlashInfo->m_nMaxSize = pFlashInfo->m_nMinSize * FRand( .5f * m_flFlashMaxScale, m_flFlashMaxScale );

	Assert( pFlashInfo->m_nMaxSize > pFlashInfo->m_nMinSize );

	pFlashInfo->m_nCurW = pFlashInfo->m_nMinSize;
	pFlashInfo->m_nCurH = pFlashInfo->m_nMinSize;

	PlaceFlash( pFlashInfo );
	
	// Setup visibility
	pPanel->SetAlpha( 0 );
	pPanel->SetVisible( true );

	// Play a random flash sound
	int iSound = rand() % NUM_FLASH_SOUNDS + 1;
	Assert( iSound >= 1 && iSound <= NUM_FLASH_SOUNDS );
	CFmtStr fmtSoundFilename( "misc/tf_camera_%02i.wav", iSound );
	surface()->PlaySound( fmtSoundFilename.Access() );
	
	return true;
}

void CSaxxyAwardsPanel::PlaceFlash( FlashInfo_t *pFlashInfo )
{
	pFlashInfo->m_pPanel->SetBounds(
		pFlashInfo->m_nCenterX - pFlashInfo->m_nCurW / 2,
		pFlashInfo->m_nCenterY - pFlashInfo->m_nCurH / 2,
		pFlashInfo->m_nCurW,
		pFlashInfo->m_nCurH
	);
}

void CSaxxyAwardsPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/MainMenu_SaxxyAwards.res" );

	SetupContestPanels();

	m_vDialogsParent = GetDialogsParent();

	m_pBackgroundPanel = dynamic_cast< EditablePanel * >( FindChildByName( "Background" ) );
	m_pSubmitButton = dynamic_cast< CExButton *>( FindChildByName( "SubmitButton" ) );
	m_pInfoLabel = FindChildByName( "InfoLabel" );
	m_pContestOverLabel = FindChildByName( "ContestOverLabel" );
	m_pSpotlightPanel = dynamic_cast< ImagePanel * >( FindChildByName( "SpotlightPanel" ) );

	m_pStageBgPanel = dynamic_cast< ImagePanel * >( FindChildByName( "StageBackground" ) );
	m_pCurtainPanel = dynamic_cast< EditablePanel * >( FindChildByName( "CurtainsPanel" ) );
	if ( m_pCurtainPanel )
	{
		m_Curtains[0].m_pPanel = dynamic_cast< ImagePanel * >( m_pCurtainPanel->FindChildByName( "CurtainsLeft" ) );
		m_Curtains[1].m_pPanel = dynamic_cast< ImagePanel * >( m_pCurtainPanel->FindChildByName( "CurtainsRight" ) );

		// Hide curtains if this isn't the first view
		if ( sm_nShowCounter > 0 )
		{
			for ( int i = 0; i < 2; ++i )
			{
				if ( !m_Curtains[i].m_pPanel )
					continue;
				m_Curtains[i].m_pPanel->SetVisible( false );
			}
		}

		// Set all flash parents to background panel
		if ( m_pBackgroundPanel )
		{
			FOR_EACH_FLASH( i )
			{
				m_aFlashes[ i ].m_pPanel->SetParent( m_pStageBgPanel );
			}
		}

		// Setup Saxxy model
		m_pSaxxyModelPanel = dynamic_cast< CBaseModelPanel * >( m_pCurtainPanel->FindChildByName( "SaxxyModelPanel" ) );
		if ( m_pSaxxyModelPanel )
		{
			MDLHandle_t hModel = mdlcache->FindMDL( "models/effects/saxxy_flash/saxxy_flash.mdl" );
			m_pSaxxyModelPanel->SetMDL( hModel );
			m_pSaxxyModelPanel->SetSequence( ACT_RESET );
			mdlcache->Release( hModel ); // counterbalance addref from within FindMDL

			m_pSaxxyModelPanel->InvalidateLayout( true, true );	// Updates player position
			m_vSaxxyDefaultPos = m_pSaxxyModelPanel->GetPlayerPos();
		}
	}
}

void CSaxxyAwardsPanel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CSaxxyAwardsPanel::Refresh()
{
	Init();
	InvalidateLayout( true, true );
}

void CSaxxyAwardsPanel::OnCommand( const char *pCommand )
{
	if ( FStrEq( pCommand, "viewdetails" ) )
	{
		if ( steamapicontext && steamapicontext->SteamFriends() )
		{
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( "http://www.teamfortress.com/saxxyawards/" );
		}
	}
	else if ( FStrEq( pCommand, "submit" ) )
	{
		SaxxyAwards_ShowSubmitForm();
	}
	else
	{
		BaseClass::OnCommand( pCommand );
	}
}

void CSaxxyAwardsPanel::OnTick()
{
	if ( C_BasePlayer::GetLocalPlayer() )
		return;

	if ( ReplayUI_GetBrowserPanel() && ReplayUI_GetBrowserPanel()->IsVisible() )
		return;

	if ( !IsVisible() )
		return;

	// Calculate elapsed
	const float flCurTime = GetCurrentTime();
	const float flElapsed = clamp( flCurTime - m_flLastTickTime, 0, 0.1f );
	m_flLastTickTime = flCurTime;

//	Msg( "initial freakout? %s\n", InInitialFreakoutPeriod() ? "yes" : "no" );

	const bool bOtherPanelsOpen = AreOtherPanelsOpen( flCurTime );

	UpdateMousePos( flElapsed );
	CurtainsThink();
	RotateModel( flElapsed );
	FlashThink( bOtherPanelsOpen );
	SpotlightThink();
	ClapsThink( flCurTime, flElapsed, bOtherPanelsOpen );

	SetupContestPanels();
}

bool CSaxxyAwardsPanel::AreOtherPanelsOpen( float flCurTime )
{
	static bool s_bOtherPanelsOpen = false;

	if ( flCurTime < m_flNextPanelTestTime )
		return s_bOtherPanelsOpen;

	// Setup next think time
	m_flNextPanelTestTime = flCurTime + 2.0f;

	// Do tests to see if other panels are open
	bool bOtherPanelsOpen = false;
	{
		const int nNumGameUICarePanels = 4;
		const char *pCarePanels[nNumGameUICarePanels] = { "GameConsole", "character_info", "store_panel", "BugUIPanel" };
		bOtherPanelsOpen = bOtherPanelsOpen || AreNonMainMenuPanelsOpen( enginevgui->GetPanel( PANEL_GAMEUIDLL ), pCarePanels, nNumGameUICarePanels );
	}

	//	Msg( "-----\n" );

	if ( m_vDialogsParent != (VPANEL)-1 )
	{
		const int nNumCarePanels = 2;
		const char *pCarePanels[nNumCarePanels] = { "CServerBrowserDialog", "OptionsDialog" };

		bOtherPanelsOpen = bOtherPanelsOpen || AreNonMainMenuPanelsOpen( m_vDialogsParent, pCarePanels, nNumCarePanels );
	}

	// Cache so we don't have to think every frame
	s_bOtherPanelsOpen = bOtherPanelsOpen;

	return bOtherPanelsOpen;
}

void CSaxxyAwardsPanel::PaintBackground()
{
	BaseClass::PaintBackground();
}

void CSaxxyAwardsPanel::UpdateMousePos( float flElapsed )
{
	// Set target mouse pos
	int x, y;
	input()->GetCursorPos( x, y );
	m_aFilteredMousePos[1][0] = x;
	m_aFilteredMousePos[1][1] = y;

	// Move towards target
	const float t = flElapsed * 1.5f;
	m_aFilteredMousePos[0][0] = Lerp( t, m_aFilteredMousePos[0][0], m_aFilteredMousePos[1][0] );
	m_aFilteredMousePos[0][1] = Lerp( t, m_aFilteredMousePos[0][1], m_aFilteredMousePos[1][1] );

//	Msg( "mouse pos: current=(%f %f)   target=(%f %f)\n", m_aFilteredMousePos[0][0], m_aFilteredMousePos[0][1],
//		m_aFilteredMousePos[1][0], m_aFilteredMousePos[1][1] );
}

void CSaxxyAwardsPanel::CurtainsThink()
{
	if ( !m_Curtains[0].m_pPanel || !m_Curtains[1].m_pPanel )
		return;

	// Has enough time passed?  Only open the curtains m_flOpenCurtainsTime seconds after m_flShowTime.
	const float flElapsedSinceShowTime = GetCurrentTime() - m_flShowTime;
	if ( flElapsedSinceShowTime < m_flOpenCurtainsTime )
		return;

	// Start animating now?
	if ( CurtainsClosed() )
	{
		m_flCurtainStartAnimTime = GetCurrentTime();

		// Play crowd the first time the menu is shown
		if ( sm_nShowCounter == 0 )
		{
			surface()->PlaySound( "misc/boring_applause_1.wav" );
		}

		// Init positions to those set in res file
		for ( int i = 0; i < 2; ++i )
		{
			m_Curtains[i].m_pPanel->GetPos( m_Curtains[i].m_aInitialPos[0], m_Curtains[i].m_aInitialPos[1] );
		}
	}
	else	// Already animating?
	{
		// Set updated positions for both curtains
		const float flCurTime = GetCurrentTime();
		for ( int i = 0; i < 2; ++i )
		{
			CurtainInfo_t *pCurCurtain = &m_Curtains[i];
			Panel *pPanel = pCurCurtain->m_pPanel;

			const int nSign = i == 0 ? -1 : 1;
			const float t = SCurve( clamp( ( flCurTime - m_flCurtainStartAnimTime ) / m_flCurtainAnimDuration, 0.0f, 1.0f ) );
			int aCurPos[2] = {
				Lerp( t, pCurCurtain->m_aInitialPos[0], pCurCurtain->m_aInitialPos[0] + nSign * pPanel->GetWide() ),
				pCurCurtain->m_aInitialPos[1]
			};
			m_Curtains[i].m_pPanel->SetPos( aCurPos[0], aCurPos[1] );
		}
	}
}

void CSaxxyAwardsPanel::RotateModel( float flElapsed )
{
	if ( m_pSaxxyModelPanel )
	{
		const float flOsc = .28f * sinf( .5f * GetCurrentTime() );
		const Vector vCurPos(
			Lerp( m_flGlowFade, m_vSaxxyDefaultPos.x, m_vSaxxyDefaultPos.x - 8.0f ),
			m_vSaxxyDefaultPos.y,
			Lerp( m_flGlowFade, m_vSaxxyDefaultPos.z, m_vSaxxyDefaultPos.z - 3.0f ) + flOsc
		);
		m_angModelRot.y += flElapsed * 45 * 0.5f * Lerp( m_flGlowFade, 0.5f, 1.0f );
		m_pSaxxyModelPanel->SetModelAnglesAndPosition( m_angModelRot, vCurPos );
	}
}

void CSaxxyAwardsPanel::FlashThink( bool bOtherPanelsOpen )
{
	if ( !m_pSaxxyModelPanel )
		return;

	// Set the target number of flashes based on how far the mouse is from the saxxy
	int aModelBounds[4];
	m_pSaxxyModelPanel->GetSize( aModelBounds[2], aModelBounds[3] );
	ipanel()->GetAbsPos( m_pSaxxyModelPanel->GetVPanel(), aModelBounds[0], aModelBounds[1] );
	const float flDistX = aModelBounds[0] + aModelBounds[2]/2.0f - m_aFilteredMousePos[0][0];
	const float flDistY = aModelBounds[1] + aModelBounds[3]/2.0f - m_aFilteredMousePos[0][1];
	const float flDistanceToSaxxy = sqrtf( flDistX * flDistX + flDistY * flDistY );
//	Msg( "xdist: %f   ydist: %f  distance: %f\n", flDistX, flDistY, flDistanceToSaxxy );

	const float flMaxDistance = 750.0f;
	m_flGlowFade = clamp( 1.0f - clamp( flDistanceToSaxxy / flMaxDistance, 0.0f, 1.0f ), 0.0f, 1.0f );
//	Msg( "glow fade: %f\n", m_flGlowFade );

	const float flCurTime = GetCurrentTime();

	// Set alpha for active flashes, kill any that are done animating, update positions of actives
	FOR_EACH_FLASH( i )
	{
		FlashInfo_t *pCurFlashInfo = &m_aFlashes[ i ];
		if ( !pCurFlashInfo->m_bInUse )
			continue;

		const float t = ( flCurTime - pCurFlashInfo->m_flStartTime ) / pCurFlashInfo->m_flLifeLength;
		if ( t >= 1.0f )
		{
			ClearFlash( pCurFlashInfo );
		}
		else
		{
			// Set opacity
			const float flPhase = M_PI * t;
			const float flFade = clamp( sinf( flPhase ), 0.0f, 1.0f );
			pCurFlashInfo->m_pPanel->SetAlpha( 255 * flFade );

			// Update size
			pCurFlashInfo->m_nCurW = (int)Lerp( flFade, pCurFlashInfo->m_nMinSize, pCurFlashInfo->m_nMaxSize );
			pCurFlashInfo->m_nCurH = pCurFlashInfo->m_nCurW;

			// Update position/size
			PlaceFlash( pCurFlashInfo );
		}
	}

	// Create new flashes / freak out
	const bool bEnoughTimeHasPassed = flCurTime >= m_flEarliestNextFlashTime;
	if ( !bOtherPanelsOpen && bEnoughTimeHasPassed && FlashingStartTimePassed() && GetUnusedFlashCount() )
	{
		CreateFlash();

		const bool bFreakout = InInitialFreakoutPeriod() || InFreakoutMode();
		const float flOffset = bFreakout ? FRand( .1f, .2f ) : FRand( 2.0f, 5.0f );
		m_flEarliestNextFlashTime = flCurTime + flOffset;
	}
}

VPANEL CSaxxyAwardsPanel::GetDialogsParent()
{
	VPANEL vGameUI = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	const int nNumChildren = ipanel()->GetChildCount( vGameUI );

	for ( int i = 0; i < nNumChildren; ++i )
	{
		VPANEL vChild = ipanel()->GetChild( vGameUI, i );
		const char *pDlgName = ipanel()->GetName( vChild );
		if ( FStrEq( pDlgName, "BaseGameUIPanel" ) )
		{
			return vChild;
		}
	}

	return (VPANEL)-1;
}

bool CSaxxyAwardsPanel::AreNonMainMenuPanelsOpen( VPANEL vRoot, const char **pCarePanels, int nNumCarePanels )
{
	int nNumChildren = ipanel()->GetChildCount( vRoot );
	for ( int i = 0; i < nNumChildren; ++i )
	{
		VPANEL vChild = ipanel()->GetChild( vRoot, i );
		const char *pDlgName = ipanel()->GetName( vChild );
		bool bCare = false;
		for ( int j = 0; j < nNumCarePanels; ++j )
		{
			if ( FStrEq( pCarePanels[j], pDlgName ) )
			{
//				Msg( "CARE: %s\n", pDlgName );
				bCare = true;
				break;
			}
		}
		if ( !bCare )
		{
//			Msg( "DONT CARE: %s\n", pDlgName );
			continue;
		}

		if ( ipanel()->IsVisible( vChild ) )
			return true;
	}

	return false;
}

void CSaxxyAwardsPanel::SpotlightThink()
{
	if ( !m_pSpotlightPanel )
		return;

	const float flOscillateAmount = 0.10f;
	const float flSpeed = 0.7f;
	const float flFade = .1f + flOscillateAmount * ( .5f + .5f * cosf( flSpeed * GetCurrentTime() ) );
	m_pSpotlightPanel->SetAlpha( 255 * flFade );
}

void CSaxxyAwardsPanel::PlaySomeClaps()
{
	// Play each clap offset by a bit
	const float flCurTime = GetCurrentTime();
	FOR_EACH_CLAP( i )
	{
		// If this sound is already playing, continue
		float &flCurClapPlayTime = m_aClapPlayTimes[i];

		const bool bAlreadyPlaying = flCurClapPlayTime != 0.0f;
		if ( bAlreadyPlaying )
			continue;

		// Some chance that not all claps will be played
		if ( rand() % 100 < 30 )
		{
			SetNextPossibleClapTime( &flCurClapPlayTime );
			continue;
		}

		flCurClapPlayTime = flCurTime + LerpScale( FRand( 0.0f, 1.0f ), 0.0f, 1.0f, .3f, 1.5f );
	}
}

void CSaxxyAwardsPanel::ClapsThink( float flCurTime, float flElapsed, bool bOtherPanelsOpen )
{
	if ( !bOtherPanelsOpen && InFreakoutMode() && !InInitialFreakoutPeriod() )
	{
		PlaySomeClaps();
	}

	FOR_EACH_CLAP( i )
	{
		float &flCurClapTime = m_aClapPlayTimes[i];
//		Msg( "clap %i: %f\n", i, flCurClapTime );

		if ( flCurClapTime < 0.0f )
		{
			// Once enough time has elapsed, this slot will be > 0 and available again.
			flCurClapTime = MIN( flCurClapTime + flElapsed, 0.0f );
			continue;
		}

		// Start playing now?
		if ( flCurClapTime <= flCurTime && flCurClapTime != 0.0f )
		{
			// Play.
			CFmtStr fmtSound( "misc/clap_single_%i.wav", i + 1 );
			surface()->PlaySound( fmtSound.Access() );

			SetNextPossibleClapTime( &flCurClapTime );
		}
	}
}

void CSaxxyAwardsPanel::SetNextPossibleClapTime( float *pClapTime )
{
	// Set the next possible time to the duration of the sound + some randomn offset
	const float flExtraOffset = LerpScale( FRand( 0.0f, 1.0f ), 0.0f, 1.0f, 1.0f, 5.0f );
	*pClapTime = -( m_flClapSoundDuration + flExtraOffset );
}

void CSaxxyAwardsPanel::ClearFlash( FlashInfo_t *pFlashInfo )
{
	pFlashInfo->m_bInUse = false;
	pFlashInfo->m_pPanel->SetAlpha( 0 );
	pFlashInfo->m_pPanel->SetVisible( false );
}

void CSaxxyAwardsPanel::ClearFlashes()
{
	FOR_EACH_FLASH( i )
	{
		ClearFlash( &m_aFlashes[i] );
	}
}

int CSaxxyAwardsPanel::GetActiveFlashCount() const
{
	int nResult = 0;
	FOR_EACH_FLASH( i )
	{
		if ( m_aFlashes[ i ].m_bInUse )
			++nResult;
	}
	return nResult;
}

int CSaxxyAwardsPanel::GetUnusedFlashCount() const
{
	return MAX_FLASHES - GetActiveFlashCount();
}

int CSaxxyAwardsPanel::GetUnusedFlashSlot() const
{
	FOR_EACH_FLASH( i )
	{
		if ( !m_aFlashes[ i ].m_bInUse )
			return i;
	}
	return -1;
}

float CSaxxyAwardsPanel::GetCurrentTime() const
{
	return gpGlobals->curtime;
}

bool CSaxxyAwardsPanel::InInitialFreakoutPeriod() const
{
	return ( GetCurrentTime() - m_flShowTime ) <= m_flInitialFreakoutDuration;
}

bool CSaxxyAwardsPanel::InFreakoutMode() const
{
	return m_flGlowFade > FREAKOUT_THRESHOLD;
}

bool CSaxxyAwardsPanel::CurtainsClosed() const
{
	return m_flCurtainStartAnimTime < 0.0f;
}

bool CSaxxyAwardsPanel::FlashingStartTimePassed() const
{
	return ( GetCurrentTime() - m_flShowTime ) >= m_flFlashStartTime;
}

void CSaxxyAwardsPanel::SetupContestPanels()
{
	CRTime now;
	now.SetToCurrentTime();
	now.SetToGMT( true );

	CRTime deadline = CRTime::RTime32FromString( "2011-05-21 00:00:00" );
	deadline.SetToGMT( true );

	if ( now > deadline )
	{
		if ( m_pInfoLabel && m_pInfoLabel->IsVisible() )
		{
			m_pInfoLabel->SetVisible( false );
		}
		if ( m_pSubmitButton && m_pSubmitButton->IsVisible() )
		{
			m_pSubmitButton->SetVisible( false );
		}
		if ( m_pContestOverLabel && !m_pContestOverLabel->IsVisible() )
		{
			m_pContestOverLabel->SetVisible( true );
		}
	}	
}

CSaxxyAwardsSubmitForm::CSaxxyAwardsSubmitForm( Panel *pParent )
:	BaseClass( pParent, "SaxxySubmitForm" ),
	m_pURLInput( NULL ),
	m_pCategoryCombo( NULL )
{
}

void CSaxxyAwardsSubmitForm::ApplySchemeSettings( IScheme *pScheme )
{
	// Link in TF scheme
	extern IEngineVGui *enginevgui;
	vgui::HScheme pTFScheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme" );
	SetScheme( pTFScheme );
	SetProportional( true );

	BaseClass::ApplySchemeSettings( vgui::scheme()->GetIScheme( pTFScheme ) );

	LoadControlSettings( "resource/UI/SaxxyAwards_SubmitForm.res" );

	m_pURLInput = dynamic_cast< TextEntry * >( FindChildByName( "URLInput" ) );
	if ( m_pURLInput )
	{
		m_pURLInput->SetText( "#Replay_YouTubeURL" );
	}

	m_pCategoryCombo = dynamic_cast< ComboBox * >( FindChildByName( "CategoryCombo" ) );
	if ( m_pCategoryCombo )
	{
		// Add "Categories" text
		const int nDefaultItemID = m_pCategoryCombo->AddItem( "#Replay_Category", NULL );
		m_pCategoryCombo->ActivateItem( nDefaultItemID );

		// Add all categories
		const wchar_t *pCategoryText = L"";
		int i = 0;
		while ( true )
		{
			CFmtStr fmtCategoryToken( "#Replay_Contest_Category%i", i++ );
			pCategoryText = g_pVGuiLocalize->Find( fmtCategoryToken.Access() );
			if ( !pCategoryText || !pCategoryText[0] )
				break;
			m_pCategoryCombo->AddItem( pCategoryText, NULL );
		}

		m_pCategoryCombo->SetNumberOfEditLines( i );
	}
}

void CSaxxyAwardsSubmitForm::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CSaxxyAwardsSubmitForm::OnCommand( const char *pCommand )
{
	if ( FStrEq( pCommand, "submit" ) )
	{
		// need a category
		if ( m_pCategoryCombo->GetActiveItem() == 0 )
		{
			ShowMessageBox( "#Replay_Contest_StatusTitle",  "#Replay_Contest_ChooseCategory", "#GameUI_OK" );
			return;
		}

		// need a non-empty string
		char szText[256];
		m_pURLInput->GetText( szText, sizeof( szText ) );
		if ( Q_strlen( szText ) == 0 )
		{
			ShowMessageBox( "#Replay_Contest_StatusTitle",  "#Replay_Contest_EnterURL", "#GameUI_OK" );
			return;
		}

		// waiting dialog
		ShowWaitingDialog( new CGenericWaitingDialog( this ), "#Replay_Contest_Waiting", true, true, 30.0f );

		// send the url		
		GCSDK::CProtoBufMsg< CMsgReplaySubmitContestEntry > msg( k_EMsgGCReplay_SubmitContestEntry );
		msg.Body().set_youtube_url( szText );
		msg.Body().set_category( m_pCategoryCombo->GetActiveItem() - 1 );
		GCClientSystem()->GetGCClient()->BSendMessage( msg );

		// close the submit dialog
		Close();
	}
	else if ( FStrEq( pCommand, "cancel" ) )
	{
		Close();
	}
	else if ( FStrEq( pCommand, "rules" ) )
	{
		if ( steamapicontext && steamapicontext->SteamFriends() )
		{
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( "http://www.teamfortress.com/saxxyawards/#rules" );
		}
	}
	else
	{
		BaseClass::OnCommand( pCommand );
	}
}

void CSaxxyAwardsSubmitForm::Close()
{
	TFModalStack()->PopModal( this );
	MarkForDeletion();
}

void CSaxxyAwardsSubmitForm::OnKeyCodeTyped( vgui::KeyCode nCode )
{
	if ( nCode == KEY_ESCAPE )
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( nCode );
	}
}

static void SaxxyAwards_ShowSubmitForm()
{
	CSaxxyAwardsSubmitForm *pSubmitDialog = vgui::SETUP_PANEL( new CSaxxyAwardsSubmitForm( NULL ) );

	pSubmitDialog->SetVisible( true );
	pSubmitDialog->MakePopup();
	pSubmitDialog->MoveToFront();
	pSubmitDialog->SetKeyBoardInputEnabled( true );
	pSubmitDialog->SetMouseInputEnabled( true );
	TFModalStack()->PushModal( pSubmitDialog );
}

//-----------------------------------------------------------------------------
// Purpose: Refresh the saxxy awards portion of the main menu
//-----------------------------------------------------------------------------
class CGC_Replay_SubmitContestEntryResponse_Status : public GCSDK::CGCClientJob
{
public:
	CGC_Replay_SubmitContestEntryResponse_Status( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}
	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg< CMsgReplaySubmitContestEntryResponse > msg( pNetPacket );

		CloseWaitingDialog();

		if ( msg.Body().success() )
		{
			ShowMessageBox( "#Replay_Contest_StatusTitle",  "#Replay_Contest_StatusSuccess", "#GameUI_OK" );
		}
		else
		{
			ShowMessageBox( "#Replay_Contest_StatusTitle",  "#Replay_Contest_StatusFailure", "#GameUI_OK" );
		}

		return true;
	}
protected:
};
GC_REG_JOB( GCSDK::CGCClient, CGC_Replay_SubmitContestEntryResponse_Status, "CGC_Replay_SubmitContestEntryResponse_Status", k_EMsgGCReplay_SubmitContestEntryResponse, GCSDK::k_EServerTypeGCClient );
#endif	// #ifdef SAXXYMAINMENU_ENABLED