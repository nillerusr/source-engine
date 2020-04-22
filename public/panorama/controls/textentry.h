//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTENTRY_H
#define PANORAMA_TEXTENTRY_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "panorama/text/iuitextlayout.h"
#include "panorama/textinput/textinput.h"
#include "panorama/textinput/textinput_settings.h"
#if !defined(SOURCE2_PANORAMA)
#include "../common/globals.h"
#include "../common/reliabletimer.h"
#include "../common/framefunction.h"
#endif
#include "panorama/uischeduleddel.h"
#include "imesystem/imeuiinterface.h"

namespace panorama
{

class CLabel;
class CTextEntryAutocomplete;

#if defined( SOURCE2_PANORAMA )
class CTextEntryIMEControls;
#endif // defined( SOURCE2_PANORAMA )

DECLARE_PANEL_EVENT1( TextEntrySubmit, const char * );		// always raised when user is done submitting text
DECLARE_PANEL_EVENT0( TextEntryChanged );					// call CTextEntry::RaiseChangeEvents() to enable
DECLARE_PANEL_EVENT0( TextEntryShowTextInputHandler );
DECLARE_PANEL_EVENT0( TextEntryHideTextInputHandler );
DECLARE_PANEL_EVENT0( TextEntryInsertFromClipboard );
DECLARE_PANEL_EVENT0( TextEntryCopyToClipboard );
DECLARE_PANEL_EVENT0( TextEntryCutToClipboard );

//-----------------------------------------------------------------------------
// Purpose: text entry
//-----------------------------------------------------------------------------
class CTextEntry : public CPanel2D, public ITextInputControl
#if defined( SOURCE2_PANORAMA )
	, public IIMEUITextField
#endif // defined( SOURCE2_PANORAMA )
{
	DECLARE_PANEL2D( CTextEntry, CPanel2D );

public:
	CTextEntry( CPanel2D *parent, const char * pchPanelID );
	virtual ~CTextEntry();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	void SetUndoHistoryEnabled( bool bEnabled );
	void ClearUndoHistory();

	virtual bool OnKeyDown( const KeyData_t &code );
	virtual bool OnKeyTyped( const KeyData_t &unichar );
	virtual bool OnKeyUp( const KeyData_t & code ) { return BaseClass::OnKeyUp( code ); }
	virtual void Paint();
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnInitializedFromLayout();
	virtual void OnStylesChanged();

	virtual void OnMouseMove( float flMouseX, float flMouseY );
	virtual bool OnMouseButtonDown( const MouseData_t &code );
	virtual bool OnMouseButtonUp( const MouseData_t &code );
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code );
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code );

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	virtual bool BRequiresContentClipLayer() { return m_bMayDrawOutsideBounds; }

	void SetMode( ETextInputMode_t mode );
	ETextInputMode_t GetMode() { return m_modeInput; }

	void SetText( const char *pchValue );
	const char *PchGetText() const;
	const wchar_t *PwchGetText() const;
	void SetPlaceholderText( const char *pchValue );
	const char *PchGetPlaceholderText() const;
	void SetMaxChars( uint unMaxChars );
	uint GetCharCount() const { return m_vecWCharData.Count() - 1; }	// m_vecWCharData is terminated, so remove extra char count
	uint GetMaxCharCount() const { return m_unMaxChars; }
	int32 GetCursorOffset() const { return m_nCursorOffset; }
	void SetCursorOffset( int32 nCursoroffset );
	bool BSupportsImmediateTextReturn() { return true; }
	void RequestControlString() { Assert( false ); } // no op, we already have the string

	// Insert clipboard contents into text entry
	void InsertFromClipboard();

	// Cut selected text to clipboard
	void CutToClipboard();

	// Copy selected text to clipboard
	void CopyToClipboard();

	bool OnInsertFromClipboard( const CPanelPtr< IUIPanel > &pPanel );
	bool OnCutToClipboard( const CPanelPtr< IUIPanel > &pPanel );
	bool OnCopyToClipboard( const CPanelPtr< IUIPanel > &pPanel );

	void SetAlwaysRenderCaret( bool bAlwaysRenderCaret );

	// Delete currently selected text 
	void DeleteSelection( bool bDontPushUndoHistory );

	// Clear selection region, leaving nothing selected
	void ClearSelection();

	// select all the text in the control
	void SelectAll();

	// Lock the selection in place; it can only be unlocked or deleted
	void LockSelection( bool bLockSelection ) { m_bSelectionLocked = bLockSelection; }

	// Insert characters at cursor
	void InsertCharacterAtCursor( const wchar_t &unichar );
	void InsertCharactersAtCursor( const wchar_t *pwch, size_t cwch );

	bool BIsValidCharacter( const wchar_t wch );

	void RaiseChangeEvents( bool bEnable ) { m_bRaiseChangeEvents = bEnable; }

	virtual EMouseCursors GetMouseCursor();

	// ITextInputControl helpers
	virtual CPanel2D *GetAssociatedPanel() { return this; }

	panorama::CTextInputHandler *GetTextInputHandler() { return m_pTextInputHandler.Get(); }
	void SetTextInputHandler( panorama::CTextInputHandler *pTextInputHandler );

	virtual bool BRequiresFocus() OVERRIDE { return true; }

	// Autocomplete
	void ClearAutocomplete( void );
	void AddAutocomplete( const char *pszOption );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName );
#endif
	int32 GetSelectionStart() { return m_nSelectionStartIndex; }
	int32 GetSelectionEnd() { return m_nSelectionEndIndex; }

	// Interface for IIMEUITextField
#if defined( SOURCE2_PANORAMA )
	virtual void IME_SetLoggingChannel( LoggingChannelID_t loggingChannel ) OVERRIDE;
	virtual bool IME_IsEnabled() OVERRIDE;
	virtual IMEUIObjectType IME_GetObjectType() OVERRIDE;
	virtual wchar_t *IME_GetCompositionString() OVERRIDE;
	virtual void IME_CreateCompositionString() OVERRIDE;
	virtual void IME_ClearCompositionString() OVERRIDE;
	virtual void IME_CommitCompositionString( const wchar_t *pString ) OVERRIDE;
	virtual void IME_SetCompositionStringText( const wchar_t *pString ) OVERRIDE;
	virtual void IME_SetCompositionStringPosition( uint32 nPos ) OVERRIDE;
	virtual void IME_SetCursorInCompositionString( uint32 nPos ) OVERRIDE;
	virtual uint32 IME_GetCaretIndex() OVERRIDE;
	virtual uint32 IME_GetBeginIndex() OVERRIDE;
	virtual uint32 IME_GetEndIndex() OVERRIDE;
	virtual void IME_ReplaceCharacters( const wchar_t *pString, uint32 nStart, uint32 nEnd ) OVERRIDE;
	virtual void IME_SetSelection( uint32 nStart, uint32 nEnd ) OVERRIDE;
	virtual void IME_SetWideCursor( bool bWide ) OVERRIDE;
	virtual void IME_HighlightCompositionStringText( uint32 nPos, uint32 nLen, IMETextHighlightStyle highlightStyle ) OVERRIDE;
	virtual void IME_DeleteSelection() OVERRIDE;
	virtual	void IME_RemoveInputWindow() OVERRIDE;
	virtual	void IME_DisplayInputWindow( const wchar_t *pReadingString, const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_RepositionInputWindow( const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_CreateList( int nPageSize, int nListStartsAt1 ) OVERRIDE;
	virtual	void IME_RemoveList() OVERRIDE;
	virtual	void IME_ClearList() OVERRIDE;
	virtual	void IME_ShowList( bool bShow ) OVERRIDE;
	virtual	void IME_RepositionCandidateList( const IMERectF *pPosition ) OVERRIDE;
	virtual	void IME_SelectItemInList( int32 nItemToSelect ) OVERRIDE;
	virtual	void IME_AddToList( const wchar_t *pCandidateString ) OVERRIDE;
#endif // defined( SOURCE2_PANORAMA )

protected:
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions );
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

private:
	bool OnTextEntryShowTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool OnTextEntryHideTextInputHandler( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool EventActivated ( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool OnTextEntryScrollToCursor( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool EventInputFocusTopLevelChanged( CPanelPtr< CPanel2D > ptrPanel );
	bool HandleTextInputFinished( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, bool bFinished, char const *pchText );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );

	IUITextLayout *CreateTextLayout( float flWidth, float flHeight );
	void RemoveCharacter( int32 offset );
	void RaiseTextChangedEvent();

	void UpdateSelectionToInclude( int32 unPreviousCursor, int32 unNewCursorPos );

	void Undo();
	void Redo();
	void PushUndoStack();
	void PushRedoStack();

	void UpdateCapsLockWarning();
	void OnScheduledCapsLockCheck();

	void MoveCaretToEnd( bool bIsShiftHeld );

	const wchar_t *PwchGetTextDisplay(); // honors password-entry mode

	bool m_bUndoHistoryEnabled;

	bool m_bContentSizeDirty;				// content size is dirty - text has changed

	float m_flMaxWidthLastContentSize;
	float m_flMaxHeightLastContentSize;

	bool m_bCaretPositionDirty;
	bool m_bAlwaysRenderCaret;

	bool m_bMayDrawOutsideBounds;

	bool m_bShowTextInputHandlerOnLeftMouseUp;

	bool m_bSelectionLocked;
	bool m_bMultiline;					// used to determine whether to swallow multiline-only characters (\n, etc.)

	Vector2D m_LastMousePos;
	CUtlVector<wchar_t> m_vecWCharData;
	CUtlVector<wchar_t> m_vecWCharDataPasswordDisplay;
	mutable CUtlString m_UTF8String;
	mutable bool m_bUTF8StringInvalid;
	uint32 m_unMaxChars;

	int32 m_nCursorOffset;
	Vector2D m_CaretCoords;
	float m_flCaretHeight;

	bool m_bLeftMouseIsDown;
	bool m_bSelectionRectDirty;
	int32 m_nSelectionStartIndex;
	int32 m_nSelectionEndIndex;

	bool m_bScrollableSizeDirty;
	float m_flLastFinalWidthToScrollable;
	float m_flLastFinalHeightToScrollable;

	// Translation of text in single line entries to scroll to show where the cursor is
	float m_flTextXTranslate;
   
	CPanelPtr< CTextInputHandler > m_pTextInputHandler; 

	CUtlVector<IUITextLayout::HitTestRegionRect_t> m_vecSelectionRects;
	panorama::CTextInputHandlerSettings m_settingsTextInput;

	CUtlVector< wchar_t * > m_vecUndoStack;
	CUtlVector< wchar_t * > m_vecRedoStack;

	double m_flFocusTime;

	// only raise text changed events when requested, as we have to convert every character to UTF8 from unicode
	bool m_bRaiseChangeEvents;

	ETextInputMode_t m_modeInput;

	bool m_bDisplayInput;
	bool m_bWarnOnCapsLock;
	panorama::CUIScheduledDel m_scheduledCapsLockCheck;

	CLabel *m_pPlaceholderText;
	
	CPanelPtr< CTextEntryAutocomplete > m_pAutocompleteMenu;

#if defined( SOURCE2_PANORAMA )
	// IME State
	bool m_bIMEWideCursor;
	CUtlVector< wchar_t > m_IMECompositionString;
	int32 m_nIMECompositionCursor;
	int32 m_nIMEStartingCursorInsertionOffset;
	int32 m_nIMEEndingCursorInsertionOffset;
	bool m_bIMERejectBackspace;

	CPanelPtr< CTextEntryIMEControls > m_pIMEControls;
	LoggingChannelID_t m_IMELoggingChannel;
#endif // defined( SOURCE2_PANORAMA )
};

class CTextEntryAutocomplete : public CPanel2D
{
	DECLARE_PANEL2D( CTextEntryAutocomplete, CPanel2D );

public:
	CTextEntryAutocomplete( CTextEntry *pParent, const char *pchPanelID );
	virtual ~CTextEntryAutocomplete();

	void DeleteSelf( bool bSetFocusToTarget = true );

	void AddOption( const char *pszOption );

private:
	// forward keys, arrows back to my parent
	virtual bool OnKeyDown( const KeyData_t &code );
	virtual bool OnKeyUp( const KeyData_t & code );
	virtual bool OnKeyTyped( const KeyData_t &unichar );

	void PositionNearParent();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	// events
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &pPanel );

	void SuggestionSelected( CLabel *pLabel );

	CPanelPtr< CTextEntry > m_pTextEntryParent;
	bool m_bClosing;
};

#if defined( SOURCE2_PANORAMA )
class CTextEntryIMEControls : public CPanel2D
{
	DECLARE_PANEL2D( CTextEntryIMEControls, CPanel2D );

public:
	CTextEntryIMEControls( CTextEntry *pParent, const char *pchPanelID );
	virtual ~CTextEntryIMEControls();

	void ClearCandidateList();
	void CreateCandidateList( int nPageSize, int nListStartsAt1 );
	void AddCandidate( const wchar_t *pszCandidateString );
	void SetSelectedCandidate( int nItemToSelect );
	void ShowCandidateList( bool bShow );

	void SetReadingString( const wchar_t *pReadingString );

private:
	void PositionNearParent();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	CPanelPtr< CTextEntry > m_pTextEntryParent;

	CPanelPtr< CLabel > m_pReadingStringLabel;
	CPanelPtr< CPanel2D > m_pCandidateList;

	int m_nCandidateListPageSize;
	int m_nCandidateListSelectedIndex;

	bool m_bShowCandidateList;
};
#endif // defined( SOURCE2_PANORAMA )

} // namespace panorama

#endif // PANORAMA_TEXTENTRY_H
