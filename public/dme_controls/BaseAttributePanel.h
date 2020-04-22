//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: base class for all element attribute panels
//    An attribute panel is a one line widget that can be used by a list
//    or tree control.
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef BASEATTRIBUTEPANEL_H
#define BASEATTRIBUTEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattribute.h"
#include "vgui_controls/Panel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class IDmNotify;
class IElementPropertiesChoices;
struct AttributeWidgetInfo_t;
class CDmeEditorAttributeInfo;
class CDmeEditorTypeDictionary;

namespace vgui
{
	class Label;
}

using namespace vgui;


//-----------------------------------------------------------------------------
// CBaseAttributePanel
//-----------------------------------------------------------------------------
class CBaseAttributePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CBaseAttributePanel, vgui::Panel );

public:
	CBaseAttributePanel( vgui::Panel *pParent, const AttributeWidgetInfo_t &info );

	virtual void	PostConstructor();
	virtual void	PerformLayout();
	virtual void	SetFont( HFont font );
	virtual void	ApplySchemeSettings( IScheme *pScheme );
	virtual void	OnCreateDragData( KeyValues *msg );

	void			SetDirty( bool dirty );
	bool			GetDirty() const;
	bool			IsAutoApply() const;

	// Sets/gets the attribute value
	template< class T >
	void SetAttributeValue( const T& value );
	void SetAttributeValue( const char *pValue );

	template< class T >
	const T& GetAttributeValue( );

	// Helper to get/set the attribute value for elements
	CDmElement *GetAttributeValueElement();
	void SetAttributeValueElement( CDmElement *pElement );

	void SetAttributeValueFromString( const char *pString );
	const char *GetAttributeValueAsString( char *pBuf, int nLength );

	// Returns the attribute type to edit
	DmAttributeType_t	GetAttributeType() const;

	// Returns the editor info
	CDmeEditorTypeDictionary *GetEditorTypeDictionary();
	CDmeEditorAttributeInfo *GetEditorInfo();

	// Call this when the data changed
	IDmNotify			*GetNotify();

protected:
	enum
	{
		HIDETYPE	= 0x01,
		HIDEVALUE	= 0x02,
		READONLY	= 0x04,
		DIRTY		= 0x08,
		AUTOAPPLY	= 0x10,
	};

	// Inherited classes must implement this
	virtual	Panel		*GetDataPanel() = 0;
	virtual void		Apply() = 0;
	virtual void		Refresh() = 0;

	// Methods to get/set column size
	int					GetSizeForColumn( Panel *panel );
	void				SetColumnSize( Panel *panel, int width );

	// Returns the element being edited by the panel
	CDmElement			*GetPanelElement();
	const CDmElement	*GetPanelElement() const;

	// Returns the attribute name
	const char*			GetAttributeName() const;

	// Does the element have the attribute we're attempting to reference?
	bool				HasAttribute() const;

	// Returns the attribute array count
	int					GetAttributeArrayCount() const;

	// Are we editing an entry in an attribute array?
	bool				IsArrayEntry() const;

	// Is a particular flag set?
	bool				HasFlag( int flagMask ) const;

private:
	struct colinfo_t
	{
		Panel		*panel;
		int			width;
	};

	// Set a flag
	void				SetFlag( int flagMask, bool bOn );

	// Used to sort the column list
 	static bool			ColInfoLessFunc( const colinfo_t& lhs, const colinfo_t& rhs );

	// Initializes flags from the attribute editor info
	void				InitializeFlags( const AttributeWidgetInfo_t &info );

	// Called when the OK / Apply button is pressed.  Changed data should be written into document.
	MESSAGE_FUNC( OnApplyChanges, "ApplyChanges" );
	MESSAGE_FUNC( OnRefresh, "Refresh" );

protected:
	Label *m_pType;

private:
	CDmeHandle< CDmElement >				m_hObject;
	CDmeHandle< CDmeEditorAttributeInfo >	m_hEditorInfo;
	CDmeHandle< CDmeEditorTypeDictionary >	m_hEditorTypeDict;

	char m_szAttributeName[ 256 ];
	int m_nArrayIndex;
	DmAttributeType_t m_AttributeType;
	IDmNotify *m_pNotify;
	int m_nFlags;
	CUtlRBTree< colinfo_t, int > m_ColumnSize;
	HFont m_hFont;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline bool CBaseAttributePanel::HasFlag( int flagMask ) const
{
	return ( m_nFlags & flagMask ) ? true : false;
}

inline void CBaseAttributePanel::SetFlag( int flagMask, bool bOn )
{
	if ( bOn )
	{
		m_nFlags |= flagMask;
	}
	else
	{
		m_nFlags &= ~flagMask;
	}
}

inline bool CBaseAttributePanel::GetDirty() const
{
	return HasFlag( DIRTY );
}

inline bool CBaseAttributePanel::IsAutoApply() const
{
	return HasFlag( AUTOAPPLY );
}

inline DmAttributeType_t CBaseAttributePanel::GetAttributeType() const
{
	return m_AttributeType;
}

inline IDmNotify *CBaseAttributePanel::GetNotify()
{
	return m_pNotify;
}

inline const char* CBaseAttributePanel::GetAttributeName() const
{
	return m_szAttributeName;
}

inline bool CBaseAttributePanel::IsArrayEntry() const
{
	return ( m_nArrayIndex >= 0 );
}


template< class T > inline const T& GetArrayAttributeValue( CDmElement *pElement, const char *pAttribute, int nArrayIndex )
{
	const CDmrArray<T> array( pElement, pAttribute );
	return array[ nArrayIndex ];
}

template<> inline const DmElementHandle_t& GetArrayAttributeValue<DmElementHandle_t>( CDmElement *pElement, const char *pAttribute, int nArrayIndex )
{
	const CDmrElementArray<> array( pElement, pAttribute );
	return array.GetHandle( nArrayIndex );
}


template< class T > inline void SetArrayAttributeValue( CDmElement *pElement, const char *pAttribute, int nArrayIndex, const T& value )
{
	CDmrArray<T> array( pElement, pAttribute );
	array.Set( nArrayIndex, value );
}

template<> inline void SetArrayAttributeValue<DmElementHandle_t>( CDmElement *pElement, const char *pAttribute, int nArrayIndex, const DmElementHandle_t& value )
{
	CDmrElementArray<> array( pElement, pAttribute );
	array.SetHandle( nArrayIndex, value );
}


//-----------------------------------------------------------------------------
// Sets/gets the attribute value
//-----------------------------------------------------------------------------
template< class T >
void CBaseAttributePanel::SetAttributeValue( const T& value )
{
	if ( !IsArrayEntry() )
	{
		GetPanelElement()->SetValue( m_szAttributeName, value );
	}
	else
	{
		SetArrayAttributeValue<T>( GetPanelElement(), m_szAttributeName, m_nArrayIndex, value );
	}
}

inline void CBaseAttributePanel::SetAttributeValue( const char *pValue )
{
	if ( !IsArrayEntry() )
	{
		GetPanelElement()->SetValue( m_szAttributeName, pValue );
	}
	else
	{
		SetArrayAttributeValue<CUtlString>( GetPanelElement(), m_szAttributeName, m_nArrayIndex, pValue );
	}
}

template< class T >
const T& CBaseAttributePanel::GetAttributeValue( )
{
	if ( !IsArrayEntry() )
		return GetPanelElement()->GetValue<T>( m_szAttributeName );
	return GetArrayAttributeValue<T>( GetPanelElement(), m_szAttributeName, m_nArrayIndex );
}


#endif // BASEATTRIBUTEPANEL_H
