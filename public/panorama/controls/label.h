//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_LABEL_H
#define PANORAMA_LABEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "panorama/localization/ilocalize.h"
#include "panorama/text/iuitextlayout.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( CopySelectedLabelText );

//-----------------------------------------------------------------------------
// Purpose: Label
//-----------------------------------------------------------------------------
class CLabel : public CPanel2D, public ILocalizationStringSizeResolver
{
	DECLARE_PANEL2D( CLabel, CPanel2D );

public:
	CLabel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CLabel();

	virtual void Paint();
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	enum ETextType
	{
		k_ETextTypePlain,
		k_ETextTypeUnlocalized,
		k_ETextTypeHTML
	};
	virtual void SetText( const char *pchValue, ETextType eTextType = k_ETextTypePlain );
	virtual void AppendText( const char *pchValue, ETextType eTextType = k_ETextTypePlain );
	virtual const char *PchGetText() const { return m_pLocText ? m_pLocText->String() : ""; }	
	
	bool OnLocalizationChanged( const CPanelPtr< IUIPanel > &pPanel, const ILocalizationString *pString ); // can't be virtual as it is an event catcher
	
	void SetMaxChars( EStringTruncationStyle eTruncationStyle, uint32 nMaxChars );
	void SetStyleForRange( int iStartIndex, int iEndIndex, CPanoramaSymbol symStyle );

	virtual bool BRequiresContentClipLayer() { return m_bMayDrawOutsideBounds; }
	virtual bool BAcceptsFocus() { return false; }

	virtual bool GetContextUIBounds( float *pflX, float *pflY, float *pflWidth, float *pflHeight ) OVERRIDE;

	bool BHasSelection() { return m_nSelectionEndIndex != -1; }
	void CopySelectionToClipboard();

	bool OnCopySelectedLabelText( const CPanelPtr< IUIPanel > &pPanel );

	// for cloning
	virtual bool IsClonable() { return AreChildrenClonable(); }
	virtual CPanel2D *Clone();

	// Get the count of anchor tags in this label, will be zero if not HTML
	uint32 GetHREFCount();

	// Get vector of all url targets in the label
	void GetHREFTargets( CUtlVector< CUtlString > &vecTargets );

	// enable or disable selection of text via mouse clicks
	void SetAllowTextSelection( bool bAllow ) { m_bAllowTextSelection = bAllow; }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
	void ValidateLinkVector( CValidator &validator, CUtlVector< CUtlString > *pvecLinks );
#endif

	// Allow us to recalc HTML style flags on scale factor changes
	virtual void OnUIScaleFactorChanged( float flScaleFactor ) OVERRIDE;

	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	enum EHTMLFormatFlag
	{
		k_EHTMLFormatTagNone = 0,
		k_EHTMLFormatTagAnchor = 1,
		k_EHTMLFormatTagBold = 1 << 1,
		k_EHTMLFormatTagItalics = 1 << 2,
		k_EHTMLFormatTagEmphasized = 1 << 3,
		k_EHTMLFormatTagStrong = 1 << 4,
		k_EHTMLFormatTagSpan = 1 << 5,
		k_EHTMLFormatTagHeader1 = 1 << 6,
		k_EHTMLFormatTagHeader2 = 1 << 7,
		k_EHTMLFormatTagFont = 1 << 8,
		k_EHTMLFormatTagPre = 1 << 9,
	};

protected:
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void OnStylesChanged();
	virtual void OnMouseMove( float flMouseX, float flMouseY );
	virtual bool OnMouseButtonDown( const MouseData_t &code );
	virtual bool OnMouseButtonUp( const MouseData_t &code );
	bool EventStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel );
	bool OnFindLongestStringForLocVariable( const CPanelPtr< IUIPanel > &pPanel );

	virtual void InitClonedPanel( CPanel2D *pPanel );

	virtual void SetTextFromJS(const char *pchValue) 
	{ 
		if( m_bParseAsHTML )
			SetText( pchValue, k_ETextTypeHTML );
		else
			SetText( pchValue, k_ETextTypeUnlocalized );
	}

	bool BParseAsHTML() const { return m_bParseAsHTML; }
	void SetParseAsHTML( bool bParseAsHTML ) { m_bParseAsHTML = bParseAsHTML; }

private:
	struct TextRangeFormat_t
	{
		static const Color k_colorUnspecified;

		TextRangeFormat_t()
		{
			m_iStartChar = -1;
			m_iEndChar = -1;
			m_unHTMLFormatFlags = 0;
			m_iHREF = -1;
			m_iMouseOver = -1;
			m_iMouseOut = -1;
			m_iContextMenu = -1;
			m_pStyle = NULL;
			m_unStyleFlags = 0;
			m_color = k_colorUnspecified;
			m_bChildOwner = false;
		}
		
		void CopyWithoutRecalcData( const TextRangeFormat_t &rhs );		

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

		int m_iStartChar;
		int m_iEndChar;

		uint m_unHTMLFormatFlags;
		CUtlVector< CPanoramaSymbol > m_vecClasses;
		int m_iHREF;					// index into m_vecParsedHREFs if clickable. Multiple ranges could have the same URL, so hold indexes instead of copy strings
		int m_iMouseOver;				// index into m_vecParsedMouseOvers if clickable. Multiple ranges could have the same URL, so hold indexes instead of copy strings
		int m_iMouseOut;				// index into m_vecParsedMouseOuts if clickable. Multiple ranges could have the same URL, so hold indexes instead of copy strings
		int m_iContextMenu;				// index into m_vecParsedContextMenus if clickable. Multiple ranges could have the same URL, so hold indexes instead of copy strings

		IUIPanelStyle *m_pStyle;
		uint m_unStyleFlags;

		Color m_color;

		CUtlString m_strChildID;
		bool m_bChildOwner;

		struct RangeFormatBox_t
		{
			Vector2D topLeft;
			Vector2D bottomRight;
		};

		CUtlVector< RangeFormatBox_t > m_vecBoundingBoxes;
	};

	void SetTextInternal( const char *pchValue, ETextType eTextType, bool bTrustedSource );
	void SetFromHTMLInternal( const char *pchText, bool bAppend, bool bTrusted );
	int ParseStringFromTag( const char *pchTag, const char *pchString, CUtlVector<CUtlString> **ppVecStrings );
	int ParseHREFFromTag( const char *pchTag );
	int ParseMouseOverFromTag( const char *pchTag );
	int ParseMouseOutFromTag( const char *pchTag );
	int ParseContextMenuFromTag( const char *pchTag );
	void UpdateTextRangeStyles();
	TextRangeFormat_t &AppendTextRangeFormat( int iStartChar, int iEndChar, uint unHTMLFormatFlags, const CUtlVector< CPanoramaSymbol > &vecStyles, int iHREF, int iMouseOver, int iMouseOut, int iContextMenu, const Color &color, const char *pszChildID, bool bChildOwner );
	void RemoveTextRangeFormats();
	bool BCoordsInTextRange( const TextRangeFormat_t &rangeFormat, float flX, float flY );
	int FindTextRangeAt( float flX, float flY );

	IUITextLayout *CreateTextLayout( float flWidth, float flHeight, bool bUseChildDesiredSize, const char *pchOptStringToUse = NULL );
	IUITextLayout *CreateCurrentLayoutTextLayout();
	virtual int ResolveStringLengthInPixels( const char *pchString );

	bool HandleAnchorTagEvent( CUtlVector< CUtlString > *pvecLinks, int iLinkIndex );

	static void CloneLinksVector( CUtlVector< CUtlString > *pSourceLinks, CUtlVector< CUtlString > **ppDestinationLinks );

	bool m_bContentSizeDirty;
	float m_flMaxWidthLastContentSize;
	float m_flMaxHeightLastContentSize;
	float m_flLastUIScaleFactor;

	bool m_bLeftMouseIsDown;
	Vector2D m_LastMousePos;
	bool m_bSelectionRectDirty;
	int32 m_nSelectionStartIndex;
	int32 m_nSelectionEndIndex;
	CUtlVector<IUITextLayout::HitTestRegionRect_t> m_vecSelectionRects;

	bool m_bMayDrawOutsideBounds;
	bool m_bAllowTextSelection;
	
	CMutableLocalizationString m_pLocText;
	CMutableLocalizationString m_pLocTextHTML; // the base string with html markup preserved
	uint32 m_nMaxChars;										// max chars to store in this label
	EStringTruncationStyle m_eStringTruncationStyle;		// how to truncate our text
	bool m_bParseAsHTML;									// if true, set text will be parsed as html
	CUtlVector< TextRangeFormat_t > m_vecTextRangeFormats;	// list of parsed text range formats
	CUtlVector< CUtlString > *m_pvecParsedHREFs;			// parsed href.. indexes are added to text range formats. Multiple ranges could have same URL.
	CUtlVector< CUtlString > *m_pvecParsedMouseOvers;		// parsed onmouseover.. indexes are added to text range formats. Multiple ranges could have same URL.
	CUtlVector< CUtlString > *m_pvecParsedMouseOuts;		// parsed onmouseout.. indexes are added to text range formats. Multiple ranges could have same URL.
	CUtlVector< CUtlString > *m_pvecParsedContextMenus;		// parsed oncontextmenu.. indexes are added to text range formats. Multiple ranges could have same URL.
	TextRangeFormat_t *m_pLastHoverRange;					// last text range the mouse was hovering over	
	TextRangeFormat_t *m_pMouseDownRange;					// index into m_vecTextRangeFormats

	static uint32 s_unNextInlineImageID;
};

} // namespace panorama


#endif // PANORAMA_LABEL_H
