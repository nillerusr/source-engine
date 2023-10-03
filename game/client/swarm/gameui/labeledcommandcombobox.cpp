//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "LabeledCommandComboBox.h"
#include "EngineInterface.h"
#include <KeyValues.h>
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CLabeledCommandComboBox::CLabeledCommandComboBox( vgui::Panel *parent, const char *panelName ) : vgui::ComboBox( parent, panelName, 6, false )
{
	AddActionSignalTarget(this);
	m_iCurrentSelection = -1;
	m_iStartSelection = -1;
}

CLabeledCommandComboBox::~CLabeledCommandComboBox( void )
{
}

void CLabeledCommandComboBox::DeleteAllItems()
{
	BaseClass::DeleteAllItems();
	m_Items.RemoveAll();
}

void CLabeledCommandComboBox::AddItem( char const *text, char const *engineCommand )
{
	int idx = m_Items.AddToTail();
	COMMANDITEM *item = &m_Items[ idx ];

	item->comboBoxID = BaseClass::AddItem( text, NULL );

	Q_strncpy( item->name, text, sizeof( item->name )  );

	if (text[0] == '#')
	{
		// need to localize the string
		wchar_t *localized = g_pVGuiLocalize->Find(text);
		if (localized)
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI(localized, item->name, sizeof(item->name));
		}
	}

	Q_strncpy( item->command, engineCommand, sizeof( item->command ) );
}

void CLabeledCommandComboBox::ActivateItem(int index)
{
	if ( index< m_Items.Count() )
	{
		int comboBoxID = m_Items[index].comboBoxID;
		BaseClass::ActivateItem(comboBoxID);
		m_iCurrentSelection = index;
	}
}

void CLabeledCommandComboBox::SetInitialItem(int index)
{
	if ( index< m_Items.Count() )
	{
		m_iStartSelection = index;
		int comboBoxID = m_Items[index].comboBoxID;
		ActivateItem(comboBoxID);
	}
}

void CLabeledCommandComboBox::OnTextChanged( char const *text )
{
	int i;
	for ( i = 0; i < m_Items.Count(); i++ )
	{
		COMMANDITEM *item = &m_Items[ i ];
		if ( !stricmp( item->name, text ) )
		{
		//	engine->pfnClientCmd( item->command );
			m_iCurrentSelection = i;
			break;
		}
	}

	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
//	PostMessage( GetParent()->GetVPanel(), new vgui::KeyValues( "TextChanged", "text", text ) );
}

const char *CLabeledCommandComboBox::GetActiveItemCommand()
{
	if (m_iCurrentSelection == -1)
		return NULL;

	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	return item->command;
}

void CLabeledCommandComboBox::ApplyChanges()
{
	if (m_iCurrentSelection == -1)
		return;
	if (m_Items.Count() < 1)
		return;

	Assert( m_iCurrentSelection < m_Items.Count() );
	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	engine->ClientCmd_Unrestricted( item->command );
	m_iStartSelection = m_iCurrentSelection;
}

bool CLabeledCommandComboBox::HasBeenModified()
{
	return m_iStartSelection != m_iCurrentSelection;
}

void CLabeledCommandComboBox::Reset()
{
	if (m_iStartSelection != -1)
	{
		ActivateItem(m_iStartSelection);
	}
}
