//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Describes an asset: something that is compiled from sources, 
// in potentially multiple steps, to a compiled resource
//
//=============================================================================


#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemdl.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceSkin, CDmeSourceSkin );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceSkin::OnConstruction()
{
	m_SkinName.Init( this, "skinName" );
	m_bFlipTriangles.Init( this, "flipTriangles" );
	m_flScale.InitAndSet( this, "scale", 1.0f );
}

void CDmeSourceSkin::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// These can be built from DCC makefiles
//-----------------------------------------------------------------------------
static const char *s_pSkinMakeFiles[] = 
{
	"DmeMayaModelMakefile",
	"DmeXSIModelMakefile",
	NULL
};

const char **CDmeSourceSkin::GetSourceMakefileTypes()
{ 
	return s_pSkinMakeFiles; 
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceCollisionModel, CDmeSourceCollisionModel );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceCollisionModel::OnConstruction()
{
}

void CDmeSourceCollisionModel::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// These can be built from DCC makefiles
//-----------------------------------------------------------------------------
const char **CDmeSourceCollisionModel::GetSourceMakefileTypes()
{ 
	return s_pSkinMakeFiles; 
}


//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSourceAnimation, CDmeSourceAnimation );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeSourceAnimation::OnConstruction()
{
	m_AnimationName.Init( this, "animationName" );
	m_SourceAnimationName.Init( this, "sourceAnimationName" );
}

void CDmeSourceAnimation::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// These can be built from DCC makefiles
//-----------------------------------------------------------------------------
static const char *s_pAnimationMakeFiles[] = 
{
	"DmeMayaAnimationMakefile",
	"DmeXSIAnimationMakefile",
	NULL
};

const char **CDmeSourceAnimation::GetSourceMakefileTypes()
{ 
	return s_pAnimationMakeFiles; 
}



//-----------------------------------------------------------------------------
// Hook into datamodel
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMDLMakefile, CDmeMDLMakefile );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMDLMakefile::OnConstruction()
{
	m_hMDL = CreateElement< CDmeMDL >( "MDLMakefile Preview" );
	m_bFlushMDL = false;
}

void CDmeMDLMakefile::OnDestruction()
{
	DestroyElement( m_hMDL.Get() );
}


//-----------------------------------------------------------------------------
// Returns source types
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_pSourceTypes[] = 
{
	{ "DmeSourceSkin",				"Skin",				true,	"makefiledir:models\\dmx", "*.dmx", "Valve DMX File (*.dmx)" },
	{ "DmeSourceAnimation",			"Animation",		false,	"makefiledir:animations\\dmx", "*.dmx", "Valve DMX File (*.dmx)" },
	{ "DmeSourceCollisionModel",	"Collision Model",	true,	"makefiledir:models\\dmx", "*.dmx", "Valve DMX File (*.dmx)" },
	{ NULL, NULL, false, NULL, NULL, NULL },
};

DmeMakefileType_t* CDmeMDLMakefile::GetSourceTypes()
{
	return s_pSourceTypes;
}


//-----------------------------------------------------------------------------
// Makefile type 
//-----------------------------------------------------------------------------
static DmeMakefileType_t s_MakefileType = 
{ 
	"DmeMDLMakefile", "Model", true, "contentdir:models", "*.dmx", "Valve Model MakeFile (*.dmx)" 
};


DmeMakefileType_t *CDmeMDLMakefile::GetMakefileType()
{
	return &s_MakefileType;
}


//-----------------------------------------------------------------------------
// Add, remove sources
//-----------------------------------------------------------------------------
void CDmeMDLMakefile::SetSkin( const char *pFullPath )
{
	RemoveAllSources( "DmeSourceSkin" );
	AddSource( "DmeSourceSkin", pFullPath );
}

void CDmeMDLMakefile::AddAnimation( const char *pFullPath )
{
	AddSource( "animation", pFullPath );
}

void CDmeMDLMakefile::RemoveAnimation( const char *pFullPath )
{
	RemoveSource( "animation", pFullPath );
}

void CDmeMDLMakefile::RemoveAllAnimations( )
{
	RemoveAllSources( "animation" );
}


//-----------------------------------------------------------------------------
// Inherited classes should re-implement these methods
//-----------------------------------------------------------------------------
CDmElement *CDmeMDLMakefile::CreateOutputElement( )
{
	if ( m_bFlushMDL )
	{
		// Flush the model out of the cache; detach it from the MDL
		MDLHandle_t h = m_hMDL->GetMDL();
		if ( h != MDLHANDLE_INVALID )
		{
			g_pMDLCache->Flush( h );
		}
		m_bFlushMDL = false;
	}
	m_hMDL->SetMDL( MDLHANDLE_INVALID );

	// FIXME: Should we ask the tool (studiomdl) for this?
	// Should we have output type names? Not sure yet..
	// Doing the simplest thing first.
	char pOutputName[MAX_PATH];
	Q_FileBase( GetFileName(), pOutputName, sizeof(pOutputName) );
	if ( !pOutputName[0] )
		return m_hMDL.Get();

	char pOutputDir[MAX_PATH];
	GetOutputDirectory( pOutputDir, sizeof(pOutputDir) );
	if ( !pOutputDir[0] )
		return m_hMDL.Get();

	Q_StripTrailingSlash( pOutputDir );
	char pFullPath[MAX_PATH];
	Q_snprintf( pFullPath, sizeof(pFullPath), "%s\\%s.mdl", pOutputDir, pOutputName );

	char pRelativePath[MAX_PATH];
	g_pFullFileSystem->FullPathToRelativePathEx( pFullPath, "GAME", pRelativePath, sizeof( pRelativePath ) );

	MDLHandle_t h = g_pMDLCache->FindMDL( pRelativePath );
	m_hMDL->SetMDL( h );
	return m_hMDL.Get();	
}

void CDmeMDLMakefile::DestroyOutputElement( CDmElement *pOutput )
{
	m_bFlushMDL = true;
}


//-----------------------------------------------------------------------------
// Compile assets 
//-----------------------------------------------------------------------------
static const char *s_pOutputExtensions[] =
{
	"dx80.vtx",
	"dx90.vtx",
	"sw.vtx",
	"mdl",
	"vvd",
	"phy",
	NULL
};

void CDmeMDLMakefile::GetOutputs( CUtlVector<CUtlString> &fullPaths )
{
	fullPaths.RemoveAll();

	// FIXME: Should we ask the tool (studiomdl) for this?
	// Should we have output type names? Not sure yet..
	// Doing the simplest thing first.
	char pOutputName[MAX_PATH];
	Q_FileBase( GetFileName(), pOutputName, sizeof(pOutputName) );
	if ( !pOutputName[0] )
		return;

	char pOutputDir[MAX_PATH];
	GetOutputDirectory( pOutputDir, sizeof(pOutputDir) );
	if ( !pOutputDir[0] )
		return;

	Q_StripTrailingSlash( pOutputDir );
	char pFullPath[MAX_PATH];
	for ( int i = 0; s_pOutputExtensions[i]; ++i )
	{
		Q_snprintf( pFullPath, sizeof(pFullPath), "%s\\%s.%s", pOutputDir, pOutputName, s_pOutputExtensions[i] );
		fullPaths.AddToTail( pFullPath );
	}
}

