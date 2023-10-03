#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Scrollbar.h>
#include "vgui/ISurface.h"
#include "vgui/KeyCode.h"
#include "vgui/missionchooser_tgaimagepanel.h"
#include "vgui/npc_spawns_panel.h"
#include "MapLayoutPanel.h"
#include "TileGenDialog.h"
#include "TileSource/RoomTemplate.h"
#include "TileSource/Room.h"
#include "TileSource/MapLayout.h"
#include "PlacedRoomTemplatePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CMapLayoutPanel::CMapLayoutPanel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	SetPaintBackgroundEnabled(true);
	SetMouseInputEnabled(true);
	
	m_pPlayerStartImagePanel = new CMissionChooserTGAImagePanel(this, "PlayerStartImagePanel");
	m_pPlayerStartImagePanel->SetZPos(10);	// make sure it's above the rooms

	m_pSpawnsPanel = new CNPC_Spawns_Panel( this, "SpawnsPanel" );
	m_pSpawnsPanel->SetZPos( 9 );
}

CMapLayoutPanel::~CMapLayoutPanel( void )
{
}

void CMapLayoutPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	if (!g_pTileGenDialog)
		return;

	// make sure we're big enough to hold the max map size
	int w = g_pTileGenDialog->RoomTemplatePanelTileSize() * g_pTileGenDialog->MapLayoutTilesWide();
	SetSize(w, w);

	m_pPlayerStartImagePanel->SetSize(g_pTileGenDialog->RoomTemplatePanelTileSize(), g_pTileGenDialog->RoomTemplatePanelTileSize());
	// position playerstart within the grid
	int pos_y = GetMapLayout()->m_iPlayerStartTileY;
	pos_y = g_pTileGenDialog->MapLayoutTilesWide() - pos_y;	// reverse the Y axis	
	
	int pos_x = GetMapLayout()->m_iPlayerStartTileX * g_pTileGenDialog->RoomTemplatePanelTileSize();
	pos_y *= g_pTileGenDialog->RoomTemplatePanelTileSize();
	m_pPlayerStartImagePanel->SetPos(pos_x, pos_y);
}

void CMapLayoutPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,255));
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);

	m_pPlayerStartImagePanel->SetTGA("tilegen/roomtemplates/playerstart.tga");
}

void CMapLayoutPanel::Paint()
{
	if ( !g_pTileGenDialog )
		return;

	int iTiles = g_pTileGenDialog->MapLayoutTilesWide();
	int iTileSize = g_pTileGenDialog->RoomTemplatePanelTileSize();
		
	// draw dark grid lines
	surface()->DrawSetColor(Color(32,32,32,255));
	for (int x=0;x<iTiles;x++)
	{
		if (x % 4 != 0)
		{
			vgui::surface()->DrawLine(x*iTileSize, 0, x*iTileSize, GetTall());
		}
	}
	for (int y=0;y<iTiles;y++)
	{
		if (y % 4 != 0)
		{
			vgui::surface()->DrawLine(0, y*iTileSize, GetWide(), y*iTileSize);
		}
	}

	// draw brighter grid lines
	surface()->DrawSetColor(Color(64,64,64,255));
	for (int x=0;x<iTiles;x++)
	{
		if (x % 4 == 0)
		{
			vgui::surface()->DrawLine(x*iTileSize, 0, x*iTileSize, GetTall());
		}
	}
	for (int y=0;y<iTiles;y++)
	{
		if (y % 4 == 0)
		{
			vgui::surface()->DrawLine(0, y*iTileSize, GetWide(), y*iTileSize);
		}
	}

	int sx, sy;
	if ( g_pTileGenDialog->GetRubberBandStart( sx, sy ) )
	{
		int mx, my;
		input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);

		surface()->DrawSetColor( Color( 255,255,64,255 ) );

		if ( sx > mx )
		{
			int i = mx;
			mx = sx;
			sx = i;
		}
		if ( sy > my )
		{
			int i = my;
			my = sy;
			sy = i;
		}
		vgui::surface()->DrawOutlinedRect( sx, sy, mx, my );
	}
}

void CMapLayoutPanel::GetCursorTile(int &tilex, int &tiley)
{
	if ( !g_pTileGenDialog )
		return;

	// find the tile we clicked in
	int mx, my;
	input()->GetCursorPos(mx, my);
	ScreenToLocal(mx, my);
	tilex = mx / g_pTileGenDialog->RoomTemplatePanelTileSize();
	tiley = (my / g_pTileGenDialog->RoomTemplatePanelTileSize());
	// reverse the Y axis
	tiley = g_pTileGenDialog->MapLayoutTilesWide() - tiley - 1;
}

void CMapLayoutPanel::OnMouseReleased(vgui::MouseCode code)
{
	if (g_pTileGenDialog && g_pTileGenDialog->m_pCursorTemplate)	// if we're in 'room stamping down' mode
	{
		if (code == MOUSE_LEFT)	// if we left click with a room to stamp down
		{
			int iTileX = 0;
			int iTileY = 0;
			GetCursorTile(iTileX, iTileY);

			// check our room wouldn't go out of bounds if placed here
			if (iTileX < 0 || iTileY < 0 || iTileX >= g_pTileGenDialog->MapLayoutTilesWide() || iTileY >= g_pTileGenDialog->MapLayoutTilesWide())
				return;

			if (iTileX + g_pTileGenDialog->m_pCursorTemplate->GetTilesX() > g_pTileGenDialog->MapLayoutTilesWide()
				|| iTileY + g_pTileGenDialog->m_pCursorTemplate->GetTilesY() > g_pTileGenDialog->MapLayoutTilesWide())
				return;

			if ( !GetMapLayout()->TemplateFits( g_pTileGenDialog->m_pCursorTemplate, iTileX, iTileY ) )
			{
				Msg("  Template doesn't fit there!\n" );
				return;
			}

			// place the room
			AddRoom(g_pTileGenDialog->m_pCursorTemplate, iTileX, iTileY);
			Msg( "Manually added room %s at %d,%d\n", g_pTileGenDialog->m_pCursorTemplate->GetFullName(), iTileX, iTileY );
		}
		else if ( code == MOUSE_RIGHT )
		{
			// clear the current room attached to the cursor if we right click over the map
			g_pTileGenDialog->SetCursorRoomTemplate(NULL);
		}
	}
	else if ( g_pTileGenDialog )		// if we right click over the map without a room to stamp down, then manipulate existing rooms
	{
		//g_pTileGenDialog->ClearRoomSelection();

		int mx, my;
		input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);

		g_pTileGenDialog->EndRubberBandSelection( mx, my );
	}
}

void CMapLayoutPanel::OnMousePressed(vgui::MouseCode code)
{
	if ( !g_pTileGenDialog )
		return;

	if ( !g_pTileGenDialog->m_pCursorTemplate && ( code == MOUSE_LEFT ) )
	{
		int mx, my;
		input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);

		g_pTileGenDialog->StartRubberBandSelection( mx, my );
	}
}

void CMapLayoutPanel::AddRoom( const CRoomTemplate *pTemplate, int iTileX, int iTileY )
{
	if ( !g_pTileGenDialog )
		return;

	CRoom *pRoom = new CRoom( GetMapLayout(), g_pTileGenDialog->m_pCursorTemplate, iTileX, iTileY );
	CreateRoomUIPanel( pRoom );
}

void CMapLayoutPanel::RemoveRoom(CRoom *pRoom)
{
	delete pRoom;
}

void CMapLayoutPanel::CreateAllUIPanels()
{
	if ( !GetMapLayout() )	
		return;

	for ( int i=0; i<GetMapLayout()->m_PlacedRooms.Count(); i++ )
	{
		CRoom *pRoom = GetMapLayout()->m_PlacedRooms[i];
		CreateRoomUIPanel( pRoom );
	}
}

void CMapLayoutPanel::CreateRoomUIPanel( CRoom *pRoom )
{
	if ( !pRoom->m_pPlacedRoomPanel )
	{
		pRoom->m_pPlacedRoomPanel = new CPlacedRoomTemplatePanel(pRoom, this, "PlacedRoomPanel");
	}
}

CMapLayout* CMapLayoutPanel::GetMapLayout()
{
	return g_pTileGenDialog ? g_pTileGenDialog->GetMapLayout() : NULL;
}