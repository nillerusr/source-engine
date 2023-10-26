#include "location_layout_panel.h"
#include <vgui/ILocalize.h>
#include "vgui/ISurface.h"
#include "asw_mission_chooser.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui/IInput.h>
#include "asw_location_grid.h"
#include "location_editor_frame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

#define YRES(y)	( y  * ( ( float )1024.0f / 480.0 ) )
#define UNDO_YRES(y)	( y / ( ( float )1024.0f / 480.0 ) )

//-----------------------------------------------------------------------------
// Purpose: Shows hex locations
//-----------------------------------------------------------------------------

CLocation_Layout_Panel::CLocation_Layout_Panel( Panel *parent, Panel *pActionTarget, const char *name ) : BaseClass( parent, name )
{
	m_pActionTarget = pActionTarget;
	m_pEditorFrame = dynamic_cast<CLocation_Editor_Frame*>( parent );
	
	m_pBgImage = new vgui::ImagePanel( this, "LayoutBgImage" );
	m_pBgImage->SetShouldScaleImage( true );
	m_pBgImage->SetMouseInputEnabled( false );
}

CLocation_Layout_Panel::~CLocation_Layout_Panel()
{

}

void CLocation_Layout_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( YRES(853), YRES(480) );
	m_pBgImage->SetImage( "briefing/map.vmt" );
	m_pBgImage->SetSize( YRES(853), YRES(480) );
}

void CLocation_Layout_Panel::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 0 );
	SetBgColor( Color( 0, 0, 0, 255 ) );
}

void CLocation_Layout_Panel::OnThink()
{
	CreateLocationPanels();
}

void CLocation_Layout_Panel::CreateLocationPanels()
{
	if ( !LocationGrid() )
		return;

	int iCount = 0;
	for ( int i = 0; i < LocationGrid()->GetNumGroups(); i++ )
	{
		IASW_Location_Group *pGroup = LocationGrid()->GetGroup( i );
		if ( !pGroup )
			continue;

		for ( int k = 0; k < pGroup->GetNumLocations(); k++ )
		{
			if ( m_LocationPanels.Count() <= iCount )
			{
				CLocation_Panel *pPanel = new CLocation_Panel( this, "Location_Panel" );
				pPanel->AddActionSignalTarget( m_pActionTarget );
				m_LocationPanels.AddToTail( pPanel );
			}
			m_LocationPanels[iCount]->SetLocationID( pGroup->GetLocation( k )->GetID() );
			iCount++;
		}
	}
}

// ====================================================================================================

CLocation_Panel::CLocation_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_iLocationID = -1;
	m_pHexImage = new vgui::ImagePanel( this, "HexImage" );
	m_pHexImage->SetShouldScaleImage( true );
	m_pHexImage->SetMouseInputEnabled( false );
	m_actionMessage = NULL;	
	m_pLayoutPanel = dynamic_cast<CLocation_Layout_Panel*>( parent );
	m_bDragging = false;
	m_pIDLabel = new vgui::Label( this, "IDLabel", "" );
	m_pIDLabel->SetContentAlignment( vgui::Label::a_center );
	m_pIDLabel->SetMouseInputEnabled( false );
}

CLocation_Panel::~CLocation_Panel()
{
	if ( m_actionMessage )
	{
		m_actionMessage->deleteThis();
	}
}

void CLocation_Panel::SetLocationID( int i )
{
	m_iLocationID = i;
	if ( m_actionMessage )
	{
		m_actionMessage->deleteThis();
	}
	char buffer[32];
	Q_snprintf( buffer, sizeof( buffer ), "Location%d", i );
	m_actionMessage = new KeyValues( "command", "command", buffer );
	Q_snprintf( buffer, sizeof( buffer ), "%d", i );
	m_pIDLabel->SetText( buffer );
}


void CLocation_Panel::OnThink()
{
	if ( m_bDragging )
	{
		if ( !vgui::input()->IsMouseDown( MOUSE_LEFT ) )
		{
			m_bDragging = false;
			// update pos with editor		
			int x, y;
			GetPos( x, y );

			int fx = UNDO_YRES( x );
			int fy = UNDO_YRES( y );

			IASW_Location *pLocation = LocationGrid()->GetLocationByID( m_iLocationID );
			if ( pLocation )
			{
				pLocation->SetPos( fx, fy );
				if ( pLocation->GetID() == 21 )
				{
					int pw, ph;
					GetParent()->GetSize( pw, ph );
					float flXFraction = (float) x / (float) pw;
					float flYFraction = (float) y / (float) ph;
					Msg( "EdLoc21 x=%d y=%d fx=%d fx=%d xfrac=%f yfrac=%f edw=%d edh=%d\n",
						x, y,
						fx, fy,
						flXFraction, flYFraction,
						pw, ph );
				}
				m_pLayoutPanel->GetLocationEditorFrame()->SetGroup( pLocation->GetGroup() );
			}
		}
		else
		{
			int current_posx, current_posy;
			vgui::input()->GetCursorPos( current_posx, current_posy );

			//GetParent()->ScreenToLocal( current_posx, current_posy );
			SetPos( current_posx + m_MouseOffset.x, current_posy + m_MouseOffset.y );
		}
	}

	UpdateHexColor();

	if ( IsCursorOverHex() )
	{
		//MissionGridPanel *pParent = dynamic_cast<MissionGridPanel*>( GetParent() );
		//if ( pParent )
		//{
			//pParent->OnCursorOverLocationPanel( this );
		//}
	}

	if ( !LocationGrid() )
		return;

	IASW_Location *pLocation = LocationGrid()->GetLocationByID( m_iLocationID );
	if ( !pLocation )
	{
		return;
	}

	if ( m_iLastXPos != YRES( pLocation->GetXPos() ) || m_iLastYPos != YRES( pLocation->GetYPos() ) )
	{
		InvalidateLayout( true );
	}
}

void CLocation_Panel::OnMousePressed( vgui::MouseCode code )
{
	if ( !IsCursorOverHex() )
		return;

	// don't drag if we're not already selected
	if ( m_pLayoutPanel->GetLocationEditorFrame()->GetCurrentLocationID() != m_iLocationID )
		return;

	m_bDragging = true;

	int x, y;
	GetPos( x, y );

	int current_posx, current_posy;
	vgui::input()->GetCursorPos( current_posx, current_posy );

	m_MouseOffset.x = x - current_posx;
	m_MouseOffset.y = y - current_posy;
}

void CLocation_Panel::OnMouseReleased( vgui::MouseCode code )
{
	if ( !IsCursorOverHex() )
		return;

	PostActionSignal( m_actionMessage->MakeCopy() );
}

bool CLocation_Panel::IsCursorOverHex()
{
	if ( !IsCursorOver() )
		return false;

	int current_posx, current_posy;
	vgui::input()->GetCursorPos( current_posx, current_posy );
	ScreenToLocal( current_posx, current_posy );
	// this is a hacky way to constraining the "click" location to match the current art, if we change the art, we should change the bounds
	if ( current_posx > GetWide() * 0.75f || current_posy > GetTall() * 0.85f )
		return false;


	return true;
}

void CLocation_Panel::UpdateHexColor()
{
	if ( !LocationGrid() )
		return;

	IASW_Location *pLocation = LocationGrid()->GetLocationByID( m_iLocationID );
	if ( !pLocation )
	{
		m_pHexImage->SetDrawColor( Color( 0, 0, 0, 0 ) );
		return;
	}

	CLocation_Editor_Frame *pFrame = m_pLayoutPanel->GetLocationEditorFrame();
	
	if ( IsCursorOverHex() )
	{
		m_pHexImage->SetDrawColor( Color( 255, 255, 255, 200 ) );
	}
	else if ( pFrame && pFrame->GetCurrentLocationID() == m_iLocationID )
	{
		m_pHexImage->SetDrawColor( Color( 255, 255, 0, 200 ) );
	}
	else
	{
		m_pHexImage->SetDrawColor( Color( 200, 200, 200, 200 ) );
	}
}

void CLocation_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	int hex_size = YRES( 22 );

	if ( !LocationGrid() )
		return;

	IASW_Location *pLocation = LocationGrid()->GetLocationByID( m_iLocationID );
	if ( !pLocation )
	{
		return;
	}

	m_iLastXPos = YRES( pLocation->GetXPos() );
	m_iLastYPos = YRES( pLocation->GetYPos() );

	int x = m_iLastXPos;
	int y = m_iLastYPos;

	int iShrink = 0;
	if ( pLocation->IsMissionOptional() )
		iShrink = hex_size * 0.2;

	if ( !m_bDragging )
	{
		SetBounds( x + iShrink, y + iShrink, hex_size - iShrink * 2, hex_size - iShrink * 2 );
	}
	else
	{
		SetSize( hex_size - iShrink * 2, hex_size - iShrink * 2 );
	}
	m_pHexImage->SetBounds( 0, 0, hex_size - iShrink * 2, hex_size - iShrink * 2 );
	m_pIDLabel->SetBounds( 0, 0, hex_size, hex_size );
	m_pIDLabel->SetZPos( 2 );
}

void CLocation_Panel::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled( false );
	
	m_pIDLabel->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	m_pIDLabel->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pIDLabel->SetPaintBackgroundEnabled( false );

	m_pHexImage->SetImage( "briefing/map_icon_available.vmt" );
	UpdateHexColor();
}