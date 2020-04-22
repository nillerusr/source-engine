//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// updates:
// 1-4-99	fixed file texture load and file read bug

////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "StudioModel.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "ViewerSettings.h"
#include "bone_setup.h"
#include "UtlMemory.h"
#include "mxtk/mx.h"
#include "filesystem.h"
#include "IStudioRender.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "MDLViewer.h"
#include "optimize.h"

extern char g_appTitle[];
Vector *StudioModel::m_AmbientLightColors;

#pragma warning( disable : 4244 ) // double to float


static StudioModel g_studioModel;
static StudioModel *g_pActiveModel;

// Expose it to the rest of the app
StudioModel *g_pStudioModel = &g_studioModel;
StudioModel *g_pStudioExtraModel[HLMV_MAX_MERGED_MODELS];

StudioModel::StudioModel()
{
	m_MDLHandle = MDLHANDLE_INVALID;
	ClearLookTargets();
}

void StudioModel::Init()
{
	m_AmbientLightColors = new Vector[g_pStudioRender->GetNumAmbientLightSamples()];
	
	// JasonM & garymcthack - should really only do this once a frame and at init time.
	UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
							  g_viewerSettings.showNormals,
							  g_viewerSettings.showTangentFrame );
}

void StudioModel::Shutdown( void )
{
	g_pStudioModel->FreeModel( false );
	delete [] m_AmbientLightColors;
}

void StudioModel::SetCurrentModel()
{
	// track the correct model
	g_pActiveModel = this;
}

void StudioModel::ReleaseStudioModel()
{
	SaveViewerSettings( g_pStudioModel->GetFileName(), g_pStudioModel );
	g_pStudioModel->FreeModel( true ); 
}

void StudioModel::RestoreStudioModel()
{
	// should view settings be loaded before the model is loaded?
	if ( g_pStudioModel->LoadModel( g_pStudioModel->m_pModelName ) )
	{
		g_pStudioModel->PostLoadModel( g_pStudioModel->m_pModelName );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Frees the model data and releases textures from OpenGL.
//-----------------------------------------------------------------------------
void StudioModel::FreeModel( bool bReleasing )
{
	if ( m_pStudioHdr )
	{
		delete m_pStudioHdr;
		m_pStudioHdr = NULL;
	}

	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_MDLHandle );
		m_MDLHandle = MDLHANDLE_INVALID;
	}

	if ( !bReleasing )
	{
		if (m_pModelName)
		{
			delete[] m_pModelName;
			m_pModelName = NULL;
		}
	}

	m_SurfaceProps.Purge();

	DestroyPhysics( m_pPhysics );
	m_pPhysics = NULL;
}

void *StudioModel::operator new( size_t stAllocateBlock )
{
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
}

void StudioModel::operator delete( void *pMem )
{
#ifdef _DEBUG
	// set the memory to a known value
	int size = _msize( pMem );
	memset( pMem, 0xcd, size );
#endif

	// get the engine to free the memory
	free( pMem );
}

void *StudioModel::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	// call into engine to get memory
	Assert( stAllocateBlock != 0 );
	return calloc( 1, stAllocateBlock );
}

void StudioModel::operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )
{
#ifdef _DEBUG
	// set the memory to a known value
	int size = _msize( pMem );
	memset( pMem, 0xcd, size );
#endif
	// get the engine to free the memory
	free( pMem );
}

bool StudioModel::LoadModel( const char *pModelName )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if (!pModelName)
		return 0;

	// In the case of restore, m_pModelName == modelname
	if (m_pModelName != pModelName)
	{
		// Copy over the model name; we'll need it later...
		if (m_pModelName)
		{
			delete[] m_pModelName;
		}
		m_pModelName = new char[Q_strlen(pModelName) + 1];
		strcpy( m_pModelName, pModelName );
	}

	m_MDLHandle = g_pMDLCache->FindMDL( pModelName );

	// allocate a pool for a studiohdr cache
	if (m_pStudioHdr != NULL)
	{
		delete m_pStudioHdr;
	}
	m_pStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_MDLHandle ), g_pMDLCache );

	// manadatory to access correct verts
	SetCurrentModel();

	m_pPhysics = LoadPhysics( m_MDLHandle );

	// Copy over all of the hitboxes; we may add and remove elements
	m_HitboxSets.RemoveAll();

	CStudioHdr *pStudioHdr = GetStudioHdr();

	int i;
	for ( int s = 0; s < pStudioHdr->numhitboxsets(); s++ )
	{
		mstudiohitboxset_t *pSrcSet = pStudioHdr->pHitboxSet( s );
		if ( !pSrcSet )
			continue;

		int j = m_HitboxSets.AddToTail();
		HitboxSet_t &set = m_HitboxSets[j];
		set.m_Name = pSrcSet->pszName();

		for ( i = 0; i < pSrcSet->numhitboxes; ++i )
		{
			mstudiobbox_t *pHit = pSrcSet->pHitbox(i);
			int nIndex = set.m_Hitboxes.AddToTail( );
			HitboxInfo_t &hitbox = set.m_Hitboxes[nIndex];

			hitbox.m_Name = pHit->pszHitboxName();
			hitbox.m_BBox = *pHit;

			// Blat out bbox name index so we don't use it by mistake...
			hitbox.m_BBox.szhitboxnameindex = 0;
		}
	}

	// Copy over all of the surface props; we may change them...
	for ( i = 0; i < pStudioHdr->numbones(); ++i )
	{
		mstudiobone_t* pBone = pStudioHdr->pBone(i);

		CUtlSymbol prop( pBone->pszSurfaceProp() );
		m_SurfaceProps.AddToTail( prop );
	}

	m_physPreviewBone = -1;

	bool forceOpaque = (pStudioHdr->flags() & STUDIOHDR_FLAGS_FORCE_OPAQUE) != 0;
	bool vertexLit = false;
	m_bIsTransparent = false;
	m_bHasProxy = false;

	studiohwdata_t *pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	if ( !pHardwareData )
	{
		Assert( 0 );
		return false;
	}

	for( int lodID = pHardwareData->m_RootLOD; lodID < pHardwareData->m_NumLODs; lodID++ )
	{
		studioloddata_t *pLODData = &pHardwareData->m_pLODs[lodID];
		for ( i = 0; i < pLODData->numMaterials; ++i )
		{
			if (pLODData->ppMaterials[i]->IsVertexLit())
			{
				vertexLit = true;
			}
			if ((!forceOpaque) && pLODData->ppMaterials[i]->IsTranslucent())
			{
				m_bIsTransparent = true;
				//Msg("Translucent material %s for model %s\n", pLODData->ppMaterials[i]->GetName(), pStudioHdr->name );
			}
			if (pLODData->ppMaterials[i]->HasProxy())
			{
				m_bHasProxy = true;
			}
		}
	}

	return true;
}



bool StudioModel::PostLoadModel( const char *modelname )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr == NULL)
		return false;

	SetSequence (0);
	SetController (0, 0.0f);
	SetController (1, 0.0f);
	SetController (2, 0.0f);
	SetController (3, 0.0f);
	SetBlendTime( DEFAULT_BLEND_TIME );
	// SetHeadTurn( 1.0f );  // FIXME:!!!

	int n;
	for (n = 0; n < pStudioHdr->numbodyparts(); n++)
	{
		SetBodygroup (n, 0);
	}

	SetSkin (0);

/*
	Vector mins, maxs;
	ExtractBbox (mins, maxs);
	if (mins[2] < 5.0f)
		m_origin[2] = -mins[2];
*/
	return true;
}


//------------------------------------------------------------------------------
// Returns true if the model has at least one body part with model data, false if not.
//------------------------------------------------------------------------------
bool StudioModel::HasModel()
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;
		
	for ( int i = 0; i < pStudioHdr->numbodyparts(); i++ )
	{
		if ( pStudioHdr->pBodypart(i)->nummodels )
		{
			return true;
		}
	}

	return false;
}


////////////////////////////////////////////////////////////////////////


int StudioModel::GetSequence( )
{
	return m_sequence;
}

int StudioModel::SetSequence( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iSequence > pStudioHdr->GetNumSeq())
		return m_sequence;

	m_prevsequence = m_sequence;
	m_sequence = iSequence;
	m_cycle = 0;
	m_sequencetime = 0.0;

	return m_sequence;
}

const char* StudioModel::GetSequenceName( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return NULL;

	if (iSequence < 0)
		return NULL;

	if (iSequence > pStudioHdr->GetNumSeq())
		return NULL;

	return pStudioHdr->pSeqdesc( iSequence ).pszLabel();
}

void StudioModel::ClearOverlaysSequences( void )
{
	ClearAnimationLayers( );
	memset( m_Layer, 0, sizeof( m_Layer ) );
}

void StudioModel::ClearAnimationLayers( void )
{
	m_iActiveLayers = 0;
}

int	StudioModel::GetNewAnimationLayer( int iPriority )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if ( m_iActiveLayers >= MAXSTUDIOANIMLAYERS )
	{
		Assert( 0 );
		return MAXSTUDIOANIMLAYERS - 1;
	}

	m_Layer[m_iActiveLayers].m_priority = iPriority;

	return m_iActiveLayers++;
}

int StudioModel::SetOverlaySequence( int iLayer, int iSequence, float flWeight )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if (iSequence < 0)
		return 0;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	if (iSequence > pStudioHdr->GetNumSeq())
		return m_Layer[iLayer].m_sequence;

	m_Layer[iLayer].m_sequence = iSequence;
	m_Layer[iLayer].m_weight = flWeight;
	m_Layer[iLayer].m_playbackrate = 1.0;

	return iSequence;
}

float StudioModel::SetOverlayRate( int iLayer, float flCycle, float flPlaybackRate )
{
	if (iLayer >= 0 && iLayer < MAXSTUDIOANIMLAYERS)
	{
		m_Layer[iLayer].m_cycle = flCycle;
		m_Layer[iLayer].m_playbackrate = flPlaybackRate;
	}
	return flCycle;
}


int StudioModel::GetOverlaySequence( int iLayer )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	return m_Layer[iLayer].m_sequence;
}


float StudioModel::GetOverlaySequenceWeight( int iLayer )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	if (iLayer < 0 || iLayer >= MAXSTUDIOANIMLAYERS)
	{
		Assert(0);
		return 0;
	}

	return m_Layer[iLayer].m_weight;
}


int StudioModel::LookupSequence( const char *szSequence )
{
	int i;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		if (!stricmp( szSequence, pStudioHdr->pSeqdesc( i ).pszLabel() ))
		{
			return i;
		}
	}
	return -1;
}

int StudioModel::LookupActivity( const char *szActivity )
{
	int i;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		if (!stricmp( szActivity, pStudioHdr->pSeqdesc( i ).pszActivityName() ))
		{
			return i;
		}
	}
	return -1;
}

int StudioModel::SetSequence( const char *szSequence )
{
	return SetSequence( LookupSequence( szSequence ) );
}

void StudioModel::StartBlending( void )
{
	// Switch back to old sequence ( this will oscillate between this one and the last one )
	SetSequence( m_prevsequence );
}

void StudioModel::SetBlendTime( float blendtime )
{
	if ( blendtime > 0.0f )
	{
		m_blendtime = blendtime;
	}
}

float StudioModel::GetTransitionAmount( void )
{
	if ( g_viewerSettings.blendSequenceChanges &&
		m_sequencetime < m_blendtime && m_prevsequence != m_sequence )
	{
		float s;
		s = ( m_sequencetime / m_blendtime );
		return s;
	}

	return 0.0f;
}

LocalFlexController_t StudioModel::LookupFlexController( char *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return LocalFlexController_t(0);

	for (LocalFlexController_t iFlex = LocalFlexController_t(0); iFlex < pStudioHdr->numflexcontrollers(); iFlex++)
	{
		if (stricmp( szName, pStudioHdr->pFlexcontroller( iFlex )->pszName() ) == 0)
		{
			return iFlex;
		}
	}
	return LocalFlexController_t(-1);
}


void StudioModel::SetFlexController( char *szName, float flValue )
{
	SetFlexController( LookupFlexController( szName ), flValue );
}

void StudioModel::SetFlexController( LocalFlexController_t iFlex, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);

		if (pflex->min != pflex->max)
		{
			flValue = (flValue - pflex->min) / (pflex->max - pflex->min);
		}
		m_flexweight[iFlex] = clamp( flValue, 0.0f, 1.0f );
	}
}


void StudioModel::SetFlexControllerRaw( LocalFlexController_t iFlex, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
//		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);
		m_flexweight[iFlex] = clamp( flValue, 0.0f, 1.0f );
	}
}

float StudioModel::GetFlexController( char *szName )
{
	return GetFlexController( LookupFlexController( szName ) );
}

float StudioModel::GetFlexController( LocalFlexController_t iFlex )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);

		float flValue = m_flexweight[iFlex];

		if (pflex->min != pflex->max)
		{
			flValue = flValue * (pflex->max - pflex->min) + pflex->min;
		}
		return flValue;
	}
	return 0.0;
}


float StudioModel::GetFlexControllerRaw( LocalFlexController_t iFlex )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	if (iFlex >= 0 && iFlex < pStudioHdr->numflexcontrollers())
	{
//		mstudioflexcontroller_t *pflex = pStudioHdr->pFlexcontroller(iFlex);
		return m_flexweight[iFlex];
	}
	return 0.0;
}

int StudioModel::GetNumLODs() const
{
	return g_pStudioRender->GetNumLODs( *GetHardwareData() );
}

float StudioModel::GetLODSwitchValue( int lod ) const
{
	return g_pStudioRender->GetLODSwitchValue( *GetHardwareData(), lod );
}

void StudioModel::SetLODSwitchValue( int lod, float switchValue )
{
	g_pStudioRender->SetLODSwitchValue( *GetHardwareData(), lod, switchValue );
}

void StudioModel::ExtractBbox( Vector &mins, Vector &maxs )
{
	studiohdr_t *pStudioHdr = GetStudioRenderHdr();
	if ( !pStudioHdr )
		return;

	// look for hull
	if ( ((Vector)pStudioHdr->hull_min).Length() != 0 )
	{
		mins = pStudioHdr->hull_min;
		maxs = pStudioHdr->hull_max;
	}
	// look for view clip
	else if (((Vector)pStudioHdr->view_bbmin).Length() != 0)
	{
		mins = pStudioHdr->view_bbmin;
		maxs = pStudioHdr->view_bbmax;
	}
	else
	{
		mstudioseqdesc_t &pseqdesc = pStudioHdr->pSeqdesc( m_sequence );

		mins = pseqdesc.bbmin;
		maxs = pseqdesc.bbmax;
	}
}



void StudioModel::GetSequenceInfo( int iSequence, float *pflFrameRate, float *pflGroundSpeed )
{
	float t = GetDuration( iSequence );

	if (t > 0)
	{
		*pflFrameRate = 1.0 / t;
	}
	else
	{
		*pflFrameRate = 1.0;
	}
	*pflGroundSpeed = GetGroundSpeed( iSequence );
}

void StudioModel::GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed )
{
	GetSequenceInfo( m_sequence, pflFrameRate, pflGroundSpeed );
}

float StudioModel::GetFPS( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	return Studio_FPS( pStudioHdr, iSequence, m_poseparameter );
}

float StudioModel::GetFPS( void )
{
	return GetFPS( m_sequence );
}

float StudioModel::GetDuration( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0.0f;

	return Studio_Duration( pStudioHdr, iSequence, m_poseparameter );
}


int StudioModel::GetNumFrames( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr || iSequence < 0 || iSequence >= pStudioHdr->GetNumSeq() )
	{
		return 1;
	}

	return Studio_MaxFrame( pStudioHdr, iSequence, m_poseparameter );
}

static int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence )
{
	if ( !pstudiohdr || 
		sequence < 0 || 
		sequence >= pstudiohdr->GetNumSeq() )
	{
		return 0;
	}

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	return seqdesc.flags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSequence - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool StudioModel::GetSequenceLoops( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	int flags = GetSequenceFlags( pStudioHdr, iSequence );
	bool looping = flags & STUDIO_LOOPING ? true : false;
	return looping;
}

float StudioModel::GetDuration( )
{
	return GetDuration( m_sequence );
}


void StudioModel::GetMovement( float prevcycle[5], Vector &vecPos, QAngle &vecAngles )
{
	vecPos.Init();
	vecAngles.Init();

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;

  	// assume that changes < -0.5 are loops....
  	if (m_cycle - prevcycle[0] < -0.5)
  	{
  		prevcycle[0] = prevcycle[0] - 1.0;
  	}

	Studio_SeqMovement( pStudioHdr, m_sequence, prevcycle[0], m_cycle, m_poseparameter, vecPos, vecAngles );
	prevcycle[0] = m_cycle;

	int i;
	for (i = 0; i < 4; i++)
	{
		Vector vecTmp;
		QAngle angTmp;

  		if (m_Layer[i].m_cycle - prevcycle[i+1] < -0.5)
  		{
  			prevcycle[i+1] = prevcycle[i+1] - 1.0;
  		}

		if (m_Layer[i].m_weight > 0.0)
		{
			vecTmp.Init();
			angTmp.Init();
			if (Studio_SeqMovement( pStudioHdr, m_Layer[i].m_sequence, prevcycle[i+1], m_Layer[i].m_cycle, m_poseparameter, vecTmp, angTmp ))
			{
				vecPos = vecPos * ( 1.0 - m_Layer[i].m_weight ) + vecTmp * m_Layer[i].m_weight;
			}
		}
		prevcycle[i+1] = m_Layer[i].m_cycle;
	}

	return;
}


void StudioModel::GetMovement( int iSequence, float prevCycle, float nextCycle, Vector &vecPos, QAngle &vecAngles )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
	{
		vecPos.Init();
		vecAngles.Init();
		return;
	}

	// FIXME: this doesn't consider layers
	Studio_SeqMovement( pStudioHdr, iSequence, prevCycle, nextCycle, m_poseparameter, vecPos, vecAngles );

	return;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the specifed sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetGroundSpeed( int iSequence )
{
	Vector vecMove;
	QAngle vecAngles;
	GetMovement( iSequence, 0, 1, vecMove, vecAngles );

	float t = GetDuration( iSequence );

	float flGroundSpeed = 0;
	if (t > 0)
	{
		flGroundSpeed = vecMove.Length() / t;
	}

	return flGroundSpeed;
}



//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the current sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetGroundSpeed( void )
{
	return GetGroundSpeed( m_sequence );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the ground speed of the current sequence.
//-----------------------------------------------------------------------------
float StudioModel::GetCurrentVelocity( void )
{
	Vector vecVelocity;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr && Studio_SeqVelocity( pStudioHdr, m_sequence, m_cycle, m_poseparameter, vecVelocity ))
	{
		return vecVelocity.Length();
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the the sequence should be hidden or not
//-----------------------------------------------------------------------------
bool StudioModel::IsHidden( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (pStudioHdr->pSeqdesc( iSequence ).flags & STUDIO_HIDDEN)
		return true;

	return false;
}



void StudioModel::GetSeqAnims( int iSequence, mstudioanimdesc_t *panim[4], float *weight )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, m_poseparameter, panim, weight );
}

void StudioModel::GetSeqAnims( mstudioanimdesc_t *panim[4], float *weight )
{
	GetSeqAnims( m_sequence, panim, weight );
}


float StudioModel::SetController( int iController, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_SetController( pStudioHdr, iController, flValue, m_controller[iController] );
}



int	StudioModel::LookupPoseParameter( char const *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return false;

	for (int iParameter = 0; iParameter < pStudioHdr->GetNumPoseParameters(); iParameter++)
	{
		if (stricmp( szName, pStudioHdr->pPoseParameter( iParameter ).pszName() ) == 0)
		{
			return iParameter;
		}
	}
	return -1;
}

float StudioModel::SetPoseParameter( char const *szName, float flValue )
{
	return SetPoseParameter( LookupPoseParameter( szName ), flValue );
}

float StudioModel::SetPoseParameter( int iParameter, float flValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_SetPoseParameter( pStudioHdr, iParameter, flValue, m_poseparameter[iParameter] );
}

float StudioModel::GetPoseParameter( char const *szName )
{
	return GetPoseParameter( LookupPoseParameter( szName ) );
}

float* StudioModel::GetPoseParameters()
{
	return m_poseparameter;
}

float StudioModel::GetPoseParameter( int iParameter )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0.0f;

	return Studio_GetPoseParameter( pStudioHdr, iParameter, m_poseparameter[iParameter] );
}

bool StudioModel::GetPoseParameterRange( int iParameter, float *pflMin, float *pflMax )
{
	*pflMin = 0;
	*pflMax = 0;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return false;

	if (iParameter < 0 || iParameter >= pStudioHdr->GetNumPoseParameters())
		return false;

	const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( iParameter );

	*pflMin = Pose.start;
	*pflMax = Pose.end;

	return true;
}

int StudioModel::LookupAttachment( char const *szName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	for (int i = 0; i < pStudioHdr->GetNumAttachments(); i++)
	{
		if (stricmp( pStudioHdr->pAttachment( i ).pszName(), szName ) == 0)
		{
			return i;
		}
	}
	return -1;
}



int StudioModel::SetBodygroup( int iGroup, int iValue /*= -1*/ )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0;

	if (iGroup > pStudioHdr->numbodyparts())
		return -1;

	mstudiobodyparts_t *pbodypart = pStudioHdr->pBodypart( iGroup );

	int iCurrent = (m_bodynum / pbodypart->base) % pbodypart->nummodels;

	// if the submodel index is not specified or out of range, just use the current value
	if ( iValue < 0 || iValue >= pbodypart->nummodels )
		return iCurrent;

	m_bodynum = (m_bodynum - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));

	return iValue;
}


int StudioModel::SetSkin( int iValue )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return 0;

	if (iValue >= pStudioHdr->numskinfamilies())
	{
		return m_skinnum;
	}

	m_skinnum = iValue;

	return iValue;
}



void StudioModel::scaleMeshes (float scale)
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	int i, j, k;

	// manadatory to access correct verts
	SetCurrentModel();

	// scale verts
	int tmp = m_bodynum;
	for (i = 0; i < pStudioHdr->numbodyparts(); i++)
	{
		mstudiobodyparts_t *pbodypart = pStudioHdr->pBodypart( i );
		for (j = 0; j < pbodypart->nummodels; j++)
		{
			SetBodygroup (i, j);
			SetupModel (i);

			const mstudio_modelvertexdata_t *vertData = m_pmodel->GetVertexData();
			Assert( vertData ); // This can only return NULL on X360 for now

			for (k = 0; k < m_pmodel->numvertices; k++)
			{
				*vertData->Position(k) *= scale;
			}
		}
	}

	m_bodynum = tmp;

	// scale complex hitboxes
	int hitboxset = g_MDLViewer->GetCurrentHitboxSet();

	mstudiobbox_t *pbboxes = pStudioHdr->pHitbox( 0, hitboxset );
	for (i = 0; i < pStudioHdr->iHitboxCount( hitboxset ); i++)
	{
		VectorScale (pbboxes[i].bbmin, scale, pbboxes[i].bbmin);
		VectorScale (pbboxes[i].bbmax, scale, pbboxes[i].bbmax);
	}

	// scale bounding boxes
	for (i = 0; i < pStudioHdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( i );
		Vector tmp;

		tmp = seqdesc.bbmin;
		VectorScale( tmp, scale, tmp );
		seqdesc.bbmin = tmp;

		tmp = seqdesc.bbmax;
		VectorScale( tmp, scale, tmp );
		seqdesc.bbmax = tmp;

	}

	// maybe scale exeposition, pivots, attachments
}



void StudioModel::scaleBones (float scale)
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!pStudioHdr)
		return;

	mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	for (int i = 0; i < pStudioHdr->numbones(); i++)
	{
		pbones[i].pos *= scale;
		pbones[i].posscale *= scale;
	}	
}

int	StudioModel::Physics_GetBoneCount( void )
{
	return m_pPhysics->Count();
}


const char *StudioModel::Physics_GetBoneName( int index ) 
{ 
	CPhysmesh *pmesh = m_pPhysics->GetMesh( index );

	if ( !pmesh )
		return NULL;

	return pmesh->m_boneName;
}


void StudioModel::Physics_GetData( int boneIndex, hlmvsolid_t *psolid, constraint_ragdollparams_t *pConstraint ) const
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( psolid, &pMesh->m_solid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		*pConstraint = pMesh->m_constraint;
	}
}

void StudioModel::Physics_SetData( int boneIndex, const hlmvsolid_t *psolid, const constraint_ragdollparams_t *pConstraint )
{
	CPhysmesh *pMesh = m_pPhysics->GetMesh( boneIndex );
	
	if ( !pMesh )
		return;

	if ( psolid )
	{
		memcpy( &pMesh->m_solid, psolid, sizeof(*psolid) );
	}

	if ( pConstraint )
	{
		pMesh->m_constraint = *pConstraint;
	}
}


float StudioModel::Physics_GetMass( void )
{
	return m_pPhysics->GetMass();
}

void StudioModel::Physics_SetMass( float mass )
{
	m_physMass = mass;
}


char *StudioModel::Physics_DumpQC( void )
{
	return m_pPhysics->DumpQC();
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void * pModelData )
{
	Assert( pModelData == NULL );
	Assert( g_pActiveModel );

	return g_pStudioDataCache->CacheVertexData( g_pActiveModel->GetStudioRenderHdr() );
}


//-----------------------------------------------------------------------------
// FIXME: This trashy glue code is really not acceptable. Figure out a way of making it unnecessary.
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = (void*)handle;
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return g_pMDLCache->GetVirtualModel( (MDLHandle_t)virtualModel );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_pMDLCache->GetAnimBlock( (MDLHandle_t)virtualModel, i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( (MDLHandle_t)virtualModel, pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( (MDLHandle_t)cache );
}
