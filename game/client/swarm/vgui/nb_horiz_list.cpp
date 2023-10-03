#include "cbase.h"
#include "nb_horiz_list.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_bitmapbutton.h"
#include "nb_select_weapon_entry.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Horiz_List::CNB_Horiz_List( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pBackgroundImage = new vgui::ImagePanel( this, "BackgroundImage" );
	m_pForegroundImage = new vgui::ImagePanel( this, "ForegroundImage" );
	m_pLeftArrowButton = new CBitmapButton( this, "LeftArrowButton", "" );
	m_pRightArrowButton = new CBitmapButton( this, "RightArrowButton", "" );	
	// == MANAGED_MEMBER_CREATION_END ==
	m_pHorizScrollBar = new vgui::ScrollBar( this, "HorizScrollBar", false );
	m_pHorizScrollBar->AddActionSignalTarget( this );
	m_pLeftArrowButton->AddActionSignalTarget( this );
	m_pRightArrowButton->AddActionSignalTarget( this );
	m_pLeftArrowButton->SetCommand( "LeftArrow" );
	m_pRightArrowButton->SetCommand( "RightArrow" );

	m_pForegroundImage->SetMouseInputEnabled( false );

	m_pChildContainer = new vgui::Panel( this, "ChildContainer" );

	m_nHighlightedEntry = -1;

	m_flContainerPos = 0;
	m_flContainerTargetPos = 0;
	m_bShowScrollBar = true;
	m_bShowArrows = false;
	m_bHighlightOnMouseOver = true;
	m_bAutoScrollChange = false;
	m_nEntryWide = 10;

	m_fScrollVelocity = 0.0f;
	m_fScrollChange = 0.0f;
}

CNB_Horiz_List::~CNB_Horiz_List()
{

}

void CNB_Horiz_List::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_horiz_list.res" );

	color32 regular;
	regular.r = 224;
	regular.g = 224;
	regular.b = 224;
	regular.a = 224;

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;

	color32 dark;
	dark.r = 128;
	dark.g = 128;
	dark.b = 128;
	dark.a = 128;

	m_pLeftArrowButton->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/arrow_left", regular );
	m_pLeftArrowButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/arrow_left", white );
	m_pLeftArrowButton->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/arrow_left", regular );
	m_pLeftArrowButton->SetImage( CBitmapButton::BUTTON_DISABLED, "vgui/arrow_left", dark );

	m_pRightArrowButton->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/arrow_right", regular );
	m_pRightArrowButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/arrow_right", white );
	m_pRightArrowButton->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/arrow_right", regular );
	m_pRightArrowButton->SetImage( CBitmapButton::BUTTON_DISABLED, "vgui/arrow_right", dark );

	m_pHorizScrollBar->UseImages( "scroll_up", "scroll_down", "scroll_line", "scroll_box" );

	m_bInitialPlacement = true;

	m_pLeftArrowButton->SetVisible( m_bShowArrows );
	m_pRightArrowButton->SetVisible( m_bShowArrows );

	OnSliderMoved( 0 );
}

void CNB_Horiz_List::PerformLayout()
{
	BaseClass::PerformLayout();

	// stack up children side by side
	int curx = 0;
	int padding = 0;
	int maxh = 0;
	int entry_tall;

	if ( m_Entries.Count() > 0 )
	{
		curx = m_Entries[0]->GetTall() / 2 + padding;
	}

	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		int x, y;
		m_Entries[i]->GetPos( x, y );
		
		x = curx;
		m_Entries[i]->SetPos( x, y );
		m_Entries[i]->InvalidateLayout( true );

		m_Entries[i]->GetSize( m_nEntryWide, entry_tall );
		maxh = MAX( maxh, entry_tall );

		curx += m_nEntryWide + padding;
	}

	if ( m_Entries.Count() > 0 )
	{
		curx += m_Entries[0]->GetTall() / 2;
	}

	m_pChildContainer->SetSize( curx - padding, maxh );		// stretch container to fit all children
	
	// set our height from the tallest entry
	int sw, sh;
	GetSize( sw, sh );	
	m_pBackgroundImage->SetSize( sw, maxh );
	m_pForegroundImage->SetSize( sw, maxh );

	int ax, ay, aw, ah;
	m_pLeftArrowButton->InvalidateLayout( true );
	m_pLeftArrowButton->GetSize( aw, ah );
	m_pLeftArrowButton->GetPos( ax, ay );
	m_pLeftArrowButton->SetPos( ax, maxh * 0.5f - ah * 0.5f );

	m_pRightArrowButton->InvalidateLayout( true );
	m_pRightArrowButton->GetSize( aw, ah );
	m_pRightArrowButton->GetPos( ax, ay );
	m_pRightArrowButton->SetPos( ax, maxh * 0.5f - ah * 0.5f );

	if ( m_bShowScrollBar )
	{
		int sbx, sby, sbw, sbt;
		m_pHorizScrollBar->GetBounds( sbx, sby, sbw, sbt );
		m_pHorizScrollBar->SetPos( sbx, maxh );
		//maxh = MAX( maxh, sby + sbt );
		maxh += sbt;

		int cw = m_pChildContainer->GetWide();
		cw += m_nEntryWide * 2;	// pad by 2 entries - assumes all entries are the same size
		int xrange = MAX( 0, cw - GetWide() );
		m_pHorizScrollBar->SetRange( 0, xrange + GetWide() );
		m_pHorizScrollBar->SetRangeWindow( GetWide() );
		m_pChildContainer->SetPos( m_nEntryWide - m_pHorizScrollBar->GetValue(), 0 );
	}

	SetSize( sw, maxh );
}

#define SCROLL_SPEED 1200.0f

void CNB_Horiz_List::OnThink()
{
	BaseClass::OnThink();

	if ( MouseOverScrollbar() )
	{
		// No slidy auto scroll while using the scrollbar
		m_fScrollVelocity = 0.0f;
		m_fScrollChange = 0.0f;
	}
	else
	{
		int nMouseX, nMouseY;
		ASWInput()->GetFullscreenMousePos( &nMouseX, &nMouseY );
		ScreenToLocal( nMouseX, nMouseY );

		float fVelocityMax = 1200.0f;

		const float fAccelerationMax = 3000.0f;
		float fScrollAcceleration = 0.0f;

		if ( nMouseX > 0 && nMouseX < GetWide() && nMouseY > 0 && nMouseY < GetTall() )
		{
			const float fDeadZoneSize = 0.55f;
			float fMouseXInterp = static_cast<float>( nMouseX ) / GetWide();

			if ( fMouseXInterp > 0.5f + fDeadZoneSize * 0.5f )
			{
				fMouseXInterp = 0.25f + ( fMouseXInterp - ( 0.5f + fDeadZoneSize * 0.5f ) ) / ( ( 1.0f - fDeadZoneSize ) * 0.666666666f );
			}
			else if ( fMouseXInterp < 0.5f - fDeadZoneSize * 0.5f )
			{
				fMouseXInterp = -0.25f - ( ( 0.5f - fDeadZoneSize * 0.5f ) - fMouseXInterp ) / ( ( 1.0f - fDeadZoneSize ) * 0.666666666f );
			}
			else
			{
				fMouseXInterp = 0.0f;
			}

			fScrollAcceleration = fMouseXInterp * fAccelerationMax;
			fVelocityMax *= fabsf( fMouseXInterp );
		}

		if ( fScrollAcceleration == 0.0f )
		{
			// Dampen velocity in the dead zone
			m_fScrollVelocity = Approach( 0.0f, m_fScrollVelocity, gpGlobals->frametime * fAccelerationMax * 0.75f );
		}
		else
		{
			// Apply acceleration to the velocity
			m_fScrollVelocity = clamp( m_fScrollVelocity + fScrollAcceleration * gpGlobals->frametime, -fVelocityMax, fVelocityMax );
		}

		// Accumulate into a float for smaller than 1 changes
		m_fScrollChange += m_fScrollVelocity * gpGlobals->frametime;

		// Apply the integer amount
		int nScrollChange = m_fScrollChange;

		if ( nScrollChange <= -1 || nScrollChange >= 1 )
		{
			bool bChanged = ChangeScrollValue( nScrollChange );

			if ( bChanged )
			{
				m_fScrollChange -= nScrollChange;
			}
			else
			{
				// Hit the end
				m_fScrollVelocity = 0.0f;
				m_fScrollChange = 0.0f;
			}
		}
	}

	if ( m_bHighlightOnMouseOver )
	{
		for ( int i = 0; i < m_Entries.Count(); i++ )
		{
			if ( m_Entries[i]->IsCursorOver() )
			{
				SetHighlight( i );
				break;
			}
		}
	}

	vgui::Panel *pHighlighted = GetHighlightedEntry();
	if ( !pHighlighted )
		return;

	int x, y;
	pHighlighted->GetPos( x, y );
	m_flContainerTargetPos = YRES( 154 ) - x;

	if ( m_bInitialPlacement )
	{
		m_bInitialPlacement = false;
		m_flContainerPos = m_flContainerTargetPos;
	}
	else
	{
		if ( m_flContainerPos < m_flContainerTargetPos )
		{
			m_flContainerPos = MIN( m_flContainerPos + gpGlobals->frametime * SCROLL_SPEED, m_flContainerTargetPos );
		}
		else if ( m_flContainerPos > m_flContainerTargetPos )
		{
			m_flContainerPos = MAX( m_flContainerPos - gpGlobals->frametime * SCROLL_SPEED, m_flContainerTargetPos );
		}
	}
	//Msg( "m_flContainerPos = %f target = %f\n", m_flContainerPos, m_flContainerTargetPos );
	if ( !m_bShowScrollBar )
	{
		m_pChildContainer->SetPos( m_flContainerPos, 0 );
	}

	m_pLeftArrowButton->SetEnabled( m_nHighlightedEntry > 0 );
	m_pRightArrowButton->SetEnabled( m_nHighlightedEntry < m_Entries.Count() - 1 );
}

void CNB_Horiz_List::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "LeftArrow" ) )
	{
		if ( m_nHighlightedEntry > 0 )
		{
			m_nHighlightedEntry--;
		}
		return;
	}
	else if ( !Q_stricmp( command, "RightArrow" ) )
	{
		if ( m_nHighlightedEntry < m_Entries.Count() - 1 )
		{
			m_nHighlightedEntry++;
		}
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Horiz_List::AddEntry( vgui::Panel *pPanel )
{
	pPanel->SetParent( m_pChildContainer );
	vgui::PHandle handle;
	handle = pPanel;
	m_Entries.AddToTail( handle );

	if ( m_nHighlightedEntry == -1 && m_Entries.Count() == 2 )
	{
		m_nHighlightedEntry = 1;	// always select the 2nd entry in the list at the start
	}
	m_bInitialPlacement = true;
	InvalidateLayout();
}

vgui::Panel* CNB_Horiz_List::GetHighlightedEntry()
{
	if ( m_nHighlightedEntry < 0 || m_nHighlightedEntry >= m_Entries.Count() )
		return NULL;

	return m_Entries[ m_nHighlightedEntry ].Get();
}

void CNB_Horiz_List::SetHighlight( int nEntryIndex )
{
	if ( nEntryIndex < 0 || nEntryIndex >= m_Entries.Count() )
		return;

	if ( m_nHighlightedEntry != -1 && m_Entries[ m_nHighlightedEntry ].Get() != m_Entries[ nEntryIndex ].Get() )
	{
		CNB_Select_Weapon_Entry *pWeaponEntry = dynamic_cast<CNB_Select_Weapon_Entry*>( m_Entries[ m_nHighlightedEntry ].Get() );
		if ( pWeaponEntry )
		{
			pWeaponEntry->m_pWeaponImage->NavigateFrom();
		}
	}

	m_nHighlightedEntry = nEntryIndex;
}

bool CNB_Horiz_List::ChangeScrollValue( int nChange )
{
	int nOldValue = m_pHorizScrollBar->GetValue();
	m_pHorizScrollBar->SetValue( nOldValue + nChange );

	m_bAutoScrollChange = true;

	return ( nOldValue != m_pHorizScrollBar->GetValue() );
}

bool CNB_Horiz_List::MouseOverScrollbar( void ) const
{
	int nMouseX, nMouseY;
	ASWInput()->GetFullscreenMousePos( &nMouseX, &nMouseY );
	m_pHorizScrollBar->ScreenToLocal( nMouseX, nMouseY );

	return ( nMouseX > 0 && nMouseX < m_pHorizScrollBar->GetWide() && nMouseY > 0 && nMouseY < m_pHorizScrollBar->GetTall() );
}

void CNB_Horiz_List::OnSliderMoved( int position )
{
	InvalidateLayout();
	Repaint();

	// figure out which item is in the middle and highlight it
	int nChildOffset = m_nEntryWide - m_pHorizScrollBar->GetValue();
	int nClosestEntry = -1;
	int nClosestDist = INT_MAX;
	vgui::PHandle hClosestPanel;
	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		int x, y;
		m_Entries[i]->GetPos( x, y );
		int nDist = abs( ( ( GetWide() * 0.5f - m_Entries[i]->GetWide() * 0.5f ) - nChildOffset ) - x );
		if ( nDist < nClosestDist )
		{
			nClosestDist = nDist;
			nClosestEntry = i;
			hClosestPanel = m_Entries[i];
		}
	}
	if ( nClosestEntry != -1 )
	{
		if ( !m_bAutoScrollChange )
		{
			SetHighlight( nClosestEntry );

			CNB_Select_Weapon_Entry *pWeaponEntry = dynamic_cast<CNB_Select_Weapon_Entry*>( hClosestPanel.Get() );
			if ( pWeaponEntry && pWeaponEntry->m_bCanEquip )
			{
				pWeaponEntry->m_pWeaponImage->NavigateTo();
			}
		}
	}

	m_bAutoScrollChange = false;
}