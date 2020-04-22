//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Describes an asset: something that is compiled from sources, 
// in potentially multiple steps, to a compiled resource
//
//=============================================================================


#include "movieobjects/dmedccmakefile.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/fileutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceDCCFile, CDmeSourceDCCFile );

				    
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceDCCFile::OnConstruction()
{
	m_RootDCCObjects.Init( this, "rootDCCObjects" );
	m_ExportType.InitAndSet( this, "exportType", 0 );
	m_FrameStart.InitAndSet( this, "frameStart", 0.0f );
	m_FrameEnd.InitAndSet( this, "frameEnd", 0.0f );
	m_FrameIncrement.InitAndSet( this, "frameIncrement", 1.0f );
}

void CDmeSourceDCCFile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceMayaFile, CDmeSourceMayaFile );
IMPLEMENT_ELEMENT_FACTORY( DmeSourceMayaModelFile, CDmeSourceMayaModelFile );
IMPLEMENT_ELEMENT_FACTORY( DmeSourceMayaAnimationFile, CDmeSourceMayaAnimationFile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceMayaFile::OnConstruction()
{
}

void CDmeSourceMayaFile::OnDestruction()
{
}

void CDmeSourceMayaModelFile::OnConstruction()
{
	m_ExportType = 0;
}

void CDmeSourceMayaModelFile::OnDestruction()
{
}

void CDmeSourceMayaAnimationFile::OnConstruction()
{
	m_ExportType = 1;
}

void CDmeSourceMayaAnimationFile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceXSIFile, CDmeSourceXSIFile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceXSIFile::OnConstruction()
{
}

void CDmeSourceXSIFile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDCCMakefile, CDmeDCCMakefile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeDCCMakefile::OnConstruction()
{
	m_bFlushFile = false;
}

void CDmeDCCMakefile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Compile assets 
//-----------------------------------------------------------------------------
void CDmeDCCMakefile::GetOutputs( CUtlVector<CUtlString> &fullPaths )
{
	fullPaths.RemoveAll();

	char pOutputName[MAX_PATH];
	Q_FileBase( GetFileName(), pOutputName, sizeof(pOutputName) );
	if ( !pOutputName[0] )
		return;

	// FIXME: We need to come up with an appropriate directory structure for export
	char pOutputDir[MAX_PATH];
	GetMakefilePath( pOutputDir, sizeof(pOutputDir) );
	if ( !pOutputDir[0] )
		return;

	Q_StripTrailingSlash( pOutputDir );
	char pFullPath[MAX_PATH];
	Q_snprintf( pFullPath, sizeof(pFullPath), "%s\\%s.dmx", pOutputDir, pOutputName );
	fullPaths.AddToTail( pFullPath );
}

  
//-----------------------------------------------------------------------------
// Creates, destroys the output element associated with this makefile
//-----------------------------------------------------------------------------
CDmElement *CDmeDCCMakefile::CreateOutputElement( )
{
	if ( m_bFlushFile )
	{  
		m_bFlushFile = false;
		if ( GetFileId() != DMFILEID_INVALID )
		{
			// NOTE: CDmeHandles will correctly re-hook up to the new makefile after load
			// If the file fails to load, we have the copy. If the file correctly has the make in it
			// it will replace this copy I made
			CDmeHandle< CDmeDCCMakefile > hMakefileOld;
			hMakefileOld = this;

			// NOTE NOTE NOTE
			// UnloadFile essentially calls delete this!
			// So don't refer to any state in this DmElement after that
			DmFileId_t fileId = GetFileId(); 
			g_pDataModel->UnloadFile( fileId );

			CDmElement *pRoot = NULL;
			if ( g_pDataModel->RestoreFromFile( g_pDataModel->GetFileName( fileId ), NULL, NULL, &pRoot, CR_DELETE_OLD ) != DMFILEID_INVALID )
			{
				// NOTE: Unload/restore kills the this pointer, we need to redo this
				if ( hMakefileOld.Get() )
				{
					hMakefileOld->SetDirty( false );
					return hMakefileOld->CreateOutputElement();
				}
			}

			// NOTE: We expect file backup prior to compile to avoid really fatal errors
			// This case happens if the file failed to load. In this case, we must use
			// the copy of the makefile
			Assert( 0 );
			return NULL;
		}
	}

	// The output element is the root element containing the makefile
	return FindReferringElement< CDmElement >( this, "makefile" );
}

void CDmeDCCMakefile::DestroyOutputElement( CDmElement *pOutput )
{
	m_bFlushFile = true;
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMayaMakefile, CDmeMayaMakefile );
IMPLEMENT_ELEMENT_FACTORY( DmeMayaModelMakefile, CDmeMayaModelMakefile );
IMPLEMENT_ELEMENT_FACTORY( DmeMayaAnimationMakefile, CDmeMayaAnimationMakefile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMayaMakefile::OnConstruction()
{
}

void CDmeMayaMakefile::OnDestruction()
{
}

void CDmeMayaModelMakefile::OnConstruction()
{
}

void CDmeMayaModelMakefile::OnDestruction()
{
}

void CDmeMayaAnimationMakefile::OnConstruction()
{
}

void CDmeMayaAnimationMakefile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns source types
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_pMayaModelSourceTypes[] = 
{
	{ "DmeSourceMayaModelFile", "Maya Model File", true, "makefiledir:../maya", "*.ma;*.mb", "Maya File (*.ma,*.mb)" },
	{ NULL, NULL, false, NULL, NULL, NULL },
};

DmeMakefileType_t* CDmeMayaModelMakefile::GetSourceTypes()
{
	return s_pMayaModelSourceTypes;
}

static DmeMakefileType_t s_pMayaAnimationSourceTypes[] = 
{
	{ "DmeSourceMayaAnimationFile", "Maya Animation File", true, "makefiledir:../maya", "*.ma;*.mb", "Maya File (*.ma,*.mb)" },
	{ NULL, NULL, false, NULL, NULL, NULL },
};

DmeMakefileType_t* CDmeMayaAnimationMakefile::GetSourceTypes()
{
	return s_pMayaAnimationSourceTypes;
}


//-----------------------------------------------------------------------------
// Makefile type 
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_MayaModelMakefileType = 
{
	"DmeMayaModelMakefile", "Maya Model Component", true, "contentdir:models", "*.dmx", "DMX File (*.dmx)"
};

DmeMakefileType_t *CDmeMayaModelMakefile::GetMakefileType()
{
	return &s_MayaModelMakefileType;
}

static DmeMakefileType_t s_MayaAnimationMakefileType = 
{
	"DmeMayaAnimationMakefile", "Maya Animation Component", true, "contentdir:models", "*.dmx", "DMX File (*.dmx)"
};

DmeMakefileType_t *CDmeMayaAnimationMakefile::GetMakefileType()
{
	return &s_MayaAnimationMakefileType;
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeXSIMakefile, CDmeXSIMakefile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeXSIMakefile::OnConstruction()
{
}

void CDmeXSIMakefile::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns source types
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_pXSISourceTypes[] = 
{
	{ "DmeSourceXSIFile", "XSI File", true, "makefiledir:../xsi", "*.xsi", "XSI File (*.xsi)" },
	{ NULL, NULL, false, NULL, NULL, NULL },
};

DmeMakefileType_t* CDmeXSIMakefile::GetSourceTypes()
{
	return s_pXSISourceTypes;
}


//-----------------------------------------------------------------------------
// Makefile type 
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_XSIMakefileType = 
{
	"DmeXSIMakefile", "XSI Model Component", true, "contentdir:models", "*.dmx", "DMX File (*.dmx)",
};

DmeMakefileType_t *CDmeXSIMakefile::GetMakefileType()
{
	return &s_XSIMakefileType;
}


