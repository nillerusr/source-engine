//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "IconComboBox.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CIconComboBox::CIconComboBox()
{
}


//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CIconComboBox::~CIconComboBox()
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CIconComboBox::Init( void )
{
	// initialize the icon size
	m_IconSize.cx = GetSystemMetrics( SM_CXICON );
	m_IconSize.cy = GetSystemMetrics( SM_CYICON );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::AddIcon( LPCTSTR pIconName )
{
	//
	// create/load an icon from file
	//
	// NULL - no icons in file
	// 1 - not a proper icon file
	//
	HICON hIcon = ExtractIcon( AfxGetInstanceHandle(), pIconName, 0 );
	if( ( hIcon == ( HICON )1 ) || !hIcon )
		return CB_ERR;

	//
	// add the icon to the combo box - returning the index
	//
	// CB_ERR - general error adding icon
	// CB_ERRSPACE - insufficient space necessary to add icon
	//
	int ndx = CComboBox::AddString( pIconName );
	if( ( ndx == CB_ERR ) || ( ndx == CB_ERRSPACE ) )
		return ndx;

	//
	// associate the icon with the index
	//
	// CB_ERR - general error
	//
	int result = SetItemData( ndx, ( DWORD )hIcon );
	if( result == CB_ERR )
		return result;

	// return the icon index
	return ndx;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::InsertIcon( LPCTSTR pIconName, int ndx )
{
	//
	// create an icon from file
	// 
	// NULL - no icons in file
	// 1 - not a proper icon file
	//
	HICON hIcon = ExtractIcon( AfxGetInstanceHandle(), pIconName, 0 );
	if( ( hIcon == ( HICON )1 ) || !hIcon )
		return CB_ERR;

	//
	// insert the icon into the combo box -- returning the index
	//
	// CB_ERR - general error adding icon
	// CB_ERRSPACE - insufficient space necessary to add icon
	//
	int result = CComboBox::InsertString( ndx, pIconName );
	if( ( result == CB_ERR ) || ( result == CB_ERRSPACE ) )
		return result;

	//
	// associate the icon with the index
	//
	// CB_ERR - general error
	//
	result = SetItemData( ndx, ( DWORD )hIcon );
	if( result == CB_ERR )
		return result;
	
	// return the icon index
	return ndx;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::SelectIcon( LPCTSTR pIconName )
{
	//
	// search the combo box list for the given string, -1 = search the whole list
	//
	// CB_ERR - unsuccessful search
	//
	int ndx = CComboBox::FindStringExact( -1, pIconName );
	if( ndx == CB_ERR )
		return CB_ERR;

	// set the selection current
	return CComboBox::SetCurSel( ndx );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::SelectIcon( int ndx )
{
	// set the selection current
	return CComboBox::SetCurSel( ndx );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::DeleteIcon( LPCTSTR pIconName )
{
	//
	// search the combo box list for the given string, -1 = search the whole list
	//
	// CB_ERR - unsuccessful search
	//
	int ndx = CComboBox::FindStringExact( -1, pIconName );
	if( ndx == CB_ERR )
		return CB_ERR;

	// remove the icon from the combo box
	return CComboBox::DeleteString( ndx );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CIconComboBox::DeleteIcon( int ndx )
{
	// remove the icon from the combo box
	return CComboBox::DeleteString( ndx );
}


//-----------------------------------------------------------------------------
// Purpose: don't allow the icon combo box to "AddString"
//-----------------------------------------------------------------------------
int CIconComboBox::AddString( LPCTSTR lpszString )
{
	assert( FALSE ); 
	return CB_ERR;
}


//-----------------------------------------------------------------------------
// Purpose: don't allow the icon combo box to "InsertString"
//-----------------------------------------------------------------------------
int CIconComboBox::InsertString( int nIndex, LPCTSTR lpszString )
{
	assert( FALSE );
	return CB_ERR;
}


//-----------------------------------------------------------------------------
// Purpose: don't allow the icon combo box to "DeleteString"
//-----------------------------------------------------------------------------
int CIconComboBox::DeleteString( int nIndex )
{
	assert( FALSE );
	return CB_ERR;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CIconComboBox::MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{ 
	lpMeasureItemStruct->itemWidth = m_IconSize.cx;
	lpMeasureItemStruct->itemHeight = m_IconSize.cy + 1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CIconComboBox::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	CBrush *pOldBrush = NULL;
	CPen *pOldPen = NULL;

	//
	// the icon is "disabled"
	//
	if( !IsWindowEnabled() )
	{
		SetDisabledBrushAndPen( lpDrawItemStruct, &pOldBrush, &pOldPen );
        OnDrawIcon( lpDrawItemStruct );
		ResetBrushAndPen( lpDrawItemStruct, pOldBrush, pOldPen );
		return;
	}

	//
	// the icon is "selected"
	//
	if( ( lpDrawItemStruct->itemState & ODS_SELECTED ) && 
		( lpDrawItemStruct->itemAction & ( ODA_SELECT | ODA_DRAWENTIRE ) ) ) 
	{
		SetSelectedBrushAndPen( lpDrawItemStruct, &pOldBrush, &pOldPen );
		OnDrawIcon( lpDrawItemStruct );
		ResetBrushAndPen( lpDrawItemStruct, pOldBrush, pOldPen );
	}

	//
	// the icon is "un-selected"
	//
	if( !( lpDrawItemStruct->itemState & ODS_SELECTED ) && 
		 ( lpDrawItemStruct->itemAction & ( ODA_SELECT | ODA_DRAWENTIRE ) ) ) 
	{
		SetUnSelectedBrushAndPen( lpDrawItemStruct, &pOldBrush, &pOldPen );
		OnDrawIcon( lpDrawItemStruct );
		ResetBrushAndPen( lpDrawItemStruct, pOldBrush, pOldPen );
	}

	//
    // icon gains focus
	//
    if( lpDrawItemStruct->itemAction & ODA_FOCUS ) 
	{ 
		// get the device context
		CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

		// render the focus rectangle
        pDC->DrawFocusRect( &lpDrawItemStruct->rcItem );
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIconComboBox::OnDrawIcon( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	// any items to draw?
	if( GetCount() == 0 )
		return;

	// get the device context - to draw
	CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

	// get the current icon to render
	HICON hIcon = ( HICON )lpDrawItemStruct->itemData;
	if( !hIcon )
		return;

	// calculate the icon's upper left corner
	int UpperLeftX = lpDrawItemStruct->rcItem.left + 
		             ( ( lpDrawItemStruct->rcItem.right - lpDrawItemStruct->rcItem.left ) / 2 ) - 
			         ( m_IconSize.cx / 2 );
	int UpperLeftY = lpDrawItemStruct->rcItem.top + 
		             ( ( lpDrawItemStruct->rcItem.bottom - lpDrawItemStruct->rcItem.top ) / 2 ) - 
					 ( m_IconSize.cy / 2 );

	// render the icon
	pDC->DrawIcon( UpperLeftX, UpperLeftY, hIcon );
}               


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIconComboBox::ResetBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, 
									  CBrush *pBrush, CPen *pPen )
{
	// get the device context
    CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

	// reset brush and pen
	pDC->SelectObject( pBrush );
	pDC->SelectObject( pPen );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIconComboBox::SetDisabledBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, 
										    CBrush **ppOldBrush, CPen **ppOldPen )
{
	// get the device context
    CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

	// set brush and pen to light gray
	CBrush brushDisabled( RGB( 192, 192, 192 ) );
	CPen penDisabled( PS_SOLID, 1, RGB( 192, 192, 192 ) );

	// set the brush and pen current -- saving the old brush and pen state
	*ppOldBrush = pDC->SelectObject( &brushDisabled );
	*ppOldPen = pDC->SelectObject( &penDisabled );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIconComboBox::SetUnSelectedBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, 
											  CBrush **ppOldBrush, CPen **ppOldPen )
{
	// get the device context
    CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

	// set the brush and pen "un-highlighted"
	CBrush brushUnSelected( GetSysColor( COLOR_WINDOW ) );
	CPen penUnSelected( PS_SOLID, 1, GetSysColor( COLOR_WINDOW ) );

	// set the brush and pen current -- saving the old brush and pen state
	*ppOldBrush = pDC->SelectObject( &brushUnSelected );
	*ppOldPen = pDC->SelectObject( &penUnSelected );

	//
	// set some addition render state - background  and text color
	//
	pDC->Rectangle( &lpDrawItemStruct->rcItem );
	pDC->SetBkColor( GetSysColor( COLOR_WINDOW ) );
	pDC->SetTextColor( GetSysColor( COLOR_WINDOWTEXT ) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIconComboBox::SetSelectedBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, 
										    CBrush **ppOldBrush, CPen **ppOldPen )
{
	// get the device context
    CDC* pDC = CDC::FromHandle( lpDrawItemStruct->hDC );

	// set the brush and pen "highlighted"
	CBrush brushSelected( GetSysColor( COLOR_HIGHLIGHT ) );
	CPen penSelected( PS_SOLID, 1, GetSysColor( COLOR_HIGHLIGHT ) );

	// set the brush and pen current -- saving the old brush and pen state
	*ppOldBrush = pDC->SelectObject( &brushSelected );
	*ppOldPen = pDC->SelectObject( &penSelected );
	
	//
	// set some addition render state - background  and text color
	//
	pDC->Rectangle( &lpDrawItemStruct->rcItem );
	pDC->SetBkColor( GetSysColor( COLOR_HIGHLIGHT ) );
	pDC->SetTextColor( GetSysColor( COLOR_HIGHLIGHTTEXT ) );
}


