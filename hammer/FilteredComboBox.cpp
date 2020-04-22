
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "FilteredComboBox.h"


BEGIN_MESSAGE_MAP(CFilteredComboBox, CComboBox)
	//{{AFX_MSG_MAP(CFilteredComboBox)
	ON_CONTROL_REFLECT_EX(CBN_SELCHANGE, OnSelChange)
	ON_CONTROL_REFLECT_EX(CBN_EDITCHANGE, OnEditChange)
	ON_CONTROL_REFLECT_EX(CBN_CLOSEUP, OnCloseUp)
	ON_CONTROL_REFLECT_EX(CBN_DROPDOWN, OnDropDown)
	ON_CONTROL_REFLECT_EX(CBN_SELENDOK, OnSelEndOK)
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


static const char *s_pStringToMatch = NULL;
static int s_iStringToMatchLen;


// This can help debug events in the combo box.
static int g_iFunctionMarkerEvent = 1;
class CFunctionMarker
{
public:
	CFunctionMarker( const char *p )
	{
#if 0
		m_iEvent = g_iFunctionMarkerEvent++;
		
		char str[512];
		Q_snprintf( str, sizeof( str ), "enter %d: %s\n", m_iEvent, p );
		OutputDebugString( str );
		m_p = p;
#endif
	}
	
	~CFunctionMarker()
	{
#if 0
		char str[512];
		Q_snprintf( str, sizeof( str ), "exit %d: %s\n", m_iEvent, m_p );
		OutputDebugString( str );
#endif
	}
	const char *m_p;
	int m_iEvent;
};

// ------------------------------------------------------------------------------------------------------------ //
// CFilteredComboBox implementation.
// ------------------------------------------------------------------------------------------------------------ //
CFilteredComboBox::CFilteredComboBox( CFilteredComboBox::ICallbacks *pCallbacks )
	: m_pCallbacks( pCallbacks )
{
	m_hQueuedFont = NULL;
	m_bInSelChange = false;
	m_bNotifyParent = true;
	m_dwTextColor = RGB(0, 0, 0);
	m_bOnlyProvideSuggestions = true;
	m_hEditControlFont = NULL;
	m_bInEnterKeyPressedHandler = false;
}


void CFilteredComboBox::SetSuggestions( CUtlVector<CString> &suggestions, int flags )
{
	CreateFonts();
	
	// Verify some of the window styles. This class requires these, and it doesn't get a change to set them
	// unless you call Create on it.
	// If we use owner draw variable, we get the bug described here: http://support.microsoft.com/kb/813791.
	Assert( GetStyle() & CBS_OWNERDRAWFIXED );
	Assert( GetStyle() & CBS_HASSTRINGS );
	Assert( !( GetStyle() & CBS_SORT ) );
	
	// Copy the list.
	m_Suggestions = suggestions;

	CString str;
	GetWindowText( str );
	DWORD sel = GetEditSel();

	FillDropdownList( NULL, false );
	
	// Force it to provide the first one if they only want suggestions and the current text in there is not valid.
	bool bSelectFirst = ((flags & SETSUGGESTIONS_SELECTFIRST) != 0);
	bool bCallback = ((flags & SETSUGGESTIONS_CALLBACK) != 0);
	bool bForceFirst = (m_bOnlyProvideSuggestions && FindSuggestion( str ) == -1);
	if ( bSelectFirst || bForceFirst )
	{
		SetCurSel( 0 );
		
		if ( GetCount() > 0 )
		{
			CString strLB;
			GetLBText( 0, strLB );
			if ( bCallback )
				DoTextChangedCallback( strLB );
		}
		else
		{
			m_LastTextChangedValue = "";
		}
	}
	else
	{	
		SetWindowText( str );
		SetEditSel( LOWORD( sel ), HIWORD( sel ) );
		if ( bCallback )
			DoTextChangedCallback( str );
	}
	
	SetRedraw( true );
	Invalidate();
}


void CFilteredComboBox::AddSuggestion( const CString &suggestion )
{
	if ( FindSuggestion( suggestion ) == -1 )
		m_Suggestions.AddToTail( suggestion );
}


void CFilteredComboBox::Clear()
{
	m_Suggestions.Purge();
	SetWindowText( "" );
}


void CFilteredComboBox::ForceEditControlText( const char *pStr )
{
	SetWindowText( pStr );
}


void CFilteredComboBox::SelectItem( const char *pStr )
{
	if ( !pStr )
	{
		SetEditControlText( "" );
		return;
	}
	
	// See if we already have this item selected. If so, don't do anything.
	int iCurSel = GetCurSel();
	if ( iCurSel != CB_ERR )
	{
		CString str;
		GetLBText( iCurSel, str );
		if ( Q_stricmp( pStr, str ) == 0 )
		{
			// Make sure the edit control has the right text in there. If they called ForceEditControlText,
			// then it might not.
			CString strWindow;
			GetWindowText( strWindow );
			if ( Q_stricmp( strWindow, pStr ) != 0 )
			{
				SetWindowText( pStr );
			}			
			
			m_LastTextChangedValue = pStr;
			return;
		}
	}
	
	if ( m_bOnlyProvideSuggestions && FindSuggestion( pStr ) == -1 )
	{
		// This item doesn't match any suggestion. We can get rid of this assert
		// if it becomes a nuissance, but for now it's good to note that this
		// is a weird situation.
		Assert( false );
		SetEditControlText( pStr );
		return;
	}
	
	FillDropdownList( pStr );
}


CString CFilteredComboBox::GetCurrentItem()
{
	return m_LastTextChangedValue;
}


void CFilteredComboBox::SetEditControlFont( HFONT hFont )
{
	if ( !hFont )
		return;
	
	if ( m_bInSelChange )
	{
		m_hQueuedFont = hFont;
		return;
	}

	CString str;
	GetWindowText( str );
	DWORD sel = GetEditSel();
	
	InternalSetEditControlFont( hFont, str, sel );
}


void CFilteredComboBox::InternalSetEditControlFont( HFONT hFont, const char *pEditText, DWORD sel )
{	
	if ( hFont != m_hEditControlFont )
	{
		CFunctionMarker marker( "InternalSetEditControlFont" );

		// Don't let it mess with everything here.
		SetRedraw( false );
		
		CRect rcMyRect;
		GetWindowRect( rcMyRect );
		CWnd *pParent = GetParent();
		if ( pParent )
			pParent->ScreenToClient( &rcMyRect );
		
		BOOL bWasDropped = GetDroppedState();
		
		
		m_hEditControlFont = hFont;
		SetFont( CFont::FromHandle( m_hEditControlFont ), false );

		
		SetWindowText( pEditText );
		SetEditSel( LOWORD( sel ), HIWORD( sel ) );
	
		if ( pParent )
			MoveWindow( rcMyRect );

		if ( bWasDropped )
			ShowDropDown( true );
			
					
		SetRedraw( true );
		Invalidate();
	}
}


HFONT CFilteredComboBox::GetEditControlFont() const
{
	return m_hEditControlFont;
}


void CFilteredComboBox::SetEditControlTextColor(COLORREF dwColor)
{
	m_dwTextColor = dwColor;
}


COLORREF CFilteredComboBox::GetEditControlTextColor() const
{
	return m_dwTextColor;
}


void CFilteredComboBox::SetEditControlText( const char *pText )
{
	SetWindowText( pText );
}


CString CFilteredComboBox::GetEditControlText() const
{
	CString ret;
	GetWindowText( ret );
	return ret;
}

bool CFilteredComboBox::IsWindowEnabled() const
{
	return (BaseClass::IsWindowEnabled() == TRUE);
}


void CFilteredComboBox::EnableWindow( bool bEnable )
{
	BaseClass::EnableWindow( bEnable );
}


void CFilteredComboBox::SetOnlyProvideSuggestions( bool bOnlyProvideSuggestions )
{
	m_bOnlyProvideSuggestions = bOnlyProvideSuggestions;
}


void CFilteredComboBox::FillDropdownList( const char *pInitialSel, bool bEnableRedraw )
{
	CFunctionMarker marker( "FillDropdownList" );

	SetRedraw( FALSE );
	ResetContent();
	
	// Fill the box with the initial set of values.
	CUtlVector<CString> items;
	GetItemsMatchingString( "", items );
	
	for ( int i=0; i < items.Count(); i++ )
		AddString( items[i] );

	if ( pInitialSel )
	{
		CString str = pInitialSel;
		if ( m_bOnlyProvideSuggestions )
		{
			str = GetBestSuggestion( pInitialSel );
			if ( !InternalSelectItemByName( pInitialSel) )
			{
				Assert( false );
			}
		}
		else
		{
			// Make sure we're putting the item they requested in there.
			if ( !InternalSelectItemByName( str ) )
			{
				// Add the typed text to the combobox here otherwise it'll select the nearest match when they drop it down with the mouse.
				AddString( str );
				InternalSelectItemByName( str );
			}
		}

		DoTextChangedCallback( str );
	}

	if ( bEnableRedraw )
	{
		SetRedraw( TRUE );
		Invalidate();
	}
}


LRESULT CFilteredComboBox::DefWindowProc(
   UINT message,
   WPARAM wParam,
   LPARAM lParam 
)
{
	// We handle the enter key specifically because the default combo box behavior is to
	// reset the text and all this stuff we don't want.
	if ( message == WM_KEYDOWN )
	{
		if ( wParam == '\r' )
		{
			OnEnterKeyPressed( NULL );
			return 0;
		}
		else if ( wParam == 27 )
		{
			// Escape..
			OnEscapeKeyPressed();
			return 0;
		}
	}
	
	return BaseClass::DefWindowProc( message, wParam, lParam );
}


BOOL CFilteredComboBox::PreCreateWindow( CREATESTRUCT& cs )
{
	// We need these styles in order for owner draw to work.
	// If we use CBS_OWNERDRAWVARIABLE, then we run into this bug: http://support.microsoft.com/kb/813791.
	cs.style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
	cs.style &= ~CBS_SORT;
	return BaseClass::PreCreateWindow( cs );
}

void CFilteredComboBox::OnEnterKeyPressed( const char *pForceText )
{
	if ( m_bInEnterKeyPressedHandler )
		return;

	CFunctionMarker marker( "OnEnterKeyPressed" );
		
	m_bInEnterKeyPressedHandler = true;
	
	// Must do this before ShowDropDown because that will change these variables underneath us.
	CString szTypedText;
	DWORD sel;
	if ( pForceText )
	{
		szTypedText = pForceText;
		sel = 0;
	}
	else
	{
		GetWindowText( szTypedText );
		sel = GetEditSel();
	}

	CRect rcMyRect;
	GetWindowRect( rcMyRect );
	CWnd *pParent = GetParent();
	if ( pParent )
		pParent->ScreenToClient( &rcMyRect );

	SetRedraw( false );	
	ShowDropDown( FALSE );

	// They can get into here a variety of ways. Editing followed by enter. Editing+arrow keys, followed by enter, etc.
	if ( m_bOnlyProvideSuggestions )
	{
		CString str;
		if ( FindSuggestion( szTypedText ) == -1 && m_pCallbacks->OnUnknownEntry( szTypedText ) )
		{
			// They want us to KEEP this unknown entry, so add it to our list and select it.
			m_Suggestions.AddToTail( szTypedText );
			str = szTypedText;
		}
		else
		{
			// They returned false, so do the default behavior: go to the best match we can find.
			str = GetBestSuggestion( szTypedText );
		}
		
		DoTextChangedCallback( str );
		FillDropdownList( str, false );

		if ( GetCurSel() == CB_ERR )
			SetCurSel( 0 );
	}
	else
	{
		FillDropdownList( szTypedText, false );
		SetWindowText( szTypedText );
		SetEditSel( LOWORD(sel), HIWORD(sel) );
	}

	// Restore our window if necessary.
	if ( pParent )
		MoveWindow( &rcMyRect );
	SetRedraw( true );
	Invalidate();

	DoTextChangedCallback( GetEditControlText() );
	m_bInEnterKeyPressedHandler = false;
}


void CFilteredComboBox::OnEscapeKeyPressed()
{
	// Fill it with everything and force it to select whatever we last selected.
	m_bInEnterKeyPressedHandler = true;
	ShowDropDown( FALSE );
	m_bInEnterKeyPressedHandler = false;
	
	FillDropdownList( m_LastTextChangedValue, true );
}


BOOL CFilteredComboBox::OnDropDown()
{
	CFunctionMarker marker( "OnDropDown" );
	// This is necessary to keep the cursor from disappearing.
	SendMessage( WM_SETCURSOR, 0, 0 );
	return !m_bNotifyParent;
}

//-----------------------------------------------------------------------------
// Purpose: Attaches this object to the given dialog item.
//-----------------------------------------------------------------------------
void CFilteredComboBox::SubclassDlgItem(UINT nID, CWnd *pParent)
{
	//
	// Disable parent notifications for CControlBar-derived classes. This is
	// necessary because these classes result in multiple message reflections
	// unless we return TRUE from our message handler.
	//
	if (pParent->IsKindOf(RUNTIME_CLASS(CControlBar)))
	{
		m_bNotifyParent = false;
	}
	else
	{
		m_bNotifyParent = true;
	}

	BaseClass::SubclassDlgItem(nID, pParent);
}

BOOL CFilteredComboBox::OnSelChange()
{
	if ( !m_bInSelChange )
	{
		CFunctionMarker marker( "OnSelChange" );

		CString strOriginalText;
		GetWindowText( strOriginalText );
		DWORD dwOriginalEditSel = GetEditSel();
		
		
		m_bInSelChange = true;
		
		int iSel = GetCurSel();
		if ( iSel != CB_ERR )
		{
			CString str;
			GetLBText( iSel, str );
			strOriginalText = str;
			DoTextChangedCallback( str );
		}

		m_bInSelChange = false;

		if ( m_hQueuedFont )
		{
			HFONT hFont = m_hQueuedFont;
			m_hQueuedFont = NULL;
			m_bInSelChange = false;
			InternalSetEditControlFont( hFont, strOriginalText, dwOriginalEditSel );
		}
	}
	
	//
	// Despite MSDN's lies, returning FALSE here allows the parent
	// window to hook the notification message as well, not TRUE.
	//
	return !m_bNotifyParent;
}

BOOL CFilteredComboBox::OnCloseUp()
{
	if ( !m_bInEnterKeyPressedHandler )
	{
		CFunctionMarker marker( "OnCloseUp" );

		CString str;
		if ( GetCurSel() == CB_ERR || GetCount() == 0 )
			str = m_LastTextChangedValue;
		else
			GetLBText( GetCurSel(), str );
		OnEnterKeyPressed( str );
	}

	//
	// Despite MSDN's lies, returning FALSE here allows the parent
	// window to hook the notification message as well, not TRUE.
	//
	return !m_bNotifyParent;
}

BOOL CFilteredComboBox::OnSelEndOK()
{
	//
	// Despite MSDN's lies, returning FALSE here allows the parent
	// window to hook the notification message as well, not TRUE.
	//
	return !m_bNotifyParent;
}

BOOL CFilteredComboBox::OnEditChange()
{
	CFunctionMarker marker( "OnEditChange" );

	// Remember the text in the edit control because we're going to slam the
	// contents of the list and we'll want to put the text back in.
	CString szTypedText;
	DWORD dwEditSel;
	GetWindowText( szTypedText );
	dwEditSel = GetEditSel();

	// Show all the matching autosuggestions.
	CUtlVector<CString> items;
	GetItemsMatchingString( szTypedText, items );

	SetRedraw( FALSE );
	ResetContent();

	for ( int i=0; i < items.Count(); i++ )
	{
		AddString( items[i] );
	}

	// Add the typed text to the combobox here otherwise it'll select the nearest match when they drop it down with the mouse.
	if ( !m_bOnlyProvideSuggestions && FindSuggestion( szTypedText ) == -1 )
		AddString( szTypedText );

	// Note: for arcane and unspeakable MFC reasons, the placement of this call is VERY sensitive.
	// For example, if CTargetNameComboBox changes from a bold font to a normal font, then if this
	// call comes before ResetContent(), it will resize the dropdown listbox to a small size and not
	// size it back until it is cloesd and opened again.
	ShowDropDown();

	SetRedraw( TRUE );
	Invalidate();

	// Possibly tell the app about this change.
	if ( m_bOnlyProvideSuggestions )
	{
		if ( FindSuggestion( szTypedText ) != -1 )
			DoTextChangedCallback( szTypedText );
	}
	else
	{
		DoTextChangedCallback( szTypedText );
	}

	// Put the text BACK in there.
	SetWindowText( szTypedText );
	SetEditSel( LOWORD( dwEditSel ), HIWORD( dwEditSel ) );

	//
	// Despite MSDN's lies, returning FALSE here allows the parent
	// window to hook the notification message as well, not TRUE.
	//
	return !m_bNotifyParent;
}

int CFilteredComboBox::FindSuggestion( const char *pTest ) const
{
	for ( int i=0; i < m_Suggestions.Count(); i++ )
	{
		if ( Q_stricmp( m_Suggestions[i], pTest ) == 0 )
			return i;
	}
	return -1;
}


CString CFilteredComboBox::GetBestSuggestion( const char *pTest )
{
	// If it's an exact match, use that.
	if ( FindSuggestion( pTest ) != -1 )
		return pTest;

	// Look for the first autocomplete suggestion.
	CUtlVector<CString> matches;
	GetItemsMatchingString( pTest, matches );
	if ( matches.Count() > 0 )
		return matches[0];
	
	// Ok, fall back to the last known good one.
	return m_LastTextChangedValue;
}


CFont& CFilteredComboBox::GetNormalFont()
{
	CreateFonts();
	return m_NormalFont;
}


void CFilteredComboBox::GetItemsMatchingString( const char *pStringToMatch, CUtlVector<CString> &matchingItems )
{
	for ( int i=0; i < m_Suggestions.Count(); i++ )
	{
		if ( MatchString( pStringToMatch, m_Suggestions[i] ) )
			matchingItems.AddToTail( m_Suggestions[i] );
	}

	s_pStringToMatch = pStringToMatch;	
	s_iStringToMatchLen = V_strlen( pStringToMatch );
	matchingItems.Sort( &CFilteredComboBox::SortFn );
	s_pStringToMatch = NULL;
}


int CFilteredComboBox::SortFn( const CString *pItem1, const CString *pItem2 )
{
	// If one of them matches the prefix we're looking at, then that one should be listed first.
	// Otherwise, just do an alphabetical sort.
	bool bPrefixMatch1=false, bPrefixMatch2=false;
	if ( s_pStringToMatch )
	{
		bPrefixMatch1 = ( V_strnistr( *pItem1, s_pStringToMatch, s_iStringToMatchLen ) != NULL );
		bPrefixMatch2 = ( V_strnistr( *pItem2, s_pStringToMatch, s_iStringToMatchLen ) != NULL );
	}
	
	if ( bPrefixMatch1 == bPrefixMatch2 )
	{
		return Q_stricmp( *pItem1, *pItem2 );
	}
	else
	{
		return bPrefixMatch1 ? -1 : 1;
	}
}


bool CFilteredComboBox::MatchString( const char *pStringToMatchStart, const char *pTestStringStart )
{
	if ( !pStringToMatchStart || pStringToMatchStart[0] == 0 )
		return true;
	
	while ( *pTestStringStart )
	{
		const char *pStringToMatch = pStringToMatchStart;
		const char *pTestString = pTestStringStart;
		
		while ( 1 )
		{
			// Skip underscores in both strings.
			while ( *pStringToMatch == '_' )
				++pStringToMatch;
			
			while ( *pTestString == '_' )
				++pTestString;
			
			// If we're at the end of pStringToMatch with no mismatch, then treat this as a prefix match.
			// If we're at the end of pTestString, but pStringToMatch has more to go, then it's not a match.
			if ( *pStringToMatch == 0 )
				return true;
			else if ( *pTestString == 0 )
				break;
			
			// Match this character.
			if ( toupper( *pStringToMatch ) != toupper( *pTestString ) )
				break;
			
			++pStringToMatch;
			++pTestString;
		}
		
		++pTestStringStart;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called before painting to override default colors.
// Input  : pDC - DEvice context being painted into.
//			pWnd - Control asking for color.
//			nCtlColor - Type of control asking for color.
// Output : Returns the handle of a brush to use as the background color.
//-----------------------------------------------------------------------------
HBRUSH CFilteredComboBox::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hBrush = CComboBox::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_EDIT)
	{
		pDC->SetTextColor(m_dwTextColor);
	}

	return(hBrush);
}


void CFilteredComboBox::DoTextChangedCallback( const char *pText )
{
	// Sometimes it'll call here from a few places in a row. Only pass the result
	// to the owner once.
	if ( Q_stricmp( pText, m_LastTextChangedValue ) == 0 )
		return;
		
	m_LastTextChangedValue = pText;
	m_pCallbacks->OnTextChanged( pText );
}


void CFilteredComboBox::CreateFonts()
{
	//
	// Create a normal and bold font.
	//
	if (!m_NormalFont.m_hObject)
	{
		CFont *pFont = GetFont();
		if (pFont)
		{
			LOGFONT LogFont;
			pFont->GetLogFont(&LogFont);
			m_NormalFont.CreateFontIndirect(&LogFont);
		}
	}
}


void CFilteredComboBox::MeasureItem(LPMEASUREITEMSTRUCT pStruct)
{
	HFONT hFont;
	CFont *pFont = GetFont();
	if ( pFont )
		hFont = *pFont;
	else
		hFont = (HFONT)GetStockObject( DEFAULT_GUI_FONT );

	CFont *pActualFont = CFont::FromHandle( hFont );
	if ( pActualFont )
	{
		LOGFONT logFont;
		pActualFont->GetLogFont( &logFont );
		pStruct->itemHeight = abs( logFont.lfHeight ) + 5;
	}
	else
	{
		pStruct->itemHeight = 16;
	}
}


void CFilteredComboBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	if ( GetCount() == 0 )
		return;
		
	CString str;
	GetLBText( lpDrawItemStruct->itemID, str );

	CDC dc;
	dc.Attach( lpDrawItemStruct->hDC );

	// Save these values to restore them when done drawing.
	COLORREF crOldTextColor = dc.GetTextColor();
	COLORREF crOldBkColor = dc.GetBkColor();

	// If this item is selected, set the background color 
	// and the text color to appropriate values. Erase
	// the rect by filling it with the background color.
	// The left side of this expression was originally
	// "(lpDrawItemStruct->itemAction | ODA_SELECT)", which is always true.
	// To suppress the associated /analyze warning without changing
	// behavior the expression was fixed but commented out.
	if ( /*(lpDrawItemStruct->itemAction & ODA_SELECT) &&*/ (lpDrawItemStruct->itemState & ODS_SELECTED) )
	{
		dc.SetTextColor( ::GetSysColor(COLOR_HIGHLIGHTTEXT) );
		dc.SetBkColor( ::GetSysColor(COLOR_HIGHLIGHT) );
		dc.FillSolidRect( &lpDrawItemStruct->rcItem, ::GetSysColor(COLOR_HIGHLIGHT) );
	}
	else
	{
		dc.FillSolidRect(&lpDrawItemStruct->rcItem, crOldBkColor);
	}

	CFont *pOldFont = dc.SelectObject( &m_NormalFont );

	// Draw the text.
	RECT rcDraw = lpDrawItemStruct->rcItem;
	rcDraw.left += 1;
	dc.DrawText( str, -1, &rcDraw, DT_LEFT|DT_SINGLELINE|DT_VCENTER );

	// Restore stuff.
	dc.SelectObject( pOldFont );
	dc.SetTextColor(crOldTextColor);
	dc.SetBkColor(crOldBkColor);

	dc.Detach();
}


bool CFilteredComboBox::InternalSelectItemByName( const char *pName )
{
	int i = FindStringExact( -1, pName );
	if ( i == CB_ERR )
	{
		return false;
	}
	else
	{
		SetCurSel( i );
		
		CString str;
		GetWindowText( str );
		if ( Q_stricmp( str, pName ) != 0 )
			SetWindowText( pName );
		
		return true;
	}
}
