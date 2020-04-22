//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include "choreowidget.h"
#include "choreoview.h"

// Static elements
CChoreoScene *CChoreoWidget::m_pScene = NULL;
CChoreoView *CChoreoWidget::m_pView = NULL;

static int widgets = 0;
//-----------------------------------------------------------------------------
// CChoreoWidget new/delete
// All fields in the object are all initialized to 0.
//-----------------------------------------------------------------------------
void *CChoreoWidget::operator new( size_t stAllocateBlock )
{
	widgets++;
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMem - 
//-----------------------------------------------------------------------------
void CChoreoWidget::operator delete( void *pMem )
{
	widgets--;
	// set the memory to a known value
	int size = _msize( pMem );
	memset( pMem, 0xfe, size );

	// get the engine to free the memory
	free( pMem );
}

//-----------------------------------------------------------------------------
// Purpose: Construct widget, all widgets clip their children and brethren
// Input  : *parent - 
//-----------------------------------------------------------------------------
CChoreoWidget::CChoreoWidget( CChoreoWidget *parent )
{
	m_bSelected = false;
	m_pParent = parent;
	m_bVisible = true;

	m_rcBounds.left = m_rcBounds.right = m_rcBounds.top = m_rcBounds.bottom = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoWidget::~CChoreoWidget( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Default implementation, just return base row height
//-----------------------------------------------------------------------------
int	CChoreoWidget::GetItemHeight( void )
{
	return m_pView->GetRowHeight();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CChoreoWidget::LocalToScreen( int& mx, int& my )
{
	/*
	HWND wnd = (HWND)getHandle();
	if ( !wnd )
		return;

	POINT pt;
	pt.x = (short)mx;
	pt.y = (short)my;
	
	ClientToScreen( wnd, &pt );
	
	mx = pt.x;
	my = pt.y;
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoWidget::IsSelected( void )
{
	return m_bSelected;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : selected - 
//-----------------------------------------------------------------------------
void CChoreoWidget::SetSelected( bool selected )
{
	m_bSelected = selected;
}

void CChoreoWidget::setBounds( int x, int y, int w, int h )
{
	m_rcBounds.left = x;
	m_rcBounds.right = x + w;
	m_rcBounds.top = y;
	m_rcBounds.bottom = y + h;
}

int CChoreoWidget::x( void )
{
	return m_rcBounds.left;
}

int CChoreoWidget::y( void )
{
	return m_rcBounds.top;
}

int CChoreoWidget::w( void )
{
	return m_rcBounds.right - m_rcBounds.left;
}

int CChoreoWidget::h( void )
{
	return m_rcBounds.bottom - m_rcBounds.top;
}

CChoreoWidget *CChoreoWidget::getParent( void )
{
	return m_pParent;
}

void CChoreoWidget::setVisible( bool visible )
{
	m_bVisible = visible;
}

bool CChoreoWidget::getVisible( void )
{
	return m_bVisible;
}

void CChoreoWidget::getBounds( RECT& bounds )
{
	bounds = m_rcBounds;
}

RECT &CChoreoWidget::getBounds( void )
{
	return m_rcBounds;
}
