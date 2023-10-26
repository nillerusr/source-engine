//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "particlemgr.h"
#include "particles_new.h"
#include "iclientmode.h"
#include "engine/ivdebugoverlay.h"
#include "particle_property.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "model_types.h"
#include "vprof.h"
#include "datacache/iresourceaccesscontrol.h"
#include "tier2/tier2.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


extern ConVar cl_particleeffect_aabb_buffer;
extern ConVar cl_particles_show_bbox;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, CParticleSystemDefinition *pEffect )
{
	m_hOwner = pOwner;
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pEffect->GetName() ) )
		{
			pEffect = g_pParticleSystemMgr->FindParticleSystem( "error" );
		}
	}
	Init( pEffect );
	Construct();
}

CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, const char* pEffectName )
{
	m_hOwner = pOwner;
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pEffectName ) )
		{
			pEffectName = "error";
		}
	}
	Init( pEffectName );
	Construct();
}

CNewParticleEffect::CNewParticleEffect( CBaseEntity *pOwner, int nPrecacheIndex )
{
	m_hOwner = pOwner;
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_PARTICLE_SYSTEM, pDef->GetName() ) )
		{
			// This is the error effect
			pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
		}
	}

	Init( pDef );
	Construct();
}

static ConVar cl_aggregate_particles( "cl_aggregate_particles", "1" );

CNewParticleEffect *CNewParticleEffect::CreateOrAggregate( CBaseEntity *pOwner, CParticleSystemDefinition *pDef,
																	 Vector const &vecAggregatePosition, const char *pDebugName,
																	 int nSplitScreenSlot )
{
	CNewParticleEffect *pAggregateTarget = NULL;
	// see if we should aggregate
	bool bCanAggregate = ( pOwner == NULL ) && ( pDef->m_flAggregateRadius > 0.0 ) && ( cl_aggregate_particles.GetInt() != 0 );
	if ( bCanAggregate )
	{
		CParticleSystemDefinition *pDefFallback = pDef;
		do
		{
			float flAggregateDistSqr =  ( pDefFallback->m_flAggregateRadius * pDefFallback->m_flAggregateRadius ) + 0.1;
			for( CParticleCollection *pSystem = pDefFallback->FirstCollection(); pSystem; pSystem = pSystem->GetNextCollectionUsingSameDef() )
			{
				CNewParticleEffect *pEffectCheck = static_cast<CNewParticleEffect *>( pSystem );
				if ( ! pEffectCheck->m_bDisableAggregation )
				{
					float flDistSQ = vecAggregatePosition.DistToSqr( pEffectCheck->m_vecAggregationCenter );
					if ( ( flDistSQ < flAggregateDistSqr ) && 
						 ( pSystem->m_nMaxAllowedParticles - pSystem->m_nActiveParticles > pDefFallback->m_nAggregationMinAvailableParticles ) &&
						 ( pEffectCheck->m_nSplitScreenUser == nSplitScreenSlot ) )
					{
						flAggregateDistSqr = flDistSQ;
						pAggregateTarget = pEffectCheck;
					}
				}
			}
			pDefFallback = pDefFallback->GetFallbackReplacementDefinition();
		} while ( pDefFallback );
	}
	if ( ! pAggregateTarget )
	{
		// we need a new one
		pAggregateTarget = new CNewParticleEffect( pOwner, pDef );
		pAggregateTarget->SetDrawOnlyForSplitScreenUser( nSplitScreenSlot );
		pAggregateTarget->SetDynamicallyAllocated( true );
	}
	else
	{
		// just reset the old one
		pAggregateTarget->Restart( RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN );
	}
	if ( bCanAggregate )
	{
		pAggregateTarget->m_vecAggregationCenter = vecAggregatePosition;
	}
	pAggregateTarget->m_pDebugName = pDebugName;
	pAggregateTarget->m_bDisableAggregation = false;
	return pAggregateTarget;

}

void CNewParticleEffect::RemoveParticleEffect( int nPrecacheIndex )
{
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );

	if ( pDef == NULL )
		return;

	for( CParticleCollection *pSystem = pDef->FirstCollection(); pSystem; pSystem = pSystem->GetNextCollectionUsingSameDef() )
	{
		CNewParticleEffect *pEffectCheck = static_cast<CNewParticleEffect *>( pSystem );

		if ( pEffectCheck )
		{
			pEffectCheck->SetRemoveFlag();
		}
	}
}

CNewParticleEffect *CNewParticleEffect::CreateOrAggregate( 
	CBaseEntity *pOwner, const char *pParticleSystemName,
	Vector const &vecAggregatePosition, 
	const char *pDebugName,
	int nSplitScreenUser )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", pParticleSystemName );
		pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
	}
	return CreateOrAggregate( pOwner, pDef, vecAggregatePosition, pDebugName, nSplitScreenUser );
}

CNewParticleEffect *CNewParticleEffect::CreateOrAggregatePrecached( 
	CBaseEntity *pOwner, int nPrecacheIndex,
	Vector const &vecAggregatePosition, const char *pDebugName,
	int nSplitScreenUser )
{
	CParticleSystemDefinition* pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", GetParticleSystemNameFromIndex( nPrecacheIndex ) );
		pDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
	}
	return CreateOrAggregate( pOwner, pDef, vecAggregatePosition, pDebugName, nSplitScreenUser );
}


void CNewParticleEffect::Construct()
{
	m_vSortOrigin.Init();

	m_bDontRemove = false;
	m_bRemove = false;
	m_bDrawn = false;
	m_bNeedsBBoxUpdate = false;
	m_bIsFirstFrame = true;
	m_bAutoUpdateBBox = false;
	m_bAllocated = true;
	m_bSimulate = true;
	m_bRecord = false;
	m_bShouldPerformCullCheck = false;
	m_bDisableAggregation = true;							// will be reset when someone creates it via CreateOrAggregate


	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
	m_RefCount = 0;
	ParticleMgr()->AddEffect( this );
	m_LastMax = Vector( -1.0e6, -1.0e6, -1.0e6 );
	m_LastMin = Vector( 1.0e6, 1.0e6, 1.0e6 );
	m_MinBounds = Vector( 1.0e6, 1.0e6, 1.0e6 );
	m_MaxBounds = Vector( -1.0e6, -1.0e6, -1.0e6 );
	m_pDebugName = NULL;
	m_nSplitScreenUser = -1;

	SetRenderable( this );

	RecordCreation();
}

CNewParticleEffect::~CNewParticleEffect(void)
{
	if ( m_bRecord && m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID && clienttools->IsInRecordingMode() )
	{
		static ParticleSystemDestroyedState_t state;
		state.m_nParticleSystemId = GetToolParticleEffectId();
		state.m_flTime = gpGlobals->curtime;

		KeyValues *msg = new KeyValues( "ParticleSystem_Destroy" );
		msg->SetPtr( "state", &state );

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID; 
	}

	m_bAllocated = false;
	if ( m_hOwner )
	{
		// NOTE: This can provoke another NotifyRemove call which is why flags is set to 0
		m_hOwner->ParticleProp()->OnParticleSystemDeleted( this );
	}
}


//-----------------------------------------------------------------------------
// Refcounting
//-----------------------------------------------------------------------------
void CNewParticleEffect::AddRef()
{
	++m_RefCount;
}

void CNewParticleEffect::Release()
{
	Assert( m_RefCount > 0 );
	--m_RefCount;

	// If all the particles are already gone, delete ourselves now.
	// If there are still particles, wait for the last NotifyDestroyParticle.
	if ( m_RefCount == 0 )
	{
		if ( m_bAllocated )
		{
			if ( IsFinished() )
			{
				SetRemoveFlag();
			}
		}
	}
}

void CNewParticleEffect::NotifyRemove()
{
	if ( m_bAllocated )
	{
		delete this;
	}
}

int CNewParticleEffect::IsReleased()
{
	return m_RefCount == 0;
}


//-----------------------------------------------------------------------------
// Refraction and soft particle support
//-----------------------------------------------------------------------------
int CNewParticleEffect::GetRenderFlags( void )
{
	int nRet = 0;
    // NOTE: Need to do this because CParticleCollection's version is non-virtual
	if ( CParticleCollection::UsesPowerOfTwoFrameBufferTexture( true ) )
		nRet |= ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB | ERENDERFLAGS_REFRACT_ONLY_ONCE_PER_FRAME;
	if ( CParticleCollection::UsesFullFrameBufferTexture( true ) )
		nRet |= ERENDERFLAGS_NEEDS_FULL_FB;
	return nRet;
}


//-----------------------------------------------------------------------------
// Overrides for recording
//-----------------------------------------------------------------------------
void CNewParticleEffect::StopEmission( bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop, bool bPlayEndCap )
{
	if ( m_bRecord && m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID && clienttools->IsInRecordingMode() )
	{
		KeyValues *msg = new KeyValues( "ParticleSystem_StopEmission" );

		static ParticleSystemStopEmissionState_t state;
		state.m_nParticleSystemId = GetToolParticleEffectId();
		state.m_flTime = gpGlobals->curtime;
		state.m_bInfiniteOnly = bInfiniteOnly;

		msg->SetPtr( "state", &state );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	}

	CParticleCollection::StopEmission( bInfiniteOnly, bRemoveAllParticles, bWakeOnStop, bPlayEndCap );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewParticleEffect::SetDormant( bool bDormant )
{
	CParticleCollection::SetDormant( bDormant );
}

void CNewParticleEffect::SetControlPointEntity( int nWhichPoint, CBaseEntity *pEntity )
{
	if ( m_bRecord && m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID && clienttools->IsInRecordingMode() )
	{
		static ParticleSystemSetControlPointObjectState_t state;
		state.m_nParticleSystemId = GetToolParticleEffectId();
		state.m_flTime = gpGlobals->curtime;
		state.m_nControlPoint = nWhichPoint;
		state.m_nObject = pEntity ? pEntity->entindex() : -1;

		KeyValues *msg = new KeyValues( "ParticleSystem_SetControlPointObject" );
		msg->SetPtr( "state", &state );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	}

	if ( pEntity )
	{
		CParticleCollection::SetControlPointObject( nWhichPoint, &m_hControlPointOwners[ nWhichPoint ] );
		m_hControlPointOwners[ nWhichPoint ] = pEntity;
	}
	else
		CParticleCollection::SetControlPointObject( nWhichPoint, NULL );
}


void CNewParticleEffect::SetControlPoint( int nWhichPoint, const Vector &v )
{
	if ( m_bRecord && m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID && clienttools->IsInRecordingMode() )
	{
		static ParticleSystemSetControlPointPositionState_t state;
		state.m_nParticleSystemId = GetToolParticleEffectId();
		state.m_flTime = gpGlobals->curtime;
		state.m_nControlPoint = nWhichPoint;
		state.m_vecPosition = v;

		KeyValues *msg = new KeyValues( "ParticleSystem_SetControlPointPosition" );
		msg->SetPtr( "state", &state );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	}

	CParticleCollection::SetControlPoint( nWhichPoint, v );
}

void CNewParticleEffect::SetToolRecording( bool bRecord )
{
	if ( bRecord == m_bRecord )
		return;

	m_bRecord = bRecord;

	if ( m_bRecord )
	{
		RecordCreation();
	}
}

void CNewParticleEffect::RecordCreation()
{
	if ( IsValid() && clienttools->IsInRecordingMode() )
	{
		m_bRecord = true;

		int nId = AllocateToolParticleEffectId();	

		static ParticleSystemCreatedState_t state;
		state.m_nParticleSystemId = nId;
		state.m_flTime = gpGlobals->curtime;
		state.m_pName = m_pDef->GetName();
		state.m_nOwner = m_hOwner ? m_hOwner->entindex() : -1;

		KeyValues *msg = new KeyValues( "ParticleSystem_Create" );
		msg->SetPtr( "state", &state );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	}
}

void CNewParticleEffect::RecordControlPointOrientation( int nWhichPoint )
{
	if ( m_bRecord && m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID && clienttools->IsInRecordingMode() )
	{
		// FIXME: Make a more direct way of getting 
		QAngle angles;
		VectorAngles( ControlPoint( nWhichPoint ).m_ForwardVector, ControlPoint( nWhichPoint ).m_UpVector, angles );

		static ParticleSystemSetControlPointOrientationState_t state;
		state.m_nParticleSystemId = GetToolParticleEffectId();
		state.m_flTime = gpGlobals->curtime;
		state.m_nControlPoint = nWhichPoint;
		AngleQuaternion( angles, state.m_qOrientation );

		KeyValues *msg = new KeyValues( "ParticleSystem_SetControlPointOrientation" );
		msg->SetPtr( "state", &state );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	}
}

void CNewParticleEffect::SetControlPointOrientation( int nWhichPoint, 
	const Vector &forward, const Vector &right, const Vector &up )
{
	CParticleCollection::SetControlPointOrientation( nWhichPoint, forward, right, up );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointOrientation( int nWhichPoint, const Quaternion &q )
{
	CParticleCollection::SetControlPointOrientation( nWhichPoint, q );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointForwardVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointForwardVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointUpVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointUpVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}

void CNewParticleEffect::SetControlPointRightVector( int nWhichPoint, const Vector &v )
{
	CParticleCollection::SetControlPointRightVector( nWhichPoint, v );
	RecordControlPointOrientation( nWhichPoint );
}


//-----------------------------------------------------------------------------
// Called when the particle effect is about to update
//-----------------------------------------------------------------------------
void CNewParticleEffect::Update( float flTimeDelta )
{
	if ( m_hOwner )
	{
		m_hOwner->ParticleProp()->OnParticleSystemUpdated( this, flTimeDelta );
	}
}


//-----------------------------------------------------------------------------
// Bounding box
//-----------------------------------------------------------------------------
CNewParticleEffect* CNewParticleEffect::ReplaceWith( const char *pParticleSystemName )
{
	StopEmission( false, true, true );
	if ( !pParticleSystemName || !pParticleSystemName[0] )
		return NULL;

	CNewParticleEffect *pNewEffect = CNewParticleEffect::Create( GetOwner(), pParticleSystemName, pParticleSystemName );
	if ( !pNewEffect )
		return NULL;

	// Copy over the control point data
	for ( int i = 0; i < MAX_PARTICLE_CONTROL_POINTS; ++i )
	{
		if ( !ReadsControlPoint( i ) )
			continue;

		Vector vecForward, vecRight, vecUp;
		pNewEffect->SetControlPoint( i, GetControlPointAtCurrentTime( i ) );
		GetControlPointOrientationAtCurrentTime( i, &vecForward, &vecRight, &vecUp );
		pNewEffect->SetControlPointOrientation( i, vecForward, vecRight, vecUp );
		pNewEffect->SetControlPointParent( i, GetControlPointParent( i ) );
	}

	if ( m_hOwner )
	{
		m_hOwner->ParticleProp()->ReplaceParticleEffect( this, pNewEffect );
	}

	// fixup any other references to the old system, to point to the new system
	while( m_References.m_pHead )
	{
		// this will remove the reference from m_References
		m_References.m_pHead->Set( pNewEffect );
	}

	// At this point any references should have been redirected,
	// but we may still be running with some stray particles, so we
	// might not be flagged for removal - force the issue!
	Assert( m_RefCount == 0 );
	SetRemoveFlag();

	return pNewEffect;
}


//-----------------------------------------------------------------------------
// Bounding box
//-----------------------------------------------------------------------------
void CNewParticleEffect::SetParticleCullRadius( float radius )
{
}

bool CNewParticleEffect::RecalculateBoundingBox()
{
	BloatBoundsUsingControlPoint();
	if ( !m_bBoundsValid )
	{
		m_MaxBounds = m_MinBounds = GetRenderOrigin();
		return false;
	}

	return true;
}


void CNewParticleEffect::GetRenderBounds( Vector& mins, Vector& maxs )
{
	if ( !m_bBoundsValid )
	{
		mins = vec3_origin;
		maxs = mins;
		return;
	}
	VectorSubtract( m_MinBounds, GetRenderOrigin(), mins );
	VectorSubtract( m_MaxBounds, GetRenderOrigin(), maxs );
}

void CNewParticleEffect::DetectChanges()
{
	// if we have no render handle, return
	if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		return;

	// Turn off rendering if the bounds aren't valid
	g_pClientLeafSystem->EnableRendering( m_hRenderHandle, m_bBoundsValid );

	if ( !m_bBoundsValid )
	{
		m_LastMin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		m_LastMin.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		return;
	}

	if ( m_MinBounds != m_LastMin || m_MaxBounds != m_LastMax ) 
	{
		// call leafsystem to update this guy
		ClientLeafSystem()->RenderableChanged( m_hRenderHandle );

		// remember last parameters
		m_LastMin = m_MinBounds;
		m_LastMax = m_MaxBounds;
	}
}

extern ConVar r_DrawParticles;


//-----------------------------------------------------------------------------
// Rendering
//-----------------------------------------------------------------------------
int CNewParticleEffect::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "CNewParticleEffect::DrawModel", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	if ( r_DrawParticles.GetBool() == false )
		return 0;

	if ( !GetClientMode()->ShouldDrawParticles() || !ParticleMgr()->ShouldRenderParticleSystems() )
		return 0;

	if( flags & STUDIO_SHADOWDEPTHTEXTURE )
		return 0;

	if ( m_hOwner && m_hOwner->IsDormant() )
		return 0;
	
	// do distance cull check here. We do it here instead of in particles so we can easily only do
	// it for root objects, not bothering to cull children individually
	CMatRenderContextPtr pRenderContext( materials );
	if ( !m_pDef->IsScreenSpaceEffect() && !m_pDef->IsViewModelEffect() )
	{
		Vector vecCamera;
		pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
		if ( ( CalcSqrDistanceToAABB( m_MinBounds, m_MaxBounds, vecCamera ) > ( m_pDef->m_flMaxDrawDistance * m_pDef->m_flMaxDrawDistance ) ) )
			return 0;
	}

	if ( ( flags & STUDIO_TRANSPARENCY ) || !IsBatchable() || !m_pDef->IsDrawnThroughLeafSystem() )
	{
		int viewentity = render->GetViewEntity();
		C_BaseEntity *pCameraObject = cl_entitylist->GetEnt( viewentity );
		// apply logic that lets you skip rendering a system if the camera is attached to its entity
		if   ( ( ( pCameraObject &&
			 ( m_pDef->m_nSkipRenderControlPoint != -1 ) &&
			 ( m_pDef->m_nSkipRenderControlPoint <= m_nHighestCP ) &&
			 ( GetControlPointEntity( m_pDef->m_nSkipRenderControlPoint ) == pCameraObject ) ) ) ||
			 ( ( pCameraObject &&
			 ( m_pDef->m_nAllowRenderControlPoint != -1 ) &&
			 ( m_pDef->m_nAllowRenderControlPoint <= m_nHighestCP ) &&
			 ( GetControlPointEntity( m_pDef->m_nAllowRenderControlPoint ) != pCameraObject ) ) ) )
			return 0;

		Vector4D vecDiffuseModulation( 1.0f, 1.0f, 1.0f, 1.0f ); //instance.m_nAlpha / 255.0f );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		Render( pRenderContext, vecDiffuseModulation, ( flags & STUDIO_TRANSPARENCY ) ? IsTwoPass() : false, pCameraObject );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
	}
	else
	{
		g_pParticleSystemMgr->AddToRenderCache( this );
	}

	if ( cl_particles_show_bbox.GetBool() )
	{
		Vector center = GetRenderOrigin();
		Vector mins   = m_MinBounds - center;
		Vector maxs   = m_MaxBounds - center;
	
		int r, g;
		if ( GetAutoUpdateBBox() )
		{
			// red is bad, the bbox update is costly
			r = 255;
			g = 0;
		}
		else
		{
			// green, this effect presents less cpu load 
			r = 0;
			g = 255;
		}
		Vector vecCurCPPos = GetControlPointAtCurrentTime( 0 );
		debugoverlay->AddBoxOverlay( center, mins, maxs, QAngle( 0, 0, 0 ), r, g, 0, 16, 0 );
		debugoverlay->AddTextOverlayRGB( center, 0, 0, r, g, 0, 64, "%s:(%d)", GetEffectName(),
										 m_nActiveParticles );
	}

	return 1;
}

void CNewParticleEffect::SetDrawOnlyForSplitScreenUser( int nSlot )
{
	if ( nSlot != m_nSplitScreenUser )
	{
		m_nSplitScreenUser = nSlot;
		if ( IsSplitScreenSupported() && ( m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE ) )
		{
			g_pClientLeafSystem->EnableSplitscreenRendering( m_hRenderHandle, ComputeSplitscreenRenderingFlags( this ) );
		}
	}
}

bool CNewParticleEffect::ShouldDrawForSplitScreenUser( int nSlot )
{
	if ( m_nSplitScreenUser == -1 )
		return true;
	return m_nSplitScreenUser == nSlot;
}



bool CNewParticleEffect::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	matrix3x4a_t mat;
	mat.Init( Vector( 0, -1, 0), Vector( 1, 0, 0), Vector( 0, 0, 1 ), vec3_origin );
	MatrixMultiply( m_DrawModelMatrix, mat, pBoneToWorldOut[0] );
	return true;
}


static void DumpParticleStats_f( void )
{
	g_pParticleSystemMgr->DumpProfileInformation();
}

static ConCommand cl_dump_particle_stats( "cl_dump_particle_stats", DumpParticleStats_f, "dump particle profiling info to particle_profile.csv") ;

CON_COMMAND( cl_particles_dumplist, "Dump all new particles, optional name substring." )
{
	if ( args.ArgC() == 2 )
	{
		g_pParticleSystemMgr->DumpParticleList( args.Arg(1) );
	}
	else
	{
		g_pParticleSystemMgr->DumpParticleList( NULL );
	}
}
