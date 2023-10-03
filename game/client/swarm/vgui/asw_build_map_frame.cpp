#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui/ischeme.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/ProgressBar.h>
#include "convar.h"
#include "asw_build_map_frame.h"
#include <vgui/isurface.h>
#include "ienginevgui.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "missionchooser/iasw_random_missions.h"
#include "missionchooser/iasw_map_builder.h"
#include "asw_loading_panel.h"

// includes needed for the creating of a new process and handling its output
#pragma warning( disable : 4005 )
#include <windows.h>
#include <iostream>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Build_Map_Frame::CASW_Build_Map_Frame( vgui::Panel *pParent, const char *pElementName ) :
vgui::Frame(pParent, pElementName)
{
	m_fCloseWindowTime = 0;
	m_bRunMapAfterBuild = true;
	m_bEditMapAfterBuild = false;

	SetDeleteSelfOnClose(true);	
	SetSizeable( false );

	SetTitle("Building map...", true);

	m_pStatusLabel = new vgui::Label(this, "StatusLabel", "#asw_campaign_initializing");
	m_pStatusLabel->SetContentAlignment(vgui::Label::a_northwest);

	m_pProgressBar = new vgui::ProgressBar(this, "ProgressBar");
	m_pProgressBar->SetProgress(0.0f);

	m_pMapBuilder = missionchooser ? missionchooser->MapBuilder() : NULL;
}

CASW_Build_Map_Frame::~CASW_Build_Map_Frame()
{
}

void CASW_Build_Map_Frame::PerformLayout()
{
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );	

	int wide = 350;
	int tall = 128;

	SetSize(wide, tall);
	SetPos((sw * 0.5f) - (wide * 0.5f), (sh * 0.5f) - (tall * 0.5f));

	BaseClass::PerformLayout();

	int padding = 20;
	m_pStatusLabel->SetBounds(padding, padding + 20, GetWide() - padding * 2, 20);
	m_pProgressBar->SetBounds(padding, padding + 50, GetWide() - padding * 2, 20);
}

void CASW_Build_Map_Frame::OnThink()
{
	BaseClass::OnThink();

	if ( m_pMapBuilder )
	{
		m_pMapBuilder->Update( Plat_FloatTime() );
	}

	UpdateProgress();

	if (m_fCloseWindowTime > 0 && m_fCloseWindowTime <= Plat_FloatTime())
	{
		// tell the engine we've finished compiling the map, so it can continue connecting to a remote server
		engine->ClientCmd( "asw_engine_finished_building_map" );

		if ( m_pMapBuilder )
		{
			if ( m_bEditMapAfterBuild )
			{
				char buffer[512];
				Q_snprintf(buffer, sizeof(buffer), "progress_enable\nmap %s edit %s\n", m_pMapBuilder->GetMapName(), m_szMapEditFilename );
				m_pMapBuilder = NULL;
				engine->ClientCmd(buffer);
			}
			else if ( m_bRunMapAfterBuild  )
			{
				char buffer[512];
				Q_snprintf(buffer, sizeof(buffer), "progress_enable\nmap %s\n", m_pMapBuilder->GetMapName() );
				m_pMapBuilder = NULL;
				engine->ClientCmd(buffer);
			}
		}

		OnClose();
		m_fCloseWindowTime = 0;
	}
}

void CASW_Build_Map_Frame::UpdateProgress()
{
	if ( !m_pMapBuilder )
		return;

	m_pProgressBar->SetProgress( m_pMapBuilder->GetProgress() );
	m_pStatusLabel->SetText( m_pMapBuilder->GetStatusMessage() );

	if ( m_pMapBuilder->GetProgress() == 1.0f && m_fCloseWindowTime == 0)
	{
		m_fCloseWindowTime = Plat_FloatTime() + 1.5f;
	}
}

void CASW_Build_Map_Frame::SetEditMapAfterBuild( bool bEditMap, const char *szMapEditFilename )
{
	Q_snprintf( m_szMapEditFilename, sizeof( m_szMapEditFilename ), "%s.vmf", szMapEditFilename );
	m_bEditMapAfterBuild = bEditMap;
}



void CC_ASW_Build_Map( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_build_map <map name>");
		return;
	}

	vgui::VPANEL GameUIRoot = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	CASW_Build_Map_Frame *pFrame = new CASW_Build_Map_Frame( NULL, "BuildMapFrame" );
	pFrame->SetParent( GameUIRoot );
	pFrame->Activate();

	int nArgC = args.ArgC();
	for ( int i = 1; i < nArgC; i++ )
	{
		if ( !Q_stricmp( args.Arg(i), "connecting" ) )
		{
			pFrame->SetRunMapAfterBuild( false );
		}
		else if ( !Q_stricmp( args.Arg(i), "edit" ) && i + 1< nArgC )
		{
			pFrame->SetEditMapAfterBuild( true, args.Arg( i + 1 ) );		// argument immediately after "edit" is the .vmf name
		}
	}

	if ( GASWLoadingPanel() )
	{
		if ( !Q_strnicmp( args[1], "maps/", 5 ) || !Q_strnicmp( args[1], "maps\\", 5 ) )
		{
			GASWLoadingPanel()->SetLoadingMapName( args[1] + 5 );
		}
		else
		{
			GASWLoadingPanel()->SetLoadingMapName( args[1] );
		}
	}

	pFrame->GetMapBuilder()->ScheduleMapBuild( args[1], Plat_FloatTime() + 0.6f );	// give some time for the window to open and appear
}
static ConCommand asw_build_map("asw_build_map", CC_ASW_Build_Map, 0 );

void CC_ASW_TileGen( const CCommand &args )
{
	if ( !missionchooser || !missionchooser->RandomMissions() )
		return;

	vgui::VPANEL GameUIRoot = enginevgui->GetPanel( PANEL_GAMEUIDLL );	
	vgui::Frame *pFrame = dynamic_cast<vgui::Frame*>( missionchooser->RandomMissions()->CreateTileGenFrame( NULL ) );

	pFrame->SetParent( GameUIRoot );
	pFrame->MoveToCenterOfScreen();
	pFrame->Activate();
}
static ConCommand asw_tilegen("asw_tilegen", CC_ASW_TileGen, "Experimental tile based level generator.", FCVAR_CHEAT );
