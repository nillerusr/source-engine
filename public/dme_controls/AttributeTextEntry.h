//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTETEXTENTRY_H
#define ATTRIBUTETEXTENTRY_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/TextEntry.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CAttributeTextPanel;
class KeyValues;

namespace vgui
{
	class IScheme;
	class Label;
	class Menu;
}


//-----------------------------------------------------------------------------
// CAttributeTextEntry
//-----------------------------------------------------------------------------
class CAttributeTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CAttributeTextEntry, vgui::TextEntry );

public:
	CAttributeTextEntry( Panel *parent, const char *panelName );
	virtual bool GetSelectedRange(int& cx0,int& cx1)
	{
		return BaseClass::GetSelectedRange( cx0, cx1 );
	}

protected:
	CAttributeTextPanel *GetParentAttributePanel();
	virtual void OnMouseWheeled( int delta );

	// We'll only create an "undo" record if the values differ upon focus change
	virtual void OnSetFocus();
	virtual void OnKillFocus();
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	virtual void OnPanelDropped( CUtlVector< KeyValues * >& data  );
	virtual bool GetDropContextMenu( vgui::Menu *menu, CUtlVector< KeyValues * >& msglist );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msglist );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

private:
	enum
	{
		MAX_TEXT_LENGTH = 1024
	};

    template<class T> void ApplyMouseWheel( T newValue, T originalValue );
	void StoreInitialValue( bool bForce = false );
	void WriteValueToAttribute();
	void WriteInitialValueToAttribute();

	bool				m_bValueStored;
	char				m_szOriginalText[ MAX_TEXT_LENGTH ];
	union
	{
		float			m_flOriginalValue;
		int				m_nOriginalValue;
		bool			m_bOriginalValue;
	};
};


// ----------------------------------------------------------------------------
#endif // ATTRIBUTETEXTENTRY_H
