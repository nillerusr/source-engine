//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeTextEntry.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Menu.h"
#include "datamodel/dmelement.h"
#include "dme_controls/AttributeTextPanel.h"
#include "vgui/MouseCode.h"
#include "vgui/KeyCode.h"
#include "vgui/IInput.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/inotifyui.h"

using namespace vgui;

// ----------------------------------------------------------------------------
// CAttributeTextEntry

CAttributeTextEntry::CAttributeTextEntry( Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName ),
	m_bValueStored( false ),
	m_flOriginalValue( 0.0f )
{
	SetDragEnabled( true );
	SetDropEnabled( true, 0.5f );
	m_szOriginalText[ 0 ] = 0;
	AddActionSignalTarget( this );
}

void CAttributeTextEntry::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder(NULL);

	//HFont font = pScheme->GetFont( "DmePropertyVerySmall", IsProportional() );
	//SetFont(font);
}


//-----------------------------------------------------------------------------
// Returns the parent panel
//-----------------------------------------------------------------------------
inline CAttributeTextPanel *CAttributeTextEntry::GetParentAttributePanel()
{
	return static_cast< CAttributeTextPanel * >( GetParent() );
}


//-----------------------------------------------------------------------------
// Drag + drop
//-----------------------------------------------------------------------------
bool CAttributeTextEntry::GetDropContextMenu( Menu *menu, CUtlVector< KeyValues * >& msglist )
{
	menu->AddMenuItem( "Drop as Text", "#BxDropText", "droptext", this );
	return true;
}

bool CAttributeTextEntry::IsDroppable( CUtlVector< KeyValues * >& msglist )
{
	if ( !IsEnabled() )
		return false;

	if ( msglist.Count() != 1 )
		return false;

	KeyValues *msg = msglist[ 0 ];

	Panel *draggedPanel = ( Panel * )msg->GetPtr( "panel", NULL );
	if ( draggedPanel == GetParent() )
		return false;

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	if ( !pPanel )
		return false;

	// If a specific text type is specified, then filter if it doesn't match
	const char *pTextType = pPanel->GetTextType();
	if ( pTextType[0] )
	{
		const char *pMsgTextType = msg->GetString( "texttype" );
		if ( Q_stricmp( pTextType, pMsgTextType ) )
			return false;
	}

	DmAttributeType_t t = pPanel->GetAttributeType();
	switch ( t )
	{
	default:
		break;
	case AT_ELEMENT:
		{
			CDmElement *ptr = reinterpret_cast< CDmElement * >( g_pDataModel->GetElement( DmElementHandle_t( msg->GetInt( "root" ) ) ) );
			if ( ptr )
			{
				return true;
			}
			return false;
		}
		break;

	case AT_ELEMENT_ARRAY:
		return false;
	}

	return true;
}

void CAttributeTextEntry::OnPanelDropped( CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() != 1 )
		return;

	KeyValues *data = msglist[ 0 ];
	Panel *draggedPanel = ( Panel * )data->GetPtr( "panel", NULL );
	if ( draggedPanel == GetParent() )
		return;

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	if ( !pPanel )
		return;

	// If a specific text type is specified, then filter if it doesn't match
	const char *pTextType = pPanel->GetTextType();
	if ( pTextType[0] )
	{
		const char *pMsgTextType = data->GetString( "texttype" );
		if ( Q_stricmp( pTextType, pMsgTextType ) )
			return;
	}

	const char *cmd = data->GetString( "command" );
	if ( !Q_stricmp( cmd, "droptext" ) || !Q_stricmp( cmd, "default" ) )
	{
		DmAttributeType_t t = pPanel->GetAttributeType();
		switch ( t )
		{
		default:
			{
				pPanel->SetDirty( true );
				SetText( data->GetString( "text" ) );
				if ( pPanel->IsAutoApply() )
				{
					pPanel->Apply();
				}
			}
			break;

		case AT_ELEMENT:
			{
				CDmElement *ptr = reinterpret_cast< CDmElement * >( g_pDataModel->GetElement( DmElementHandle_t( data->GetInt( "root" ) ) ) );
				if ( !ptr )
				{
					break;
				}
				pPanel->SetDirty( true );
				SetText( data->GetString( "text" ) );
				if ( pPanel->IsAutoApply() )
				{
					pPanel->Apply();
				}
			}
			break;
		case AT_ELEMENT_ARRAY:
			Assert( 0 );
			break;
		}
	}

	StoreInitialValue( true );
}


//-----------------------------------------------------------------------------
// Enter causes changes to be applied
//-----------------------------------------------------------------------------
void CAttributeTextEntry::OnKeyCodeTyped(KeyCode code)
{
	bool bCtrl = (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL));

	switch ( code )
	{
	case KEY_ENTER:
		{
			CAttributeTextPanel *pPanel = GetParentAttributePanel();
			if ( !pPanel->IsAutoApply() )
			{
				pPanel->Apply();
				StoreInitialValue( true );
			}
			else
			{
				WriteValueToAttribute();
			}
		}
		break;

	// Override the base class undo feature, it behaves poorly when typing in data
	case KEY_Z:
		if ( bCtrl )
		{
			WriteInitialValueToAttribute( );
			break;
		}

		// NOTE: Fall through to default if it's not Ctrl-Z

	default:
		BaseClass::OnKeyCodeTyped(code);
		break;
	}
}


void CAttributeTextEntry::OnTextChanged( KeyValues *data )
{
	m_bValueStored = true;
}


//-----------------------------------------------------------------------------
// We'll only create an "undo" record if the values differ upon focus change
//-----------------------------------------------------------------------------
void CAttributeTextEntry::StoreInitialValue( bool bForce )
{
	// Already storing value???
	if ( m_bValueStored && !bForce )
		return;

	m_bValueStored = true;

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	Assert( pPanel );

	switch ( pPanel->GetAttributeType() )
	{
	case AT_FLOAT:
		m_flOriginalValue = pPanel->GetAttributeValue<float>( );
		break;
	case AT_INT:
		m_nOriginalValue = pPanel->GetAttributeValue<int>( );
		break;
	case AT_BOOL:
		m_bOriginalValue = pPanel->GetAttributeValue<bool>( );
		break;
	default:
		GetText( m_szOriginalText, sizeof( m_szOriginalText ) );
		break;
	}
}


//-----------------------------------------------------------------------------
// Performs undo
//-----------------------------------------------------------------------------
void CAttributeTextEntry::WriteInitialValueToAttribute( )
{
	// Already storing value???
	if ( !m_bValueStored )
		return;

	CDisableUndoScopeGuard guard;

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	Assert( pPanel );

	switch ( pPanel->GetAttributeType() )
	{
	case AT_FLOAT:
		pPanel->SetAttributeValue( m_flOriginalValue );
		break;
	case AT_INT:
		pPanel->SetAttributeValue( m_nOriginalValue );
		break;
	case AT_BOOL:
		pPanel->SetAttributeValue<bool>( m_bOriginalValue );
		break;
	default:
		pPanel->SetAttributeValueFromString( m_szOriginalText );
		break;
	}

	pPanel->SetDirty( false );
	pPanel->Refresh();
}


//-----------------------------------------------------------------------------
// We'll only create an "undo" record if the values differ upon focus change
//-----------------------------------------------------------------------------
void CAttributeTextEntry::OnSetFocus()
{
	BaseClass::OnSetFocus();
	StoreInitialValue();
}


//-----------------------------------------------------------------------------
// Called when focus is lost
//-----------------------------------------------------------------------------
template<class T> 
void CAttributeTextEntry::ApplyMouseWheel( T newValue, T originalValue )
{
	CAttributeTextPanel *pPanel = GetParentAttributePanel();

	// Kind of an evil hack, but "undo" copies the "old value" off for doing undo, and that value is the new value because
	//  we haven't been tracking undo while manipulating this.  So we'll turn off undo and set the value to the original value.

	// In effect, all of the wheeling will drop out and it'll look just like we started at the original value and ended up at the
	//  final value...
	{
		CDisableUndoScopeGuard guard;
		pPanel->SetAttributeValue( originalValue );
	}

	if ( pPanel->IsAutoApply() )
	{
		pPanel->Apply();
	}
	else
	{
		CElementTreeUndoScopeGuard guard( 0, pPanel->GetNotify(), "Set Attribute Value", "Set Attribute Value" );
		pPanel->SetAttributeValue( newValue );
	}
}


void CAttributeTextEntry::WriteValueToAttribute()
{
	if ( !m_bValueStored )
		return;

	m_bValueStored = false;

	char newText[ MAX_TEXT_LENGTH ];
	GetText( newText, sizeof( newText ) );

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	Assert( pPanel );
	switch (pPanel->GetAttributeType() )
	{
	case AT_FLOAT:
		ApplyMouseWheel( (float)atof(newText), m_flOriginalValue );
		break;
	case AT_INT:
		ApplyMouseWheel( atoi(newText), m_nOriginalValue );
		break;
	case AT_BOOL:
		ApplyMouseWheel( atoi(newText) != 0, m_bOriginalValue );
		break;
	default:
		if ( Q_strcmp( newText, m_szOriginalText ) )
		{
			pPanel->SetDirty( true );
			if ( pPanel->IsAutoApply() )
			{
				pPanel->Apply();
				StoreInitialValue( true );
			}
		}
		else
		{
			pPanel->SetDirty( false );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Called when focus is lost
//-----------------------------------------------------------------------------
void CAttributeTextEntry::OnKillFocus()
{
	BaseClass::OnKillFocus();
	WriteValueToAttribute();
	StoreInitialValue();
}

void CAttributeTextEntry::OnMouseWheeled( int delta )
{
	// Must have *keyboard* focus for it to work
	if ( !HasFocus() )
	{
		// Otherwise, let the base class scroll up + down
		BaseClass::OnMouseWheeled( delta );
		return;
	}

	CAttributeTextPanel *pPanel = GetParentAttributePanel();
	if ( pPanel->GetDirty() )
	{
		if ( pPanel->IsAutoApply() )
		{
			pPanel->Apply();
			StoreInitialValue( true );
		}
		else
		{
			// FIXME: Make this work for non-auto-apply panels
		}
	}

	switch ( pPanel->GetAttributeType() )
	{
	case AT_FLOAT:
		{
			float deltaFactor;
			if ( input()->IsKeyDown(KEY_LSHIFT) )
			{
				deltaFactor = ((float)delta) * 10.0f;
			}
			else if ( input()->IsKeyDown(KEY_LCONTROL) )
			{
				deltaFactor = ((float)delta) / 100.0;
			}
			else
			{
				deltaFactor = ((float)delta) / 10.0;
			}

			float val = pPanel->GetAttributeValue<float>() + deltaFactor;

			if ( input()->IsKeyDown(KEY_LALT) )
			{
				//val = clamp(val, 0.0, 1.0);
				val = (val > 1) ? 1 : ((val < 0) ? 0 : val);
			}

			{
				// Note, these calls to Set won't create Undo Records, 
				// since we'll check the value in SetFocus/KillFocus so that we
				// don't gum up the undo system with hundreds of records...
				CDisableUndoScopeGuard guard;
				pPanel->SetAttributeValue( val );
			}
		}
		break;

	case AT_INT:
		{
			if ( input()->IsKeyDown(KEY_LSHIFT) )
			{
				delta *= 10;
			}

			int val = pPanel->GetAttributeValue<int>() + delta;

			{
				// Note, these calls to Set won't create Undo Records, 
				// since we'll check the value in SetFocus/KillFocus so that we
				// don't gum up the undo system with hundreds of records...
				CDisableUndoScopeGuard guard;
				pPanel->SetAttributeValue( val );
			}
		}
		break;

	case AT_BOOL:
		{
			bool val = !pPanel->GetAttributeValue<bool>();

			{
				// Note, these calls to Set won't create Undo Records, 
				// since we'll check the value in SetFocus/KillFocus so that we
				// don't gum up the undo system with hundreds of records...
				CDisableUndoScopeGuard guard;
				pPanel->SetAttributeValue( val );
			}
		}
		break;

	default:
		return;
	}

	pPanel->Refresh();
	if ( pPanel->IsAutoApply() )
	{
		// NOTE: Don't call Apply since that generates an undo record
		CElementTreeNotifyScopeGuard notify( "CAttributeTextEntry::OnMouseWheeled", NOTIFY_CHANGE_ATTRIBUTE_VALUE | NOTIFY_SETDIRTYFLAG, pPanel->GetNotify() );
	}
	else
	{
		pPanel->SetDirty( true );
	}

	//SetDirty(true);
	//UpdateTime( m_flLastMouseTime );
	//UpdateZoom( -10.0f * delta );
	//UpdateTransform();
}

// ----------------------------------------------------------------------------
