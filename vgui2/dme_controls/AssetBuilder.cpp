//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/AssetBuilder.h"
#include "dme_controls/DmePanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/FileOpenStateMachine.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui/ischeme.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "tier1/tier1.h"
#include "movieobjects/dmemakefile.h"
#include "matsys_controls/picker.h"
#include "tier2/fileutils.h"
#include "vgui/keycode.h"
#include "filesystem.h"
#include "movieobjects/idmemakefileutils.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

#define ASSET_FILE_FORMAT "model"

//-----------------------------------------------------------------------------
// Compile status bar
//-----------------------------------------------------------------------------
class CCompileStatusBar : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CCompileStatusBar, EditablePanel );

public:
	enum CompileStatus_t
	{
		NOT_COMPILING,
		CURRENTLY_COMPILING,
		COMPILATION_FAILED,
		COMPILATION_SUCCESSFUL
	};

	CCompileStatusBar( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CCompileStatusBar();

	virtual void PaintBackground();

	void SetStatus( CompileStatus_t status, const char *pMessage );

private:
	vgui::Label *m_pStatus;
	CompileStatus_t m_Status;
	int m_CompilingId;
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CCompileStatusBar::CCompileStatusBar( vgui::Panel *pParent, const char *pPanelName ) :
	BaseClass( pParent, pPanelName )
{ 
	m_pStatus = new vgui::Label( this, "StatusLabel", "" );
	m_pStatus->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	m_pStatus->SetContentAlignment( vgui::Label::a_center );
	m_pStatus->SetTextColorState( vgui::Label::CS_BRIGHT );
	SetStatus( NOT_COMPILING, "" );
	SetPaintBackgroundEnabled( true );
	m_CompilingId = vgui::surface()->DrawGetTextureId( "vgui/progressbar" );
	if ( m_CompilingId == -1 ) // we didn't find it, so create a new one
	{
		m_CompilingId = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_CompilingId, "vgui/progressbar", true, false );
	}
}

CCompileStatusBar::~CCompileStatusBar()
{
}


//-----------------------------------------------------------------------------
// Sets compile status
//-----------------------------------------------------------------------------
void CCompileStatusBar::SetStatus( CompileStatus_t status, const char *pMessage )
{
	m_Status = status;
	m_pStatus->SetText( pMessage );
}


void CCompileStatusBar::PaintBackground()
{
	int w, h;
	GetSize( w, h );

	switch( m_Status )
	{
	case NOT_COMPILING:
		break;

	case COMPILATION_FAILED:
		vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		break;

	case COMPILATION_SUCCESSFUL:
		vgui::surface()->DrawSetColor( 0, 255, 0, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		break;

	case CURRENTLY_COMPILING:
		{ 
			float du = Plat_FloatTime() / 5.0f;
		    du -= (int)du;
			du = 1.0f - du;

			Vertex_t verts[4];
			verts[0].Init( Vector2D( 0.0f, 0.0f ),	Vector2D( du, 0.0f ) );
			verts[1].Init( Vector2D( w, 0.0f ),		Vector2D( 1.0f + du, 0.0f ) );
			verts[2].Init( Vector2D( w, h ),		Vector2D( 1.0f + du, 1.0f ) );
			verts[3].Init( Vector2D( 0.0f, h ),		Vector2D( du, 1.0f ) );

			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTexture( m_CompilingId );
			vgui::surface()->DrawTexturedPolygon( 4, verts );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
//
// Asset Builder
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CAssetBuilder, DmeMakefile, "DmeMakeFileDefault", "MakeFile Editor", true );


//-----------------------------------------------------------------------------
// Static data
//-----------------------------------------------------------------------------
static PickerList_t s_AssetTypes;
static bool s_bAssetTypeListBuilt = false;


//-----------------------------------------------------------------------------
// Builds the list of asset types
//-----------------------------------------------------------------------------
void BuildAssetTypeList( )
{
	if ( s_bAssetTypeListBuilt )
		return;

	s_bAssetTypeListBuilt = true;

	CDisableUndoScopeGuard guard;

	int hFactory = g_pDataModel->GetFirstFactory();
	while ( g_pDataModel->IsValidFactory( hFactory ) )
	{
		// Add all DmeElements that inherit from DmeMakefile 
		const char *pFactoryName = g_pDataModel->GetFactoryName( hFactory );
		CDmElement *pElement = GetElement< CDmElement >( g_pDataModel->CreateElement( pFactoryName, "temp" ) );
		CDmeMakefile *pMakeFile = CastElement<CDmeMakefile>( pElement );
		if ( pMakeFile && pMakeFile->GetMakefileType() )
		{
			int i = s_AssetTypes.AddToTail();
			s_AssetTypes[i].m_pChoiceString = pMakeFile->GetMakefileType()->m_pHumanReadableName;
			s_AssetTypes[i].m_pChoiceValue = pFactoryName;
		}
		DestroyElement( pElement );

		hFactory = g_pDataModel->GetNextFactory( hFactory );
	}
}


//-----------------------------------------------------------------------------
// Builds the list of asset types
//-----------------------------------------------------------------------------
static PickerList_t &BuildAssetSubTypeList( const char **ppSubTypes, PickerList_t &pickerList )
{
	if ( !ppSubTypes )
		return s_AssetTypes;

	pickerList.RemoveAll();

	CDisableUndoScopeGuard guard;

	int nCount = s_AssetTypes.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Add all DmeElements that inherit from DmeMakefile 
		CDmElement *pElement = GetElement< CDmElement >( g_pDataModel->CreateElement( s_AssetTypes[i].m_pChoiceValue, "temp" ) );
		CDmeMakefile *pMakeFile = CastElement< CDmeMakefile >( pElement );

		for ( int j = 0; ppSubTypes[j]; ++j )
		{
			if ( !pElement->IsA( ppSubTypes[j] ) )
				continue;

			int k = pickerList.AddToTail();
			pickerList[k].m_pChoiceString = pMakeFile->GetMakefileType()->m_pHumanReadableName;
			pickerList[k].m_pChoiceValue = s_AssetTypes[i].m_pChoiceValue;
			break;
		}
		DestroyElement( pElement );
	}

	return pickerList;
}


//-----------------------------------------------------------------------------
// Shows the overwrite existing file dialog
//-----------------------------------------------------------------------------
static void OverwriteFileDialog( vgui::Panel *pActionTarget, const char *pFileName, KeyValues *pOkCommand )
{
	if ( !g_pFullFileSystem->FileExists( pFileName ) )
	{
		pActionTarget->PostMessage( pActionTarget->GetVPanel(), pOkCommand );
		return;
	}

	char pBuf[1024];
	Q_snprintf( pBuf, sizeof(pBuf), "File already exists. Overwrite it?\n\n\"%s\"\n", pFileName ); 
	vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Overwrite Existing File?", pBuf, pActionTarget );
	pMessageBox->AddActionSignalTarget( pActionTarget );
	pMessageBox->SetOKButtonVisible( true );
	pMessageBox->SetOKButtonText( "Yes" );
	pMessageBox->SetCancelButtonVisible( true );
	pMessageBox->SetCancelButtonText( "No" );
	pMessageBox->SetCloseButtonVisible( false ); 
	pMessageBox->SetCommand( pOkCommand );
	pMessageBox->DoModal();
}


//-----------------------------------------------------------------------------
// Utility to load a makefile
//-----------------------------------------------------------------------------
static CDmeMakefile *ReadMakefile( const char *pFileName, CDmElement **ppRoot = NULL ) 
{
	if ( ppRoot )
	{
		*ppRoot = NULL;
	}

	CDmElement *pRoot;
	DmFileId_t fileid = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pRoot, CR_DELETE_OLD ); 
	if ( fileid == DMFILEID_INVALID || !pRoot )
	{
		Warning( "Unable to read makefile \"%s\"!\n", pFileName );
		return NULL;
	}

	CDmeMakefile *pMakeFile = CastElement< CDmeMakefile >( pRoot );
	if ( !pMakeFile )
	{
		CDmElement *pElement = CastElement< CDmElement >( pRoot );
		pMakeFile = pElement->GetValueElement< CDmeMakefile >( "makefile" );
		if ( !pMakeFile )
		{
			DmFileId_t fileId = pRoot->GetFileId();
			DestroyElement( pRoot );
			if ( fileId != DMFILEID_INVALID && g_pDataModel->GetFileName( fileId )[0] )
			{
				g_pDataModel->RemoveFileId( fileId );
			}
			return NULL;
		}
	}

	if ( ppRoot )
	{
		*ppRoot = CastElement< CDmElement >( pRoot );
	}
	return pMakeFile;
}



//-----------------------------------------------------------------------------
// Sort by MDL name
//-----------------------------------------------------------------------------
static int __cdecl TypeSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("type");
	const char *string2 = item2.kv->GetString("type");
	int nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	string1 = item1.kv->GetString("file");
	string2 = item2.kv->GetString("file");
	nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	int nIndex1 = item1.kv->GetInt( "index" );
	int nIndex2 = item2.kv->GetInt( "index" );
	return nIndex1 - nIndex2;
}

static int __cdecl FileSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("file");
	const char *string2 = item2.kv->GetString("file");
	int nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	string1 = item1.kv->GetString("type");
	string2 = item2.kv->GetString("type");
	nRetVal = Q_stricmp( string1, string2 );
	if ( nRetVal != 0 )
		return nRetVal;

	int nIndex1 = item1.kv->GetInt( "index" );
	int nIndex2 = item2.kv->GetInt( "index" );
	return nIndex1 - nIndex2;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor, destructor
//-----------------------------------------------------------------------------
CAssetBuilder::CAssetBuilder( vgui::Panel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{
	m_hContextMenu = NULL;
	m_hRootMakefile = NULL;
	m_bIsCompiling = false;
	m_bDestroyMakefileOnClose = true;

	m_pInputOutputSheet = new vgui::PropertySheet( this, "InputOutputSheet" );
	m_pInputOutputSheet->AddActionSignalTarget( this );

	m_pInputPage = new PropertyPage( m_pInputOutputSheet, "InputPage" );
	m_pOutputPage = new PropertyPage( m_pInputOutputSheet, "OutputPage" );
	m_pCompilePage = new PropertyPage( m_pInputOutputSheet, "CompilePage" );
	m_pOutputPreviewPage = new PropertyPage( m_pInputOutputSheet, "OutputPreviewPage" );

	m_pPropertiesSplitter = new vgui::Splitter( m_pInputPage, "PropertiesSplitter", SPLITTER_MODE_VERTICAL, 1 );

	vgui::Panel *pSplitterLeftSide = m_pPropertiesSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pPropertiesSplitter->GetChild( 1 );
	m_pDmePanel = new CDmePanel( pSplitterRightSide, "CompileOptions" );
	m_pDmePanel->AddActionSignalTarget( this );

	m_pOututPreviewPanel = new CDmePanel( m_pOutputPreviewPage, "OutputPreview", false );
	m_pOututPreviewPanel->AddActionSignalTarget( this );

	m_pSourcesList = new vgui::ListPanel( pSplitterLeftSide, "SourcesList" );
	m_pSourcesList->AddColumnHeader( 0, "type", "type", 100, 0 );
	m_pSourcesList->AddColumnHeader( 1, "file", "file", 52, 0 );
	m_pSourcesList->AddActionSignalTarget( this );
	m_pSourcesList->SetSortFunc( 0, TypeSortFunc );
	m_pSourcesList->SetSortFunc( 1, FileSortFunc );
	m_pSourcesList->SetSortColumn( 0 );
//	m_pSourcesList->SetSelectIndividualCells( true );
	m_pSourcesList->SetEmptyListText("No sources");
//	m_pSourcesList->SetDragEnabled( true );

	m_pOutputList = new vgui::ListPanel( m_pOutputPage, "OutputList" );
	m_pOutputList->AddColumnHeader( 0, "type", "type", 100, 0 );
	m_pOutputList->AddColumnHeader( 1, "file", "file", 52, 0 );
	m_pOutputList->AddActionSignalTarget( this );
	m_pOutputList->SetSortFunc( 0, TypeSortFunc );
	m_pOutputList->SetSortFunc( 1, FileSortFunc );
	m_pOutputList->SetSortColumn( 0 );
	m_pOutputList->SetEmptyListText("No outputs");

	m_pCompileOutput = new vgui::TextEntry( m_pCompilePage, "CompileOutput" );
	m_pCompileOutput->SetMultiline( true );
	m_pCompileOutput->SetVerticalScrollbar( true );
	m_pCompile = new vgui::Button( this, "CompileButton", "Compile", this, "OnCompile" );
	m_pPublish = new vgui::Button( this, "PublishButton", "Publish", this, "OnPublish" );
	m_pAbortCompile = new vgui::Button( this, "AbortCompileButton", "AbortCompile", this, "OnAbortCompile" );
	m_pCompileStatusBar = new CCompileStatusBar( this, "CompileStatus" );

	m_pInputPage->LoadControlSettingsAndUserConfig( "resource/assetbuilderinputpage.res" );
	m_pOutputPage->LoadControlSettingsAndUserConfig( "resource/assetbuilderoutputpage.res" );
	m_pCompilePage->LoadControlSettingsAndUserConfig( "resource/assetbuildercompilepage.res" );
	m_pOutputPreviewPage->LoadControlSettingsAndUserConfig( "resource/assetbuilderoutputpreviewpage.res" );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/assetbuilder.res" );

	// NOTE: Page adding happens *after* LoadControlSettingsAndUserConfig
	// because the layout of the sheet is correct at this point.
	m_pInputOutputSheet->AddPage( m_pInputPage, "Input" );
	m_pInputOutputSheet->AddPage( m_pOutputPage, "Output" );
	m_pInputOutputSheet->AddPage( m_pCompilePage, "Compile" );
	m_pInputOutputSheet->AddPage( m_pOutputPreviewPage, "Preview" );

	m_pCompile->SetEnabled( false );
	m_pPublish->SetEnabled( false );
	m_pAbortCompile->SetEnabled( false );
}

CAssetBuilder::~CAssetBuilder()
{
	if ( m_bDestroyMakefileOnClose )
	{
		CleanupMakefile();
	}
	CleanupContextMenu();
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Default behavior is to destroy the makefile when we close
//-----------------------------------------------------------------------------
void CAssetBuilder::DestroyMakefileOnClose( bool bEnable )
{
	m_bDestroyMakefileOnClose = bEnable;
}


//-----------------------------------------------------------------------------
// Builds a unique list of file IDs
//-----------------------------------------------------------------------------
void CAssetBuilder::BuildFileIDList( CDmeMakefile *pMakeFile, CUtlVector<DmFileId_t> &fileIds )
{
	if ( !pMakeFile )
		return;

	// NOTE: Not hugely efficient. If the CDmeDependencyMakefile starts
	// getting large, we can optimize this
	DmFileId_t id = pMakeFile->GetFileId();
	int nCount = fileIds.Count();
	int i;
	for ( i = 0; i < nCount; ++i )
	{
		if ( fileIds[i] == id )
			break;
	}

	if ( i == nCount )
	{
		fileIds.AddToTail( id );
	}

	int nSourceCount = pMakeFile->GetSourceCount();
	for ( int i = 0; i < nSourceCount; ++i )
	{
		CDmeSource *pSource = pMakeFile->GetSource(i);
		BuildFileIDList( pSource->GetDependentMakefile(), fileIds );
	}
}


//-----------------------------------------------------------------------------
// Removes a makefile from memory
//-----------------------------------------------------------------------------
void CAssetBuilder::CleanupMakefile()
{
	m_hMakefileStack.Clear();
	m_pDmePanel->SetDmeElement( NULL );
	m_pOututPreviewPanel->SetDmeElement( NULL );

	if ( !m_hRootMakefile.Get() )
		return;

	// First, build a list of unique file IDs
	CUtlVector<DmFileId_t> fileIds;
	BuildFileIDList( m_hRootMakefile, fileIds );

	CDisableUndoScopeGuard guard;

	m_hRootMakefile = NULL;

	int nCount = fileIds.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( fileIds[i] != DMFILEID_INVALID && g_pDataModel->GetFileName( fileIds[i] )[0] )
		{
			g_pDataModel->RemoveFileId( fileIds[i] );
		}
	}
}


//-----------------------------------------------------------------------------
// Marks the file as dirty (or not)
//-----------------------------------------------------------------------------
void CAssetBuilder::SetDirty()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}


//-----------------------------------------------------------------------------
// Returns the current makefile
//-----------------------------------------------------------------------------
CDmeMakefile *CAssetBuilder::GetMakeFile()
{
	return m_hMakefile.Get();
}

CDmeMakefile *CAssetBuilder::GetRootMakeFile()
{
	return m_hRootMakefile.Get();
}


//-----------------------------------------------------------------------------
// Resets the lists; called when file name changes
//-----------------------------------------------------------------------------
void CAssetBuilder::Refresh()
{
	RefreshSourceList();
	RefreshOutputList();
}


//-----------------------------------------------------------------------------
// Resets the root makefile
//-----------------------------------------------------------------------------
void CAssetBuilder::SetRootMakefile( CDmeMakefile *pMakeFile )
{
	CleanupMakefile();

	if ( pMakeFile )
	{
		m_hRootMakefile = pMakeFile;
		m_hMakefileStack.Push( m_hRootMakefile );
	}
	SetCurrentMakefile( pMakeFile );
}


//-----------------------------------------------------------------------------
// Resets the current makefile
//-----------------------------------------------------------------------------
void CAssetBuilder::SetCurrentMakefile( CDmeMakefile *pMakeFile )
{
	m_hMakefile = pMakeFile;
	m_pDmePanel->SetDmeElement( NULL );
	m_pOututPreviewPanel->SetDmeElement( pMakeFile, true, "DmeMakeFileOutputPreview" );
	RefreshSourceList();
	RefreshOutputList();

	// Lets the asset builder update the title bar
	PostActionSignal( new KeyValues( "UpdateFileName" ) );
}
 
	
//-----------------------------------------------------------------------------
// Hook into the DME panel framework
//-----------------------------------------------------------------------------
void CAssetBuilder::SetDmeElement( CDmeMakefile *pMakeFile )
{
	SetRootMakefile( pMakeFile );
}


//-----------------------------------------------------------------------------
// Refresh the source list
//-----------------------------------------------------------------------------
void CAssetBuilder::RefreshSourceList( )
{
	m_pSourcesList->RemoveAll();
	if ( !m_hMakefile.Get() )
		return;

	DmeMakefileType_t *pSourceTypes = m_hMakefile->GetSourceTypes();
	for ( int i = 0; pSourceTypes[i].m_pTypeName; ++i )
	{
		CUtlVector< CDmeHandle< CDmeSource > > sources;
		m_hMakefile->GetSources( pSourceTypes[i].m_pTypeName, sources );
		int nCount = sources.Count();
		for ( int j = 0; j < nCount; ++j )
		{
			char pFullPath[MAX_PATH];
			m_hMakefile->GetSourceFullPath( sources[j], pFullPath, sizeof(pFullPath) );

			KeyValues *pItemKeys = new KeyValues( "node", "type", pSourceTypes[i].m_pHumanReadableName );
			pItemKeys->SetString( "file", pFullPath );
			pItemKeys->SetInt( "sourceTypeIndex", i );
			pItemKeys->SetInt( "index", j ); // for sorting in the listpanel
			SetElementKeyValue( pItemKeys, "dmeSource", sources[j] );
			m_pSourcesList->AddItem( pItemKeys, 0, false, false );
		}
	}

	m_pSourcesList->SortList();
}


//-----------------------------------------------------------------------------
// Refreshes the output list
//-----------------------------------------------------------------------------
void CAssetBuilder::RefreshOutputList()
{
	m_pOutputList->RemoveAll();
	m_pCompile->SetEnabled( false );
	m_pPublish->SetEnabled( false );
	if ( !m_hMakefile.Get() )
		return;

	CUtlVector<CUtlString> outputs;
	m_hMakefile->GetOutputs( outputs );
	int nCount = outputs.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		KeyValues *pItemKeys = new KeyValues( "node", "type", "Output" );
		pItemKeys->SetString( "file", outputs[j] );
		pItemKeys->SetInt( "index", j );
		m_pOutputList->AddItem( pItemKeys, 0, false, false );
	}

	bool bEnabled = ( nCount > 0 ) && ( g_pDmeMakefileUtils != NULL );
	m_pCompile->SetEnabled( bEnabled );
	m_pPublish->SetEnabled( bEnabled );

	m_pOutputList->SortList();
}


//-----------------------------------------------------------------------------
// Selects a particular source
//-----------------------------------------------------------------------------
void CAssetBuilder::SelectSource( CDmeSource *pSource )
{
	int nItemID = m_pSourcesList->FirstItem();
	for ( ; nItemID != m_pSourcesList->InvalidItemID(); nItemID = m_pSourcesList->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pSourcesList->GetItem( nItemID );
		if ( GetElementKeyValue< CDmeSource >( kv, "dmeSource" ) != pSource )
			continue;

		m_pSourcesList->SetSingleSelectedItem( nItemID );
		return;
	}
}


//-----------------------------------------------------------------------------
// Called by the picker popped up in OnFileNew
//-----------------------------------------------------------------------------
void CAssetBuilder::OnPicked( KeyValues *kv )
{
	const char *pValue = kv->GetString( "choice" );
	
	KeyValues *pContextKeys = kv->FindKey( "OnAddSource" );
	if ( pContextKeys )
	{
		OnSourceFileAdded( "", pValue );
		return;
	}

	CDisableUndoScopeGuard guard;
	CDmeMakefile *pMakeFile = GetElement< CDmeMakefile >( g_pDataModel->CreateElement( pValue, "unnamed" ) );
	if ( !pMakeFile )
		return;

	DmeMakefileType_t *pType = pMakeFile->GetMakefileType();

	char pContext[MAX_PATH];
	Q_snprintf( pContext, sizeof(pContext), "asset_builder_session_%s", pType->m_pTypeName );

	char pStartingDir[MAX_PATH];
	pMakeFile->GetDefaultDirectory( pType->m_pDefaultDirectoryID, pStartingDir, sizeof(pStartingDir) );
	g_pFullFileSystem->CreateDirHierarchy( pStartingDir );

	KeyValues *pDialogKeys = new KeyValues( "NewSourceFileSelected", "makefileType", pValue );
	FileOpenDialog *pDialog = new FileOpenDialog( this, "Select Asset Builder File Name", false, pDialogKeys );
	pDialog->SetStartDirectoryContext( pContext, pStartingDir );
	pDialog->AddFilter( pType->m_pFileFilter, pType->m_pFileFilterString, true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
	DestroyElement( pMakeFile );
}


//-----------------------------------------------------------------------------
// Creates a new source file, hooks it in
//-----------------------------------------------------------------------------
void CAssetBuilder::OnNewSourceFile( )
{
	KeyValues *pKeyValues = GetSelectedSourceKeyvalues();
	CDmeSource *pSource = GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
	if ( !pSource )
		return;

	BuildAssetTypeList();

	PickerList_t typePickerList;
	PickerList_t &pickerList = BuildAssetSubTypeList( pSource->GetSourceMakefileTypes(), typePickerList );

	// Create a list indicating which type of asset to create
	CPickerFrame *pPicker = new CPickerFrame( this, "Select Sub-Asset Type", "Asset Type", "assetType" );
	pPicker->DoModal( pickerList );
}


//-----------------------------------------------------------------------------
// Called when the button to add a file is clicked
//-----------------------------------------------------------------------------
void CAssetBuilder::OnAddSource( )
{
	if ( !m_hMakefile.Get() )
		return;

	PickerList_t sourceType;

	DmeMakefileType_t *pSourceTypes = m_hMakefile->GetSourceTypes();
	for ( int i = 0; pSourceTypes[i].m_pTypeName; ++i )
	{
		if ( pSourceTypes[i].m_bIsSingleton )
		{
			if ( m_hMakefile->HasSourceOfType( pSourceTypes[i].m_pTypeName ) )
				continue;
		}

		int j = sourceType.AddToTail( );
		sourceType[j].m_pChoiceString = pSourceTypes[i].m_pHumanReadableName;
		sourceType[j].m_pChoiceValue = pSourceTypes[i].m_pTypeName;
	}

	if ( sourceType.Count() == 0 )
		return;

	KeyValues *pContextKeys = new KeyValues( "OnAddSource" );
	CPickerFrame *pPicker = new CPickerFrame( this, "Select Source Type", "Source Type", "sourceType" );
	pPicker->DoModal( sourceType, pContextKeys );
}


//-----------------------------------------------------------------------------
// Returns the curerntly selected row
//-----------------------------------------------------------------------------
int CAssetBuilder::GetSelectedRow( )
{
	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	return ( nItemID != -1 ) ? m_pSourcesList->GetItemCurrentRow( nItemID ) : -1;
}


//-----------------------------------------------------------------------------
// Selects a particular row of the source list
//-----------------------------------------------------------------------------
void CAssetBuilder::SelectSourceListRow( int nRow )
{
	int nVisibleRowCount = m_pSourcesList->GetItemCount();
	if ( nVisibleRowCount == 0 || nRow < 0 )
		return;

	if ( nRow >= nVisibleRowCount )
	{
		nRow = nVisibleRowCount - 1;
	}

	int nNewItemID = m_pSourcesList->GetItemIDFromRow( nRow );
	m_pSourcesList->SetSingleSelectedItem( nNewItemID );
}


//-----------------------------------------------------------------------------
// Called when the button to remove a file is clicked
//-----------------------------------------------------------------------------
void CAssetBuilder::OnRemoveSource( )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount == 0 || !m_hMakefile.Get() )
		return;

	int nRow = GetSelectedRow();
	Assert( nRow >= 0 );

	// Update the selection to be reasonable after deletion
	CDisableUndoScopeGuard guard;
	for ( int i = 0; i < nCount; ++i )
	{
		int nItemID = m_pSourcesList->GetSelectedItem( i );
		KeyValues *pKeyValues = m_pSourcesList->GetItem( nItemID );
		CDmeSource *pSource = GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
		if ( pSource )
		{
			m_hMakefile->RemoveSource( pSource );
			DestroyElement( pSource );
			SetDirty( );
		}
	}

	RefreshSourceList();

	SelectSourceListRow( nRow );
}


//-----------------------------------------------------------------------------
// Called to make a particular source the currently selected source
//-----------------------------------------------------------------------------
void CAssetBuilder::OnZoomInSource()
{
	// Called to zoom into the currently selected source
	CDmeSource *pSource = GetSelectedSource( );
	if ( !pSource )
		return;

	CDmeMakefile *pChild = m_hMakefile->FindDependentMakefile( pSource );
	if ( pChild )
	{
		CDmeHandle< CDmeMakefile > hChild;
		hChild = pChild;
		m_hMakefileStack.Push( hChild );
		SetCurrentMakefile( pChild );
	}
}


//-----------------------------------------------------------------------------
// Called to zoom out of a particular source
//-----------------------------------------------------------------------------
void CAssetBuilder::OnZoomOutSource()
{
	// Called to zoom into the currently selected source
	if ( m_hMakefileStack.Count() <= 1 )
		return;

	CDmeMakefile *pOldParent = m_hMakefileStack.Top().Get();
	m_hMakefileStack.Pop( );
	CDmeMakefile *pParent = m_hMakefileStack.Top().Get();
	if ( pParent )
	{
		SetCurrentMakefile( pParent );
		CDmeSource *pSource = pParent->FindAssociatedSource( pOldParent );
		if ( pSource )
		{
			SelectSource( pSource );
		}
	}
}


//-----------------------------------------------------------------------------
// Called when a key is typed
//-----------------------------------------------------------------------------
void CAssetBuilder::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE )
	{
		OnRemoveSource();
		return;
	}

	if ( code == KEY_ENTER )
	{
		OnZoomInSource();
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called when we're browsing for a source file and one was selected
//-----------------------------------------------------------------------------
void CAssetBuilder::OnSourceFileAdded( const char *pFileName, const char *pTypeName )
{
	CDmeSource *pSource = NULL;
	{
		CDisableUndoScopeGuard guard;
		pSource = m_hMakefile->AddSource( pTypeName, pFileName );
	}
	SetDirty( );
	RefreshSourceList( );
	SelectSource( pSource );
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CAssetBuilder::OnNewSourceFileSelected( const char *pFileName, KeyValues *kv )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount != 1 || !m_hMakefile.Get() )
		return;

	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pSourcesList->GetItem( nItemID );
	CDmeSource *pSource =  GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
	if ( !pSource )
		return;

	const char *pMakeFileType = kv->GetString( "makefileType" );

	{
		CDisableUndoScopeGuard guard;
		m_hMakefile->SetSourceFullPath( pSource, pFileName );

		DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( pFileName );
		CDmeMakefile *pSourceMakeFile = CreateElement< CDmeMakefile >( pMakeFileType, pFileName, fileid );
		pSourceMakeFile->SetFileName( pFileName );

		m_hMakefile->SetAssociation( pSource, pSourceMakeFile );
		SetDirty( );
	}

	pKeyValues->SetString( "file", pFileName );
	m_pSourcesList->ApplyItemChanges( nItemID );
	m_pSourcesList->SortList();
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CAssetBuilder::OnFileSelected( KeyValues *kv )
{
	const char *pFileName = kv->GetString( "fullpath", NULL );
	if ( !pFileName )
		return;

	KeyValues *pDialogKeys = kv->FindKey( "SelectSourceFile" );
	if ( pDialogKeys )
	{
		OnSourceFileNameChanged( pFileName );
		return;
	}

	pDialogKeys = kv->FindKey( "NewSourceFileSelected" );
	if ( pDialogKeys )
	{
		if ( !g_pFullFileSystem->FileExists( pFileName ) )
		{
			OnNewSourceFileSelected( pFileName, pDialogKeys );
		}
		else
		{
			OnSourceFileNameChanged( pFileName );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Shows the source file browser
//-----------------------------------------------------------------------------
void CAssetBuilder::ShowSourceFileBrowser( const char *pTitle, DmeMakefileType_t *pSourceType, KeyValues *pDialogKeys )
{
	char pContext[MAX_PATH];
	Q_snprintf( pContext, sizeof(pContext), "asset_builder_session_%s", pSourceType->m_pTypeName );

	char pStartingDir[MAX_PATH];
	m_hMakefile->GetDefaultDirectory( pSourceType->m_pDefaultDirectoryID, pStartingDir, sizeof(pStartingDir) );
	g_pFullFileSystem->CreateDirHierarchy( pStartingDir );

	FileOpenDialog *pDialog = new FileOpenDialog( this, pTitle, true, pDialogKeys );
	pDialog->SetStartDirectoryContext( pContext, pStartingDir );
	pDialog->AddFilter( pSourceType->m_pFileFilter, pSourceType->m_pFileFilterString, true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
}


//-----------------------------------------------------------------------------
// Called when the button to browse for a source file is clicked
//-----------------------------------------------------------------------------
void CAssetBuilder::OnBrowseSourceFile( )
{
	KeyValues *pKeyValues = GetSelectedSourceKeyvalues();
	if ( !pKeyValues )
		return;

	int nSourceTypeIndex = pKeyValues->GetInt( "sourceTypeIndex", -1 );

	KeyValues *pDialogKeys = new KeyValues( "SelectSourceFile" );
	DmeMakefileType_t &sourceType = m_hMakefile->GetSourceTypes()[nSourceTypeIndex];
	ShowSourceFileBrowser( "Select Source File", &sourceType, pDialogKeys );
}


//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------
void CAssetBuilder::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "OnCompile" ) )
	{
		OnCompile();
		return;
	}

	if ( !Q_stricmp( pCommand, "OnAbortCompile" ) )
	{
		OnAbortCompile();
		return;
	}

	if ( !Q_stricmp( pCommand, "OnPublish" ) )
	{
		OnPublish();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CAssetBuilder::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CAssetBuilder::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hMakefile.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	int nItemID = kv->GetInt( "itemID", -1 );

	if ( pPanel != m_pSourcesList )
		return;

	m_hContextMenu = new Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "Add...", new KeyValues( "AddSource" ), this );
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount > 0 )
	{
		m_hContextMenu->AddMenuItem( "Remove", new KeyValues( "RemoveSource" ), this );
	}

	bool bShowZoomIn = false;
	bool bShowZoomOut = m_hMakefileStack.Count() > 1;
	bool bShowLoadSourceFile = false;
	bool bHasValidSourceFile = false;
	if ( nCount == 1 && nItemID != -1 )
	{
		KeyValues *kv = m_pSourcesList->GetItem( nItemID );
		CDmeSource *pSource = GetElementKeyValue< CDmeSource >( kv, "dmeSource" );
		if ( pSource )
		{
			bHasValidSourceFile = pSource->GetRelativeFileName()[0] != 0;
			if ( m_hMakefile->FindDependentMakefile( pSource ) )
			{
				bShowZoomIn = true;
			}
			else
			{
				bShowLoadSourceFile = bHasValidSourceFile;
			}
		}
	}

	if ( bShowZoomIn || bShowZoomOut )
	{
		m_hContextMenu->AddSeparator();
		if ( bShowZoomIn )
		{
			m_hContextMenu->AddMenuItem( "Zoom In", new KeyValues( "ZoomInSource" ), this );
		}
		if ( bShowZoomOut )
		{
			m_hContextMenu->AddMenuItem( "Zoom Out", new KeyValues( "ZoomOutSource" ), this );
		}
	}

	if ( nCount == 1 )
	{
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "New Source File...", new KeyValues( "NewSourceFile" ), this );
		m_hContextMenu->AddMenuItem( "Select Source File...", new KeyValues( "BrowseSourceFile" ), this );
		if ( bShowLoadSourceFile )
		{
			m_hContextMenu->AddMenuItem( "Load Source File", new KeyValues( "LoadSourceFile" ), this );
		}
		if ( bHasValidSourceFile )
		{
			m_hContextMenu->AddMenuItem( "Edit Source File", new KeyValues( "EditSourceFile" ), this );
		}
	}

	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CAssetBuilder::OnSourceItemSelectionChanged( )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount != 1 )
	{
		m_pDmePanel->SetDmeElement( NULL );
		return;
	}

	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pSourcesList->GetItem( nItemID );
	CDmeSource *pSource = GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
	m_pDmePanel->SetDmeElement( pSource );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CAssetBuilder::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pSourcesList )
	{
		OnSourceItemSelectionChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CAssetBuilder::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pSourcesList )
	{
		OnSourceItemSelectionChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Returns the selected source (if there's only 1 source selected)
//-----------------------------------------------------------------------------
CDmeSource *CAssetBuilder::GetSelectedSource( )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount != 1 || !m_hMakefile.Get() )
		return NULL;

	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pSourcesList->GetItem( nItemID );
	return GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
}

KeyValues *CAssetBuilder::GetSelectedSourceKeyvalues( )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount != 1 || !m_hMakefile.Get() )
		return NULL;

	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	return m_pSourcesList->GetItem( nItemID );
}


//-----------------------------------------------------------------------------
// Called when the source file name changes
//-----------------------------------------------------------------------------
void CAssetBuilder::OnSourceFileNameChanged( const char *pFileName )
{
	int nCount = m_pSourcesList->GetSelectedItemsCount();
	if ( nCount != 1 || !m_hMakefile.Get() )
		return;

	int nItemID = m_pSourcesList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pSourcesList->GetItem( nItemID );
	CDmeSource *pSource = GetElementKeyValue< CDmeSource >( pKeyValues, "dmeSource" );
	if ( !pSource )
		return;

	{
		CDisableUndoScopeGuard guard;
		m_hMakefile->SetSourceFullPath( pSource, pFileName );
		SetDirty( );
	}

	pKeyValues->SetString( "file", pFileName );
	m_pSourcesList->ApplyItemChanges( nItemID );
	m_pSourcesList->SortList();
}


//-----------------------------------------------------------------------------
// Called during compilation
//-----------------------------------------------------------------------------
void CAssetBuilder::OnLoadSourceFile()
{
	CDmeSource *pSource = GetSelectedSource( );
	if ( !pSource )
		return;

	char pFullPath[MAX_PATH];
	m_hMakefile->GetSourceFullPath( pSource, pFullPath, sizeof(pFullPath) );

	{
		CDisableUndoScopeGuard guard;

		CDmElement *pRoot;
		CDmeMakefile *pMakeFile = ReadMakefile( pFullPath, &pRoot );
		if ( !pMakeFile )
			return;

		// Successfully loaded a makefile. Set up the association.
		m_hMakefile->SetAssociation( pSource, pMakeFile );

		// Refresh the dme panel... setting association could provoke changes
		m_pDmePanel->SetDmeElement( pSource, true );
	}
}


//-----------------------------------------------------------------------------
// Called to open an external editor for this source file
//-----------------------------------------------------------------------------
void CAssetBuilder::OnEditSourceFile()
{
	CDmeSource *pSource = GetSelectedSource( );
	if ( pSource )
	{
		pSource->OpenEditor();
	}
}


//-----------------------------------------------------------------------------
// Finishes compilation
//-----------------------------------------------------------------------------
void CAssetBuilder::FinishCompilation( CompilationState_t state )
{
	// NOTE: compilation can cause the makefile to be completely
	// rebuilt if it's sitting in the output file. Therefore,
	// Detach the source preview panel from the source and refresh the
	// source list to get it to correctly reconnect to the new source elements
	m_pDmePanel->SetDmeElement( NULL );
	int nRow = GetSelectedRow();

	m_pOututPreviewPanel->SetDmeElement( m_hMakefile, true, "DmeMakeFileOutputPreview" );
	m_bIsCompiling = false;

	// NOTE: Sort of side-effecty. These two things must be done after
	// m_pOututPreviewPanel->SetDmeElement, since that's what reloads the output element,
	// which is also what can cause a reload of the makefile
	RefreshSourceList();
	SelectSourceListRow( nRow );

	// Lets the asset builder update the title bar
	// (compilation could have changed the dirty state if the makefile is in the file)
	PostActionSignal( new KeyValues( "UpdateFileName" ) );

	if ( state == COMPILATION_FAILED )
	{
		char pBuf[256];
		Q_snprintf( pBuf, sizeof(pBuf), "Compilation Error (return code %d)", g_pDmeMakefileUtils->GetExitCode() );
		m_pCompileStatusBar->SetStatus( CCompileStatusBar::COMPILATION_FAILED, pBuf );
	}
	else
	{
		m_pCompileStatusBar->SetStatus( CCompileStatusBar::COMPILATION_SUCCESSFUL, "Compile Successful!" );
	}
}


//-----------------------------------------------------------------------------
// Called during compilation
//-----------------------------------------------------------------------------
void CAssetBuilder::OnTick()
{
	BaseClass::OnTick();

	if ( m_bIsCompiling )
	{
		int nLen = g_pDmeMakefileUtils->GetCompileOutputSize( );
		char *pBuf = (char*)_alloca( nLen+1 );
		CompilationState_t state = g_pDmeMakefileUtils->UpdateCompilation( pBuf, nLen );
		if ( nLen > 0 )
		{
			m_pCompileOutput->InsertString( pBuf );
		}
		Assert( m_hMakefile.Get() );
		if ( state != COMPILATION_NOT_COMPLETE )
		{
			FinishCompilation( state );
		}
	}

	if ( !m_bIsCompiling )
	{
		m_pAbortCompile->SetEnabled( false );
		vgui::ivgui()->RemoveTickSignal( GetVPanel() );
	}
}


//-----------------------------------------------------------------------------
// Abort compile asset
//-----------------------------------------------------------------------------
void CAssetBuilder::OnAbortCompile()
{
	if ( m_bIsCompiling )
	{
		g_pDmeMakefileUtils->AbortCurrentCompilation();
		m_bIsCompiling = false;
		m_pAbortCompile->SetEnabled( false );
		m_pCompileStatusBar->SetStatus( CCompileStatusBar::COMPILATION_FAILED, "Compile Aborted" );
	}
}


//-----------------------------------------------------------------------------
// Compile asset
//-----------------------------------------------------------------------------
void CAssetBuilder::OnCompile( )
{
	if ( !m_hMakefile.Get() )
		return;

	OnAbortCompile();

	m_pCompileOutput->SetText( "" );
	g_pDmeMakefileUtils->PerformCompile( m_hMakefile, false ); 
	m_bIsCompiling = true;
	m_pAbortCompile->SetEnabled( true );
	m_pCompileStatusBar->SetStatus( CCompileStatusBar::CURRENTLY_COMPILING, "Compiling..." );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 10 );
}


//-----------------------------------------------------------------------------
// Compile, then publish
//-----------------------------------------------------------------------------
void CAssetBuilder::OnPublish( )
{
	if ( !m_hMakefile.Get() )
		return;

	OnAbortCompile();

	m_pCompileOutput->SetText( "" );
	g_pDmeMakefileUtils->PerformCompile( m_hMakefile, false ); 
	m_bIsCompiling = true;
	m_pAbortCompile->SetEnabled( true );
	m_pCompileStatusBar->SetStatus( CCompileStatusBar::CURRENTLY_COMPILING, "Compiling..." );
	vgui::ivgui()->AddTickSignal( GetVPanel(), 10 );
}



//-----------------------------------------------------------------------------
// Purpose: Constructor, destructor
//-----------------------------------------------------------------------------
CAssetBuilderFrame::CAssetBuilderFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "AssetBuilderFrame" )
{
	m_TitleString = pTitle;

	SetMenuButtonVisible( true );
	SetImages( "resource/downarrow" );

	m_pAssetBuilder = new CAssetBuilder( this, "AssetBuilder" );
	m_pAssetBuilder->AddActionSignalTarget( this );

	vgui::Menu *pMenu = new vgui::Menu( NULL, "FileMenu" );
	pMenu->AddMenuItem( "new", "#AssetBuilder_FileNew", new KeyValues( "FileNew" ), this );
	pMenu->AddMenuItem( "open", "#AssetBuilder_FileOpen", new KeyValues( "FileOpen" ), this );
	pMenu->AddMenuItem( "save", "#AssetBuilder_FileSave", new KeyValues( "FileSave" ), this );
	pMenu->AddMenuItem( "saveas", "#AssetBuilder_FileSaveAs", new KeyValues( "FileSaveAs" ), this );
	SetSysMenu( pMenu );

	m_pFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_pFileOpenStateMachine->AddActionSignalTarget( this );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/assetbuilderframe.res" );

	UpdateFileName();
}

CAssetBuilderFrame::~CAssetBuilderFrame()
{
}


//-----------------------------------------------------------------------------
// Inherited from IFileOpenStateMachineClient
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Compute starting directory
	char pStartingDir[ MAX_PATH ];
	GetModContentSubdirectory( "", pStartingDir, sizeof(pStartingDir) );

	if ( bOpenFile )
	{
		// Clear out the existing makefile if we're opening a file
		m_pAssetBuilder->SetRootMakefile( NULL );
		pDialog->SetTitle( "Open Asset MakeFile", true );
	}
	else
	{
		pDialog->SetTitle( "Save Asset MakeFile As", true );
	}

	pDialog->SetStartDirectoryContext( "asset_browser_makefile", pStartingDir );
	pDialog->AddFilter( "*.*", "All Files (*.*)", false );
	pDialog->AddFilter( "*.dmx", "Asset MakeFiles (*.dmx)", true, "keyvalues2" );
}

bool CAssetBuilderFrame::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	CDmElement *pRoot;
	CDmeMakefile *pMakeFile = ReadMakefile( pFileName, &pRoot );
	if ( !pMakeFile )
		return false;

	Reset( pMakeFile );
	return true;
}

bool CAssetBuilderFrame::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Recompute relative paths for each source now that we know the file name
	// NOTE: This also updates the name of the fileID in the datamodel system
	CDmeMakefile *pMakefile = m_pAssetBuilder->GetMakeFile();
	bool bOk;
	{
		CDisableUndoScopeGuard guard;
		bOk = pMakefile->SetFileName( pFileName );
	}
	if ( !bOk )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#AssetBuilder_CannotRenameSourceFiles", "#AssetBuilder_CannotRenameSourceFilesText", this );
		pError->DoModal();
		return false;
	}

	CDmElement *pRoot = GetElement< CDmElement >( g_pDataModel->GetFileRoot( pMakefile->GetFileId() ) );
	if ( !pRoot )
	{
		pRoot = pMakefile;
	}
	bOk = g_pDataModel->SaveToFile( pFileName, NULL, g_pDataModel->GetDefaultEncoding( pFileFormat ), pFileFormat, pRoot ); 
	m_pAssetBuilder->Refresh();
	return bOk;
}


//-----------------------------------------------------------------------------
// Updates the file name
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::UpdateFileName( )
{
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( !pMakeFile )
	{
		SetTitle( m_TitleString.Get(), true );
		return;
	}

	DmeMakefileType_t *pMakefileType = pMakeFile->GetMakefileType();

	DmFileId_t fileId = pMakeFile->GetFileId();
	const char *pFileName = ( fileId != DMFILEID_INVALID ) ? g_pDataModel->GetFileName( fileId ) : "<unnamed>";
	if ( !pFileName || !pFileName[0] )
	{
		pFileName = "<unnamed>";
	}

	char pBuf[2*MAX_PATH];
	if ( m_TitleString.Get() )
	{
		Q_snprintf( pBuf, sizeof(pBuf), "%s - %s - %s%s", m_TitleString.Get(), pMakefileType->m_pHumanReadableName, pFileName, pMakeFile->IsDirty() ? " *" : "" );
	}
	else
	{
		Q_snprintf( pBuf, sizeof(pBuf), "%s - %s%s", pMakefileType->m_pHumanReadableName, pFileName, pMakeFile->IsDirty() ? " *" : "" );
	}
	SetTitle( pBuf, true );
}


//-----------------------------------------------------------------------------
// Marks the file as dirty (or not)
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::SetDirty( bool bDirty )
{
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( pMakeFile && ( pMakeFile->IsDirty() != bDirty ) )
	{
		pMakeFile->SetDirty( bDirty );

		// Necessary because we draw a * if it's dirty before the name
		UpdateFileName();
	}
}


//-----------------------------------------------------------------------------
// Called when the asset builder changes something
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnDmeElementChanged()
{
	SetDirty( true );
}


//-----------------------------------------------------------------------------
// Resets the state
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::Reset( CDmeMakefile *pMakeFile )
{
	// NOTE: Don't need to call SetDirty because we call UpdateFileName below
	m_pAssetBuilder->SetRootMakefile( pMakeFile );
	UpdateFileName();
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for selecting the new asset name is selected
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnPerformFileNew( KeyValues *kv )
{
	const char *pMakefileType = kv->GetString( "makefileType" );
	const char *pFileName = kv->GetString( "fileName" );
	CDmeMakefile *pMakeFile;
	{
		DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( pFileName );
		CDisableUndoScopeGuard guard;
		pMakeFile = CreateElement< CDmeMakefile >( pMakefileType, pFileName, fileid );
	}
	if ( !pMakeFile )
		return;

	pMakeFile->SetFileName( pFileName );
	Reset( pMakeFile );
	SetDirty( true );
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnFileSelected( KeyValues *kv )
{
	const char *pFileName = kv->GetString( "fullpath", NULL );
	if ( !pFileName )
		return;

	KeyValues *pDialogKeys = kv->FindKey( "OnFileNew" );
	if ( pDialogKeys )
	{
		KeyValues *pOkCommand = new KeyValues( "PerformFileNew", "makefileType", pDialogKeys->GetString( "makefileType" ) );
		pOkCommand->SetString( "fileName", pFileName );
		OverwriteFileDialog( this, pFileName, pOkCommand );
		return;
	}
}


//-----------------------------------------------------------------------------
// Called by the picker popped up in OnFileNew
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnPicked( KeyValues *kv )
{
	const char *pValue = kv->GetString( "choice" );

	CDisableUndoScopeGuard guard;
	CDmeMakefile *pMakeFile = GetElement< CDmeMakefile >( g_pDataModel->CreateElement( pValue, "unnamed" ) );
	if ( !pMakeFile )
		return;

	DmeMakefileType_t *pType = pMakeFile->GetMakefileType();

	char pContext[MAX_PATH];
	Q_snprintf( pContext, sizeof(pContext), "asset_builder_session_%s", pType->m_pTypeName );

	char pStartingDir[MAX_PATH];
	pMakeFile->GetDefaultDirectory( pType->m_pDefaultDirectoryID, pStartingDir, sizeof(pStartingDir) );
	g_pFullFileSystem->CreateDirHierarchy( pStartingDir );

	char pTitle[MAX_PATH];
	Q_snprintf( pTitle, sizeof(pTitle), "Select %s File Name", pType->m_pHumanReadableName );

	KeyValues *pDialogKeys = new KeyValues( "OnFileNew", "makefileType", pValue );
	FileOpenDialog *pDialog = new FileOpenDialog( this, pTitle, false, pDialogKeys );
	pDialog->SetStartDirectoryContext( pContext, pStartingDir );
	pDialog->AddFilter( pType->m_pFileFilter, pType->m_pFileFilterString, true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
	DestroyElement( pMakeFile );
}


//-----------------------------------------------------------------------------
// Called by the file open state machine when an operation has completed
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnFileStateMachineFinished( KeyValues *pKeyValues )
{
	KeyValues *pNewFile = pKeyValues->FindKey( "FileNew" );
	if ( pNewFile )
	{
		if ( pKeyValues->GetInt( "wroteFile", 0 ) != 0 )
		{
			SetDirty( false );
			UpdateFileName();
		}
		if ( pKeyValues->GetInt( "completionState", FileOpenStateMachine::IN_PROGRESS ) == FileOpenStateMachine::SUCCESSFUL )
		{
			ShowNewAssetPicker();
		}
		return;
	}

	KeyValues *pSaveFile = pKeyValues->FindKey( "FileSave" );
	if ( pSaveFile )
	{
		if ( pKeyValues->GetInt( "wroteFile", 0 ) != 0 )
		{
			SetDirty( false );
			UpdateFileName();
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Shows a picker for creating a new asset
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::ShowNewAssetPicker( )
{
	BuildAssetTypeList();

	// Create a list indicating which type of asset to create
	CPickerFrame *pPicker = new CPickerFrame( this, "Select Asset Type", "Asset Type", "assetType" );
	pPicker->DoModal( s_AssetTypes );
}


//-----------------------------------------------------------------------------
// Creates a new file
//-----------------------------------------------------------------------------
void CAssetBuilderFrame::OnFileNew( )
{
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( pMakeFile && pMakeFile->IsDirty() )
	{
		KeyValues *pContextKeyValues = new KeyValues( "FileNew" );
		const char *pFileName = g_pDataModel->GetFileName( pMakeFile->GetFileId() );
		m_pFileOpenStateMachine->SaveFile( pContextKeyValues, pFileName, ASSET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY );
		return;
	}

	ShowNewAssetPicker();
}

void CAssetBuilderFrame::OnFileOpen( )
{
	int nFlags = 0;
	const char *pFileName = NULL;
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( pMakeFile && pMakeFile->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pFileName = g_pDataModel->GetFileName( pMakeFile->GetFileId() );
	}
	KeyValues *pContextKeyValues = new KeyValues( "FileOpen" );
	m_pFileOpenStateMachine->OpenFile( ASSET_FILE_FORMAT, pContextKeyValues, pFileName, NULL, nFlags );
}

void CAssetBuilderFrame::OnFileSave( )
{
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( !pMakeFile )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "FileSave" );
	const char *pFileName = g_pDataModel->GetFileName( pMakeFile->GetFileId() );
	m_pFileOpenStateMachine->SaveFile( pContextKeyValues, pFileName, ASSET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS );
}

void CAssetBuilderFrame::OnFileSaveAs( )
{
	CDmeMakefile *pMakeFile = m_pAssetBuilder->GetMakeFile();
	if ( !pMakeFile )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "FileSave" );
	m_pFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, ASSET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS );
}


