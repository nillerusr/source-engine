//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "stdafx.h"
#include "Manifest.h"
#include "CustomMessages.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "MapInstance.h"
#include "ToolManager.h"
#include "ChunkFile.h"
#include "ManifestDialog.h"
#include "History.h"
#include "HelperFactory.h"
#include "SaveInfo.h"
#include "tier2/tier2.h"
#include "p4lib/ip4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_DYNCREATE(CManifest, CMapDoc)


BEGIN_MESSAGE_MAP(CManifest, CMapDoc)
	//{{AFX_MSG_MAP(CManifest)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


IMPLEMENT_MAPCLASS( CManifestInstance )


//-----------------------------------------------------------------------------
// Purpose: default constructor
//-----------------------------------------------------------------------------
CManifestMap::CManifestMap( void )
{
	m_Map = NULL;
	m_RelativeMapFileName = "";
	m_AbsoluteMapFileName = "";
	m_FriendlyName = "unnamed";
	m_bTopLevelMap = false;
	m_bPrimaryMap = false;
	m_bProtected = false;
	m_bReadOnly = false;
	m_bIsVersionControlled = false;
	m_bCheckedOut = false;
	m_bDefaultCheckin = false;
	m_bVisible = true;
	m_Entity = NULL;
	m_InternalID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the manifest map is editable
//-----------------------------------------------------------------------------
bool CManifestMap::IsEditable( void )
{
	return ( m_bProtected == false && m_bReadOnly == false && m_bPrimaryMap );
}


//-----------------------------------------------------------------------------
// Purpose: default constructor
//-----------------------------------------------------------------------------
CManifestInstance::CManifestInstance( void ) :
	CMapEntity()
{
	m_pManifestMap = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: default constructor
//-----------------------------------------------------------------------------
CManifestInstance::CManifestInstance( CManifestMap *pManifestMap ) :
	CMapEntity()
{
	m_pManifestMap = pManifestMap;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the manifest map this instance owns is editable
//-----------------------------------------------------------------------------
bool CManifestInstance::IsEditable( void )
{
	return m_pManifestMap->IsEditable();
}


//-----------------------------------------------------------------------------
// Purpose: default constructor
//-----------------------------------------------------------------------------
CManifest::CManifest( void ) :
	CMapDoc()
{
	m_bIsValid = false;
	m_bRelocateSave = false;
	m_ManifestDir[ 0 ] = 0;
	m_pPrimaryMap = NULL;
	m_ManifestWorld = NULL;
	m_NextInternalID = 1;
	m_bManifestChanged = false;
	m_bManifestUserPrefsChanged = false;
	m_pSaveUndo = m_pUndo;
	m_pSaveRedo = m_pRedo;
	m_bReadOnly = true;
	m_bIsVersionControlled = false;
	m_bCheckedOut = false;
	m_bDefaultCheckin = false;
}


//-----------------------------------------------------------------------------
// Purpose: default destructor
//-----------------------------------------------------------------------------
CManifest::~CManifest( void )
{
	m_Maps.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: this function will parse through the known keys for the manifest map entry
// Input  : szKey - the key name
//			szValue - the value
//			pManifestMap - the manifest map this belongs to
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadKeyInfoCallback( const char *szKey, const char *szValue, CManifest *pDoc )
{
	if ( !stricmp( szKey, "NextInternalID" ) )
	{
		pDoc->m_NextInternalID = atoi( szValue );
	}

	return ChunkFile_Ok;
}


//-----------------------------------------------------------------------------
// Purpose: this function is responsible for setting up the manifest map about to be read in
// Input  : pFile - the chunk file being read
//			pDoc - the owning manifest document
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestInfoCallback( CChunkFile *pFile, CManifest *pDoc )
{
	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadKeyInfoCallback, pDoc );

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: this function will parse through the known keys for the manifest map entry
// Input  : szKey - the key name
//			szValue - the value
//			pManifestMap - the manifest map this belongs to
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadKeyCallback( const char *szKey, const char *szValue, CManifestMap *pManifestMap )
{
	if ( !stricmp( szKey, "InternalID" ) )
	{
		pManifestMap->m_InternalID = atoi( szValue );
	}
	else if ( !stricmp( szKey, "Name" ) )
	{
		pManifestMap->m_FriendlyName = szValue;
	}
	else if ( !stricmp( szKey, "File" ) )
	{
		pManifestMap->m_RelativeMapFileName = szValue;
		pManifestMap->m_AbsoluteMapFileName += szValue;
		if ( !pManifestMap->m_Map->LoadVMF( pManifestMap->m_AbsoluteMapFileName, VMF_LOAD_ACTIVATE | VMF_LOAD_IS_SUBMAP ) )
		{
			delete pManifestMap->m_Map;
			pManifestMap->m_Map = NULL;
		}
		pManifestMap->m_bReadOnly = true;
	}
	else if ( !stricmp( szKey, "TopLevel" ) )
	{
		pManifestMap->m_bTopLevelMap = ( atoi( szValue ) == 1 );
	}

	return ChunkFile_Ok;
}


//-----------------------------------------------------------------------------
// Purpose: this function is responsible for setting up the manifest map about to be read in
// Input  : pFile - the chunk file being read
//			pDoc - the owning manifest document
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestVMFCallback( CChunkFile *pFile, CManifest *pDoc )
{
	char FileName[ MAX_PATH ];

	strcpy( FileName, pDoc->m_ManifestDir );

	CManifestMap	*pManifestMap = pDoc->CreateNewMap( FileName, "", false );
	SetActiveMapDoc( pManifestMap->m_Map );

	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadKeyCallback, pManifestMap );

	if ( pManifestMap->m_Map )
	{
		pManifestMap->m_Map->SetEditable( false );
	}
	SetActiveMapDoc( pDoc );

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: this function will load the VMF chunk
// Input  : pFile - the chunk file being read
//			pDoc - the owning manifest document
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestMapsCallback( CChunkFile *pFile, CManifest *pDoc )
{
	CChunkHandlerMap Handlers;
	Handlers.AddHandler( "VMF", ( ChunkHandler_t )LoadManifestVMFCallback, pDoc );
	pFile->PushHandlers(&Handlers);

	ChunkFileResult_t eResult = ChunkFile_Ok;

	eResult = pFile->ReadChunk();

	pFile->PopHandlers();

	return( eResult );
}


typedef struct SManifestLoadPrefs
{
	CManifest		*pDoc;
	CManifestMap	*pManifestMap;
} TManifestLoadPrefs;


//-----------------------------------------------------------------------------
// Purpose: this function will parse through the known keys for the manifest map entry
// Input  : szKey - the key name
//			szValue - the value
//			pManifestMap - the manifest map this belongs to
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadKeyPrefsCallback( const char *szKey, const char *szValue, TManifestLoadPrefs *pManifestLoadPrefs )
{
	if ( !stricmp( szKey, "InternalID" ) )
	{
		pManifestLoadPrefs->pManifestMap = pManifestLoadPrefs->pDoc->FindMapByID( atoi( szValue ) );
	}
	else if ( !stricmp( szKey, "IsPrimary" ) )
	{
		if ( pManifestLoadPrefs->pManifestMap )
		{
			pManifestLoadPrefs->pManifestMap->m_bPrimaryMap = ( atoi( szValue ) == 1 );
		}
	}
	else if ( !stricmp( szKey, "IsProtected" ) )
	{
		if ( pManifestLoadPrefs->pManifestMap )
		{
			pManifestLoadPrefs->pManifestMap->m_bProtected = ( atoi( szValue ) == 1 );
		}
	}
	else if ( !stricmp( szKey, "IsVisible" ) )
	{
		if ( pManifestLoadPrefs->pManifestMap )
		{
			pManifestLoadPrefs->pManifestMap->m_bVisible = ( atoi( szValue ) == 1 );
		}
	}

	return ChunkFile_Ok;
}


//-----------------------------------------------------------------------------
// Purpose: this function is responsible for setting up the manifest map about to be read in
// Input  : pFile - the chunk file being read
//			pDoc - the owning manifest document
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestVMFPrefsCallback( CChunkFile *pFile, CManifest *pDoc )
{
	TManifestLoadPrefs	ManifestLoadPrefs;
	
	ManifestLoadPrefs.pDoc = pDoc;
	ManifestLoadPrefs.pManifestMap = NULL;

	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadKeyPrefsCallback, &ManifestLoadPrefs );

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: this function will load the VMF chunk
// Input  : pFile - the chunk file being read
//			pDoc - the owning manifest document
// Output : ChunkFileResult_t - result of the parsing
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestMapsPrefsCallback( CChunkFile *pFile, CManifest *pDoc )
{
	CChunkHandlerMap Handlers;
	Handlers.AddHandler( "VMF", ( ChunkHandler_t )LoadManifestVMFPrefsCallback, pDoc );
	pFile->PushHandlers(&Handlers);

	ChunkFileResult_t eResult = ChunkFile_Ok;

	eResult = pFile->ReadChunk();

	pFile->PopHandlers();

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
ChunkFileResult_t CManifest::LoadManifestCordoningPrefsCallback( CChunkFile *pFile, CManifest *pDoc )
{
	CChunkHandlerMap Handlers;
	Handlers.AddHandler( "cordons", ( ChunkHandler_t )CMapDoc::LoadCordonCallback, pDoc );
	pFile->PushHandlers(&Handlers);

	ChunkFileResult_t eResult = ChunkFile_Ok;

	eResult = pFile->ReadChunk();

	pFile->PopHandlers();

	return( eResult );
}


//-----------------------------------------------------------------------------
// Purpose: This function will load in a vmf manifest
// Input  : pszFileName - the file name of the manifest to load
// Output : returns true if the load was successful
//-----------------------------------------------------------------------------
bool CManifest::LoadVMFManifest( const char *pszFileName )
{
	FILE *fp = fopen( pszFileName, "rb" );
	if ( !fp )
	{
		return false;
	}

	V_StripExtension( pszFileName, m_ManifestDir, sizeof( m_ManifestDir ) );
	strcat( m_ManifestDir, "\\" );

	CChunkFile File;
	ChunkFileResult_t eResult = File.Open( pszFileName, ChunkFile_Read );

	m_bLoading = true;

	if (eResult == ChunkFile_Ok)
	{
		//
		// Set up handlers for the subchunks that we are interested in.
		//
		CChunkHandlerMap Handlers;
		Handlers.AddHandler( "Info", ( ChunkHandler_t )CManifest::LoadManifestInfoCallback, this );
		Handlers.AddHandler( "Maps", ( ChunkHandler_t )CManifest::LoadManifestMapsCallback, this );

		Handlers.SetErrorHandler( ( ChunkErrorHandler_t )CMapDoc::HandleLoadError, this);

		File.PushHandlers(&Handlers);

		while (eResult == ChunkFile_Ok)
		{
			eResult = File.ReadChunk();
		}

		if (eResult == ChunkFile_EOF)
		{
			eResult = ChunkFile_Ok;
		}

		File.PopHandlers();
	}

	if (eResult == ChunkFile_Ok)
	{
	}
	else
	{
		GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error loading manifest!", MB_OK | MB_ICONEXCLAMATION );
	}

	if ( GetNumMaps() == 0 )
	{
		GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Manifest file does not contain any maps!", MB_OK | MB_ICONEXCLAMATION );
		return false;
	}

	SetActiveMapDoc( this );
	Postload( pszFileName );
	m_ManifestWorld->PostloadWorld();

	bool bSetIDs = false;

	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		if ( pManifestMap->m_InternalID == 0 )
		{
			pManifestMap->m_InternalID = m_NextInternalID;
			m_NextInternalID++;
			bSetIDs = true;
		}

		if ( pManifestMap->m_Map == NULL || pManifestMap->m_Map->GetMapWorld() == NULL )
		{
			pManifestMap->m_bPrimaryMap = false;
			RemoveSubMap( pManifestMap );
			i = -1;
		}
	}

	LoadVMFManifestUserPrefs( pszFileName );

	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		if ( pManifestMap->m_bPrimaryMap )
		{
			SetPrimaryMap( pManifestMap );
		}
	}

	if ( !m_pPrimaryMap )
	{
		SetPrimaryMap( GetMap( 0 ) );
	}

	m_bLoading = false;
	m_bIsValid = true;

	m_bManifestChanged = bSetIDs;

	GetMainWnd()->m_ManifestFilterControl.UpdateManifestList();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: this function will load the user prefs for the manifest file.
// Input  : pszFileName - the manifest file name.
// Output : true if prefs were loaded.
//-----------------------------------------------------------------------------
bool CManifest::LoadVMFManifestUserPrefs( const char *pszFileName )
{
	char		UserName[ MAX_PATH ], FileName[ MAX_PATH ], UserPrefsFileName[ MAX_PATH ];
	DWORD		UserNameSize;

	m_bManifestUserPrefsChanged = false;

	UserNameSize = sizeof( UserName );
	if ( GetUserName( UserName, &UserNameSize ) == 0 )
	{
		strcpy( UserPrefsFileName, "default" );
	}

	strcpy( FileName, m_ManifestDir );
	sprintf( UserPrefsFileName, "%s.vmm_prefs", UserName );
	strcat( FileName, UserPrefsFileName );

	FILE *fp = fopen( FileName, "rb" );
	if ( !fp )
	{
		return false;
	}

	CChunkFile File;
	ChunkFileResult_t eResult = File.Open( FileName, ChunkFile_Read );

	m_bLoading = true;

	if ( eResult == ChunkFile_Ok )
	{
		//
		// Set up handlers for the subchunks that we are interested in.
		//
		CChunkHandlerMap Handlers;
		Handlers.AddHandler( "Maps", ( ChunkHandler_t )CManifest::LoadManifestMapsPrefsCallback, this );
		Handlers.AddHandler( "cordoning", ( ChunkHandler_t )CManifest::LoadManifestCordoningPrefsCallback, this );

		Handlers.SetErrorHandler( ( ChunkErrorHandler_t )CMapDoc::HandleLoadError, this);

		File.PushHandlers(&Handlers);

		while( eResult == ChunkFile_Ok )
		{
			eResult = File.ReadChunk();
		}

		if ( eResult == ChunkFile_EOF )
		{
			eResult = ChunkFile_Ok;
		}

		File.PopHandlers();
	}

	if ( eResult == ChunkFile_Ok )
	{
	}
	else
	{
		// no pref message for now
//		GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error loading manifest!", MB_OK | MB_ICONEXCLAMATION );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: this function will load in a manifest file ( and its user prefs )
// Input  : pszFileName - the name of the manifest file.
// Output : returns true if the manifest was loaded.
//-----------------------------------------------------------------------------
bool CManifest::Load( const char *pszFileName )
{
	if ( !LoadVMFManifest( pszFileName ) )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: This function will save the manifest, the associated maps, and user prefs. 
// Input  : pszFileName - the name of the manifest
//			bForce - if true, we need to save all files, as we are relocating.
// Output : returns true if all files saved successfully.
//-----------------------------------------------------------------------------
bool CManifest::Save( const char *pszFileName, bool bForce )
{
	bool	bSuccess = true;

	if ( bForce || m_bManifestChanged )
	{
		if ( !SaveVMFManifest( pszFileName ) )
		{
			bSuccess = false;
		}
	}

	if ( !SaveVMFManifestMaps( pszFileName ) )
	{
		bSuccess = false;
	}

//	if ( bForce || m_bManifestUserPrefsChanged )
	{
		if ( !SaveVMFManifestUserPrefs( pszFileName ) )
		{
			bSuccess = false;
		}
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: this function will save the manifest file and all modified maps.  If we are
//			relocating the manifest to a new place, all maps will be saved relative to 
//			the new place
// Input  : the file name the manifest should be saved as
// Output : returns true if the save was completely successful.  A partial save will
//			return false.
//-----------------------------------------------------------------------------
bool CManifest::SaveVMFManifest( const char *pszFileName )
{
	bool		bSaved = true;
	CChunkFile	File;	

	ChunkFileResult_t eResult = File.Open( pszFileName, ChunkFile_Write );
	if (eResult != ChunkFile_Ok)
	{
		GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error saving Manifest!" , MB_OK | MB_ICONEXCLAMATION );
		bSaved = false;
	}
	else
	{
		eResult = File.BeginChunk( "Info" );
		eResult = File.WriteKeyValueInt( "NextInternalID", m_NextInternalID );
		eResult = File.EndChunk();

		eResult = File.BeginChunk( "Maps" );
		if (eResult == ChunkFile_Ok)
		{
			for( int i = 0; i < GetNumMaps(); i++ )
			{
				CManifestMap	*pManifestMap = GetMap( i );

				eResult = File.BeginChunk("VMF");
				if (eResult == ChunkFile_Ok)
				{
					eResult = File.WriteKeyValue( "Name", pManifestMap->m_FriendlyName );
					eResult = File.WriteKeyValue( "File", pManifestMap->m_RelativeMapFileName );
					eResult = File.WriteKeyValueInt( "InternalID", pManifestMap->m_InternalID );
					if ( pManifestMap->m_bTopLevelMap == true )
					{
						eResult = File.WriteKeyValue( "TopLevel", "1" );
					}

					eResult = File.EndChunk();
				}
			}
		}

		if (eResult == ChunkFile_Ok)
		{
			eResult = File.EndChunk();
		}
		else
		{
			GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error saving Manifest!", MB_OK | MB_ICONEXCLAMATION );
			bSaved = false;
		}

		File.Close();
	}

	V_StripExtension( pszFileName, m_ManifestDir, sizeof( m_ManifestDir ) );
	CreateDirectory( m_ManifestDir, NULL );
	strcat( m_ManifestDir, "\\" );

	if ( bSaved )
	{
		m_bManifestChanged = false;
	}

	return bSaved;
}


//-----------------------------------------------------------------------------
// Purpose: This function will save all maps associated with a manifest.  Only modified
//			maps are saved unless we are relocating the manifest.
// Input  : pszFileName - the name of the manifest file
// Output : returns true if the maps were saved successfully.
//-----------------------------------------------------------------------------
bool CManifest::SaveVMFManifestMaps( const char *pszFileName )
{
	bool	bSaved = true;

	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		if ( m_bRelocateSave )
		{
			char FileName[ MAX_PATH ];

			strcpy( FileName, m_ManifestDir );
			strcat( FileName, pManifestMap->m_RelativeMapFileName );
			pManifestMap->m_AbsoluteMapFileName = FileName;
		}

		if ( ( pManifestMap->m_Map->IsModified() || m_bRelocateSave ) )
		{
			if ( pManifestMap->m_Map->SaveVMF( pManifestMap->m_AbsoluteMapFileName, 0 ) == false )
			{
				bSaved = false;
			}
		}
	}

	if ( !bSaved )
	{
		GetMainWnd()->MessageBox( "Not all pieces of the manifest were saved!", "Error saving Manifest!", MB_OK | MB_ICONEXCLAMATION );
	}

	return bSaved;
}


//-----------------------------------------------------------------------------
// Purpose: this function will save the user prefs of the manifest.
// Input  : pszFileName - the name of the manifest file.
// Output : returns true if the prefs were saved.
//-----------------------------------------------------------------------------
bool CManifest::SaveVMFManifestUserPrefs( const char *pszFileName )
{
	bool		bSaved = true;
	CChunkFile	File;	
	char		UserName[ MAX_PATH ], FileName[ MAX_PATH ], UserPrefsFileName[ MAX_PATH ];
	DWORD		UserNameSize;

	UserNameSize = sizeof( UserName );
	if ( GetUserName( UserName, &UserNameSize ) == 0 )
	{
		strcpy( UserPrefsFileName, "default" );
	}

	strcpy( FileName, m_ManifestDir );
	sprintf( UserPrefsFileName, "%s.vmm_prefs", UserName );
	strcat( FileName, UserPrefsFileName );

	ChunkFileResult_t eResult = File.Open( FileName, ChunkFile_Write );
	if (eResult != ChunkFile_Ok)
	{
		GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error saving Manifest User Prefs!" , MB_OK | MB_ICONEXCLAMATION );
		bSaved = false;
	}
	else
	{
		eResult = File.BeginChunk( "Maps" );
		if (eResult == ChunkFile_Ok)
		{
			for( int i = 0; i < GetNumMaps(); i++ )
			{
				CManifestMap	*pManifestMap = GetMap( i );

				eResult = File.BeginChunk("VMF");
				if (eResult == ChunkFile_Ok)
				{
					eResult = File.WriteKeyValueInt( "InternalID", pManifestMap->m_InternalID );
					if ( pManifestMap->m_bPrimaryMap )
					{
						eResult = File.WriteKeyValue( "IsPrimary", "1" );
					}
					if ( pManifestMap->m_bProtected == true )
					{
						eResult = File.WriteKeyValue( "IsProtected", "1" );
					}
					if ( pManifestMap->m_bVisible == false )
					{
						eResult = File.WriteKeyValue( "IsVisible", "0" );
					}

					eResult = File.EndChunk();
				}
			}
		}

		if (eResult == ChunkFile_Ok)
		{
			eResult = File.EndChunk();
		}
		else
		{
			GetMainWnd()->MessageBox( File.GetErrorText( eResult ), "Error saving Manifest User Prefs!", MB_OK | MB_ICONEXCLAMATION );
			bSaved = false;
		}

		eResult = File.BeginChunk( "cordoning" );
		eResult = CordonSaveVMF( &File, NULL );

		if ( m_bIsCordoning )
		{
			CSaveInfo	SaveInfo;

			SaveInfo.SetVisiblesOnly( false );
			CMapWorld *pCordonWorld = CordonCreateWorld();
			eResult = pCordonWorld->SaveSolids( &File, &SaveInfo, 0 );
		}

		eResult = File.EndChunk();

		File.Close();
	}

	if ( bSaved )
	{
		m_bManifestUserPrefsChanged = false;
	}

	return bSaved;
}


//-----------------------------------------------------------------------------
// Purpose: this function will initialize the manifest
//-----------------------------------------------------------------------------
void CManifest::Initialize( void )
{
	__super::Initialize();

	m_ManifestWorld = new CMapWorld( this );
	m_ManifestWorld->CullTree_Build();
}


//-----------------------------------------------------------------------------
// Purpose: this function will update the manifest and all of its sub maps
//-----------------------------------------------------------------------------
void CManifest::Update( void )
{
	__super::Update();

	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		pManifestMap->m_Map->Update();
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function allows you to indicate if the user prefs have been modified.
// Input  : bModified - the new status of the user prefs
//-----------------------------------------------------------------------------
void CManifest::SetManifestPrefsModifiedFlag( bool bModified )
{
	m_bManifestUserPrefsChanged = bModified;
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the routing of the manifest's modified flag down to the primary map
// Input  : bModified - the new modified status
//-----------------------------------------------------------------------------
void CManifest::SetModifiedFlag( BOOL bModified )
{
	if ( m_pPrimaryMap )
	{
		m_pPrimaryMap->m_Map->SetModifiedFlag( bModified );
	}
	
	if ( bModified == false )
	{
		for( int i = 0; i < GetNumMaps(); i++ )
		{
			CManifestMap	*pManifestMap = GetMap( i );

			if ( pManifestMap->m_Map->IsModified() )
			{
				bModified = true;
				break;
			}
		}
	}

	if ( bModified != IsModified() )
	{
		GetMainWnd()->m_ManifestFilterControl.Invalidate();
	}

	__super::SetModifiedFlag( bModified );
}


//-----------------------------------------------------------------------------
// Purpose: this function will return the full path to a sub map.
// Input  : pManifestMapFileName - the relative name of the sub map
// Output : pOutputPath - the full path to the sub map
//-----------------------------------------------------------------------------
void CManifest::GetFullMapPath( const char *pManifestMapFileName, char *pOutputPath )
{
	strcpy( pOutputPath, m_ManifestDir );
	strcat( pOutputPath, pManifestMapFileName );
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt to find the manifest map that owns the map doc
// Input  : pMap - the map doc to look up
// Output : returns a pointer to the owning manifest map, otherwise NULL
//-----------------------------------------------------------------------------
CManifestMap *CManifest::FindMap( CMapDoc *pMap )
{
	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		if ( pManifestMap->m_Map == pMap )
		{
			return pManifestMap;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt to look up a map by its internal id.
// Input  : InternalID - the internal ID.
// Output : returns the manifest map if one is found.
//-----------------------------------------------------------------------------
CManifestMap *CManifest::FindMapByID( int InternalID )
{
	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		if ( pManifestMap->m_InternalID == InternalID )
		{
			return pManifestMap;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: this function will set the manifest map as the primary map.  All entity / brush 
//			operations happen exclusively on the primary map.
// Input  : pManifestMap - the manifest map to make primary
//-----------------------------------------------------------------------------
void CManifest::SetPrimaryMap( CManifestMap	*pManifestMap )
{
	if ( m_pPrimaryMap )
	{
		m_pPrimaryMap->m_bPrimaryMap = false;
		m_pPrimaryMap->m_Map->m_nNextMapObjectID = m_nNextMapObjectID;
		m_pPrimaryMap->m_Map->m_nNextMapObjectID = m_nNextNodeID;
		m_pPrimaryMap->m_Map->SetEditable( false );
	}

	ClearSelection();
	CheckFileStatus();

	m_pPrimaryMap = pManifestMap;
	if ( m_pPrimaryMap )
	{
		m_pPrimaryMap->m_bPrimaryMap = true;
		m_pWorld = m_pPrimaryMap->m_Map->GetMapWorld();
		m_VisGroups = m_pPrimaryMap->m_Map->m_VisGroups;
		m_RootVisGroups = m_pPrimaryMap->m_Map->m_RootVisGroups;
		m_nNextMapObjectID = m_pPrimaryMap->m_Map->m_nNextMapObjectID;
		m_nNextNodeID = m_pPrimaryMap->m_Map->m_nNextMapObjectID;
		m_pPrimaryMap->m_Map->SetEditable( !m_pPrimaryMap->m_bReadOnly );

		m_pUndo = m_pPrimaryMap->m_Map->m_pUndo;
		m_pRedo = m_pPrimaryMap->m_Map->m_pRedo;
		CHistory::SetHistory( m_pPrimaryMap->m_Map->m_pUndo );

//		m_pSelection = m_pPrimaryMap->m_Map->m_pSelection;
	}

	m_bManifestUserPrefsChanged = true;

	GetMainWnd()->GlobalNotify( WM_MAPDOC_CHANGED );
	GetMainWnd()->m_ManifestFilterControl.Invalidate();
	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: sets the visibility flag of a sub map.
// Input  : pManifestMap - the map to set the flag
//			bIsVisible - the visiblity status
//-----------------------------------------------------------------------------
void CManifest::SetVisibility( CManifestMap	*pManifestMap, bool bIsVisible )
{
	pManifestMap->m_bVisible = bIsVisible;

	GetMainWnd()->m_ManifestFilterControl.Invalidate();
	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: this function will create and default a new manifest map, add it to the world
// Input  : AbsoluteFileName - the full path of the vmf file
//			RelativeFileName - the relative path of the vmf file
// Output : returns a pointer to the newly created manifest map.
//-----------------------------------------------------------------------------
CManifestMap *CManifest::CreateNewMap( const char *AbsoluteFileName, const char *RelativeFileName, bool bSetID )
{
	CManifestMap	*pManifestMap = new CManifestMap();

	pManifestMap->m_AbsoluteMapFileName = AbsoluteFileName;
	pManifestMap->m_RelativeMapFileName = RelativeFileName;
	pManifestMap->m_Map = new CMapDoc();
	SetActiveMapDoc( pManifestMap->m_Map );
	pManifestMap->m_Map->SetManifest( this );
	pManifestMap->m_Map->SetEditable( false );

	pManifestMap->m_Entity = new CManifestInstance( pManifestMap );

	pManifestMap->m_Entity->SetPlaceholder( true );
	pManifestMap->m_Entity->SetOrigin( Vector( 0.0f, 0.0f, 0.0f ) );
	pManifestMap->m_Entity->SetClass( "func_instance" );
	pManifestMap->m_Entity->SetKeyValue( "classname", "func_instance" );

	// ensure we are a pure instance of only the instance helper!
	pManifestMap->m_Entity->RemoveAllChildren();
	CHelperInfo	HI;

	HI.SetName( "instance" );

	CMapClass *pHelper = CHelperFactory::CreateHelper( &HI, pManifestMap->m_Entity );
	if ( pHelper != NULL )
	{
		pManifestMap->m_Entity->AddHelper( pHelper, false );
	}

	if ( bSetID )
	{
		pManifestMap->m_InternalID = m_NextInternalID;
		m_NextInternalID++;
	}

	CMapInstance	*pMapInstance = pManifestMap->m_Entity->GetChildOfType( ( CMapInstance * )NULL );
	if ( pMapInstance )
	{
		pMapInstance->SetManifest( pManifestMap );
	}
	AddManifestObjectToWorld( pManifestMap->m_Entity );
	m_Maps.AddToTail( pManifestMap );

	m_bManifestChanged = true;

	return pManifestMap;
}


//-----------------------------------------------------------------------------
// Purpose: This function will move the selection of the active map to a new sub map.
// Input  : pManifestMap - the sub map the selection shoud be moved to
//			CenterContents - if the contents should be centered
//-----------------------------------------------------------------------------
void CManifest::MoveSelectionToSubmap( CManifestMap *pManifestMap, bool CenterContents )
{
#if 0
	if ( s_Clipboard.Objects.Count() != 0 )
	{
		return false;
	}
#endif

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
	{
		return;
	}

	pDoc->Copy();
	if ( pDoc->GetClipboardCount() == 0 )
	{
		return;
	}
	pDoc->Delete();

	pManifestMap->m_Map->ManifestPaste( pManifestMap->m_Map->GetMapWorld(), Vector( 0.0f, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ), NULL, false, NULL );
	pManifestMap->m_Entity->CalcBounds( TRUE );
	
	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
}


//-----------------------------------------------------------------------------
// Purpose: this function will cut the selection and move it into a newly created 
//			map doc and manifest map.
// Input  : FriendlyName - this is the text friendly name that the user can refer 
//				to the map as
//			FileName - The relative file name for this new map to be saved as
//			CenterContents - whether or not we should center the contents in the new map
// Output : returns a pointer to the newly created manifest map.
//-----------------------------------------------------------------------------
CManifestMap *CManifest::MoveSelectionToNewSubmap( CString &FriendlyName, CString &FileName, bool CenterContents )
{
#if 0
	if ( s_Clipboard.Objects.Count() != 0 )
	{
		return false;
	}
#endif

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if ( !pDoc )
	{
		return NULL;
	}

	pDoc->Copy();
	if ( pDoc->GetClipboardCount() == 0 )
	{
		return NULL;
	}
	
	char AbsoluteFileName[ MAX_PATH ];

	strcpy( AbsoluteFileName, m_ManifestDir );
	strcat( AbsoluteFileName, FileName );

	CManifestMap	*pManifestMap = CreateNewMap( AbsoluteFileName, FileName, true );
	pManifestMap->m_FriendlyName = FriendlyName;

	pManifestMap->m_Map->Initialize();
	if ( pManifestMap->m_Map->SaveVMF( pManifestMap->m_AbsoluteMapFileName, 0 ) == false )
	{
		m_bLoading = false;
		SetActiveMapDoc( this );
		delete pManifestMap;
		return NULL;
	}
	pDoc->Delete();
	pManifestMap->m_Map->ManifestPaste( pManifestMap->m_Map->GetMapWorld(), Vector( 0.0f, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ), NULL, false, NULL );
	pManifestMap->m_Entity->CalcBounds( TRUE );
	SetPrimaryMap( pManifestMap );

	SetActiveMapDoc( this );
	__super::SetModifiedFlag( true );
	pDoc->SetModifiedFlag( true );
	pManifestMap->m_Map->SetModifiedFlag( true );

	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
	GetMainWnd()->m_ManifestFilterControl.UpdateManifestList();

	return pManifestMap;
}


//-----------------------------------------------------------------------------
// Purpose: this function will add a new sub map to the manifest.  
// Input  : FriendlyName - the friendly name string
//			FileName - the file name of the sub map
// Output : returns a pointer to the new manifest map.
//-----------------------------------------------------------------------------
CManifestMap *CManifest::AddNewSubmap( CString &FriendlyName, CString &FileName )
{
	char AbsoluteFileName[ MAX_PATH ];

	strcpy( AbsoluteFileName, m_ManifestDir );
	strcat( AbsoluteFileName, FileName );

	CManifestMap	*pManifestMap = CreateNewMap( AbsoluteFileName, FileName, true );

	pManifestMap->m_FriendlyName = FriendlyName;

	pManifestMap->m_Map->Initialize();
	pManifestMap->m_Entity->CalcBounds( TRUE );

	if ( pManifestMap->m_Map->SaveVMF( pManifestMap->m_AbsoluteMapFileName, 0 ) == false )
	{
		m_bLoading = false;
		SetActiveMapDoc( this );
		delete pManifestMap;
		return NULL;
	}

	SetPrimaryMap( pManifestMap );
	SetActiveMapDoc( this );
	__super::SetModifiedFlag( true );

	UpdateAllViews( MAPVIEW_UPDATE_SELECTION | MAPVIEW_UPDATE_TOOL | MAPVIEW_RENDER_NOW );
	GetMainWnd()->m_ManifestFilterControl.UpdateManifestList();

	return pManifestMap;
}


//-----------------------------------------------------------------------------
// Purpose: This function add an external vmf file to the manifest
// Input  : pszFileName - the absolute file name of the vmf file
//			bFromInstance - if the map is coming from a func_instance somewhere
// Output : returns true if the map could be loaded and the manifest was created
//-----------------------------------------------------------------------------
bool CManifest::AddExistingMap( const char *pszFileName, bool bFromInstance )
{
	char AbsoluteFileName[ MAX_PATH ], RelativeFileName[ MAX_PATH ];

	char FileExt[ MAX_PATH ];

	_splitpath_s( pszFileName, NULL, 0, NULL, 0, RelativeFileName, sizeof( RelativeFileName ), FileExt, sizeof( FileExt ) );
	strcat( RelativeFileName, FileExt );

	strcpy( AbsoluteFileName, m_ManifestDir );
	strcat( AbsoluteFileName, RelativeFileName );
	CManifestMap	*pManifestMap = CreateNewMap( AbsoluteFileName, RelativeFileName, true );

	m_bLoading = true;
	if ( !pManifestMap->m_Map->LoadVMF( pszFileName, VMF_LOAD_ACTIVATE | VMF_LOAD_IS_SUBMAP ) )
	{
		m_bLoading = false;
		SetActiveMapDoc( this );
		delete pManifestMap;
		return false;
	}

	if ( pManifestMap->m_Map->SaveVMF( pManifestMap->m_AbsoluteMapFileName, 0 ) == false )
	{
		m_bLoading = false;
		SetActiveMapDoc( this );
		delete pManifestMap;
		return false;
	}

	pManifestMap->m_Map->GetMapWorld()->CullTree_Build();
	pManifestMap->m_Entity->PostUpdate( Notify_Changed );
	if ( m_Maps.Count() == 1 )
	{
		pManifestMap->m_bTopLevelMap = true;
	}

	SetPrimaryMap( pManifestMap );

	m_bLoading = false;

	SetActiveMapDoc( this );
	__super::SetModifiedFlag( true );

	GetMainWnd()->m_ManifestFilterControl.UpdateManifestList();

	if ( GetPathName().GetLength() == 0 )
	{
		char	ManifestFile[ MAX_PATH ];

		strcpy( ManifestFile, pszFileName );
		V_SetExtension( ManifestFile, ".vmm", sizeof( ManifestFile ) );

		m_bRelocateSave = true;
		OnSaveDocument( ManifestFile );
		m_bRelocateSave = false;

		SetPathName( ManifestFile, false );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: This function will allow the user to browse to an exist map to add to the manifest.
//-----------------------------------------------------------------------------
bool CManifest::AddExistingMap( void )
{
	char szInitialDir[ MAX_PATH ];
	
	V_strcpy_safe( szInitialDir, GetPathName() );
	if ( szInitialDir[ 0 ] == '\0' )
	{
		strcpy( szInitialDir, g_pGameConfig->szMapDir );
	}

	CFileDialog dlg( TRUE, NULL, NULL, OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "Valve Map Files (*.vmf)|*.vmf||" );
	dlg.m_ofn.lpstrInitialDir = szInitialDir;
	int iRvl = dlg.DoModal();

	if ( iRvl == IDCANCEL )
	{
		return false;
	}

	//
	// Get the directory they browsed to for next time.
	//
	CString str = dlg.GetPathName();
	int nSlash = str.ReverseFind( '\\' );
	if ( nSlash != -1 )
	{
		strcpy( szInitialDir, str.Left( nSlash ) );
	}

	if ( str.Find('.') == -1 )
	{
		switch ( dlg.m_ofn.nFilterIndex )
		{
			case 1:
				str += ".vmf";
				break;
		}
	}

	return AddExistingMap( str, false );
}


//-----------------------------------------------------------------------------
// Purpose: This function will remove the sub map from the manifest
// Input  : pManifestMap - the sub map to be removed
// Output : returns true if it was successful
//-----------------------------------------------------------------------------
bool CManifest::RemoveSubMap( CManifestMap *pManifestMap )
{
	if ( m_Maps.Count() > 1 )
	{
		m_Maps.FindAndRemove( pManifestMap );

		const CMapObjectList *pChildren = m_ManifestWorld->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			CMapClass	*pChild = pChildren->Element( pos );
			CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pChild );

			if ( pEntity && stricmp( pEntity->GetClassName(), "func_instance" ) == 0 )
			{
				CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
				if ( pMapInstance )
				{
					if ( pMapInstance->GetManifestMap() == pManifestMap )
					{
						m_ManifestWorld->RemoveObjectFromWorld( pChild, true );
						break;
					}
				}
			}
		}

		delete pManifestMap;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CManifest::CheckOut( )
{
	if ( !p4 )
	{
		return false;
	}

	if ( !p4->OpenFileForEdit( GetPathName() ) )
	{
		return false;
	}

	CheckFileStatus();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CManifest::AddToVersionControl( )
{
	if ( !p4 )
	{
		return false;
	}

	if ( !p4->OpenFileForAdd( GetPathName() ) )
	{
		return false;
	}

	CheckFileStatus();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifest::CheckFileStatus( void )
{
	P4File_t	FileInfo;

	m_bReadOnly = !g_pFullFileSystem->IsFileWritable( GetPathName() );
	m_bCheckedOut = false;
	m_bIsVersionControlled = false;
	if ( p4 != NULL && p4->GetFileInfo( GetPathName(), &FileInfo ) == true )
	{
		m_bIsVersionControlled = true;
		if ( FileInfo.m_eOpenState == P4FILE_OPENED_FOR_ADD || FileInfo.m_eOpenState == P4FILE_OPENED_FOR_EDIT )
		{
			m_bCheckedOut = true;
		}
	}

	for( int i = 0; i < GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = GetMap( i );

		pManifestMap->m_bReadOnly = !g_pFullFileSystem->IsFileWritable( pManifestMap->m_AbsoluteMapFileName );
		pManifestMap->m_bCheckedOut = false;
		pManifestMap->m_bIsVersionControlled = false;

		if ( p4 != NULL && p4->GetFileInfo( pManifestMap->m_AbsoluteMapFileName, &FileInfo ) == true )
		{
			pManifestMap->m_bIsVersionControlled = true;
			if ( FileInfo.m_eOpenState == P4FILE_OPENED_FOR_ADD || FileInfo.m_eOpenState == P4FILE_OPENED_FOR_EDIT )
			{
				pManifestMap->m_bCheckedOut = true;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function will clear the selection
//-----------------------------------------------------------------------------
void CManifest::ClearSelection( void )
{
	SelectFace( NULL, 0, scClear | scSaveChanges );
	SelectObject( NULL, scClear | scSaveChanges );
}


//-----------------------------------------------------------------------------
// Purpose: This function will add the object to the primary map of the manifest
// Input  : pObject - a pointer to the object to be added
//			pParent - a pointer to the parent of this object
//-----------------------------------------------------------------------------
void CManifest::AddObjectToWorld(CMapClass *pObject, CMapClass *pParent)
{
	m_pPrimaryMap->m_Map->AddObjectToWorld( pObject, pParent );

	m_pPrimaryMap->m_Entity->PostUpdate( Notify_Changed );
}


//-----------------------------------------------------------------------------
// Purpose: This function will pass on the notification that a map has been updated
// Input  : pInstanceMapDoc - the map that was updated
//-----------------------------------------------------------------------------
void CManifest::UpdateInstanceMap( CMapDoc *pInstanceMapDoc )
{
	const CMapObjectList *pChildren = m_ManifestWorld->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		CMapClass	*pChild = pChildren->Element( pos );
		CMapEntity	*pEntity = dynamic_cast< CMapEntity * >( pChild );

		if ( pEntity && stricmp( pEntity->GetClassName(), "func_instance" ) == 0 )
		{
			CMapInstance	*pMapInstance = pEntity->GetChildOfType( ( CMapInstance * )NULL );
			if ( pMapInstance )
			{
				if ( pMapInstance->GetInstancedMap() == pInstanceMapDoc )
				{
					pMapInstance->UpdateInstanceMap();
					m_ManifestWorld->UpdateChild( pMapInstance );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will add the object to the local manifest world.  the
//			only objects that should be added are ManifestInstance.
// Input  : pObject - a pointer to the object to be added
//			pParent - a pointer to the parent of this object
//-----------------------------------------------------------------------------
void CManifest::AddManifestObjectToWorld( CMapClass *pObject, CMapClass *pParent )
{
	m_ManifestWorld->AddObjectToWorld( pObject, pParent );
}


//-----------------------------------------------------------------------------
// Purpose: Removes an object from the manifest world.
// Input  : pObject - object to remove from the world.
//			bChildren - whether we're removing the object's children as well.
//-----------------------------------------------------------------------------
void CManifest::RemoveManifestObjectFromWorld( CMapClass *pObject, bool bRemoveChildren )
{
	m_ManifestWorld->RemoveObjectFromWorld( pObject, bRemoveChildren );
}

//-----------------------------------------------------------------------------
// Purpose: this function will start the loading process for a manifest
// Input  : lpszPathName - the absoltue path of the manifest file
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CManifest::OnOpenDocument(LPCTSTR lpszPathName) 
{
	Initialize();

	if (!SelectDocType())
	{
		return FALSE;
	}

	//
	// Call any per-class PreloadWorld functions here.
	//
	CMapSolid::PreloadWorld();

	if ( !Load( lpszPathName ) )
	{
		return FALSE;
	}

	SetModifiedFlag( FALSE );
	Msg( mwStatus, "Opened %s", lpszPathName );
	SetActiveMapDoc( this );

	//
	// We set the active doc before loading for displacements (and maybe other
	// things), but visgroups aren't available until after map load. We have to refresh
	// the visgroups here or they won't be correct.
	//
	GetMainWnd()->GlobalNotify( WM_MAPDOC_CHANGED );

	m_pToolManager->SetTool( TOOL_POINTER );

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: This function will save out the manifest
// Input  : lpszPathName - the absolute filename of the manifest
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CManifest::OnSaveDocument(LPCTSTR lpszPathName) 
{
	if ( !Save( lpszPathName, m_bRelocateSave ) )
	{
		return FALSE;
	}

	SetModifiedFlag( FALSE );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: this function will be called when the use select Save As.  This will
//			allow all of the submaps to be relocated relative to a new save location
//			of the manifest itself.  Directories should be created automatically.
//-----------------------------------------------------------------------------
void CManifest::OnFileSaveAs(void)
{
	m_bRelocateSave = true;

	__super::OnFileSaveAs();

	m_bRelocateSave = false;
}


//-----------------------------------------------------------------------------
// Purpose: this function will delete the manifest world and rest of the contents.
//-----------------------------------------------------------------------------
void CManifest::DeleteContents( void )
{
	m_pSelection->RemoveAll();

	if ( m_ManifestWorld )
	{
		delete m_ManifestWorld;
		m_ManifestWorld = NULL;
	}

	m_pWorld = NULL;
	m_VisGroups = NULL;
	m_RootVisGroups = NULL;
	m_pUndo = m_pSaveUndo;
	m_pRedo = m_pSaveRedo;

	__super::DeleteContents();
}

#include <tier0/memdbgoff.h>
