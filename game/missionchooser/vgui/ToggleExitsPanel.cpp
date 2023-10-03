#include <vgui/IVGui.h>
#include "vgui/ISurface.h"
#include "ToggleExitsPanel.h"
#include "RoomTemplate.h"
#include "TileGenDialog.h"
#include "RoomTemplateEditDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// width of the buttons that get positioned around the target CRoomTemplatePanel
inline int GetBorderSize()
{
	//static int w, t;
	//vgui::surface()->GetScreenSize( w, t );
	//return 5 * ( ( float ) t / 480.0 );
	return 16;
}

// ===============================================================================================================

CToggleExitsPanel::CToggleExitsPanel( Panel *parent, const char *name ) : vgui::Panel( parent, name )
{
	m_pRoomTemplatePanel = NULL;
}

CToggleExitsPanel::~CToggleExitsPanel()
{

}

void CToggleExitsPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

void CToggleExitsPanel::PerformLayout()
{
	// position ourselves around the target room template panel
	if ( m_pRoomTemplatePanel )
	{
		int x, y, w, t;
		m_pRoomTemplatePanel->GetBounds( x, y, w, t );
		w += GetBorderSize() * 2;
		t += GetBorderSize() * 2;

		SetBounds( x - GetBorderSize(), y - GetBorderSize(), w, t );
	}
}

void CToggleExitsPanel::SetRoomTemplatePanel( CRoomTemplatePanel *pPanel, bool bForceUpdate )
{
	if ( m_pRoomTemplatePanel == pPanel && !bForceUpdate )
		return;

	m_pRoomTemplatePanel = pPanel;

	const CRoomTemplate *pRoomTemplate = GetRoomTemplate();
	if ( !pRoomTemplate )
		return;
	
	// remove all old exits
	for ( int i=0; i<m_ExitButtons.Count(); i++ )
	{
		m_ExitButtons[i]->MarkForDeletion();
		m_ExitButtons[i]->SetVisible( false );
	}
	m_ExitButtons.Purge();

	// add exit buttons for every potential exit square
	for ( int x=0 ; x<pRoomTemplate->GetTilesX() ; x++ )
	{		
		CToggleExitButton *pButton = new CToggleExitButton( this, "ToggleExitButtonN" );
		pButton->SetExitDetails( x, 0, EXITDIR_NORTH );
		m_ExitButtons.AddToTail( pButton );

		pButton = new CToggleExitButton( this, "ToggleExitButtonS" );
		pButton->SetExitDetails( x, pRoomTemplate->GetTilesY() - 1, EXITDIR_SOUTH );
		m_ExitButtons.AddToTail( pButton );
	}

	for ( int y=0 ; y<pRoomTemplate->GetTilesY() ; y++ )
	{
		CToggleExitButton *pButton = new CToggleExitButton( this, "ToggleExitButtonW" );
		pButton->SetExitDetails( 0, y, EXITDIR_WEST );
		m_ExitButtons.AddToTail( pButton );

		pButton = new CToggleExitButton( this, "ToggleExitButtonE" );
		pButton->SetExitDetails( pRoomTemplate->GetTilesX() - 1, y, EXITDIR_EAST );
		m_ExitButtons.AddToTail( pButton );
	}

	InvalidateLayout();
}

const CRoomTemplate *CToggleExitsPanel::GetRoomTemplate() const
{
	if ( !m_pRoomTemplatePanel )
	{
		return NULL;
	}

	return m_pRoomTemplatePanel->m_pRoomTemplate;
}

// ===============================================================================================================

CToggleExitButton::CToggleExitButton( Panel *parent, const char *name ) : vgui::Panel( parent, name )
{

}

CToggleExitButton::~CToggleExitButton()
{

}

void CToggleExitButton::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 0 );
}

void CToggleExitButton::PerformLayout()
{
	if ( !g_pTileGenDialog )
		return;

	int tile_size = g_pTileGenDialog->RoomTemplatePanelTileSize();

	switch ( m_ExitDirection )
	{
	case EXITDIR_NORTH:
		{
			SetBounds( GetBorderSize() + m_iTileX * tile_size, m_iTileY * tile_size, tile_size - 1, GetBorderSize() - 2 );
		}
		break;
	case EXITDIR_SOUTH:
		{
			SetBounds( GetBorderSize() + m_iTileX * tile_size, m_iTileY * tile_size + GetBorderSize() + tile_size + 2, tile_size - 1, GetBorderSize() - 2 );
		}
		break;
	case EXITDIR_EAST:
		{
			SetBounds( m_iTileX * tile_size + GetBorderSize() + tile_size + 2, GetBorderSize() + m_iTileY * tile_size, GetBorderSize() - 2, tile_size - 1 );
		}
		break;
	case EXITDIR_WEST:
		{
			SetBounds( m_iTileX * tile_size, GetBorderSize() + m_iTileY * tile_size, GetBorderSize() - 2, tile_size - 1 );
		}
		break;
	}
}

void CToggleExitButton::OnThink()
{
	// set color according to if our exit is toggled on/off
	if ( !GetToggleExitsPanel() )
		return;

	const CRoomTemplate *pRoomTemplate = GetToggleExitsPanel()->GetRoomTemplate();
	if ( !pRoomTemplate )
		return;

	// go through all exits to see if any match our position/direction
	for ( int i=0; i<pRoomTemplate->m_Exits.Count(); i++ )
	{
		CRoomTemplateExit *pExit = pRoomTemplate->m_Exits[i];
		if ( !pExit )
			continue;

		if ( pExit->m_iXPos == m_iTileX && pExit->m_iYPos == m_iTileY && pExit->m_ExitDirection == m_ExitDirection )
		{
			if ( IsCursorOver() )
			{
				SetBgColor( Color( 64, 64, 255, 255 ) );
			}
			else
			{
				SetBgColor( Color( 0, 0, 255, 255 ) );
			}
			
			return;
		}
	}
	if ( IsCursorOver() )
	{
		SetBgColor( Color( 64, 64, 64, 128 ) );
	}
	else
	{
		SetBgColor( Color( 0, 0, 0, 128 ) );
	}
}

void CToggleExitButton::OnMouseReleased( vgui::MouseCode code )
{
	CToggleExitsPanel* pToggleExitsPanel = GetToggleExitsPanel();
	if ( !pToggleExitsPanel )
		return;

	CRoomTemplateEditDialog *pEdit = dynamic_cast<CRoomTemplateEditDialog*>( pToggleExitsPanel->GetParent() );
	if ( !pEdit )
		return;

	if ( code == MOUSE_RIGHT )
	{
		pEdit->EditExit( m_iTileX, m_iTileY, m_ExitDirection );
	}
	else
	{
		pEdit->ToggleExit( m_iTileX, m_iTileY, m_ExitDirection );
	}			
}

void CToggleExitButton::SetExitDetails( int iTileX, int iTileY, ExitDirection_t exitDirection )
{
	m_iTileX = iTileX;
	m_iTileY = iTileY;
	m_ExitDirection = exitDirection;

	InvalidateLayout();
}

CToggleExitsPanel* CToggleExitButton::GetToggleExitsPanel()
{
	return dynamic_cast<CToggleExitsPanel*>( GetParent() );
}