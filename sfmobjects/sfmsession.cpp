//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A class representing session state for the SFM
//
//=============================================================================

#include "sfmobjects/sfmsession.h"
#include "studio.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmecamera.h"
#include "movieobjects/dmetimeselection.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmegamemodel.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSFMSession::CSFMSession()
{
	m_hRoot = NULL;
}


//-----------------------------------------------------------------------------
// Sets the root
//-----------------------------------------------------------------------------
void CSFMSession::SetRoot( CDmElement *pRoot )
{
	m_hRoot = pRoot;
}


//-----------------------------------------------------------------------------
// Creates a new (empty) session
//-----------------------------------------------------------------------------
void CSFMSession::Init()
{
	m_hRoot = NULL;

	// a movie currently consists of: (this is all just temporary until clips take over more completely)
	// a generic "root" node
	//		movie - a clip node whose subclips are the movie sequence
	//		cameras - an array of cameras used throughout the movie
	//		clips - an array of clips used throughout the movie
	CDmElement *pRoot = CreateElement< CDmElement >( "session" );
	Assert( pRoot );
	if ( !pRoot )
		return;
	m_hRoot = pRoot;

	// FIXME!
	pRoot->SetValue( "editorType", "ifm" );

	CDmeFilmClip *pFilmClip = CreateElement<CDmeFilmClip>( "sequence" );
	Assert( pFilmClip != NULL );
	pFilmClip->SetDuration( DmeTime_t( 60.0f ) );

	CDmeTrack *pTrack = pFilmClip->FindOrCreateFilmTrack();
	CDmeClip *pShot = CreateElement<CDmeFilmClip>( "shot" );
	pTrack->AddClip( pShot );
	pShot->SetDuration( DmeTime_t( 60.0f ) );

	pRoot->SetValue( "activeClip", pFilmClip );

	pRoot->AddAttributeElementArray< CDmElement >( "miscBin" );
	pRoot->AddAttributeElementArray< CDmeCamera >( "cameraBin" );
	CDmAttribute *pClipBin = pRoot->AddAttributeElementArray< CDmeClip >( "clipBin" );

	// Don't allow duplicates in the clipBin
	pClipBin->AddFlag( FATTRIB_NODUPLICATES );

	CDmrElementArray<> clipBin( pRoot, "clipBin" );
	clipBin.AddToTail( pFilmClip );

	CreateSessionSettings();
}


//-----------------------------------------------------------------------------
// Shuts down the session
//-----------------------------------------------------------------------------
void CSFMSession::Shutdown()
{
	if ( m_hRoot.Get() )
	{
		if ( m_hRoot->GetFileId() != DMFILEID_INVALID )
		{
			g_pDataModel->RemoveFileId( m_hRoot->GetFileId() );
			m_hRoot = NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Creates session settings
//-----------------------------------------------------------------------------
void CSFMSession::CreateSessionSettings()
{
	if ( !m_hRoot.Get() )
		return;

	m_hRoot->AddAttribute( "settings", AT_ELEMENT );

	CDmElement *pSettings = m_hRoot->GetValueElement< CDmElement >( "settings" );
	if ( !pSettings )
	{
		pSettings = CreateElement< CDmElement >( "sessionSettings", m_hRoot->GetFileId() );
		m_hRoot->SetValue( "settings", pSettings );
	}

	Assert( pSettings );

	CDmeTimeSelection *ts = NULL;
	if ( !pSettings->HasAttribute( "timeSelection" ) )
	{
		ts = CreateElement< CDmeTimeSelection >( "timeSelection", m_hRoot->GetFileId() );
		pSettings->SetValue( "timeSelection", ts );
	}

	pSettings->InitValue( "animationSetOverlayBackground", Color( 0, 0, 0, 192 ) );

	if ( !pSettings->HasAttribute( "standardColors" ) )
	{
		CDmrArray<Color> colors( pSettings, "standardColors", true );
		colors.AddToTail( Color( 0, 0, 0, 128 ) );
		colors.AddToTail( Color( 194, 120, 0, 128 ) );
		colors.AddToTail( Color( 255, 0, 100, 128 ) );
		colors.AddToTail( Color( 200, 200, 255, 128 ) );
		colors.AddToTail( Color( 255, 255, 255, 128 ) );
	}

	float flLegacyFrameRate = 24.0f;
	if ( pSettings->HasAttribute( "frameRate" ) )
	{
		flLegacyFrameRate = pSettings->GetValue<float>( "frameRate" );

		// remove this from the base level settings area since we're going to add it in renderSettings
		pSettings->RemoveAttribute( "frameRate" );
	}

	if ( !pSettings->HasAttribute( "proceduralPresets" ) )
	{
		CDmeProceduralPresetSettings *ps = CreateElement< CDmeProceduralPresetSettings >( "proceduralPresets", m_hRoot->GetFileId() );
		pSettings->SetValue( "proceduralPresets", ps );
	}

	CreateRenderSettings( pSettings, flLegacyFrameRate );
}


//-----------------------------------------------------------------------------
// Creates session render settings
// JasonM - remove flLegacyFramerate param eventually (perhaps March 2007)
//-----------------------------------------------------------------------------
void CSFMSession::CreateRenderSettings( CDmElement *pSettings, float flLegacyFramerate )
{
	pSettings->AddAttribute( "renderSettings", AT_ELEMENT );

	CDmElement *pRenderSettings = pSettings->GetValueElement< CDmElement >( "renderSettings" );
	if ( !pRenderSettings )
	{
		pRenderSettings = CreateElement< CDmElement >( "renderSettings", pSettings->GetFileId() );
		pSettings->SetValue( "renderSettings", pRenderSettings );
	}
	Assert( pRenderSettings );

	pRenderSettings->InitValue( "frameRate", flLegacyFramerate );	// Default framerate
	pRenderSettings->InitValue( "lightAverage", 0 );				// Don't light average by default
	pRenderSettings->InitValue( "showFocalPlane", 0 );				// Don't show focal plane by default
	pRenderSettings->InitValue( "modelLod", 0 );				// Don't do model LOD by default

	CreateProgressiveRefinementSettings( pRenderSettings );
}


//-----------------------------------------------------------------------------
// Creates session progrssing refinement settings
//-----------------------------------------------------------------------------
void CSFMSession::CreateProgressiveRefinementSettings( CDmElement *pRenderSettings )
{
	// Do we already have Progressive refinement settings?
	CDmElement *pRefinementSettings = pRenderSettings->GetValueElement< CDmElement >( "ProgressiveRefinement" );
	if ( !pRefinementSettings )
	{
		pRefinementSettings = CreateElement< CDmElement >( "ProgressiveRefinementSettings", pRenderSettings->GetFileId() );
		pRenderSettings->SetValue( "ProgressiveRefinement", pRefinementSettings );
	}

	// Set up defaults for progressive refinement settings...
	pRefinementSettings->InitValue( "on", true );
	pRefinementSettings->InitValue( "useDepthOfField", true );
	pRefinementSettings->InitValue( "overrideDepthOfFieldQuality", false );
	pRefinementSettings->InitValue( "overrideDepthOfFieldQualityValue", 1 );
	pRefinementSettings->InitValue( "useMotionBlur", true );
	pRefinementSettings->InitValue( "overrideMotionBlurQuality", false );
	pRefinementSettings->InitValue( "overrideMotionBlurQualityValue", 1 );
	pRefinementSettings->InitValue( "useAntialiasing", false );
	pRefinementSettings->InitValue( "overrideShutterSpeed", false );
	pRefinementSettings->InitValue( "overrideShutterSpeedValue", 1.0f / 48.0f );
}


//-----------------------------------------------------------------------------
// Creates a camera
//-----------------------------------------------------------------------------
CDmeCamera *CSFMSession::CreateCamera( const DmeCameraParams_t& params )
{
	CDmeCamera *pCamera = CreateElement< CDmeCamera >( params.name, m_hRoot->GetFileId() );

	// Set parameters
	matrix3x4_t txform;
	AngleMatrix( params.angles, params.origin, txform );

	CDmeTransform *pTransform = pCamera->GetTransform();
	if ( pTransform )
	{
		pTransform->SetTransform( txform );
	}

	pCamera->SetFOVx( params.fov );
	return pCamera;
}


//-----------------------------------------------------------------------------
// Finds or creates a scene
//-----------------------------------------------------------------------------
CDmeDag *CSFMSession::FindOrCreateScene( CDmeFilmClip *pShot, const char *pSceneName )
{
	CDmeDag *pScene = pShot->GetScene();
	if ( !pScene )
	{
		pScene = CreateElement< CDmeDag >( "scene", pShot->GetFileId() );
		pShot->SetScene( pScene );
	}
	Assert( pScene );

	int c = pScene->GetChildCount();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeDag *pChild = pScene->GetChild( i );
		if ( pChild && !Q_stricmp( pChild->GetName(), pSceneName ) )
			return pChild;
	}

	CDmeDag *pNewScene = CreateElement< CDmeDag >( pSceneName, pScene->GetFileId() );
	pScene->AddChild( pNewScene );

	return pNewScene;
}


//-----------------------------------------------------------------------------
// Creates a game model
//-----------------------------------------------------------------------------
CDmeGameModel *CSFMSession::CreateEditorGameModel( studiohdr_t *hdr, const Vector &vecOrigin, Quaternion &qOrientation )
{
	char pBaseName[ 256 ];
	Q_FileBase( hdr->pszName(), pBaseName, sizeof( pBaseName ) );

	char pGameModelName[ 256 ];
	Q_snprintf( pGameModelName, sizeof( pGameModelName ), "%s_GameModel", pBaseName );
	CDmeGameModel *pGameModel = CreateElement< CDmeGameModel >( pGameModelName, m_hRoot->GetFileId() );

	char pRelativeModelsFileName[MAX_PATH];
	Q_ComposeFileName( "models", hdr->pszName(), pRelativeModelsFileName, sizeof(pRelativeModelsFileName) );
	pGameModel->SetValue( "modelName", pRelativeModelsFileName );

	CDmeTransform *pTransform = pGameModel->GetTransform();
	if ( pTransform )
	{
		pTransform->SetPosition( vecOrigin );
		pTransform->SetOrientation( qOrientation );
	}

	// create, connect and cache each bone's pos and rot channels
	pGameModel->AddBones( hdr, pBaseName, 0, hdr->numbones );
	return pGameModel;
}
  

