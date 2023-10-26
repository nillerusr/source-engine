#include "kv_editor.h"
#include "kv_editor_base_panel.h"
#include "kv_fit_children_panel.h"
#include "ScrollingWindow.h"
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/MenuBar.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include "vgui/ISurface.h"
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//bool g_bAddedKVEditorLocalization = false;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Editor::CKV_Editor( Panel *parent, const char *name ) : BaseClass( parent, name )
{	
	//if ( !g_bAddedKVEditorLocalization )
	//{
		//g_pVGuiLocalize->AddFile( "resource/kv_editor_english.txt" );
		//g_bAddedKVEditorLocalization = true;
	//}

	m_pKeys = NULL;
	m_pFileSpec = NULL;
	m_bRequireFileSpec = true;
	m_bShowSiblings = true;
	
	m_pScrollingWindow = new CScrollingWindow( this, "ScrollingWindow" );	
	m_pContainer = new CKV_Fit_Children_Panel( this, "Container" );
	
	m_pScrollingWindow->SetChildPanel( m_pContainer );
	m_pScrollingWindow->InvalidateLayout( true );	

	m_szFileDirectory[0] = 0;
	m_szFileFilter[0] = 0;
	m_szFileFilterName[0] = 0;
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CKV_Editor::~CKV_Editor()
{
	
}

void CKV_Editor::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pScrollingWindow->SetBounds( 0, 0, GetWide(), GetTall() );
	//m_pContainer->SetPos( 30, 30 );
}

void CKV_Editor::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pContainer->SetBgColor( Color( 0, 0, 0, 255 ) );
}

CKV_Editor_Base_Panel* CKV_Editor::FindPanel( KeyValues *pKey )
{
	if ( !pKey )
		return NULL;

	for ( int i = 0; i < m_Panels.Count(); i++ )
	{
		if ( m_Panels[i]->GetKey() == pKey )
		{
			return m_Panels[i];
		}
	}
	return NULL;
}

void CKV_Editor::DeleteAllPanels()
{
	for ( int i = 0; i < m_Panels.Count(); i++ )
	{
		m_Panels[i]->SetVisible( false );
		m_Panels[i]->MarkForDeletion();
		CKV_Fit_Children_Panel *pFit = dynamic_cast< CKV_Fit_Children_Panel* >( m_Panels[i]->GetParent() );
		if ( pFit )
		{
			pFit->RemoveAutoPositionPanel( m_Panels[i] );
		}
	}
	m_Panels.Purge();
}

CKV_Editor_Base_Panel* CKV_Editor::CreatePanel( const char *szClassName, CKV_Editor_Base_Panel *pParent, KeyValues *pFileSpecNode, KeyValues *pKey )
{
	CKV_Editor_Base_Panel *pPanel = dynamic_cast<CKV_Editor_Base_Panel *>( CreateControlByName( szClassName ) );
	if ( pPanel )
	{
		pPanel->SetParent( pParent );
		pPanel->SetAllowDeletion( m_bAllowDeletion );
		pPanel->SetFileSpecNode( pFileSpecNode );
		pPanel->SetEditor( this );
		pPanel->SetName( pKey->GetName() );
		pPanel->SetKey( pKey );
		pPanel->SetKeyParent( FindParentForKey( pKey ) );
		pPanel->SetSortOrder( pFileSpecNode->GetFloat( "SortOrder" ) );
		pPanel->AddActionSignalTarget( this );
		CKV_Fit_Children_Panel *pFit = dynamic_cast< CKV_Fit_Children_Panel* >( pParent );
		if ( pFit )
		{
			pFit->AddAutoPositionPanel( pPanel );
		}
		m_Panels.AddToTail( pPanel );
	}
	else
	{
		Warning( "Failed to create panel %s\n", szClassName );
	}
	return pPanel;
}

// creates panels for target key, all its siblings and all its children
void CKV_Editor::UpdatePanels( KeyValues *pKV, CKV_Editor_Base_Panel *pParentPanel, bool bIncludeSiblings )
{
	Assert( pParentPanel );
	// go through each key, making sure it has a panel
	for ( KeyValues *pKey = pKV; pKey != NULL; pKey = pKey->GetNextKey() )
	{
		CKV_Editor_Base_Panel *pPanel = FindPanel( pKey );
		KeyValues *pFileSpecNode = FindFileSpecNodeForKey( pKey );
		if ( !pFileSpecNode && m_bRequireFileSpec )
		{
			Warning( "Key %s:%s has no FileSpecNode!\n", pKey->GetName(), pKey->GetString() );
			return;
		}
		const char *szClassName = "CKV_Node_Panel";
		if ( !pFileSpecNode )
		{
			//Warning( "Failed to FindFileSpecNodeForKey for key %s\n", pKey->GetName() );

			bool bHasChildren = ( pKey->GetFirstSubKey() != NULL );
			if ( bHasChildren )
			{
				szClassName = "CKV_Node_Panel";
			}
			else
			{
				szClassName = "CKV_Leaf_Panel";
			}			
		}
		else
		{
			szClassName = pFileSpecNode->GetString( "Panel", "CKV_Node_Panel" );
		}
		if ( !pPanel )
		{
			// create a panel of the appropriate type (as a child of pParentPanel)
			pPanel = CreatePanel( szClassName, pParentPanel, pFileSpecNode, pKey );
		}
		else
		{			
			// make sure the panel is the right type.
			if ( Q_stricmp( pPanel->GetClassName(), szClassName ) )
			{
				// destroy and recreate if not (as a child of pParentPanel)
				pPanel->SetVisible( false );
				pPanel->MarkForDeletion();
				CKV_Fit_Children_Panel *pFit = dynamic_cast< CKV_Fit_Children_Panel* >( pPanel->GetParent() );
				if ( pFit )
				{
					pFit->RemoveAutoPositionPanel( pPanel );
				}
				pPanel = CreatePanel( szClassName, pParentPanel, pFileSpecNode, pKey );
			}			
			else
			{
				pPanel->SetKey( pKey );
				pPanel->SetKeyParent( FindParentForKey( pKey ) );
			}
		}

		// if key has children
		if ( pKey->GetFirstSubKey() )
		{
			UpdatePanels( pKey->GetFirstSubKey(), pPanel, true );
		}

		if ( !bIncludeSiblings )
			break;
	}
	m_pContainer->InvalidateLayout( true );
}

KeyValues* CKV_Editor::FindParentForKey( KeyValues *pSearchChild )
{
	for ( KeyValues *pKey = m_pKeys; pKey != NULL; pKey = pKey->GetNextKey() )
	{
		if ( pKey == pSearchChild )
			return NULL;

		KeyValues *pResult = FindParentForKey( pKey, pSearchChild );
		if ( pResult )
			return pResult;
	}
	return NULL;
}

KeyValues* CKV_Editor::FindParentForKey( KeyValues *pRoot, KeyValues *pSearchChild )
{
	// see if pKV or any of our direct children are the search key
	for ( KeyValues *pKey = pRoot->GetFirstSubKey(); pKey != NULL; pKey = pKey->GetNextKey() )
	{
		if ( pKey == pSearchChild )
			return pRoot;
	}

	// if not, then ask each of our children to search deeper
	for ( KeyValues *pKey = pRoot->GetFirstSubKey(); pKey != NULL; pKey = pKey->GetNextKey() )
	{
		KeyValues *pResult = FindParentForKey( pKey, pSearchChild );
		if ( pResult )
			return pResult;
	}
	return NULL;
}

KeyValues* CKV_Editor::FindFileSpecNodeForKey( KeyValues *pKey )
{
	if ( !m_pFileSpec )
		return NULL;
	// build a list of panels from us up to the parent
	CUtlVector<KeyValues*> chain;
	KeyValues *pSearchKey = pKey;
	while ( pSearchKey )
	{
		chain.AddToHead( pSearchKey );
		pSearchKey = FindParentForKey( pSearchKey );
	}

	int iChainDepth = chain.Count();
	int iCurrentDepth = 0;

	KeyValues *pKeySpec = m_pFileSpec;
	while ( pKeySpec )
	{
		const char *szNodeName = "_unnamed_node";

		if ( !Q_stricmp( pKeySpec->GetName(), "_Node" ) || !Q_stricmp( pKeySpec->GetName(), "_Leaf" ) )
		{
			szNodeName = pKeySpec->GetString( "Name", "_unnamed_node" );
		}
		//const char *szChainName = chain[iCurrentDepth]->GetName();
		if ( !Q_stricmp( szNodeName, chain[iCurrentDepth]->GetName() ) 
			|| !Q_stricmp( szNodeName, "*" ) )		// at any level there can be a keyspec node with name "*" which will match any key
		{
			iCurrentDepth++;
			if ( iCurrentDepth >= iChainDepth )		// found appropriate file spec node that describes this key
			{
				return pKeySpec;
			}

			// Go down a level
			pKeySpec = pKeySpec->GetFirstSubKey();			
		}
		else
		{
			// Check the next key in the mission spec
			pKeySpec = pKeySpec->GetNextKey();
		}
	}
	return NULL;
}

void CKV_Editor::SetFileSpec( const char *szFilename, const char *szPathID )
{
	KeyValues *pKeys = new KeyValues( "FileSpec" );
	if ( !pKeys->LoadFromFile( g_pFullFileSystem, szFilename, szPathID ) )
	{
		Warning( "Failed to load file spec %s\n", szFilename );
		return;
	}
	SetFileSpec( pKeys );
}

void CKV_Editor::SetFileSpec( KeyValues *pKeys )
{
	m_pFileSpec = pKeys;
	DeleteAllPanels();
	UpdatePanels( m_pKeys, m_pContainer, m_bShowSiblings );
}

void CKV_Editor::SetFileFilter( const char *szFilter, const char *szFilterName )
{
    Q_snprintf( m_szFileFilter, sizeof( m_szFileFilter ), szFilter );
	Q_snprintf( m_szFileFilterName, sizeof( m_szFileFilterName ), szFilterName );
}

void CKV_Editor::SetFileDirectory( const char *szDirName )
{
	Q_snprintf( m_szFileDirectory, sizeof( m_szFileDirectory ), szDirName );
}

void CKV_Editor::SetKeys( KeyValues *pKeys )
{
	m_pKeys = pKeys;
	DeleteAllPanels();
	UpdatePanels( m_pKeys, m_pContainer, m_bShowSiblings );
	PostActionSignal( new KeyValues( "command", "command", "KeyValuesChanged" ) );
}

void CKV_Editor::OnKeyDeleted()
{
	DeleteAllPanels();
	UpdatePanels( m_pKeys, m_pContainer, m_bShowSiblings );
	PostActionSignal( new KeyValues( "command", "command", "KeyValuesChanged" ) );
}

void CKV_Editor::OnKeyAdded()
{
	DeleteAllPanels();
	UpdatePanels( m_pKeys, m_pContainer, m_bShowSiblings );
	PostActionSignal( new KeyValues( "command", "command", "KeyValuesChanged" ) );
}

void CKV_Editor::AddToKey( KeyValues *pFileSpecNode, KeyValues *pKey, const char *szNewKeyName )
{
	// go through all _Node or _Leaf children of our filespec node
	for ( KeyValues *pSubKey = pFileSpecNode->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		bool bNode = !Q_stricmp( pSubKey->GetName(), "_Node" );
		bool bLeaf = !Q_stricmp( pSubKey->GetName(), "_Leaf" );
		if ( !bLeaf && !bNode )
			continue;

		if ( szNewKeyName && Q_stricmp( pSubKey->GetString( "Name" ), szNewKeyName ) )
			continue;

		if ( !szNewKeyName && pSubKey->GetInt( "Autocreate" ) != 1 )
			continue;

		if ( bLeaf )
		{
			bool bAlreadyExists = !!pKey->FindKey( pSubKey->GetString( "Name" ) );
			bool bUnique = pSubKey->GetInt( "Unique" ) == 1;
			if ( !bUnique || !bAlreadyExists )
			{
				KeyValues *pNewKey = new KeyValues( pSubKey->GetString( "Name" ) );
				Msg( "Added leaf %s\n", pSubKey->GetString( "Name" ) );
				pNewKey->SetStringValue( pSubKey->GetString( "DefaultValue", "" ) );
				pKey->AddSubKey( pNewKey );
			}
		}
		else
		{
			KeyValues *pNewKey = new KeyValues( pSubKey->GetString( "Name" ) );
			Msg( "Added node %s\n", pSubKey->GetString( "Name" ) );
			pKey->AddSubKey( pNewKey );

			// recursively create child _nodes and any _leafs they have			
			KeyValues *pNewSpec = FindFileSpecNodeForKey( pNewKey );
			if ( pNewSpec )
			{
				AddToKey( pNewSpec, pNewKey, NULL );
			}
		}
	}	
	OnKeyAdded();
}