//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/dmepanel.h"
#include "tier1/KeyValues.h"
#include "dme_controls/dmecontrols.h"
#include "vgui_controls/combobox.h"
#include "datamodel/dmelement.h"
#include "dme_controls/dmecontrols_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// All DmePanels used by the system must be listed here to link them in
//-----------------------------------------------------------------------------
USING_DMEPANEL_FACTORY( CDmeElementPanel, DmElement );
USING_DMEPANEL_FACTORY( CDmeSourceSkinPanel, DmeSourceSkin );
USING_DMEPANEL_FACTORY( CAssetBuilder, DmeMakefile );
USING_DMEPANEL_FACTORY( CDmeSourceDCCFilePanel, DmeSourceDCCFile );
USING_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeDag );
USING_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeSourceAnimation );
USING_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeSourceSkin );
USING_DMEPANEL_FACTORY( CDmeDagRenderPanel, DmeDCCMakefile );
USING_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeDag );
USING_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeSourceAnimation );
USING_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeSourceSkin );
USING_DMEPANEL_FACTORY( CDmeDagEditPanel, DmeDCCMakefile );
USING_DMEPANEL_FACTORY( CDmeMDLPanel, DmeMDLMakefile );


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmePanel::CDmePanel( vgui::Panel *pParent, const char *pPanelName, bool bComboBoxVisible ) :
	BaseClass( pParent, pPanelName )
{
	m_pEditorNames = new vgui::ComboBox( this, "EditorDisplayNames", 6, false );
	if ( bComboBoxVisible )
	{
		m_pEditorNames->AddActionSignalTarget( this );
	}
	else
	{
		m_pEditorNames->SetVisible( false );
	}
	m_pDmeEditorPanel = NULL;
	m_hElement = NULL;

	SetDropEnabled( true );
}

CDmePanel::~CDmePanel()
{
	DeleteCachedPanels();
}


//-----------------------------------------------------------------------------
// Scheme
//-----------------------------------------------------------------------------
void CDmePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_pEditorNames->SetFont( pScheme->GetFont( "DefaultVerySmall" ) );
}


//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CDmePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );
	if ( m_pEditorNames->IsVisible() )
	{
		m_pEditorNames->SetBounds( 1, 1, w-2, 20 );
		if ( m_pDmeEditorPanel )
		{
			m_pDmeEditorPanel->SetBounds( 0, 24, w, h-24 );
		}
	}
	else
	{
		if ( m_pDmeEditorPanel )
		{
			m_pDmeEditorPanel->SetBounds( 0, 0, w, h );
		}
	}
}


//-----------------------------------------------------------------------------
// Drag/drop
//-----------------------------------------------------------------------------
bool CDmePanel::IsDroppable( CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() != 1 )
		return false;

	KeyValues *data = msglist[ 0 ];
	CDmElement *ptr = GetElementKeyValue<CDmElement>( data, "dmeelement" );
	if ( !ptr )
		return false;

	if ( ptr == m_hElement.Get() )
		return false;

	return true;
}

void CDmePanel::OnPanelDropped( CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() != 1 )
		return;

	KeyValues *data = msglist[ 0 ];
	CDmElement *ptr = GetElementKeyValue<CDmElement>( data, "dmeelement" );
	if ( !ptr )
		return;

	// Already browsing
	if ( ptr == m_hElement.Get() )
		return;

	SetDmeElement( ptr );
}


//-----------------------------------------------------------------------------
// Sets the default editor type
//-----------------------------------------------------------------------------
void CDmePanel::SetDefaultEditorType( const char *pEditorType )
{
	m_DefaultEditorType = pEditorType;
}


//-----------------------------------------------------------------------------
// Populate editor name combo box
//-----------------------------------------------------------------------------
void CDmePanel::PopulateEditorNames( const char *pPanelName )
{
	m_pEditorNames->RemoveAll();
	m_pEditorNames->SetText( "" );
	if ( !m_pEditorNames->IsVisible() )
	{
		SetEditor( pPanelName );
		return;
	}

	if ( !m_hElement.Get() )
	{
		OnTextChanged();
		return;
	}

	const char *pPreferredEditor = NULL;
	if ( m_LastUsedEditorType.Defined( m_hElement->GetTypeString() ) )
	{
		pPreferredEditor = m_LastUsedEditorType[ m_hElement->GetTypeString() ].Get();
	}
	else
	{
		pPreferredEditor = m_DefaultEditorType;
	}

	int nBestInheritanceDepth = -1;
	int nActiveItemID = -1;
	bool bFoundPanelName = false;
	DmeFactoryHandle_t h = DmePanelFirstFactory( m_hElement.Get() );
	for ( ; h != DMEFACTORY_HANDLE_INVALID; h = DmePanelNextFactory( h, m_hElement.Get() ) )
	{
		const char *pDisplayName = DmePanelFactoryDisplayName( h );
		const char *pEditorName = DmePanelFactoryName( h );
		KeyValues *pKeyValues = new KeyValues( "entry", "editorName", pEditorName );

		int nItemID = m_pEditorNames->AddItem( pDisplayName, pKeyValues );

		if ( pPanelName && !Q_stricmp( pPanelName, pEditorName ) )
		{
			nBestInheritanceDepth = 0;
			nActiveItemID = nItemID;
			bFoundPanelName = true;
			continue;
		}

		if ( pPreferredEditor && !bFoundPanelName && !Q_stricmp( pPreferredEditor, pEditorName ) )
		{
			nBestInheritanceDepth = 0;
			nActiveItemID = nItemID;
			continue;
		}

		// Don't select this as the default if it's not a default factory
		if ( !DmePanelFactoryIsDefault(h) )
			continue;

		// Choose this factory if it's more derived than the previous best
		const char *pElementType = DmePanelFactoryElementType( h );
		int nInheritanceDepth = m_hElement->GetInheritanceDepth( pElementType );
		Assert( nInheritanceDepth >= 0 );
		if ( nBestInheritanceDepth >= 0 && ( nInheritanceDepth >= nBestInheritanceDepth ) )
			continue;

		nBestInheritanceDepth = nInheritanceDepth;
		nActiveItemID = nItemID;
	}

	if ( m_pEditorNames->GetItemCount() == 0 )
	{
		// ItemCount == 0;
		m_pEditorNames->SetText( "" );
		m_CurrentEditorName = NULL;
		OnTextChanged();
		return;
	}

	if ( nActiveItemID >= 0 )
	{
		m_pEditorNames->ActivateItem( nActiveItemID );
	}
	else 
	{
		m_pEditorNames->ActivateItemByRow( 0 );
	}
}


//-----------------------------------------------------------------------------
// Called when the dme element was changed
//-----------------------------------------------------------------------------
void CDmePanel::OnDmeElementChanged()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}


//-----------------------------------------------------------------------------
// Context menu support
//-----------------------------------------------------------------------------
void CDmePanel::OnOpenContextMenu( KeyValues *params )
{
	// Forward the context menu message to the DME panel
	KeyValues *pMsg = params->MakeCopy();
	if ( m_pDmeEditorPanel )
	{
		PostMessage( m_pDmeEditorPanel->GetVPanel(), pMsg );
	}
}


//-----------------------------------------------------------------------------
// Copy/paste support
//-----------------------------------------------------------------------------
void CDmePanel::PostMessageToDmePanel( const char *pMessage )
{
	if ( m_pDmeEditorPanel )
	{
		PostMessage( m_pDmeEditorPanel->GetVPanel(), new KeyValues( pMessage ) );
	}
}

void CDmePanel::OnCut()
{
	PostMessageToDmePanel( "OnCut" );
}

void CDmePanel::OnCopy()
{
	PostMessageToDmePanel( "OnCopy" );
}

void CDmePanel::OnPaste()
{
	PostMessageToDmePanel( "OnPaste" );
}

void CDmePanel::OnPasteInsert()
{
	PostMessageToDmePanel( "OnPasteInsert" );
}

void CDmePanel::OnPasteReference()
{
	PostMessageToDmePanel( "OnPasteReference" );
}

void CDmePanel::OnEditDelete()
{
	PostMessageToDmePanel( "OnEditDelete" );
}



//-----------------------------------------------------------------------------
// Called when a child of the dme panel switches the thing it's looking at
//-----------------------------------------------------------------------------
void CDmePanel::OnViewedElementChanged( KeyValues *kv )
{
	// This is kind of tricky. It's called by the element properties tree
	// when doing the back/forward searching. Just calling the normal SetDmeElement
	// doesn't work because it reorders the history. What we want is to
	// populate the combo box without causing the OnTextChanged message to get sent.

	// FIXME: Perhaps it would be better to extract the back/forward/search
	// out of the element properties tree and put it into the dme panel?
	CDmElement *pElement = GetElementKeyValue<CDmElement>( kv, "dmeelement" );
	if ( pElement == m_hElement )
		return;

	// If the current editor isn't supported by this new element, then just reset. Too bad.
	bool bFound = false;
	if ( m_CurrentEditorName.Length() && pElement )
	{
		DmeFactoryHandle_t h = DmePanelFirstFactory( pElement );
		for ( ; h != DMEFACTORY_HANDLE_INVALID; h = DmePanelNextFactory( h, pElement ) )
		{
			const char *pEditorName = DmePanelFactoryName( h );
			if ( !Q_stricmp( m_CurrentEditorName, pEditorName ) )
			{
				bFound = true;
				break;
			}
		}
	}

	if ( !bFound )
	{
		SetDmeElement( pElement );
		return;
	}

	// Remove obsolete items
	int nCount = m_pEditorNames->GetItemCount();
	while ( --nCount >= 0 )
	{
		int nItemID = m_pEditorNames->GetItemIDFromRow( nCount );
		KeyValues *kv = m_pEditorNames->GetItemUserData( nItemID );
		if ( Q_stricmp( m_CurrentEditorName, kv->GetString( "editorName" ) ) )
		{
			m_pEditorNames->DeleteItem( nItemID );
		}
	}

	// Just want to populate the combo box with new items 
	DmeFactoryHandle_t h = DmePanelFirstFactory( pElement );
	for ( ; h != DMEFACTORY_HANDLE_INVALID; h = DmePanelNextFactory( h, pElement ) )
	{
		const char *pEditorName = DmePanelFactoryName( h );
		if ( Q_stricmp( pEditorName, m_CurrentEditorName ) )
		{
			const char *pDisplayName = DmePanelFactoryDisplayName( h );
			KeyValues *pKeyValues = new KeyValues( "entry", "editorName", pEditorName );
			m_pEditorNames->AddItem( pDisplayName, pKeyValues );
		}
	}

	m_hElement = pElement;
}


//-----------------------------------------------------------------------------
// Delete cached panels
//-----------------------------------------------------------------------------
void CDmePanel::DeleteCachedPanels()
{
	int nCount = m_EditorPanelCache.GetNumStrings();
	for ( int i = 0; i < nCount; ++i )
	{
		int nEditorCount = m_EditorPanelCache[ i ].Count();
		for ( int j = 0; j < nEditorCount; ++j )
		{
			m_EditorPanelCache[ i ][ j ].m_pEditorPanel->MarkForDeletion();
		}
	}
	m_EditorPanelCache.Clear();
}


//-----------------------------------------------------------------------------
// Refreshes the current panel owing to external change
// Values only means no topological change
//-----------------------------------------------------------------------------
void CDmePanel::Refresh( bool bValuesOnly )
{
	if ( m_pDmeEditorPanel )
	{
		KeyValues *pKeyValues = new KeyValues( "ElementChangedExternally", "valuesOnly", bValuesOnly );
		PostMessage( m_pDmeEditorPanel, pKeyValues );
	}
}


//-----------------------------------------------------------------------------
// Deactivates the current editor
//-----------------------------------------------------------------------------
void CDmePanel::DeactivateCurrentEditor()
{
	if ( m_pDmeEditorPanel )
	{
		m_pDmeEditorPanel->SetParent( (vgui::Panel*)NULL );
		m_pDmeEditorPanel = NULL;
		m_CurrentEditorName = NULL;
	}
}


//-----------------------------------------------------------------------------
// Switch to a new editor
//-----------------------------------------------------------------------------
void CDmePanel::SetEditor( const char *pEditorName )
{
	if ( pEditorName && !Q_stricmp( m_CurrentEditorName, pEditorName ) )
		return;

	DeactivateCurrentEditor();

	if ( !m_hElement.Get() || !pEditorName )
		return;

	if ( m_EditorPanelCache.Defined( pEditorName ) )
	{
		CUtlVector< EditorPanelMap_t > &entries = m_EditorPanelCache[ pEditorName ];
		int nCount = entries.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			EditorPanelMap_t &entry = entries[i];
			if ( !m_hElement->IsA( entry.m_pFactory->m_pElementType ) )
				continue;

			m_pDmeEditorPanel = entry.m_pEditorPanel;
			m_pDmeEditorPanel->SetParent( this );
			entry.m_pFactory->SetDmeElement( m_pDmeEditorPanel, m_hElement );
			break;
		}
	}
	
	if ( !m_pDmeEditorPanel )
	{
		EditorPanelMap_t entry;
		if ( CreateDmePanel( this, "DmePanelEditor", m_hElement, pEditorName, &entry ) )
		{
			m_EditorPanelCache[ pEditorName ].AddToTail( entry );
			m_pDmeEditorPanel = entry.m_pEditorPanel;
		}
	}

	if ( m_pDmeEditorPanel )
	{
		// Store the last selected type of editor
		m_LastUsedEditorType[ m_hElement->GetTypeString() ] = pEditorName;
		m_CurrentEditorName = pEditorName;
		m_pDmeEditorPanel->AddActionSignalTarget( this );
	}
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Called when a new element in the combo box has been selected
//-----------------------------------------------------------------------------
void CDmePanel::OnTextChanged()
{
	KeyValues *kv = m_pEditorNames->GetActiveItemUserData();
	const char *pEditorName = kv ? kv->GetString( "editorName", NULL ) : NULL;
	SetEditor( pEditorName );
}


//-----------------------------------------------------------------------------
// Setting a new element
//-----------------------------------------------------------------------------
void CDmePanel::SetDmeElement( CDmElement *pDmeElement, bool bForce, const char *pPanelName )
{
	if ( ( m_hElement == pDmeElement ) && !bForce )
	{
		if ( !pPanelName || !Q_stricmp( pPanelName, m_CurrentEditorName.Get() ) )
			return;
	}

	m_hElement = pDmeElement;
	m_CurrentEditorName = NULL;

	// Populate the editor type list
	PopulateEditorNames( pPanelName );
}


//-----------------------------------------------------------------------------
// Statics for the panel factory
//-----------------------------------------------------------------------------
CBaseDmePanelFactory* CBaseDmePanelFactory::s_pFirstDmePanelFactory;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBaseDmePanelFactory::CBaseDmePanelFactory( const char *pElementType, const char *pEditorName, 
	const char *pEditorDisplayName, bool bIsDefault, bool bIsOverride )
{
	// Prior to linking this in, look to see if this has been overridden
	CBaseDmePanelFactory *pPrevFactory = NULL;
	for( CBaseDmePanelFactory* pFactory = s_pFirstDmePanelFactory; pFactory; 
		pPrevFactory = pFactory, pFactory = pFactory->m_pNext )
	{
		if ( !Q_stricmp( pFactory->m_pElementType, pElementType ) &&
			 !Q_stricmp( pFactory->m_pEditorDisplayName, pEditorDisplayName ) )
		{
			// Collision found! If this is not an override, then we've been overridden
			if ( !bIsOverride )
			{
				AssertMsg( pFactory->m_bIsOverride, ( "Two DmePanel factories have the same name (\"%s\") + type (\"%s\")!\n", pElementType, pEditorName ) );
				return;
			}

			// If this *is* an override, replace the previous version
			AssertMsg( !pFactory->m_bIsOverride, ( "Two DmePanel factories have the same name (\"%s\") + type (\"%s\")!\n", pElementType, pEditorName ) );
			if ( pPrevFactory )
			{
				pPrevFactory->m_pNext = pFactory->m_pNext;
			}
			else
			{
				s_pFirstDmePanelFactory = pFactory->m_pNext;
			}
			break;
		}
	}

	m_pNext = s_pFirstDmePanelFactory;
	s_pFirstDmePanelFactory = this;

	m_pElementType = pElementType;
	m_pEditorName = pEditorName;
	m_pEditorDisplayName = pEditorDisplayName;
	m_bIsDefault = bIsDefault;
	m_bIsOverride = bIsOverride;
}


//-----------------------------------------------------------------------------
// Dme Panel factory iteration methods
//-----------------------------------------------------------------------------
DmeFactoryHandle_t DmePanelFirstFactory( CDmElement *pElement )
{
	CBaseDmePanelFactory *pFactory = CBaseDmePanelFactory::s_pFirstDmePanelFactory; 
	for ( ; pFactory; pFactory = pFactory->m_pNext )
	{
		if ( !pElement || pElement->IsA( pFactory->m_pElementType ) )
			return (DmeFactoryHandle_t)pFactory;
	}

	return DMEFACTORY_HANDLE_INVALID;
}


DmeFactoryHandle_t DmePanelNextFactory( DmeFactoryHandle_t h, CDmElement *pElement )
{
	CBaseDmePanelFactory *pFactory = (CBaseDmePanelFactory*)h; 
	if ( !pFactory )
		return DMEFACTORY_HANDLE_INVALID;

	for ( pFactory = pFactory->m_pNext; pFactory; pFactory = pFactory->m_pNext )
	{
		if ( !pElement || pElement->IsA( pFactory->m_pElementType ) )
			return (DmeFactoryHandle_t)pFactory;
	}

	return DMEFACTORY_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Dme Panel factory info methods
//-----------------------------------------------------------------------------
const char *DmePanelFactoryName( DmeFactoryHandle_t h )
{
	CBaseDmePanelFactory *pFactory = (CBaseDmePanelFactory*)h; 
	return pFactory ? pFactory->m_pEditorName : NULL;
}

const char *DmePanelFactoryDisplayName( DmeFactoryHandle_t h )
{
	CBaseDmePanelFactory *pFactory = (CBaseDmePanelFactory*)h; 
	return pFactory ? pFactory->m_pEditorDisplayName : NULL;
}

const char *DmePanelFactoryElementType( DmeFactoryHandle_t h )
{
	CBaseDmePanelFactory *pFactory = (CBaseDmePanelFactory*)h; 
	return pFactory ? pFactory->m_pElementType : NULL;
}

bool DmePanelFactoryIsDefault( DmeFactoryHandle_t h )
{
	CBaseDmePanelFactory *pFactory = (CBaseDmePanelFactory*)h; 
	return pFactory ? pFactory->m_bIsDefault : false;
}


//-----------------------------------------------------------------------------
// Dme Panel factory methods
//-----------------------------------------------------------------------------
bool CDmePanel::CreateDmePanel( vgui::Panel *pParent, const char *pPanelName, CDmElement *pElement, const char *pEditorName, EditorPanelMap_t *pMap )
{
	int nBestInheritanceDepth = -1;
	CBaseDmePanelFactory *pBestFactory = NULL;
	CBaseDmePanelFactory *pFactory = CBaseDmePanelFactory::s_pFirstDmePanelFactory; 
	for ( ; pFactory; pFactory = pFactory->m_pNext )
	{
		if ( !pElement->IsA( pFactory->m_pElementType ) )
			continue;
		
		if ( pEditorName )
		{
			if ( !Q_stricmp( pEditorName, pFactory->m_pEditorName ) )
			{
				pBestFactory = pFactory;
				break;
			}
			continue;
		}

		// No editor name specified? Only use default factories
		if ( !pFactory->m_bIsDefault )
			continue;

		// Choose this factory if it's more derived than the previous best
		int nInheritanceDepth = pElement->GetInheritanceDepth( pFactory->m_pElementType );
		Assert( nInheritanceDepth >= 0 );
		if ( nBestInheritanceDepth >= 0 && ( nInheritanceDepth > nBestInheritanceDepth ) )
			continue;

		nBestInheritanceDepth = nInheritanceDepth;
		pBestFactory = pFactory;
	}

	if ( pBestFactory )
	{
		pMap->m_pFactory = pBestFactory;
		pMap->m_pEditorPanel = pBestFactory->CreateDmePanel( pParent, pPanelName, pElement );
		return true;
	}
	return false;
}

