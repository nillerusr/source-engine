#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MessageBox.h>
#include "vgui/KeyCode.h"
#include <KeyValues.h>
#include "vgui_controls/AnimationController.h"

#include "TileGenDialog.h"
#include "PlacedRoomTemplatePanel.h"
#include "MapLayoutPanel.h"
#include "RoomTemplateEditDialog.h"
#include "TileSource/RoomTemplate.h"
#include "TileSource/Room.h"
#include "TileSource/LevelTheme.h"
#include "TileSource/MapLayout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CPlacedRoomTemplatePanel::CPlacedRoomTemplatePanel( CRoom *pRoom, Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pRoom = pRoom;
	Assert( pRoom->m_pRoomTemplate );

	SetRoomTemplate( pRoom->m_pRoomTemplate );

	m_bSetAlpha = m_bStartedGrowAnimation = false;
	m_bSelectedOnThisPress = false;
}

CPlacedRoomTemplatePanel::~CPlacedRoomTemplatePanel( void )
{
}

void CPlacedRoomTemplatePanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if ( !m_bSetAlpha )
	{
		m_bSetAlpha = true;
		SetAlpha( 0 );
		vgui::GetAnimationController()->RunAnimationCommand( this, "alpha", 255.0f, 0.0f, 0.4f, AnimationController::INTERPOLATOR_LINEAR);
	}

	// are we currently selected?
	if ( g_pTileGenDialog && g_pTileGenDialog->m_SelectedRooms.Find(m_pRoom) != g_pTileGenDialog->m_SelectedRooms.InvalidIndex() )
	{
		m_pSelectedOutline->SetVisible(true);
	}
	else
	{
		m_pSelectedOutline->SetVisible(false);
	}
}

void CPlacedRoomTemplatePanel::PerformLayout()
{
	BaseClass::PerformLayout();
	
	if ( !m_pRoom )
		return;

	AssertMsg( m_pRoom->m_pRoomTemplate, "Room without room template" );
	if ( !m_pRoom->m_pRoomTemplate )
		return;

	// position us within the grid
	int pos_y = m_pRoom->m_iPosY;// - m_pRoom->m_pRoomTemplate->GetTilesY();	// shift us up, so we're positioned relative to our lower left			
	pos_y = g_pTileGenDialog->MapLayoutTilesWide() - pos_y - 1;	// reverse the Y axis
	// shift us down by the room height on the y
	pos_y -= (m_pRoom->m_pRoomTemplate->GetTilesY() - 1);
	
	SetPos(m_pRoom->m_iPosX * g_pTileGenDialog->RoomTemplatePanelTileSize(),
			pos_y * g_pTileGenDialog->RoomTemplatePanelTileSize());

	if ( !m_bStartedGrowAnimation )
	{
		m_bStartedGrowAnimation = true;

		int x, y, w, h;
		GetBounds( x, y, w, h );

		int mx = x + w * 0.5f;
		int my = y + h * 0.5f;
		SetBounds( mx, my, 0, 0 );
		float fAnimTime = 0.4f;
		vgui::GetAnimationController()->RunAnimationCommand( this, "xpos", x, 0.0f, fAnimTime, AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand( this, "ypos", y, 0.0f, fAnimTime, AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand( this, "wide", w, 0.0f, fAnimTime, AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand( this, "tall", h, 0.0f, fAnimTime, AnimationController::INTERPOLATOR_LINEAR);
	}
}

void CPlacedRoomTemplatePanel::OnMouseReleased(vgui::MouseCode code)
{
	BaseClass::OnMouseReleased(code);

	if (code == MOUSE_RIGHT)		// right click on a placed room to place player start in this tile
	{		
		CMapLayout *pMapLayout = g_pTileGenDialog->GetMapLayout();
		if ( pMapLayout )
		{
			g_pTileGenDialog->GetMapLayoutPanel()->GetCursorTile( pMapLayout->m_iPlayerStartTileX, pMapLayout->m_iPlayerStartTileY );
			g_pTileGenDialog->GetMapLayoutPanel()->InvalidateLayout(true, false);
			g_pTileGenDialog->Repaint();
		}
	}
	else
	{
		if (vgui::input()->IsKeyDown(KEY_LCONTROL) || vgui::input()->IsKeyDown(KEY_RCONTROL))
		{
			if ( !m_bDragged && !m_bSelectedOnThisPress )
			{
				g_pTileGenDialog->ToggleRoomSelection(m_pRoom);
			}
		}
	}
}

void CPlacedRoomTemplatePanel::OnMousePressed(vgui::MouseCode code)
{
	BaseClass::OnMouseReleased(code);

	if (code == MOUSE_LEFT)
	{
		m_bDragged = false;
		m_bSelectedOnThisPress = false;

		if (g_pTileGenDialog->m_SelectedRooms.Find(m_pRoom) == -1)	// if not already selected
		{
			if (vgui::input()->IsKeyDown(KEY_LCONTROL) || vgui::input()->IsKeyDown(KEY_RCONTROL))
			{
				g_pTileGenDialog->ToggleRoomSelection(m_pRoom);
				m_bSelectedOnThisPress = true;
			}
			else
			{
				g_pTileGenDialog->SetRoomSelection(m_pRoom);
				m_bSelectedOnThisPress = true;
			}
		}
		else
		{

		}


		// if we're selected, start dragging
		if (g_pTileGenDialog->m_SelectedRooms.Find(m_pRoom) != -1)
		{
			MoveToFront();
			g_pTileGenDialog->OnStartDraggingSelectedRooms();
		}
		else
		{
			m_bSelectedOnThisPress = false;
		}
	}
}

void CPlacedRoomTemplatePanel::OnDragged()
{
	m_bDragged = true;
	InvalidateLayout(true);
}

void CPlacedRoomTemplatePanel::MarkForDeletion()
{
	BaseClass::MarkForDeletion();

	m_pRoom = NULL;
}