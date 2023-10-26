//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/QueryBox.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "ienginevgui.h"
#include <game/client/iviewport.h>
#include "asw_loading_panel.h"
#include "filesystem.h"

#include <convar.h>
#include "fmtstr.h"

using namespace vgui;


CASW_Loading_Panel *g_pASW_Loading_Panel = NULL;

CUtlVector<CASW_Loading_Panel *> g_vecLoadingPanels;

//-----------------------------------------------------------------------------
// Purpose: Returns the global stats summary panel
//-----------------------------------------------------------------------------
CASW_Loading_Panel *GASWLoadingPanel()
{
	if ( NULL == g_pASW_Loading_Panel )
	{
		g_pASW_Loading_Panel = new CASW_Loading_Panel();
	}
	return g_pASW_Loading_Panel;
}

//-----------------------------------------------------------------------------
// Purpose: Destroys the global stats summary panel
//-----------------------------------------------------------------------------
void DestroyASWLoadingPanel()
{
	if ( NULL != g_pASW_Loading_Panel )
	{
		delete g_pASW_Loading_Panel;
		g_pASW_Loading_Panel = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Loading_Panel::CASW_Loading_Panel() : BaseClass( NULL, "ASW_Loading_Panel", 
													 vgui::scheme()->LoadSchemeFromFile( "Resource/ClientScheme.res", "ClientScheme" ) )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
void CASW_Loading_Panel::Init( void )
{
	m_pBackdrop = new vgui::ImagePanel( this, "LoadingImage" );
	m_pBackdrop->SetShouldScaleImage( true );

	m_pBlackBar[0] = new vgui::Panel( this, "LoadingImageBlackBar0" );
	m_pBlackBar[1] = new vgui::Panel( this, "LoadingImageBlackBar1" );

	m_pDetailsPanel = new LoadingMissionDetailsPanel( this, "LoadingMissionDetailsPanel" );
	m_pDetailsPanel->SetProportional( true );
	m_pDetailsPanel->SetVisible( false );

	// choose pic
	static char pics[14][64] = {
		//"swarm/loading/Loading_Vereon",
		"swarm/loading/Loading_Labs1",
		"swarm/loading/Loading_Labs2",
		"swarm/loading/Loading_LandingBay",
		"swarm/loading/Loading_Mine1",
		"swarm/loading/Loading_Mine2",
		"swarm/loading/Loading_Mine3",
		"swarm/loading/Loading_Office",
		"swarm/loading/Loading_Plant1",
		"swarm/loading/Loading_Plant2",
		"swarm/loading/Loading_Plant3",
		"swarm/loading/Loading_Sewers1",
		"swarm/loading/Loading_Sewers2",
		"swarm/loading/Loading_Sewers3",
		"swarm/loading/Loading_Sewers4"
	};

	int iChosen = RandomInt(0,13);
	Q_snprintf(m_szLoadingPic, sizeof(m_szLoadingPic), pics[iChosen]);

	ListenForGameEvent( "server_spawn" );

	g_vecLoadingPanels.AddToTail( this );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Loading_Panel::CASW_Loading_Panel( vgui::Panel *parent ) : BaseClass( parent, "TFStatsSummary", 
																		  vgui::scheme()->LoadSchemeFromFile( "Resource/ClientScheme.res", "ClientScheme" ) )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Loading_Panel::~CASW_Loading_Panel()
{
	g_vecLoadingPanels.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Loading_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	SetBounds(0, 0, sw, sh);

	int lw, lh, lx;
	GetLoadingScreenSize( lw, lh, lx );
	m_pBackdrop->SetBounds( lx, (sh - lh) * 0.5f, lw, lh );

	int bar_height = GetTall() * 0.11f;
	m_pBlackBar[0]->SetBounds( 0, 0, GetWide(), bar_height );
	m_pBlackBar[1]->SetBounds( 0, GetTall() - bar_height, GetWide(), bar_height );
}


//-----------------------------------------------------------------------------
// Purpose: Applies scheme settings
//-----------------------------------------------------------------------------
void CASW_Loading_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pBackdrop->SetImage( m_szLoadingPic );

	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 0 );
	SetBgColor( Color( 0, 0, 0, 255 ) );

	for ( int i=0; i<2; i++ )
	{
		m_pBlackBar[i]->SetPaintBackgroundEnabled( true );
		m_pBlackBar[i]->SetPaintBackgroundType( 0 );
		m_pBlackBar[i]->SetBgColor( Color( 0, 0, 0, 255 ) );
	}
}

// returns the size we want our loading background to be
//   this should fill the screen, while keeping a 4:3 aspect ratio
void CASW_Loading_Panel::GetLoadingScreenSize(int &w, int &t, int &xoffset)
{
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );

	// assume screens are always wider than they are high, force 4:3
	w = sh * (1024.0f / 768.0f);
	t = sh;
	xoffset = (sw - w) * 0.5f;
}

void CASW_Loading_Panel::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();

	// when we are changing levels and 
	if ( 0 == Q_strcmp( pEventName, "server_spawn" ) )
	{
		const char *pMapName = event->GetString( "mapname" );
		if ( pMapName )
		{
			SetLoadingMapName( pMapName );
		}
	}
}

void CASW_Loading_Panel::SetLoadingMapName( const char *szMapName )
{
	m_pDetailsPanel->SetVisible( false );
	if ( !szMapName )
		return;

	// load the .layout file in
	char szStrippedFilename[MAX_PATH];	
	Q_StripExtension( szMapName, szStrippedFilename, sizeof( szStrippedFilename ) );
	char szLayoutFilename[MAX_PATH];	
	Q_snprintf( szLayoutFilename, sizeof( szLayoutFilename ), "maps/%s.layout", szStrippedFilename );
	KeyValues *pLayoutKeys = new KeyValues( "MapLayout" );
	if ( !pLayoutKeys->LoadFromFile( g_pFullFileSystem, szLayoutFilename, "GAME" ) )
	{
		pLayoutKeys->deleteThis();
		return;
	}
	KeyValues *pGenerationOptions = NULL;
	for ( KeyValues *pPeerKey = pLayoutKeys; pPeerKey; pPeerKey = pPeerKey->GetNextKey() )
	{
		// @TODO: remove this when we deprecate the old mission format
		if ( !Q_stricmp( pPeerKey->GetName(), "GenerationOptions" ) || !Q_stricmp( pPeerKey->GetName(), "mission_settings" ) )
		{
			pGenerationOptions = pPeerKey;
			break;
		}
	}
	if ( !pGenerationOptions )
	{
		pLayoutKeys->deleteThis();
		return;
	}

	SetGenerationOptions( pGenerationOptions );
	pLayoutKeys->deleteThis();
}

void CASW_Loading_Panel::SetGenerationOptions( KeyValues *pKeys )
{
	m_pDetailsPanel->SetGenerationOptions( pKeys->MakeCopy() );
	m_pDetailsPanel->SetVisible( true );
}

//=============================================================================

LoadingMissionDetailsPanel::LoadingMissionDetailsPanel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_pLocationNameLabel = new vgui::Label( this, "LocationNameLabel", "" );
	m_pLocationDescriptionLabel = new vgui::Label( this, "LocationDescriptionLabel", "" );
	m_pGenerationOptions = NULL;
}

LoadingMissionDetailsPanel::~LoadingMissionDetailsPanel()
{
	if ( m_pGenerationOptions )
	{
		m_pGenerationOptions->deleteThis();
	}
}

void LoadingMissionDetailsPanel::SetGenerationOptions( KeyValues *pMissionKeys )
{
	if ( m_pGenerationOptions )
	{
		m_pGenerationOptions->deleteThis();
	}
	m_pGenerationOptions = pMissionKeys;

	const char *szMissionName = m_pGenerationOptions->GetString( "MissionName" );
	const char *szMissionDesc = m_pGenerationOptions->GetString( "MissionDescription" );
	m_pLocationNameLabel->SetText( szMissionName );
	m_pLocationDescriptionLabel->SetText(szMissionDesc  );	
}

void LoadingMissionDetailsPanel::ApplySchemeSettings(vgui::IScheme *scheme)
{
	LoadControlSettings( "resource/UI/LoadingMissionDetailsPanel.res" );

	BaseClass::ApplySchemeSettings(scheme);
}