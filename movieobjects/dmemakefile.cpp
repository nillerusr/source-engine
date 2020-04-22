//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Describes an asset: something that is compiled from sources, 
// in potentially multiple steps, to a compiled resource
//
//=============================================================================


#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/idmemakefileutils.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/fileutils.h"
#include "tier3/tier3.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Hook into element factories
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSource, CDmeSource );


//-----------------------------------------------------------------------------
// Construction/destruction 
//-----------------------------------------------------------------------------
void CDmeSource::OnConstruction()
{
	m_DependentMakefile = NULL;
}

void CDmeSource::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets/gets the makefile that was used to build this source
//-----------------------------------------------------------------------------
void CDmeSource::SetDependentMakefile( CDmeMakefile *pMakeFile )
{
	m_DependentMakefile = pMakeFile;
}

CDmeMakefile *CDmeSource::GetDependentMakefile()
{
	return m_DependentMakefile.Get();
}


//-----------------------------------------------------------------------------
// Call this to open the source file in an editor
//-----------------------------------------------------------------------------
void CDmeSource::OpenEditor()
{
	if ( g_pDmeMakefileUtils )
	{
		g_pDmeMakefileUtils->PerformOpenEditor( this );
	}
}


//-----------------------------------------------------------------------------
// Hook into element factories
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMakefile, CDmeMakefile );


//-----------------------------------------------------------------------------
// Construction/destruction 
//-----------------------------------------------------------------------------
void CDmeMakefile::OnConstruction()
{
	m_Sources.Init( this, "sources" );
	m_hOutput = NULL;
	m_hCompileProcess = PROCESS_HANDLE_INVALID;
	m_bIsDirty = false;
}

void CDmeMakefile::OnDestruction()
{
	DestroyOutputElement( m_hOutput.Get() );
	m_hOutput = NULL;
}

						   
//-----------------------------------------------------------------------------
// Performs pre-compilation step
//-----------------------------------------------------------------------------
void CDmeMakefile::PreCompile( )
{
	// Make all outputs writeable
	MakeOutputsWriteable();

	// Destroy the current output object; we'll need to reload it
	// NOTE: Don't check for m_hOutput == 0; we always need to call DestroyOutputElement
	// Sometimes makefiles have to do stuff even if m_hOutput == NULL
	DestroyOutputElement( m_hOutput );
	m_hOutput = NULL;
}

void CDmeMakefile::PostCompile( )
{
}


//-----------------------------------------------------------------------------
// Gets the output element created by compilation of this makefile
//-----------------------------------------------------------------------------
CDmElement *CDmeMakefile::GetOutputElement( bool bCreateIfNecessary )
{
	if ( m_hOutput.Get() )
		return m_hOutput.Get();

	if ( !bCreateIfNecessary )
		return NULL;

	if ( !g_pDmeMakefileUtils || !g_pDmeMakefileUtils->IsCurrentlyCompiling() )
	{
		m_hOutput = CreateOutputElement();
	}

	return m_hOutput.Get();
}


//-----------------------------------------------------------------------------
// Gets the path of the makefile
//-----------------------------------------------------------------------------
void CDmeMakefile::GetMakefilePath( char *pFullPath, int nBufLen )
{
	DmFileId_t fileId = GetFileId();
	const char *pFileName = ( fileId != DMFILEID_INVALID ) ? g_pDataModel->GetFileName( fileId ) : "";
	Assert( !pFileName[0] || Q_IsAbsolutePath( pFileName ) );

	Q_ExtractFilePath( pFileName, pFullPath, nBufLen );
}


//-----------------------------------------------------------------------------
// Returns the output directory we expect to compile files into
//-----------------------------------------------------------------------------
bool CDmeMakefile::GetOutputDirectory( char *pFullPath, int nBufLen )
{
	return GetDefaultDirectory( GetOutputDirectoryID(), pFullPath, nBufLen );
}


//-----------------------------------------------------------------------------
// Returns the output name (output directory + filename, no extension)
//-----------------------------------------------------------------------------
bool CDmeMakefile::GetOutputName( char *pFullPath, int nBufLen )
{
	pFullPath[0] = 0;

	char pOutputPath[MAX_PATH];
	if ( !GetDefaultDirectory( GetOutputDirectoryID(), pOutputPath, sizeof(pOutputPath) ) )
		return false;

	DmFileId_t fileId = GetFileId();
	const char *pFileName = ( fileId != DMFILEID_INVALID ) ? g_pDataModel->GetFileName( fileId ) : "";
	if ( !pFileName || !pFileName[0] )
		return false;

	Q_ComposeFileName( pOutputPath, Q_UnqualifiedFileName(pFileName), pFullPath, nBufLen );
	Q_RemoveDotSlashes( pFullPath );
	return true;
}


//-----------------------------------------------------------------------------
// Converts the m_pDefaultDirectoryID field of the DmeMakefileType_t to a full path
//-----------------------------------------------------------------------------
bool CDmeMakefile::GetDefaultDirectory( const char *pDefaultDirectoryID, char *pFullPath, int nBufLen )
{
	if ( StringHasPrefix( pDefaultDirectoryID, "contentdir:" ) )
	{
		pDefaultDirectoryID += 11;
		GetModContentSubdirectory( pDefaultDirectoryID, pFullPath, nBufLen );
		Q_RemoveDotSlashes( pFullPath );
		return true;
	}

	if ( StringHasPrefix( pDefaultDirectoryID, "gamedir:" ) )
	{
		pDefaultDirectoryID += 8;
		GetModSubdirectory( pDefaultDirectoryID, pFullPath, nBufLen );
		Q_RemoveDotSlashes( pFullPath );
		return true;
	}

	if ( StringHasPrefix( pDefaultDirectoryID, "makefiledir:" ) )
	{
		char pMakefilePath[MAX_PATH];
		GetMakefilePath( pMakefilePath, sizeof(pMakefilePath) );
		pDefaultDirectoryID += 12;
		Q_ComposeFileName( pMakefilePath, pDefaultDirectoryID, pFullPath, nBufLen );
		Q_RemoveDotSlashes( pFullPath );
		return true;
	}

	if ( StringHasPrefix( pDefaultDirectoryID, "makefilegamedir:" ) )
	{
		char pMakefilePath[MAX_PATH];
		GetMakefilePath( pMakefilePath, sizeof(pMakefilePath) );

		char pModContentDirectory[MAX_PATH];
		GetModContentSubdirectory( NULL, pModContentDirectory, sizeof(pModContentDirectory) );

		char pRelativePath[MAX_PATH];
		if ( !Q_MakeRelativePath( pMakefilePath, pModContentDirectory, pRelativePath, sizeof(pRelativePath) ) )
		{
			pFullPath[0] = 0;
			return false;
		}

		char pModDirectory[MAX_PATH];
		GetModSubdirectory( NULL, pModDirectory, sizeof(pModDirectory) );

		char pMakefileGamePath[MAX_PATH];
		Q_ComposeFileName( pModDirectory, pRelativePath, pMakefileGamePath, sizeof(pMakefileGamePath) );

		pDefaultDirectoryID += 16;
		Q_ComposeFileName( pMakefileGamePath, pDefaultDirectoryID, pFullPath, nBufLen );
		Q_RemoveDotSlashes( pFullPath );
		return true;
	}

	// Assume it's a content subdir
	GetModContentSubdirectory( pDefaultDirectoryID, pFullPath, nBufLen );
	Q_RemoveDotSlashes( pFullPath );
	return true;
}


//-----------------------------------------------------------------------------
// Relative path to full path
//-----------------------------------------------------------------------------
void CDmeMakefile::RelativePathToFullPath( const char *pRelativePath, char *pFullPath, int nBufLen )
{
	if ( !pRelativePath[0] )
	{
		pFullPath[0] = 0;
		return;
	}
	char pRootDir[ MAX_PATH ];
	GetMakefilePath( pRootDir, sizeof(pRootDir) );
	Q_ComposeFileName( pRootDir, pRelativePath, pFullPath, nBufLen );
	Q_RemoveDotSlashes( pFullPath );
}


//-----------------------------------------------------------------------------
// Fullpath to relative path
//-----------------------------------------------------------------------------
void CDmeMakefile::FullPathToRelativePath( const char *pFullPath, char *pRelativePath, int nBufLen )
{
	if ( !pFullPath[0] )
	{
		pRelativePath[0] = 0;
		return;
	}
	char pRootDir[ MAX_PATH ];
	GetMakefilePath( pRootDir, sizeof(pRootDir) );
	if ( pRootDir[0] )
	{
		Q_MakeRelativePath( pFullPath, pRootDir, pRelativePath, nBufLen );
	}
	else
	{
		Q_strncpy( pRelativePath, pFullPath, nBufLen );
		Q_FixSlashes( pRelativePath );
	}
}


//-----------------------------------------------------------------------------
// Adds a single source
//-----------------------------------------------------------------------------
CDmeSource *CDmeMakefile::AddSource( const char *pSourceType, const char *pFullPath )
{
	if ( pFullPath[0] && FindSource( pSourceType, pFullPath ) )
	{
		Warning( "Attempted to add the same source twice %s!\n", pFullPath );
		return NULL;
	}

	CDmElement *pElement = GetElement< CDmElement >( g_pDataModel->CreateElement( pSourceType, "", GetFileId() ) );
	CDmeSource *pSource = CastElement< CDmeSource >( pElement );
	Assert( pSource );
	if ( !pSource )
	{
		Warning( "Invalid source type name %s!\n", pSourceType );
		if ( pElement )
		{
			DestroyElement( pElement );
		}
		return NULL;
	}

	char pRelativePath[MAX_PATH];
	FullPathToRelativePath( pFullPath, pRelativePath, sizeof( pRelativePath ) );
	pSource->SetRelativeFileName( pRelativePath );
	m_Sources.AddToTail( pSource );
	return pSource;
}


//-----------------------------------------------------------------------------
// Removes a single source
//-----------------------------------------------------------------------------
CDmeSource *CDmeMakefile::FindSource( const char *pSourceType, const char *pFullPath )
{
	char pRelativePath[MAX_PATH];
	FullPathToRelativePath( pFullPath, pRelativePath, sizeof( pRelativePath ) );
	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( Q_stricmp( pSourceType, m_Sources[i]->GetTypeString() ) )
			continue;

		if ( !Q_stricmp( pRelativePath, m_Sources[i]->GetRelativeFileName() ) )
			return m_Sources[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Sets a source to be a single source
//-----------------------------------------------------------------------------
CDmeSource *CDmeMakefile::SetSingleSource( const char *pSourceType, const char *pFullPath )
{
	// FIXME: we maybe shouldn't remove everything if the source can't be created for some reason?
	RemoveAllSources( pSourceType );
	return AddSource( pSourceType, pFullPath );
}


//-----------------------------------------------------------------------------
// Changes a source
//-----------------------------------------------------------------------------
void CDmeMakefile::SetSourceFullPath( CDmeSource *pSource, const char *pFullPath )
{
	char pRelativePath[MAX_PATH];
	FullPathToRelativePath( pFullPath, pRelativePath, sizeof( pRelativePath ) );

	if ( Q_stricmp( pRelativePath, pSource->GetRelativeFileName() ) )
	{
		pSource->SetRelativeFileName( pRelativePath );

		// FIXME: Should we delete the dependent makefile?
		pSource->SetDependentMakefile( NULL );
	}
}


//-----------------------------------------------------------------------------
// Returns the full path of a source
//-----------------------------------------------------------------------------
void CDmeMakefile::GetSourceFullPath( CDmeSource *pSource, char *pFullPath, int nBufLen )
{
	const char *pRelativePath = pSource->GetRelativeFileName( );
	RelativePathToFullPath( pRelativePath, pFullPath, nBufLen );
}


//-----------------------------------------------------------------------------
// Returns a list of sources
//-----------------------------------------------------------------------------
void CDmeMakefile::GetSources( const char *pSourceType, CUtlVector< CDmeHandle< CDmeSource > > &sources )
{
	int nCount = m_Sources.Count();
	sources.EnsureCapacity( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Sources[i]->IsA( pSourceType ) )
		{
			int j = sources.AddToTail();
			sources[j] = m_Sources[i];
		}
	}
}


//-----------------------------------------------------------------------------
// Gets a list of all sources, regardless of type
//-----------------------------------------------------------------------------
int CDmeMakefile::GetSourceCount()
{
	return m_Sources.Count();
}

CDmeSource *CDmeMakefile::GetSource( int nIndex )
{
	return m_Sources[nIndex];
}


//-----------------------------------------------------------------------------
// Removes a single source
//-----------------------------------------------------------------------------
void CDmeMakefile::RemoveSource( CDmeSource *pSource )
{
	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Sources[i] == pSource )
		{
			m_Sources.Remove( i );
			break;
		}
	}
}

void CDmeMakefile::RemoveSource( const char *pSourceType, const char *pFullPath )
{
	char pRelativePath[MAX_PATH];
	FullPathToRelativePath( pFullPath, pRelativePath, sizeof( pRelativePath ) );
	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( Q_stricmp( pSourceType, m_Sources[i]->GetTypeString() ) )
			continue;

		if ( !Q_stricmp( pRelativePath, m_Sources[i]->GetRelativeFileName() ) )
		{
			m_Sources.Remove( i );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Removes all sources of a particular type
//-----------------------------------------------------------------------------
void CDmeMakefile::RemoveAllSources( const char *pSourceType )
{
	int nCount = m_Sources.Count();
	for ( int i = nCount; --i >= 0; )
	{
		if ( !Q_stricmp( pSourceType, m_Sources[i]->GetTypeString() ) )
		{
			// NOTE: This works because we're iterating backward
			m_Sources.Remove( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Source iteration
//-----------------------------------------------------------------------------
bool CDmeMakefile::HasSourceOfType( const char *pSourceType )
{
	int nCount = m_Sources.Count();
	for ( int i = nCount; --i >= 0; )
	{
		if ( !Q_stricmp( pSourceType, m_Sources[i]->GetTypeString() ) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Updates the source names to be relative to a particular path
//-----------------------------------------------------------------------------
bool CDmeMakefile::UpdateSourceNames( const char *pOldRootDir, const char *pNewRootDir, bool bApplyChanges )
{ 
	char pOldSourcePath[ MAX_PATH ];
	char pNewSourcePath[ MAX_PATH ];

	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pOldRelativePath = m_Sources[i]->GetRelativeFileName();
		if ( pOldRelativePath[0] )
		{
			Q_ComposeFileName( pOldRootDir, pOldRelativePath, pOldSourcePath, sizeof(pOldSourcePath) );
			Q_RemoveDotSlashes( pOldSourcePath );
			if ( !Q_MakeRelativePath( pOldSourcePath, pNewRootDir, pNewSourcePath, sizeof(pNewSourcePath) ) )
			{
				Assert( !bApplyChanges );
				return false;
			}
		}
		else
		{
			pNewSourcePath[0] = 0;
		}

		if ( !bApplyChanges )
			continue;

		m_Sources[i]->SetRelativeFileName( pNewSourcePath );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Returns the filename
//-----------------------------------------------------------------------------
const char *CDmeMakefile::GetFileName() const
{
	DmFileId_t fileId = GetFileId();
	return g_pDataModel->GetFileName( fileId );
}


//-----------------------------------------------------------------------------
// Call this to change the file the makefile is stored in
// Will make all sources be relative to this path
//-----------------------------------------------------------------------------
bool CDmeMakefile::SetFileName( const char *pFileName )
{
	if ( !Q_IsAbsolutePath( pFileName ) )
		return false;

	char pOldRootDir[ MAX_PATH ];
	char pNewRootDir[ MAX_PATH ];
	GetMakefilePath( pOldRootDir,  sizeof(pOldRootDir) );
	Q_ExtractFilePath( pFileName, pNewRootDir, sizeof(pNewRootDir) );

	// Gotta do this twice; once to check for validity, once to actually do it
	if ( !UpdateSourceNames( pOldRootDir, pNewRootDir, false ) )
		return false;

	UpdateSourceNames( pOldRootDir, pNewRootDir, true );

	DmFileId_t fileId = GetFileId();
	if ( fileId == DMFILEID_INVALID )
	{
		fileId = g_pDataModel->FindOrCreateFileId( pFileName );
		SetFileId( fileId, TD_DEEP );
	}
	else
	{
		g_pDataModel->SetFileName( fileId, pFileName );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Make all outputs writeable
//-----------------------------------------------------------------------------
void CDmeMakefile::MakeOutputsWriteable( )
{
	// When we publish, we'll check them out.
	CUtlVector<CUtlString> outputs;
	GetOutputs( outputs );
	int nCount = outputs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		g_pFullFileSystem->SetFileWritable( outputs[i], true );
	}
}



//-----------------------------------------------------------------------------
// Sets a makefile/source association
//-----------------------------------------------------------------------------
void CDmeMakefile::SetAssociation( CDmeSource *pSource, CDmeMakefile *pSourceMakefile )
{
	if ( !pSource )
		return;

	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Sources[i] != pSource )
			continue;

		CDmeMakefile *pDependentMakeFile = m_Sources[i]->GetDependentMakefile();
		if ( pSourceMakefile != pDependentMakeFile )
		{
			// FIXME: Should I recursively delete pDependentMakeFile ?
			m_Sources[i]->SetDependentMakefile( pSourceMakefile );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Finds a dependent makefile
//-----------------------------------------------------------------------------
CDmeMakefile *CDmeMakefile::FindDependentMakefile( CDmeSource *pSource )
{
	if ( !pSource )
		return NULL;

	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Sources[i] == pSource )
			return m_Sources[i]->GetDependentMakefile();
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Finds the associated source
//-----------------------------------------------------------------------------
CDmeSource *CDmeMakefile::FindAssociatedSource( CDmeMakefile *pChildMakefile )
{
	if ( !pChildMakefile )
		return NULL;

	int nCount = m_Sources.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Sources[i]->GetDependentMakefile() == pChildMakefile )
			return m_Sources[i];
	}

	return NULL;
}

