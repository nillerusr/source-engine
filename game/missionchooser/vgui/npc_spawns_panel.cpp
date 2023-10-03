#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Scrollbar.h>
#include "vgui/ISurface.h"
#include "vgui/KeyCode.h"
#include "vgui/missionchooser_tgaimagepanel.h"
#include "vgui/ILocalize.h"
#include "npc_spawns_panel.h"
#include "MapLayoutPanel.h"
#include "TileGenDialog.h"
#include "TileSource/RoomTemplate.h"
#include "TileSource/Room.h"
#include "TileSource/MapLayout.h"
#include "PlacedRoomTemplatePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ConVar asw_encounter_display( "asw_encounter_display", "1", FCVAR_NONE );

CNPC_Spawns_Panel::CNPC_Spawns_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_nWhiteTextureId = -1;
	SetMouseInputEnabled( false );
}

CNPC_Spawns_Panel::~CNPC_Spawns_Panel( void )
{
}

void CNPC_Spawns_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	CMapLayoutPanel *pParent = GetMapLayoutPanel();

	SetBounds( 0, 0, pParent->GetWide(), pParent->GetTall() );
}

void CNPC_Spawns_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hTextFont = pScheme->GetFont( "DefaultVerySmall" );
}

void CNPC_Spawns_Panel::Paint()
{
	if ( !g_pTileGenDialog || asw_encounter_display.GetInt() <= 0 )
		return;

	//int iTiles = g_pTileGenDialog->MapLayoutTilesWide();
	int iTileSize = g_pTileGenDialog->RoomTemplatePanelTileSize();

	CMapLayout *pLayout = GetMapLayout();
	if ( !pLayout )
		return;

	float flPixelsPerWorldCoord = (float) iTileSize / ASW_TILE_SIZE;

	// draw encounters
	for ( int i = 0; i < pLayout->m_Encounters.Count(); i++ )
	{
		CASW_Encounter *pEncounter = pLayout->m_Encounters[ i ];

		Vector vecPos = pEncounter->GetEncounterPosition();

		// convert world coord to screenspace		
		vecPos *= flPixelsPerWorldCoord;
		vecPos.y = -vecPos.y;
		vecPos += iTileSize * MAP_LAYOUT_TILES_WIDE * 0.5f;

		vgui::surface()->DrawSetColor(Color(32,255,32,64));

		if ( m_nWhiteTextureId < 0 )
		{
			m_nWhiteTextureId = vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile( m_nWhiteTextureId, "vgui/white", true, false );
		}

		vgui::surface()->DrawSetTexture( m_nWhiteTextureId );
		int iRectSize = flPixelsPerWorldCoord * pEncounter->GetEncounterRadius();
		int iSteps = 20;
		float flAngleStep = ( ( 2 * M_PI ) / (float) iSteps );
		
		for ( int k = 0; k < iSteps; k++ )
		{
			vgui::Vertex_t v[3];

			v[0].m_Position.Init( vecPos.x, vecPos.y );
			v[1].m_Position.Init( vecPos.x + cos( flAngleStep * k ) * iRectSize, vecPos.y + sin( flAngleStep * k ) * iRectSize );
			v[2].m_Position.Init( vecPos.x + cos( flAngleStep * ( k + 1 ) ) * iRectSize, vecPos.y + sin( flAngleStep * ( k + 1 ) ) * iRectSize );

			v[0].m_TexCoord.Init( 0.0f, 0.0f );
			v[1].m_TexCoord.Init( 1.0f, 0.0f );
			v[2].m_TexCoord.Init( 1.0f, 1.0f );

			vgui::surface()->DrawTexturedPolygon( 3, v );
		}
		//vgui::surface()->DrawFilledRect( vecPos.x - iRectSize, vecPos.y - iRectSize,
			//vecPos.x + iRectSize, vecPos.y + iRectSize );

		if ( asw_encounter_display.GetInt() >= 2 )
		{
			// count up aliens
			int iAliens = 0;
			wchar_t szAlienList[ 1024 ];
			wchar_t szconverted[ 64 ];
			int ypos = vecPos.y;
			int line_height = vgui::surface()->GetFontTall( m_hTextFont );
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );

			_snwprintf( szAlienList, sizeof(szAlienList)/sizeof(wchar_t), L"" );
			for ( int d = 0; d < pEncounter->GetNumSpawnDefs(); d++ )
			{
				IASWSpawnDefinition* pSpawnDef = pEncounter->GetSpawnDef( d );
				for ( int e = 0; e < pSpawnDef->GetEntryCount(); e++ )
				{
					IASWSpawnDefinitionEntry *pEntry = pSpawnDef->GetEntry( e );
					int minAliens, maxAliens;
					pEntry->GetSpawnCountRange( minAliens, maxAliens );
					iAliens += ( ( minAliens + maxAliens ) / 2 );
					g_pVGuiLocalize->ConvertANSIToUnicode( pEntry->GetNPCClassname(), szconverted, sizeof(szconverted)  );
					_snwprintf( szAlienList, sizeof(szAlienList)/sizeof(wchar_t), L"%s (%d/%d)", szconverted, minAliens, maxAliens );

					vgui::surface()->DrawSetTextPos( vecPos.x , ypos );
					vgui::surface()->DrawPrintText( szAlienList, wcslen( szAlienList ) );
					ypos += line_height;
				}
			}
		}
	}
	// paint rooms flagged as encounters
	for ( int i = 0; i < pLayout->m_PlacedRooms.Count(); i++ )
	{
		if ( pLayout->m_PlacedRooms[i]->HasAlienEncounter() )
		{
			vgui::surface()->DrawSetColor(Color(255,0,0,128));

			if ( m_nWhiteTextureId < 0 )
			{
				m_nWhiteTextureId = vgui::surface()->CreateNewTextureID();
				vgui::surface()->DrawSetTextureFile( m_nWhiteTextureId, "vgui/white", true, false );
			}

			Vector vecMins, vecMaxs;
			pLayout->m_PlacedRooms[i]->GetWorldBounds( &vecMins, &vecMaxs );

			// convert world coord to screenspace		
			vecMins *= flPixelsPerWorldCoord;
			vecMins.y = -vecMins.y;
			vecMins += iTileSize * MAP_LAYOUT_TILES_WIDE * 0.5f;
			vecMaxs *= flPixelsPerWorldCoord;
			vecMaxs.y = -vecMaxs.y;
			vecMaxs += iTileSize * MAP_LAYOUT_TILES_WIDE * 0.5f;

			vgui::surface()->DrawSetTexture( m_nWhiteTextureId );
			vgui::surface()->DrawTexturedRect( (int) vecMins.x, (int) vecMaxs.y, (int) vecMaxs.x, (int) vecMins.y );
		}
	}
}

// assumes this panel is parented to a CMapLayoutPanel
CMapLayoutPanel* CNPC_Spawns_Panel::GetMapLayoutPanel()
{
	return static_cast<CMapLayoutPanel*>( GetParent() );
}

CMapLayout* CNPC_Spawns_Panel::GetMapLayout()
{
	return g_pTileGenDialog ? g_pTileGenDialog->GetMapLayout() : NULL;
}