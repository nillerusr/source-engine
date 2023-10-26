#include "kv_node_panel.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include "kv_editor.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( CKV_Node_Panel )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Node_Panel::CKV_Node_Panel( Panel *parent, const char *name ) : 
BaseClass( parent, name ),
m_iLabelInsetX( 2 ),
m_iLabelInsetY( 2 )
{
	m_pLabel = new vgui::Label( this, "Label", "" );
	m_pLabel->SetMouseInputEnabled( false );

	m_pDeleteButton = new vgui::Button( this, "DeleteButton", "X", this, "Delete" );
	SetKeyBoardInputEnabled( false );

	m_iAutoPositionStartY = 30;
}

void CKV_Node_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_pFileSpecNode )
	{
		m_iLabelInsetX = m_pFileSpecNode->GetInt( "LabelInsetX", m_iLabelInsetX );
		m_iLabelInsetY = m_pFileSpecNode->GetInt( "LabelInsetY", m_iLabelInsetY );
	}

	m_pLabel->SetBounds( m_iLabelInsetX * 2, m_iLabelInsetY, 300, 16 );

	int iDeleteWidth = 20;
	m_pDeleteButton->SetBounds( GetWide() - (iDeleteWidth + 5), m_iLabelInsetY, iDeleteWidth, 16 );

	int wide = GetWide();
	int cursor_y = GetTall();

	cursor_y -= m_iSpacing;
	for ( int i = 0; i < m_pAddButtons.Count(); i++ )
	{
		vgui::Button *pPanel = m_pAddButtons[i];

		if ( pPanel )
		{
			pPanel->SetPos( m_iBorder, cursor_y );
			pPanel->SetSize( 300, 25 );
			pPanel->InvalidateLayout( true );
			int x, y;
			pPanel->GetPos( x, y );
			x += pPanel->GetWide();
			y += pPanel->GetTall();
			cursor_y += pPanel->GetTall() + m_iSpacing;
			wide = MAX( wide, x );
		}
	}

	cursor_y += m_iSpacing * 2;
	SetSize( wide, cursor_y );
}

void CKV_Node_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{	
	BaseClass::ApplySchemeSettings( pScheme );
	m_pLabel->SetPaintBackgroundEnabled( false );

	m_pDeleteButton->SetVisible( m_bAllowDeletion );
}

void CKV_Node_Panel::UpdatePanel()
{
	if ( m_pFileSpecNode )
	{
		m_pDeleteButton->SetVisible( !m_pFileSpecNode->FindKey( "Autocreate" ) );
	}
	// clear out add buttons
	for ( int i = 0; i < m_pAddButtons.Count(); i++ )
	{
		m_pAddButtons[i]->SetVisible( false );
		m_pAddButtons[i]->MarkForDeletion();
	}
	m_pAddButtons.Purge();
	
	if ( !m_pKey )
	{
		m_pLabel->SetText( "INVALID KEY" );
		return;
	}
	if ( m_pFileSpecNode && m_pFileSpecNode->FindKey( "FriendlyName" ) )
	{
		m_pLabel->SetText( m_pFileSpecNode->GetString( "FriendlyName" ) );
		m_pLabel->SetVisible( Q_strlen( m_pFileSpecNode->GetString( "FriendlyName" ) ) > 0 );
	}
	else
	{
		m_pLabel->SetText( m_pKey->GetName() );
	}

	// add add buttons
	if ( m_pFileSpecNode )
	{
		// go through all _Node or _Leaf children of our filespec node
		for ( KeyValues *pSubKey = m_pFileSpecNode->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
		{			
			if ( Q_stricmp( pSubKey->GetName(), "_Node" ) && Q_stricmp( pSubKey->GetName(), "_Leaf" ) )
				continue;

			// If it's unique, then see if we already have one in our m_pKey.
			bool bMakeButton = true;
			if ( pSubKey->FindKey( "Unique" ) )
			{
				for ( KeyValues *pKeyChild = m_pKey->GetFirstSubKey(); pKeyChild; pKeyChild = pKeyChild->GetNextKey() )
				{
					if ( !Q_stricmp( pKeyChild->GetName(), pSubKey->GetString( "Name" ) ) )
					{
						bMakeButton = false;
						break;
					}
				}
			}
			
			if ( bMakeButton )
			{
				char buffer[256];
				if ( m_pFileSpecNode && pSubKey->FindKey( "FriendlyAddButtonText" ) )
				{
					Q_snprintf( buffer, sizeof( buffer ), "%s", pSubKey->GetString( "FriendlyAddButtonText", "ADD BUTTON ERROR" ) );
				}
				else
				{
					Q_snprintf( buffer, sizeof( buffer ), "Add %s", pSubKey->GetString( "FriendlyName", pSubKey->GetString( "Name", "Unknown key" ) ) );
				}
				vgui::Button *pAdd = new vgui::Button( this, "AddButton", buffer, this );
				pAdd->SetCommand( new KeyValues( "AddButtonPressed", "szKeyName", pSubKey->GetString( "Name", "Unknown key" ) ) );
				m_pAddButtons.AddToTail( pAdd );
			}
		}
	}
}

void CKV_Node_Panel::OnCommand( const char *command )
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

void CKV_Node_Panel::OnAddButtonPressed( const char *szKeyName )
{
	// Add new subkey of name matching button
	m_pEditor->AddToKey( m_pFileSpecNode, m_pKey, szKeyName );
}