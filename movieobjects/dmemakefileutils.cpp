//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Interface for makefiles to build differently depending on where they are run from
//
//===========================================================================//

#include "movieobjects/dmemakefileutils.h"
#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/dmedccmakefile.h"
#include "tier2/fileutils.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
IMPLEMENT_DMEMAKEFILE_UTIL_CLASS( CDmeMakefileUtils );


//-----------------------------------------------------------------------------
// Default implementation
//-----------------------------------------------------------------------------
static CDmeMakefileUtils s_MakefileUtils;
IDmeMakefileUtils *GetDefaultDmeMakefileUtils()
{
	return &s_MakefileUtils;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeMakefileUtils::CDmeMakefileUtils() : BaseClass( false )
{
	m_CompilationStep = NOT_COMPILING;
	m_hCompileProcess = PROCESS_HANDLE_INVALID;
	m_nCurrentCompileTask = -1;
	m_nExitCode = 0;
}

CDmeMakefileUtils::~CDmeMakefileUtils()
{

}


//-----------------------------------------------------------------------------
// Here's where systems can access other interfaces implemented by this object
//-----------------------------------------------------------------------------
void *CDmeMakefileUtils::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, DMEMAKEFILE_UTILS_INTERFACE_VERSION ) )
		return (IDmeMakefileUtils*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Initialization.. set up messagemaps
//-----------------------------------------------------------------------------
InitReturnVal_t CDmeMakefileUtils::Init()
{
	InitializeFuncMaps();
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Looks for an appropriate method to compile this element with
//-----------------------------------------------------------------------------
CCompileFuncAdapterBase *CDmeMakefileUtils::DetermineCompileAdapter( CDmElement *pElement )
{
	int nBestInheritanceDepth = -1;
	CCompileFuncAdapterBase *pBestAdapter = NULL;

	CompileFuncTree_t *pTree = GetCompileTree();
	while ( pTree )
	{
		CCompileFuncAdapterBase *pCurr = pTree->m_pFirstAdapter;
		for ( ; pCurr; pCurr = pCurr->m_pNext )
		{
			// Choose this factory if it's more derived than the previous best
			int nInheritanceDepth = pElement->GetInheritanceDepth( pCurr->m_ElementType );
			if ( nInheritanceDepth < 0 )
				continue;

			if ( nInheritanceDepth == 0 )
			{
				// Found exact match.. do it!
				return pCurr;
			}

			// Don't look for the best thingy if we're not the root
			if ( nBestInheritanceDepth >= 0 && ( nInheritanceDepth >= nBestInheritanceDepth ) )
				continue;

			nBestInheritanceDepth = nInheritanceDepth;
			pBestAdapter = pCurr;
		}

		pTree = pTree->m_pBaseAdapterTree;
	}

	// Return the closest match we could find
	return pBestAdapter;
}


//-----------------------------------------------------------------------------
// Looks for an appropriate method to open this element with
//-----------------------------------------------------------------------------
COpenEditorFuncAdapterBase *CDmeMakefileUtils::DetermineOpenEditorAdapter( CDmElement *pElement )
{
	int nBestInheritanceDepth = -1;
	COpenEditorFuncAdapterBase *pBestAdapter = NULL;
	OpenEditorFuncTree_t *pTree = GetOpenEditorTree();
	while ( pTree )
	{
		COpenEditorFuncAdapterBase *pCurr = pTree->m_pFirstAdapter;
		for ( ; pCurr; pCurr = pCurr->m_pNext )
		{
			// Choose this factory if it's more derived than the previous best
			int nInheritanceDepth = pElement->GetInheritanceDepth( pCurr->m_ElementType );
			if ( nInheritanceDepth < 0 )
				continue;

			// Found exact match.. do it!
			if ( nInheritanceDepth == 0 )
				return pCurr;

			if ( nBestInheritanceDepth >= 0 && ( nInheritanceDepth >= nBestInheritanceDepth ) )
				continue;

			nBestInheritanceDepth = nInheritanceDepth;
			pBestAdapter = pCurr;
		}

		pTree = pTree->m_pBaseAdapterTree;
	}
	return pBestAdapter;
}


//-----------------------------------------------------------------------------
// Opens a element in an external editor
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::PerformOpenEditor( CDmElement *pElement )
{
	COpenEditorFuncAdapterBase *pAdapter = DetermineOpenEditorAdapter( pElement );
	if ( pAdapter )
	{
		pAdapter->OpenEditor( pElement );
	}
}


//-----------------------------------------------------------------------------
// Queues up a compilation task
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::AddCompilationTask( CDmElement* pElement, CCompileFuncAdapterBase *pAdapter )
{
	Assert( m_CompilationStep == BUILDING_STANDARD_DEPENDENCIES || m_CompilationStep == BUILDING_ALL_DEPENDENCIES );

	// Queue up the compilation task
	int j = m_CompileTasks.AddToTail();
	m_CompileTasks[j].m_hElement = pElement;
	m_CompileTasks[j].m_pAdapter = pAdapter;
}

void CDmeMakefileUtils::AddCompilationTask( CDmElement* pElement )
{
	CCompileFuncAdapterBase *pAdapter = DetermineCompileAdapter( pElement );
	if ( pAdapter )
	{
		// Queue up the compilation task
		AddCompilationTask( pElement, pAdapter );
	}
}


//-----------------------------------------------------------------------------
// Sets the compile process
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::SetCompileProcess( ProcessHandle_t hProcess )
{
	Assert( m_CompilationStep == PERFORMING_COMPILATION );
	m_hCompileProcess = hProcess;
	if ( m_hCompileProcess == PROCESS_HANDLE_INVALID )
	{
		m_CompilationStep = AFTER_COMPILATION_FAILED;
	}
}
	

//-----------------------------------------------------------------------------
// Default implementatations for compile dependencies
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::AddCompileDependencies( CDmeMakefile *pMakefile, bool bBuildAllDependencies )
{
	if ( !pMakefile )
		return true;

	CUtlVector< CUtlString > outputs;
	int nCount = pMakefile->GetSourceCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeSource *pSource = pMakefile->GetSource( i );
		if ( !pSource )
			continue;

		CDmeMakefile *pDependentMakefile = pSource->GetDependentMakefile();
		if ( !pDependentMakefile )
			continue;

		bool bShouldBuildFile = bBuildAllDependencies;

		// Does the output files exist?
		int j = 0;
		if ( !bBuildAllDependencies )
		{
			pDependentMakefile->GetOutputs( outputs );
			int nOutputCount = outputs.Count();
			for ( j = 0; j < nOutputCount; ++j )
			{
				// If the file doesn't exist, we have to build it
				if ( !g_pFullFileSystem->FileExists( outputs[j] ) )
					break;

				bShouldBuildFile = true;
				break;
			}
		}

		if ( !bShouldBuildFile )
			continue;

		CCompileFuncAdapterBase *pAdapter = DetermineCompileAdapter( pDependentMakefile );
		if ( pAdapter )
		{
			// Add dependent makefiles first
			if ( !pAdapter->PerformCompilationStep( pDependentMakefile, bBuildAllDependencies ? BUILDING_ALL_DEPENDENCIES : BUILDING_STANDARD_DEPENDENCIES ) )
				return false;
		}

		// Queue up the compilation task
		AddCompilationTask( pDependentMakefile, pAdapter );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Default implementatations for precompilation step
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::PerformCompilationStep( CDmElement *pElement, CompilationStep_t step )
{
	// Do nothing
	return true;
}

bool CDmeMakefileUtils::PerformCompilationStep( CDmeMakefile *pMakefile, CompilationStep_t step )
{
	switch( step )
	{
	case BUILDING_ALL_DEPENDENCIES:
		return AddCompileDependencies( pMakefile, true );

	case BUILDING_STANDARD_DEPENDENCIES:
		return AddCompileDependencies( pMakefile, false );

	case BEFORE_COMPILATION:
		pMakefile->PreCompile();
		break;

	case AFTER_COMPILATION_SUCCEEDED:
		pMakefile->PostCompile();
		break;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Starts the next compile task
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::StartNextCompileTask( )
{
	Assert( m_hCompileProcess == PROCESS_HANDLE_INVALID );
	++m_nCurrentCompileTask;
	if ( m_nCurrentCompileTask == m_CompileTasks.Count() )
	{
		PerformCompilationStep( AFTER_COMPILATION_SUCCEEDED );
		m_nCurrentCompileTask = -1;
		m_CompileTasks.RemoveAll();
		return;
	}

	m_hCompileProcess = PROCESS_HANDLE_INVALID;

	// NOTE: PerformCompilationStep is expected to call SetCompileProcess to set m_hCompileProcess
	CompileInfo_t &info = m_CompileTasks[m_nCurrentCompileTask];
	bool bOk = info.m_pAdapter->PerformCompilationStep( info.m_hElement, PERFORMING_COMPILATION );

	if ( !bOk || ( m_hCompileProcess == PROCESS_HANDLE_INVALID ) )
	{
		AbortCurrentCompilation();
		return;
	}
}


//-----------------------------------------------------------------------------
// Performs the compilation step on all elements
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::PerformCompilationStep( CompilationStep_t step )
{
	// Iterate through all elements and run a compilation step
	m_CompilationStep = step;
	int nCount = m_CompileTasks.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CompileInfo_t &info = m_CompileTasks[i];
		if ( info.m_hElement.Get() )
		{
			if ( !info.m_pAdapter->PerformCompilationStep( info.m_hElement, step ) )
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for compilation
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::PerformCompile( CDmElement *pElement, bool bBuildAllDependencies )
{
	if ( IsCurrentlyCompiling() )
	{
		AbortCurrentCompilation();
	}

	CCompileFuncAdapterBase *pAdapter = DetermineCompileAdapter( pElement );
	if ( !pAdapter )
	{
		m_CompilationStep = AFTER_COMPILATION_FAILED;
		return;
	}

	// Add dependent makefiles first
	m_CompilationStep = bBuildAllDependencies ? BUILDING_ALL_DEPENDENCIES : BUILDING_STANDARD_DEPENDENCIES;
	if ( !pAdapter->PerformCompilationStep( pElement, m_CompilationStep ) )
	{
		AbortCurrentCompilation();
		return;
	}

	// Queue up the compilation task
	AddCompilationTask( pElement, pAdapter );

	// Iterate through all elements and run a precompilation step
	// NOTE: This is where perforce integration should go
	if ( !PerformCompilationStep( BEFORE_COMPILATION ) )
	{
		AbortCurrentCompilation();
		return;
	}

	// Dequeue the first compile task and start it up
	m_CompilationStep = PERFORMING_COMPILATION;
	StartNextCompileTask();
}


//-----------------------------------------------------------------------------
// Are we in the middle of compiling this makefile?
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::IsCurrentlyCompiling()
{
	return ( m_CompilationStep != NOT_COMPILING );
}


//-----------------------------------------------------------------------------
// Aborts any current compilation
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::AbortCurrentCompilation()
{
	if ( m_hCompileProcess != PROCESS_HANDLE_INVALID )
	{
		g_pProcessUtils->AbortProcess( m_hCompileProcess );
		m_hCompileProcess = PROCESS_HANDLE_INVALID;
	}

	if ( IsCurrentlyCompiling() )
	{
		PerformCompilationStep( AFTER_COMPILATION_FAILED );
		m_nCurrentCompileTask = -1;
		m_CompileTasks.RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Returns the exit code of the failed compilation (if COMPILATION_FAILED occurred)
//-----------------------------------------------------------------------------
int CDmeMakefileUtils::GetExitCode()
{
	return m_nExitCode;
}


//-----------------------------------------------------------------------------
// Returns output from the compilation
//-----------------------------------------------------------------------------
int CDmeMakefileUtils::GetCompileOutputSize()
{
	if ( m_hCompileProcess == PROCESS_HANDLE_INVALID )
		return 0;
	return g_pProcessUtils->GetProcessOutputSize( m_hCompileProcess );
}

CompilationState_t CDmeMakefileUtils::UpdateCompilation( char *pOutputBuf, int nBufLen )
{
	switch( m_CompilationStep )
	{
	case BUILDING_STANDARD_DEPENDENCIES:
	case BUILDING_ALL_DEPENDENCIES:
	case BEFORE_COMPILATION:
		return COMPILATION_NOT_COMPLETE;
 
	case AFTER_COMPILATION_FAILED:
		m_CompilationStep = NOT_COMPILING;
		return COMPILATION_FAILED;

	case AFTER_COMPILATION_SUCCEEDED:
		m_CompilationStep = NOT_COMPILING;
		return COMPILATION_SUCCESSFUL;
	}

	// This is the PERFORMING_COMPILATION case:

	// FIXME: Check return codes from compile process..
	// fail if compilation process had a problem
	if ( m_hCompileProcess == PROCESS_HANDLE_INVALID )
	{
		if ( nBufLen > 0 )
		{
			pOutputBuf[0] = 0;
		}
		return COMPILATION_SUCCESSFUL;
	}

	if ( nBufLen > 0 )
	{
		g_pProcessUtils->GetProcessOutput( m_hCompileProcess, pOutputBuf, nBufLen );
	}

	if ( !g_pProcessUtils->IsProcessComplete( m_hCompileProcess ) )
		return COMPILATION_NOT_COMPLETE;

	m_nExitCode = g_pProcessUtils->GetProcessExitCode( m_hCompileProcess ); 
	bool bCompileSucceeded = ( m_nExitCode == 0 );
	g_pProcessUtils->CloseProcess( m_hCompileProcess );
	m_hCompileProcess = PROCESS_HANDLE_INVALID;

	if ( !bCompileSucceeded )
	{
		AbortCurrentCompilation();
		return COMPILATION_NOT_COMPLETE;
	}

	StartNextCompileTask();
	if ( m_CompilationStep == PERFORMING_COMPILATION )
		return COMPILATION_NOT_COMPLETE;

	CompilationState_t retVal = ( m_CompilationStep == AFTER_COMPILATION_SUCCEEDED ) ? COMPILATION_SUCCESSFUL : COMPILATION_FAILED;
	m_CompilationStep = NOT_COMPILING;
	return retVal;
}


//-----------------------------------------------------------------------------
// Type-specific compilation functions
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::PerformCompilationStep( CDmeMDLMakefile *pMakeFile, CompilationStep_t step )
{
	if ( step != PERFORMING_COMPILATION )
		return PerformCompilationStep( static_cast<CDmeMakefile*>( pMakeFile ), step );

	char pBinDirectory[MAX_PATH];
	GetModSubdirectory( "..\\bin", pBinDirectory, sizeof(pBinDirectory) );
	Q_RemoveDotSlashes( pBinDirectory );

	char pStudioMDLCmd[MAX_PATH];
#ifdef _DEBUG
	Q_snprintf( pStudioMDLCmd, sizeof(pStudioMDLCmd), "%s\\studiomdl.exe -allowdebug %s", pBinDirectory, pMakeFile->GetFileName() );
#else
	Q_snprintf( pStudioMDLCmd, sizeof(pStudioMDLCmd), "%s\\studiomdl.exe %s", pBinDirectory, pMakeFile->GetFileName() );
#endif

	ProcessHandle_t hProcess = g_pProcessUtils->StartProcess( pStudioMDLCmd, true );
	SetCompileProcess( hProcess );
	return true;
}


//-----------------------------------------------------------------------------
// Exports a Maya file to a DMX file
//-----------------------------------------------------------------------------
bool CDmeMakefileUtils::PerformCompilationStep( CDmeMayaMakefile *pMakeFile, CompilationStep_t step )
{
	if ( step != PERFORMING_COMPILATION )
		return PerformCompilationStep( static_cast<CDmeMakefile*>( pMakeFile ), step );

	// FIXME: Create batch export command here
	CUtlString mayaCommand;
	mayaCommand = "vsDmxIO -export";

	CUtlVector< CDmeHandle< CDmeSourceMayaFile > > sources;
	pMakeFile->GetSources( sources );
	 
	if ( !sources.Count() )
		return false;

	CDmeSourceMayaFile *pDmeSourceDCCFile( sources[ 0 ].Get() );

	mayaCommand += " -selection";

	char pObjectId[128];
	UniqueIdToString( pMakeFile->GetId(), pObjectId, sizeof(pObjectId) );
	mayaCommand += " -makefileObjectId \\\"";
	mayaCommand += pObjectId;
	mayaCommand += "\\\"";

	mayaCommand += " -";
	mayaCommand += pDmeSourceDCCFile->m_ExportType.GetAttribute()->GetName();

	switch ( pDmeSourceDCCFile->m_ExportType.Get() )
	{
	case 1:		// skeletal animation
		mayaCommand += " skeletalAnimation";

		mayaCommand += " -";
		mayaCommand += pDmeSourceDCCFile->m_FrameStart.GetAttribute()->GetName();
		mayaCommand += " ";
		mayaCommand += pDmeSourceDCCFile->m_FrameStart.Get();

		mayaCommand += " -";
		mayaCommand += pDmeSourceDCCFile->m_FrameEnd.GetAttribute()->GetName();
		mayaCommand += " ";
		mayaCommand += pDmeSourceDCCFile->m_FrameEnd.Get();

		mayaCommand += " -";
		mayaCommand += pDmeSourceDCCFile->m_FrameIncrement.GetAttribute()->GetName();
		mayaCommand += " ";
		mayaCommand += pDmeSourceDCCFile->m_FrameIncrement.Get();
		break;
	default:	// Model
		mayaCommand += " model";
		break;
	}

	char pFileName[MAX_PATH];
	Q_strncpy( pFileName, pMakeFile->GetFileName(), sizeof( pFileName ) );
	Q_FixSlashes( pFileName, '/' );
	mayaCommand += " -filename \\\"";
	mayaCommand += pFileName;
	mayaCommand += "\\\"";

	const int rootObjectCount( pDmeSourceDCCFile->m_RootDCCObjects.Count() );
	for ( int rootObjectIndex( 0 ); rootObjectIndex < rootObjectCount; ++rootObjectIndex )
	{
		mayaCommand += " ";
		mayaCommand += pDmeSourceDCCFile->m_RootDCCObjects[ rootObjectIndex ];
	}

	char pSourcePath[MAX_PATH];
	pMakeFile->GetSourceFullPath( pDmeSourceDCCFile, pSourcePath, sizeof(pSourcePath) );

	// Maya wants forward slashes
	Q_FixSlashes( pSourcePath, '/' );
    
	char pMayaCommand[1024];
	Q_snprintf( pMayaCommand, sizeof(pMayaCommand), "mayabatch.exe -batch -file \"%s\" -command \"%s\"", pSourcePath, mayaCommand.Get() );
	ProcessHandle_t hProcess = g_pProcessUtils->StartProcess( pMayaCommand, true );
	SetCompileProcess( hProcess );
	return true;
}


//-----------------------------------------------------------------------------
// Opens Maya with a particular file
//-----------------------------------------------------------------------------
void CDmeMakefileUtils::OpenEditor( CDmeSourceMayaFile *pDmeSourceDCCFile )
{
	CDmeMayaMakefile *pMakefile = FindReferringElement< CDmeMayaMakefile >( pDmeSourceDCCFile, "sources" );
	if ( !pMakefile )
		return;

	char pSourcePath[MAX_PATH];
	pMakefile->GetSourceFullPath( pDmeSourceDCCFile, pSourcePath, sizeof(pSourcePath) );

	// Maya wants forward slashes
	Q_FixSlashes( pSourcePath, '/' );

	char pMayaCommand[1024];
	Q_snprintf( pMayaCommand, sizeof(pMayaCommand), "maya.exe -file \"%s\"", pSourcePath );
	g_pProcessUtils->StartProcess( pMayaCommand, true );
}
