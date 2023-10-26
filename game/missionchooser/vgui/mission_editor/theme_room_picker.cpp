#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelListPanel.h>
#include <KeyValues.h>

#include "theme_room_picker.h"
#include "TileSource/LevelTheme.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CThemeRoomPicker::CThemeRoomPicker( Panel *parent, const char *name, KeyValues *pKey, bool bPickRooms ) : BaseClass( parent, name, false )
{
	m_pKey = pKey;
	m_bPickRooms = bPickRooms;

	if ( !bPickRooms )
	{
		m_pSelectedTheme = CLevelTheme::FindTheme( pKey->GetString() );
		m_pCurrentThemeLabel->SetText( m_pSelectedTheme ? m_pSelectedTheme->m_szName : "Unknown theme" );

		LoadControlSettings( "tilegen/ThemePicker.res", "GAME" );
	}
	else
	{
		char szStartRoomTheme[128];
		char szStartRoom[MAX_PATH];
		if ( !CLevelTheme::SplitThemeAndRoom( pKey->GetString(), szStartRoomTheme, 128, szStartRoom, MAX_PATH ) )
		{
			m_pSelectedTheme = NULL;
			m_pCurrentThemeLabel->SetText( "Unknown theme" );
			return;
		}
		LoadControlSettings( "tilegen/RoomPicker.res", "GAME" );
	}
}

CThemeRoomPicker::~CThemeRoomPicker( void )
{
}

void CThemeRoomPicker::PopulateThemeList()
{	
	BaseClass::PopulateThemeList();
	
	if ( m_pSelectedTheme )
	{
		m_pCurrentThemeLabel->SetText( m_pSelectedTheme->m_szName );
	}
	else
	{
		m_pCurrentThemeLabel->SetText("None");
	}
}

void CThemeRoomPicker::ThemeClicked( CThemeDetails *pThemeDetails )
{
	m_pSelectedTheme = pThemeDetails->m_pTheme;
	m_pCurrentThemeLabel->SetText( m_pSelectedTheme->m_szName );

	if (m_pThemePanelList)
	{
		int iPanels = m_pThemePanelList->GetItemCount();
		for (int i=0;i<iPanels;i++)
		{
			vgui::Panel* pPanel = m_pThemePanelList->GetItemPanel(i);
			if (pPanel)
			{
				pPanel->InvalidateLayout();
				pPanel->OnThink();
			}
		}
	}

	if ( m_bPickRooms )
	{
		// TODO: fill room panel with rooms present in this theme
	}
}

bool CThemeRoomPicker::ShouldHighlight( CThemeDetails *pDetails )
{
	return pDetails && pDetails->m_pTheme == m_pSelectedTheme;
}

void CThemeRoomPicker::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void CThemeRoomPicker::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Okay" ) == 0 )
	{
		if ( m_pSelectedTheme && m_pKey )
		{
			m_pKey->SetStringValue( m_pSelectedTheme->m_szName );
			PostActionSignal( new KeyValues( "Command", "command", "Update" ) );
		}
		OnClose();
		return;
	}
	BaseClass::OnCommand( command );
}