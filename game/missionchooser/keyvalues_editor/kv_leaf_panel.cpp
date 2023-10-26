#include "kv_leaf_panel.h"
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include "KeyValues.h"
#include "kv_editor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( CKV_Leaf_Panel )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Leaf_Panel::CKV_Leaf_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pLabel = new vgui::Label( this, "Label", "" );
	m_pLabel->SetMouseInputEnabled( false );
	m_pTextEntry = new vgui::TextEntry( this, "TextEntry" );
	m_pTextEntry->SetAutoLocalize( false );
	m_pDeleteButton = new vgui::Button( this, "DeleteButton", "X", this, "Delete" );
}

void CKV_Leaf_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pLabel->SetBounds( 0, 0, 100, 20 );
	m_pLabel->SizeToContents();
	m_pLabel->InvalidateLayout( true );
	int iLabelSpace = MAX( m_pLabel->GetWide() + 5, 100 );

	int iExtraHeight = 0;
	int iTextEntryX = iLabelSpace;

	if ( m_pFileSpecNode && m_pFileSpecNode->FindKey( "TextEntryNewLine" ) )
	{
		iExtraHeight = m_pLabel->GetTall();
		iTextEntryX = 0;
	}

	if ( m_pFileSpecNode && m_pFileSpecNode->FindKey( "Tall" ) )
	{
		m_pTextEntry->SetBounds( iTextEntryX, iExtraHeight, 390 - iTextEntryX, m_pFileSpecNode->GetInt( "Tall" ) );
	}
	else
	{
		m_pTextEntry->SetBounds( iTextEntryX, iExtraHeight, 390 - iTextEntryX, 20 );
	}

	SetSize( 420, m_pTextEntry->GetTall() + iExtraHeight );

	int iDeleteWidth = 20;
	m_pDeleteButton->SetBounds( GetWide() - (iDeleteWidth + 5), 2, iDeleteWidth, 16 );
}

void CKV_Leaf_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{	
	BaseClass::ApplySchemeSettings( pScheme );
	m_pLabel->SetPaintBackgroundEnabled( false );
	SetPaintBackgroundEnabled( false );

	m_pDeleteButton->SetVisible( m_bAllowDeletion );
}

void CKV_Leaf_Panel::UpdatePanel()
{
	if ( m_pFileSpecNode )
	{
		m_pDeleteButton->SetVisible( !!m_pFileSpecNode->FindKey( "Deletable" ) );
	}
	if ( !m_pKey )
	{
		m_pLabel->SetText( "INVALID KEY" );
		m_pTextEntry->SetText( "INVALID KEY" );
		return;
	}
	if ( m_pFileSpecNode && m_pFileSpecNode->FindKey( "FriendlyName" ) )
	{
		m_pLabel->SetText( m_pFileSpecNode->GetString( "FriendlyName" ) );
	}
	else
	{
		m_pLabel->SetText( m_pKey->GetName() );
	}
	m_pTextEntry->SetText( m_pKey->GetString() );
	if ( m_pFileSpecNode->GetInt( "ReadOnly", 0 ) == 1 )
	{
		m_pTextEntry->SetEditable( false );
		m_pTextEntry->SetEnabled( false );
	}
}

// update our key when the text entry changes
void CKV_Leaf_Panel::TextEntryChanged( vgui::Panel* pTextEntry )
{
	if ( pTextEntry != m_pTextEntry )
		return;

	if ( !m_pKey )
	{
		return;
	}
	char buf[ 2048 ];
	m_pTextEntry->GetText( buf, 2048 );

	m_pKey->SetStringValue( buf );

	PostActionSignal( new KeyValues( "command", "command", "KeyValuesChanged" ) );
}

void CKV_Leaf_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Delete" ) && m_pKeyParent && m_bAllowDeletion )
	{
		m_pKeyParent->RemoveSubKey( m_pKey );
		m_pKey->deleteThis();
		m_pEditor->OnKeyDeleted();
		return;
	}
	BaseClass::OnCommand( command );
}