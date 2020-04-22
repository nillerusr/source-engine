//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//===========================================================================//
#include "gameeventeditpanel.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "iregistry.h"
#include "vgui/ivgui.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/radiobutton.h"
#include "vgui_controls/messagebox.h"
#include "vgui_controls/scrollbar.h"
#include "vgui_controls/scrollableeditablepanel.h"
#include "datamodel/dmelement.h"
#include "matsys_controls/picker.h"
#include "vgui_controls/fileopendialog.h"
#include "filesystem.h"
#include "tier2/fileutils.h"
#include "igameevents.h"
#include "toolutils/enginetools_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

char *VarArgs( PRINTF_FORMAT_STRING const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	Q_vsnprintf (string, sizeof( string ), format,argptr);
	va_end (argptr);

	return string;	
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameEventEditPanel::CGameEventEditPanel( CGameEventEditDoc *pDoc, vgui::Panel* pParent )
: BaseClass( pParent, "GameEventEditPanel" ), m_pDoc( pDoc )
{
	SetPaintBackgroundEnabled( true );
	SetKeyBoardInputEnabled( true );

	m_pEvents = new KeyValues( "events" );

	m_EventFiles.RemoveAll();
	m_EventFileNames.RemoveAll();

	// load the game events
	LoadEventsFromFile( "resource/GameEvents.res" );
	LoadEventsFromFile( "resource/ModEvents.res" );

	m_pEventCombo = new vgui::ComboBox( this, "EventComboBox", 30, false );
	m_pEventCombo->SetNumberOfEditLines( 30 );

	KeyValues *subkey = m_pEvents->GetFirstSubKey();
	while ( subkey )
	{
		m_pEventCombo->AddItem( subkey->GetName(), subkey );
		subkey = subkey->GetNextKey();
	}

	if ( m_pEventCombo->GetItemCount() > 0 )
	{
		m_pEventCombo->ActivateItemByRow( 0 );
	}

	for ( int i=0;i<MAX_GAME_EVENT_PARAMS;i++ )
	{
		m_pParamLabels[i] = new vgui::Label( this, VarArgs( "ParamLabel%d", i+1 ), VarArgs( "event param %d:", i+1 ) );
		m_pParamLabels[i]->AddActionSignalTarget( this );

		m_pParams[i] = new vgui::TextEntry( this, VarArgs( "Param%d", i+1 ) );
		m_pParams[i]->AddActionSignalTarget( this );
	}

	m_pSendEventButton = new vgui::Button( this, "SendEvent", "", this, "SendEvent" );

	m_pFilterBox = new vgui::TextEntry( this, "FilterBox" );

	LoadControlSettings( "resource/gameeventeditpanel.res" );
}

CGameEventEditPanel::~CGameEventEditPanel()
{
	m_pEvents->deleteThis();
}

void CGameEventEditPanel::OnTextChanged( KeyValues *params )
{
	Panel *panel = reinterpret_cast<vgui::Panel *>( params->GetPtr("panel") );

	if ( panel == m_pFilterBox )
	{
		// repopulate the list based on the filter substr
		char filter[128];
		m_pFilterBox->GetText( filter, sizeof(filter) );
		int len = Q_strlen(filter);

		m_pEventCombo->RemoveAll();

		KeyValues *subkey = m_pEvents->GetFirstSubKey();
		while ( subkey )
		{
			if ( len == 0 || Q_strstr( subkey->GetName(), filter ) )
			{
				m_pEventCombo->AddItem( subkey->GetName(), subkey );
			}

			subkey = subkey->GetNextKey();
		}

		if ( m_pEventCombo->GetItemCount() > 0 )
		{
			m_pEventCombo->ActivateItemByRow( 0 );
		}
	}

	if ( panel == m_pEventCombo )
	{
		Msg( "%s", params->GetName() );

		KeyValues *kv = m_pEventCombo->GetActiveItemUserData();

		int i = 0;

		if ( kv )
		{
			KeyValues *subkey = kv->GetFirstSubKey();
			while ( subkey && i < MAX_GAME_EVENT_PARAMS )
			{
				Msg( subkey->GetName() );

				char buf[128];
				Q_snprintf( buf, sizeof(buf), "%s (%s)", subkey->GetName(), subkey->GetString() );

				m_pParamLabels[i]->SetText( buf );
				m_pParamLabels[i]->SetVisible( true );

				const char *type = subkey->GetString();

				if ( !Q_strcmp( type, "string" ) )
				{
					m_pParams[i]->SetAllowNumericInputOnly( false );
				}
				else
				{
					m_pParams[i]->SetAllowNumericInputOnly( true );
				}
				m_pParams[i]->SetText( "" );
				m_pParams[i]->SetVisible( true );			

				subkey = subkey->GetNextKey();
				i++;
			}

			while( i < MAX_GAME_EVENT_PARAMS )
			{
				m_pParamLabels[i]->SetVisible( false );
				m_pParams[i]->SetVisible( false );	
				i++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CGameEventEditPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "SendEvent" ) )
	{
		KeyValues *pData = m_pEventCombo->GetActiveItemUserData();

		if ( pData )
		{
			const char *pEventName = pData->GetName();

			IGameEvent *event = gameeventmanager->CreateEvent( pEventName );
			if ( event )
			{
				KeyValues *subkey = pData->GetFirstSubKey();

				int i = 0;

				while( subkey  && i < MAX_GAME_EVENT_PARAMS )
				{
					char text[128];
					m_pParams[i]->GetText( text, sizeof(text) );
					event->SetString( subkey->GetName(), text );
	
					subkey = subkey->GetNextKey();
					i++;
				}

				gameeventmanager->FireEvent( event );
			}
		}

		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CGameEventEditPanel::LoadEventsFromFile( const char *filename )
{
	if ( UTL_INVAL_SYMBOL == m_EventFiles.Find( filename ) )
	{
		CUtlSymbol id = m_EventFiles.AddString( filename );
		m_EventFileNames.AddToTail( id );
	}

	KeyValues * key = new KeyValues(filename);
	KeyValues::AutoDelete autodelete_key( key );

	if  ( !key->LoadFromFile( g_pFileSystem, filename, "GAME" ) )
		return;

	KeyValues *subkey = key->GetFirstSubKey();
	while ( subkey )
	{
		KeyValues *copy = subkey->MakeCopy();

		m_pEvents->AddSubKey( copy );

		subkey = subkey->GetNextKey();
	}
}