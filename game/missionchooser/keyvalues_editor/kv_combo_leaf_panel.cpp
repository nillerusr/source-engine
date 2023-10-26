#include "kv_combo_leaf_panel.h"
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include "KeyValues.h"
#include "kv_editor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( CKV_Combo_Leaf_Panel )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Combo_Leaf_Panel::CKV_Combo_Leaf_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_bIntegerChoice = false;
	m_pLabel = new vgui::Label( this, "Label", "" );
	m_pLabel->SetMouseInputEnabled( false );
	m_pCombo = new vgui::ComboBox( this, "Combo", 8, false );
	m_pCombo->SetAutoLocalize( false );
	m_pDeleteButton = new vgui::Button( this, "DeleteButton", "X", this, "Delete" );
}

void CKV_Combo_Leaf_Panel::PerformLayout()
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
		m_pCombo->SetBounds( iTextEntryX, iExtraHeight, 390 - iTextEntryX, m_pFileSpecNode->GetInt( "Tall" ) );
	}
	else
	{
		m_pCombo->SetBounds( iTextEntryX, iExtraHeight, 390 - iTextEntryX, 20 );
	}

	SetSize( 420, m_pCombo->GetTall() + iExtraHeight );

	int iDeleteWidth = 20;
	m_pDeleteButton->SetBounds( GetWide() - (iDeleteWidth + 5), 2, iDeleteWidth, 16 );
}

void CKV_Combo_Leaf_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{	
	BaseClass::ApplySchemeSettings( pScheme );
	m_pLabel->SetPaintBackgroundEnabled( false );
	SetPaintBackgroundEnabled( false );
}

void CKV_Combo_Leaf_Panel::UpdatePanel()
{
	if ( m_pFileSpecNode )
	{
		m_pDeleteButton->SetVisible( !!m_pFileSpecNode->FindKey( "Deletable" ) );
		m_bIntegerChoice = m_pFileSpecNode->GetBool( "IntegerChoice" );
		m_pCombo->RemoveAll();
		int iChoices = 0;
		const char *szCurrentChoice = m_pKey ? m_pKey->GetString() : "0";
		int iCurrentChoice = atoi( szCurrentChoice );
		for ( KeyValues *pKey = m_pFileSpecNode->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
		{
			if ( !Q_stricmp( pKey->GetName(), "Choice" ) )
			{
				const char* szValue = pKey->GetString();
				int iIndex = m_pCombo->AddItem( szValue, NULL );
				if ( m_bIntegerChoice && iCurrentChoice == iChoices )
				{
					m_pCombo->ActivateItem( iIndex );
				}
				else if ( !m_bIntegerChoice && !Q_stricmp( szValue, pKey->GetString() ) )
				{
					m_pCombo->ActivateItem( iIndex );
				}
				iChoices++;
			}
		}
	}
	if ( !m_pKey )
	{
		m_pLabel->SetText( "INVALID KEY" );
		m_pCombo->SetText( "INVALID KEY" );
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
	//m_pCombo->SetText( m_pKey->GetString() );
	if ( m_pFileSpecNode && m_pFileSpecNode->GetInt( "ReadOnly", 0 ) == 1 )
	{
		m_pCombo->SetEditable( false );
		m_pCombo->SetEnabled( false );
	}
}

// update our key when the text entry changes
void CKV_Combo_Leaf_Panel::OnTextChanged( vgui::Panel* pTextEntry )
{
	if ( pTextEntry != m_pCombo )
		return;

	if ( !m_pKey )
	{
		return;
	}

	if ( m_bIntegerChoice )
	{
		int iChoice = m_pCombo->GetActiveItem();

		m_pKey->SetInt( NULL, iChoice );
	}
	else
	{
		char buf[ 2048 ];
		m_pCombo->GetText( buf, 2048 );

		m_pKey->SetStringValue( buf );
	}	

	PostActionSignal( new KeyValues( "command", "command", "KeyValuesChanged" ) );
}

void CKV_Combo_Leaf_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Delete" ) && m_pKeyParent )
	{
		m_pKeyParent->RemoveSubKey( m_pKey );
		m_pKey->deleteThis();
		m_pEditor->OnKeyDeleted();
		return;
	}
	BaseClass::OnCommand( command );
}