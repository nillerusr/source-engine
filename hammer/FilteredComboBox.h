//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FILTERED_COMBO_BOX_H
#define FILTERED_COMBO_BOX_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"


#pragma warning( disable: 4355 )

// Flags for the SetSuggestions call.
#define SETSUGGESTIONS_SELECTFIRST		0x0001	// Select the first item in the list.
#define SETSUGGESTIONS_CALLBACK			0x0002	// Calls OnTextChanged for whatever it winds up selecting.

// CFilteredComboBox is a glorified EDIT control.
// The user can type stuff into the edit control, and it will provide autocomplete suggestions
// in its combo box. The user of this class provides those suggestions.
//
// To use this class:
//
// 1. Implement CFilteredComboBox::ICallbacks
// 2. Call SetSuggestions to set the list of autocomplete suggestions.
// 3. Call SetOnlyProvideSuggestions to tell it how to behave.
//
// NOTE: Use CComboBox functions with caution! You could screw up the CFilteredComboBox's operation.
class CFilteredComboBox : public CComboBox
{
	typedef CComboBox BaseClass;

public:
	
	// Implement this to get updates about the state.
	class ICallbacks
	{
	public:
		// Called when the text in the box changes.
		virtual void OnTextChanged( const char *pText ) = 0;
		
		// This is sort of a backdoor for "only provide suggestions" mode. Normally, it'll only call
		// OnTextChanged with entries that are in the suggestions list. But, it will call OnUnknownEntry
		// if they type in something that's not in your suggestion list first. If you return TRUE, it 
		// will add that entry to the suggestions list. If you return FALSE (the default behavior),
		// it will find the closest match to what the user typed and use that.
		virtual bool OnUnknownEntry( const char *pText ) { return false; }
	};


	CFilteredComboBox( CFilteredComboBox::ICallbacks *pCallbacks );


// The main functions to operate the filtered combo box are here.

	// This is the list of strings that is filtered into the dropdown combo box.
	// flags is a combination of the SETSUGGESTIONS_ flags.
	void SetSuggestions( CUtlVector<CString> &suggestions, int flags=SETSUGGESTIONS_SELECTFIRST|SETSUGGESTIONS_CALLBACK );
	
	// Add a single suggestion (if it's unique).
	void AddSuggestion( const CString &suggestion );

	// This clears all items from the combo and its textbox.
	void Clear();
	
	// This will force the edit control text. It won't call OnTextChanged.
	void ForceEditControlText( const char *pStr );

	// This sets the main mode that the box runs in.
	//
	// If you pass true, then it will only ever call ICallbacks::OnTextChanged with values that are in your suggestions,
	// and it does its best to autocomplete to those suggestions (so if the user types a partial string and closes
	// the box, it will find the first possible substring match OR it will revert to the last valid suggestion it was on).
	//
	// If you pass false, then it will call OnTextChanged for anything that gets entered into the textbox. This is used
	// for the entity properties targetname box, and the entity name changes right along as you type.
	// When the user presses enter, it does NOT automatically select the first suggestion. They have to use the arrow keys for that.
	void SetOnlyProvideSuggestions( bool bOnlyProvideSuggestions );


// These provide access to special behavior like font and color.

	// Puts this string in the edit control and selects it in the combo box. 
	void SelectItem( const char *pStr );
	
	// Returns the same value as the last call to OnTextChanged().
	CString GetCurrentItem();
	
	// Get/set the font in the edit control.
	void SetEditControlFont( HFONT hFont );
	HFONT GetEditControlFont() const;
	
	// Get/set the color that the edit text is drawn in.
	void SetEditControlTextColor( COLORREF clr );
	COLORREF GetEditControlTextColor() const;


// General windows functions.

	// Enable/disable the window.
	bool IsWindowEnabled() const;
	void EnableWindow( bool bEnable );


// Helper functions.
public:

	// This takes the string the user has entered (pStringToMatch passed into GetItemsMatchingString)
	// and returns true if pTestString matches it. It ignores underscores in both strings.
	bool MatchString( const char *pStringToMatch, const char *pTestString );

	// Does this string match one of the suggestions?
	// Returns the suggestion index or -1.
	int FindSuggestion( const char *pTest ) const;
	
	// Returns the closest-matching suggestion (the first one that would appear
	// in the autocomplete list) or the last known good suggestion.
	CString GetBestSuggestion( const char *pTest );

	void SubclassDlgItem(UINT nID, CWnd *pParent);


protected:

	// Get the base font it's using.
	CFont& GetNormalFont();	

	// Get/set the text in the edit control.
	void SetEditControlText( const char *pText );
	CString GetEditControlText() const;
	
	DECLARE_MESSAGE_MAP()

	bool m_bNotifyParent;		// Whether we allow our parent to hook our notification messages.
								// This is necessary because CControlBar-derived classes result in multiple
								// message reflections unless we disable parent notification.

protected:

	// Put all suggestions into the dropdown list.
	void FillDropdownList( const char *pInitialSel, bool bEnableRedraw=true );

	// CBN_ notification handlers.
	virtual BOOL PreCreateWindow( CREATESTRUCT& cs );
	BOOL OnDropDown();
	BOOL OnSelEndOK();
	BOOL OnCloseUp();
	BOOL OnSelChange();
	virtual BOOL OnEditChange();
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);

	void OnEnterKeyPressed( const char *pForceText );
	void OnEscapeKeyPressed();

	void DoTextChangedCallback( const char *pText );

	// Gets the items matching the string and sorts the list alphabetically.
	virtual void GetItemsMatchingString( const char *pStringToMatch, CUtlVector<CString> &matchingItems );
	static int SortFn( const CString *pItem1, const CString *pItem2 );

	virtual LRESULT DefWindowProc(
		UINT message,
		WPARAM wParam,
		LPARAM lParam );

	// Overrides for owner draw.
	virtual void MeasureItem(LPMEASUREITEMSTRUCT pStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

private:

	void InternalSetEditControlFont( HFONT hFont, const char *pEditText, DWORD sel );
	void CreateFonts();
	bool InternalSelectItemByName( const char *pName );


private:
	
	CUtlVector<CString> m_Suggestions;
	HFONT m_hEditControlFont;

	CFont m_NormalFont;

	CFilteredComboBox::ICallbacks *m_pCallbacks;
	bool m_bWasEditing;
	DWORD m_dwTextColor;
	
	bool m_bOnlyProvideSuggestions;
	bool m_bInEnterKeyPressedHandler;
	
	HFONT m_hQueuedFont;
	bool m_bInSelChange;
	
	// We go back here if they type text that we can't give a suggestion on and press enter (or lose focus).
	CString m_LastTextChangedValue;
};


#endif // FILTERED_COMBO_BOX_H


