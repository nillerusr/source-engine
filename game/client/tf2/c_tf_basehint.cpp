//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_basehint.h"
#include "tf_hints.h"
#include "itfhintitem.h"
#include <vgui_controls/Controls.h>
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <vgui_controls/Label.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include "vgui_int.h"
#include "hintitembase.h"
#include <KeyValues.h>

HINTCOMPLETIONFUNCTION LookupCompletionFunction( const char *name );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			priority - 
//			player - 
//			entity - 
//-----------------------------------------------------------------------------
C_TFBaseHint::C_TFBaseHint( int id, int priority, int entity, HINTCOMPLETIONFUNCTION pfn /*=NULL*/ )
	: vgui::Panel( NULL, "TFBaseHint" ), m_CursorNone( vgui::dc_none )
{
	m_pObject = NULL;
	m_pClearLabel = NULL;
	m_pCaption = NULL;
	m_pfnCompletion = pfn;

	// Child of main panel
	SetParent( VGui_GetClientDLLRootPanel() );
	
	// Put at top of z-order (happens in Think, too)
//	MoveToFront();

	// No cursor
	SetCursor( m_CursorNone );
	// Set to default size
	SetSize( TFBASEHINT_DEFAULT_WIDTH, TFBASEHINT_DEFAULT_HEIGHT );
	// We'll expressly delete it
	SetAutoDelete( false );

	// Set up default values
	SetID( id );
	SetPriority( priority );
	SetEntity( entity );
	SetCompleted( false );
	// Target panel
	m_hTarget		= NULL; 
	
	m_bMoving		= false;
	m_flMoveRemaining	= 0.0f;
	m_flMoveTotal		= 0.0f;

	for ( int pt = 0; pt < 2; pt++ )
	{
		m_nMoveStart[ pt ] = 0;
		m_nMoveEnd[ pt ] = 0;
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	// Create clear label
	m_pClearLabel = new vgui::Label( this, "CLEAR", "[Enter] to remove, [Enter] twice quickly to remove all..." );
	m_pClearLabel->SetContentAlignment( vgui::Label::a_west );
	m_pClearLabel->SetTextInset( 3, 2 );

	// Create window caption
	m_pCaption = new vgui::Label( this, "CAPTION", "" );
	m_pCaption->SetContentAlignment( vgui::Label::a_west );
	m_pCaption->SetTextInset( 3, 0 );

	// See if the hint started out complete!
	CheckForCompletion();

	// Always start out hidden
	SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFBaseHint::~C_TFBaseHint( void )
{
	RemoveAllHintItems( true );
}


//-----------------------------------------------------------------------------
// Applying scheme settings
//-----------------------------------------------------------------------------
void C_TFBaseHint::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui::HFont hSmallFont = pScheme->GetFont( "DefaultVerySmall" );
	vgui::HFont hCaptionFont = pScheme->GetFont( "DefaultSmall" );
	m_pClearLabel->SetFont( hSmallFont );
	m_pCaption->SetFont( hCaptionFont );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pkv - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::ParseFromData( KeyValues *pkv )
{
	int priority = pkv->GetInt( "priority", 100 );
	SetPriority( priority );
	int width = pkv->GetInt( "width", TFBASEHINT_DEFAULT_WIDTH );
	if ( !width && !stricmp( pkv->GetString( "width" ), "default" ) )
	{
		width = TFBASEHINT_DEFAULT_WIDTH;
	}
	SetSize( width, TFBASEHINT_DEFAULT_HEIGHT );
	const char *title = pkv->GetString( "title" );
	if ( title )
	{
		SetTitle( title );
	}

	const char *completionfunction = pkv->GetString( "completionfunction" );
	if ( completionfunction && strlen( completionfunction ) > 0 )
	{
		SetCompletionFunction( LookupCompletionFunction( completionfunction ) );
	}

	KeyValues *items = pkv->FindKey( "items" );
	if ( items )
	{
		KeyValues *pkvItem = items->GetFirstSubKey();
		for( ; pkvItem ; pkvItem = pkvItem->GetNextKey() )
		{
			CHintItemBase *item = CreateHintItem( this, pkvItem->GetName() );
			if ( item )
			{
				item->ParseItem( pkvItem );
				item->ComputeTitle();

				item->SetSize( GetWide(), 20 );
				
				AddHintItem( item );
			}
			else
			{
				Msg( "C_TFBaseHint::ParseFromData:  Failed to create hint item %s\n", pkvItem->GetName() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : int&x - 
//			y - 
//			w - 
//			&h - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::GetClientArea( int&x, int& y, int& w, int &h )
{
	GetSize( w, h );

	x = BORDER;
	y = BORDER + CAPTION;

	w -= 2 * BORDER;
	h -= 2 * BORDER;

	h -= CAPTION;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *title - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetTitle( const char *title )
{
	if ( m_pCaption )
	{
		m_pCaption->SetText( title );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFBaseHint::PaintBackground()
{
	SetBgColor( Color( 240, 240, 220, 255 ) );
	if ( m_pCaption )
	{
		m_pCaption->SetBgColor( Color( 163, 180, 200, 255 ) );

		m_pCaption->SetFgColor( Color( 0, 0, 0, 255 ) );
	}
	if ( m_pClearLabel )
	{
		m_pClearLabel->SetBgColor( Color( 230, 230, 210, 255 ) );
		m_pClearLabel->SetFgColor( Color( 100, 127, 160, 255 ) );
	}

	BaseClass::PaintBackground();

	int w, h;
	GetSize( w, h );

	vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
	vgui::surface()->DrawOutlinedRect( 0, 0, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFBaseHint::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	GetClientArea( x, y, w, h );

	int itemh;

	int needY = 4;

	for ( int i = 0; i < GetNumHintItems(); i++ )
	{
		ITFHintItem *item = GetHintItem( i );
		if ( item )
		{
			itemh = item->GetHeight();

			item->SetPosition( x, y + 2 );
			item->SetItemNumber( i + 1 );

			if ( i == 0 )
			{
				item->SetVisible( true );
				needY += itemh + 2;
			}
			else
			{
				item->SetVisible( false );
			}
		}
	}

	needY += 8;

	if ( m_pClearLabel )
	{
		m_pClearLabel->SetBounds( x, y + needY + 2, w, 14 );
	}

	if ( m_pCaption )
	{
		m_pCaption->SetBounds( x, BORDER, w, CAPTION );
	}

	needY += 14 + BORDER;

	int needPixels = needY - h;

	int trueW, trueH;
	GetSize( trueW, trueH );

	SetSize( trueW, trueH + needPixels );
}

//-----------------------------------------------------------------------------
// Purpose: Install completion function
// Input  : pfn - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetCompletionFunction( HINTCOMPLETIONFUNCTION pfn )
{
	m_pfnCompletion = pfn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFBaseHint::CheckForCompletion( void )
{
	if ( m_pfnCompletion )
	{
		bool complete = (*m_pfnCompletion)( this );
		if ( complete )
		{
			SetCompleted( true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFBaseHint::OnTick()
{
	if ( !IsVisible() )
		return;

//	MoveToFront();

	// Check for completion of entire hint
	CheckForCompletion();

	bool setactive = false;
	int numactive = 0;
	// Remove obsolete items
	int i;
	for ( i = m_Hints.Size() - 1; i >= 0; i-- )
	{
		ITFHintItem *item = m_Hints[ i ];
		if ( !item )
			continue;

		if ( item->GetCompleted() )
		{
			RemoveHintItem( i );
		}
		else
		{
			numactive++;
		}
	}

	// Mark first one as active
	// Perform think
	for ( i = 0; i < m_Hints.Size(); i++ )
	{
		ITFHintItem *item = m_Hints[ i ];
		if ( !item )
			continue;

		if ( !setactive )
		{
			item->SetActive( true );
			setactive = true;
		}
		else
		{
			item->SetActive( false );
		}

		// Think, too
		if ( item->GetActive() )
		{
			item->Think();
		}
	}

	// No more active items
	if ( !numactive )
	{
		SetCompleted( true );
	}

	// Keep moving window to correct position
	AnimatePosition();

/*
	static float nextchange = 0.0f;

	if ( gpGlobals->curtime < nextchange )
		return;

	nextchange = gpGlobals->curtime + 1.0f;

	int w, h;
	GetSize( w, h );

	int x = random->RandomInt( 0, ScreenWidth() - w );
	int y = random->RandomInt( 0, ScreenHeight() - h );

	SetDesiredPosition( x, y, 0.9f );
*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_TFBaseHint::GetID( void )
{
	return m_nID;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetID( int id )
{
	m_nID = id;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_TFBaseHint::GetPriority( void )
{
	return m_nPriority;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : priority - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetPriority( int priority )
{
	m_nPriority = priority;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_TFBaseHint::GetEntity( void )
{
	return m_nEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
C_BaseEntity *C_TFBaseHint::GetBaseEntity( void )
{
	return m_nEntity != -1 ? cl_entitylist->GetEnt( m_nEntity ) : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entity - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetEntity( int entity )
{
	m_nEntity = entity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *entity - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetBaseEntity( C_BaseEntity *entity )
{
	m_nEntity = entity ? entity->index : -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFBaseHint::GetCompleted( void )
{
	return m_bCompleted;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : completed - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetCompleted( bool completed )
{
	m_bCompleted = completed;
	// Hide the window right away if it's finished
	SetVisible( !completed );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *item - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::AddHintItem( ITFHintItem *item )
{
	m_Hints.AddToTail( item );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::RemoveHintItem( int index )
{
	ITFHintItem *item = GetHintItem( index );
	if ( item )
	{
		m_Hints.Remove( index );
		item->DeleteThis();
		InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_TFBaseHint::GetNumHintItems( void )
{
	return m_Hints.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : deleteitems - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::RemoveAllHintItems( bool deleteitems )
{
	while ( m_Hints.Size() > 0 )
	{
		ITFHintItem *item = m_Hints[ 0 ];
		m_Hints.Remove( 0 );
		if ( deleteitems )
		{
			item->DeleteThis();
		}
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
ITFHintItem	*C_TFBaseHint::GetHintItem( int index )
{
	if ( index < 0 || index >= m_Hints.Size() )
	{
		return NULL;
	}

	return m_Hints[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			movementtime - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetDesiredPosition( int x, int y, float movementtime /*=0.3f*/ )
{
	m_bMoving = true;

	m_flMoveRemaining = movementtime;
	m_flMoveTotal = movementtime;
	
	int ox, oy;
	GetPos( ox, oy );

	m_nMoveStart[ 0 ] = ox;
	m_nMoveStart[ 1 ] = oy;

	m_nMoveEnd[ 0 ] = x;
	m_nMoveEnd[ 1 ] = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_TFBaseHint::GetMovementFraction( void )
{
	float frac = 0.0f;
	
	if ( m_flMoveTotal > 0.0f )
	{
		frac = 1.0f - ( m_flMoveRemaining / m_flMoveTotal );
	}

	float squared = frac * frac;
	
	frac = 3 * squared - 2 * frac * squared;

	// Simple spline
	frac = clamp( frac, 0.0f, 1.0f );

	return frac;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFBaseHint::AnimatePosition( void )
{
	if ( !m_bMoving )
		return;

	m_flMoveRemaining -= gpGlobals->frametime;
	if ( m_flMoveRemaining <= 0.0f )
	{
		m_bMoving = false;
		SetPos( m_nMoveEnd[ 0 ], m_nMoveEnd[ 1 ] );
		return;
	}

	float frac = GetMovementFraction();

	int dx = m_nMoveEnd[ 0 ] - m_nMoveStart[ 0 ];
	int dy = m_nMoveEnd[ 1 ] - m_nMoveStart[ 1 ];

	int x, y;

	x = m_nMoveStart[ 0 ] + ( int )( frac * dx );
	y = m_nMoveStart[ 1 ] + ( int )( frac * dy );

	SetPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: Helper to center a panel
// Input  : *panel - 
// Output : static void
//-----------------------------------------------------------------------------
static void PositionHintNoTarget( C_TFBaseHint *panel )
{
	int w, h;
	panel->GetSize( w, h );
	int y = ( ScreenHeight() - h ) / 2;

	panel->SetDesiredPosition( ScreenWidth() - w - 10, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void C_TFBaseHint::SetHintTarget( vgui::Panel *panel )
{
	m_hTarget = panel;

	if ( panel )
	{
		int hintW, hintH;
		
		GetSize( hintW, hintH );

		int x, y, w, h;
		panel->GetBounds( x, y, w, h );

		// Try and position ourselves up and to left of target item?
		x = x - ( hintW - w );
		y = y - ( hintH ) - 40;

		// Don't let it hang off screen
		if ( x < 3 )
		{
			x = 3;
		}
		else if ( x + hintW + 3 >= ScreenWidth() )
		{
			int over = ( x + hintW + 3 - ScreenWidth() );

			x -= over;
		}
		
		if ( y < 3 )
		{
			y = 3;
		}
		else if ( y + hintH >= ScreenHeight() )
		{
			int over = ( y + hintH + 3 - ScreenHeight() );

			y -= over;
		}

		SetDesiredPosition( x, y );
	}
	else
	{
		PositionHintNoTarget( this );
	}

	// Tell hint items that there is a new target
	for ( int i = 0 ; i < GetNumHintItems(); i++ )
	{
		ITFHintItem *item = GetHintItem( i );
		item->SetHintTarget( panel );
	}
}
