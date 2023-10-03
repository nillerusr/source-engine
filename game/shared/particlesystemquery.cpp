//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system definitions
//
//===========================================================================//

#include "cbase.h"
#include "particles/particles.h"
#include "baseparticleentity.h"
#include "entityparticletrail_shared.h"
#include "collisionutils.h"
#include "engine/ivdebugoverlay.h"
#include "raytrace.h"
#include "animation.h"

#if defined( CLIENT_DLL )
#include "c_pixel_visibility.h"
#include "c_effects.h"
#include "view.h"
#include "viewrender.h"
#include "model_types.h"
#include "c_env_projectedtexture.h"
#endif



#ifdef SWARM_DLL
#include "asw_shareddefs.h"
#endif

#ifdef GAME_DLL
#include "ai_utils.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#if defined( CLIENT_DLL )

typedef struct SProjectedTextureInfo
{
	int						m_nParticleID;
	IMaterial				*m_pMaterial;
	Vector					m_vOrigin;
	float					m_flSize;
	float					m_flRotation;
	float					m_r, m_g, m_b, m_a;
	C_EnvProjectedTexture	*m_pEntity;
	bool					m_bUsedThisFrame;
} TProjectedTextureInfo;

#endif // #if defined( CLIENT_DLL )



#define POINT_AT_ORIGIN_EPSILON 0.1f
//-----------------------------------------------------------------------------
// Interface to allow the particle system to call back into the game code
//-----------------------------------------------------------------------------
class CParticleSystemQuery : public CBaseAppSystem< IParticleSystemQuery >
{
public:
	virtual bool IsEditor( ) { return false; }

	// Inherited from IParticleSystemQuery
	virtual void GetLightingAtPoint( const Vector& vecOrigin, Color &cTint );
	virtual void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const IHandleEntity *ignore,
							int collisionGroup, CBaseTrace *ptr );

	virtual bool MovePointInsideControllingObject( CParticleCollection *pParticles,
												   void *pObject,
												   Vector *pPnt );
	virtual void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionalBias,
		Vector *pHitBoxRelativeCoordOut,
		int *pHitBoxIndexOut,
		int nDesiredHitbox
		);

	virtual int GetRayTraceEnvironmentFromName( const char *pszRtEnvName );

	virtual int GetCollisionGroupFromName( const char *pszCollisionGroupName );


	virtual int GetControllingObjectHitBoxInfo(
		CParticleCollection *pParticles,
		int nControlPointNumber,
		int nBufSize,										// # of output slots available
		ModelHitBoxInfo_t *pHitBoxOutputBuffer );

	virtual	bool IsPointInControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecPos, bool bBBoxOnly );

	virtual	void CParticleSystemQuery::GetControllingObjectOBBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecMin, Vector vecMax );
	
	// Traces Four Rays against a defined RayTraceEnvironment
	virtual void TraceAgainstRayTraceEnv( int envnumber, const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const ;

	virtual Vector GetLocalPlayerPos( void );
	virtual void GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight = NULL, Vector *pUp = NULL );

	virtual Vector GetCurrentViewOrigin();

	virtual int GetActivityCount();
	virtual const char *GetActivityNameFromIndex( int nActivityIndex );

	virtual float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale );
	virtual void SetUpLightingEnvironment( const Vector& pos );

	virtual void PreSimulate( );

	virtual void PostSimulate( );

	virtual void DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration );

	virtual void BeginDrawModels( int nMaxNumToDraw, Vector const &vecCenterPosition, CParticleCollection *pParticles )
	{
	}

	virtual void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
							int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f );


	virtual void FinishDrawModels( CParticleCollection *pParticles )
	{
	}

	virtual void *GetModel( char const *pMdlName );

	virtual void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar );

private:
#if defined( CLIENT_DLL )

	CTSQueue< TProjectedTextureInfo * > m_ProjectedInfoAdds;
	CUtlVector< TProjectedTextureInfo * > m_ActiveProjectedInfos;

#endif // #if defined( CLIENT_DLL )
};


static CParticleSystemQuery s_ParticleSystemQuery;
IParticleSystemQuery *g_pParticleSystemQuery = &s_ParticleSystemQuery;


//-----------------------------------------------------------------------------
// Exposes the interface (so tools can get at it)
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CParticleSystemQuery, IParticleSystemQuery, PARTICLE_SYSTEM_QUERY_INTERFACE_VERSION, s_ParticleSystemQuery );
#endif

static CThreadFastMutex s_LightMutex;
static CThreadFastMutex s_BoneMutex;

//-----------------------------------------------------------------------------
// Inherited from IParticleSystemQuery
//-----------------------------------------------------------------------------
void CParticleSystemQuery::GetLightingAtPoint( const Vector& vecOrigin, Color &cTint )
{
	VPROF("CParticleSystemQuery::GetLightingAtPoint");
#ifdef GAME_DLL

	// FIXME: Go through to the engine from the server to get these values
	cTint.SetColor( 255, 255, 255, 255 );

#else

	if ( engine->IsInGame() )
	{
		s_LightMutex.Lock();
		// Compute our lighting at our position
		Vector totalColor = engine->GetLightForPoint( vecOrigin, true );
		s_LightMutex.Unlock();

		// Get our lighting information
		cTint.SetColor( totalColor.x*255, totalColor.y*255, totalColor.z*255, 0 );
	}
	else
	{
		// FIXME: Go through to the engine from the server to get these values
		cTint.SetColor( 255, 255, 255, 255 );
 	}

#endif
}

void CParticleSystemQuery::SetUpLightingEnvironment( const Vector& pos )
{
#ifndef GAME_DLL
	if ( !engine->IsInGame() )
		return;

	s_LightMutex.Lock();
	modelrender->SetupLighting( pos );
	s_LightMutex.Unlock();
#endif
}

void CParticleSystemQuery::TraceLine( const Vector& vecAbsStart,
									  const Vector& vecAbsEnd, unsigned int mask, 
									  const IHandleEntity *ignore,
									  int collisionGroup, CBaseTrace *ptr )
{
	bool bDoTrace = false;
#ifndef GAME_DLL
	bDoTrace = engine->IsInGame();
#endif
	if ( bDoTrace )
	{
		trace_t tempTrace;
		UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, ignore, collisionGroup, &tempTrace );
		memcpy( ptr, &tempTrace, sizeof ( CBaseTrace ) );
	}
	else
	{
		ptr->startsolid = 0;
		ptr->fraction = 1.0;
	}

}

bool CParticleSystemQuery::MovePointInsideControllingObject( 
	CParticleCollection *pParticles, void *pObject, Vector *pPnt )
{
#ifdef GAME_DLL
	return true;
#else
	if (! pObject )
		return true;										// accept the input point unmodified

	Ray_t ray;
	trace_t tr;
	ray.Init( *pPnt, *pPnt );
	enginetrace->ClipRayToEntity( ray, MASK_ALL, (CBaseEntity *) pObject, &tr );
	
	return ( tr.startsolid );
#endif
}

static float GetSurfaceCoord( float flRand, float flMinX, float flMaxX )
{
	return Lerp( flRand, flMinX, flMaxX );

}


void CParticleSystemQuery::GetRandomPointsOnControllingObjectHitBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, 
	int nNumPtsOut,
	float flBBoxScale,
	int nNumTrysToGetAPointInsideTheModel,
	Vector *pPntsOut,
	Vector vecDirectionalBias,
	Vector *pHitBoxRelativeCoordOut,
	int *pHitBoxIndexOut,
	int nDesiredHitbox
	)
{

	bool bSucesss = false;


#ifndef GAME_DLL

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = NULL;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		float flRandMax = flBBoxScale;
		float flRandMin = 1.0 - flBBoxScale;
		Vector vecBasePos;
		pParticles->GetControlPointAtTime( nControlPointNumber, pParticles->m_flCurTime, &vecBasePos );

		s_BoneMutex.Lock();
		C_BaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
		if ( pAnimating )
		{
			
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{
		
				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
				
				if ( pStudioHdr )
				{
					// Try to get the "effects" set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), "effects" );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
					
					if ( set )
					{
						bSucesss = true;
						
						Vector vecWorldPosition;
						float u = 0, v = 0, w = 0;
						int nHitbox = 0;
						int nNumIters = nNumTrysToGetAPointInsideTheModel;
						if (! vecDirectionalBias.IsZero( 0.0001 ) )
							nNumIters = MAX( nNumIters, 5 );
						int nHitboxMin = 0;
						int nHitboxMax = set->numhitboxes - 1;
						if ( nDesiredHitbox >= 0 )
						{
							nHitboxMin = MIN( set->numhitboxes - 1, nDesiredHitbox );
							nHitboxMax = MIN( set->numhitboxes - 1, nDesiredHitbox );
						}
				
						for( int i=0 ; i < nNumPtsOut; i++)
						{
							int nTryCnt = nNumIters;
							float flBestPointGoodness = -1.0e20;
							do
							{
								int nTryHitbox = pParticles->RandomInt( nHitboxMin, nHitboxMax );
								mstudiobbox_t *pBox = set->pHitbox(nTryHitbox);
								
								// E3 HACK - check for hitboxes at the origin and ignore those
								if ( fabs( (*hitboxbones[pBox->bone])[0][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[1][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[2][3] ) < POINT_AT_ORIGIN_EPSILON )
								{
									continue;
								}

								float flTryU = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryV = pParticles->RandomFloat( flRandMin, flRandMax );
								float flTryW = pParticles->RandomFloat( flRandMin, flRandMax );

								Vector vecLocalPosition;
								vecLocalPosition.x = GetSurfaceCoord( flTryU, pBox->bbmin.x*pAnimating->GetModelScale(), pBox->bbmax.x*pAnimating->GetModelScale() );
								vecLocalPosition.y = GetSurfaceCoord( flTryV, pBox->bbmin.y*pAnimating->GetModelScale(), pBox->bbmax.y*pAnimating->GetModelScale() );
								vecLocalPosition.z = GetSurfaceCoord( flTryW, pBox->bbmin.z*pAnimating->GetModelScale(), pBox->bbmax.z*pAnimating->GetModelScale() );

								Vector vecTryWorldPosition;

								VectorTransform( vecLocalPosition, *hitboxbones[pBox->bone], vecTryWorldPosition );
								
								
								float flPointGoodness = pParticles->RandomFloat( 0, 72 )
									+ DotProduct( vecTryWorldPosition - vecBasePos, 
												  vecDirectionalBias );

								if ( nNumTrysToGetAPointInsideTheModel )
								{
									// do a point in solid test
									Ray_t ray;
									trace_t tr;
									ray.Init( vecTryWorldPosition, vecTryWorldPosition );
									enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
									if ( tr.startsolid )
										flPointGoodness += 1000.; // got a point inside!
								}
								if ( flPointGoodness > flBestPointGoodness )
								{
									u = flTryU;
									v = flTryV;
									w = flTryW;
									vecWorldPosition = vecTryWorldPosition;
									nHitbox = nTryHitbox;
									flBestPointGoodness = flPointGoodness;
								}
							} while ( nTryCnt-- );
							*( pPntsOut++ ) = vecWorldPosition;
							if ( pHitBoxRelativeCoordOut )
								( pHitBoxRelativeCoordOut++ )->Init( u, v, w );
							if ( pHitBoxIndexOut )
								*( pHitBoxIndexOut++ ) = nHitbox;
						}
					}
				}
			}
		}

		if ( pMoveParent->IsBrushModel() )
		{
			Vector vecMin;
			Vector vecMax;
			matrix3x4_t matOrientation;
			Vector VecOrigin;
			pMoveParent->GetRenderBounds( vecMin, vecMax  );
			VecOrigin = pMoveParent->GetRenderOrigin();
			matOrientation = pMoveParent->EntityToWorldTransform();

			

			Vector vecWorldPosition;
			float u = 0, v = 0, w = 0;
			int nHitbox = 0;
			int nNumIters = nNumTrysToGetAPointInsideTheModel;
			if (! vecDirectionalBias.IsZero( 0.0001 ) )
				nNumIters = MAX( nNumIters, 5 );

			for( int i=0 ; i < nNumPtsOut; i++)
			{
				int nTryCnt = nNumIters;
				float flBestPointGoodness = -1.0e20;
				do
				{
					float flTryU = pParticles->RandomFloat( flRandMin, flRandMax );
					float flTryV = pParticles->RandomFloat( flRandMin, flRandMax );
					float flTryW = pParticles->RandomFloat( flRandMin, flRandMax );

					Vector vecLocalPosition;
					vecLocalPosition.x = GetSurfaceCoord( flTryU, vecMin.x, vecMax.x );
					vecLocalPosition.y = GetSurfaceCoord( flTryV, vecMin.y, vecMax.y );
					vecLocalPosition.z = GetSurfaceCoord( flTryW, vecMin.z, vecMax.z );

					Vector vecTryWorldPosition;
					VectorTransform( vecLocalPosition, matOrientation, vecTryWorldPosition );

					float flPointGoodness = pParticles->RandomFloat( 0, 72 )
						+ DotProduct( vecTryWorldPosition - vecBasePos, 
						vecDirectionalBias );

					if ( nNumTrysToGetAPointInsideTheModel )
					{
						// do a point in solid test
						Ray_t ray;
						trace_t tr;
						ray.Init( vecTryWorldPosition, vecTryWorldPosition );
						enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
						if ( tr.startsolid )
							flPointGoodness += 1000.; // got a point inside!
					}
					if ( flPointGoodness > flBestPointGoodness )
					{
						u = flTryU;
						v = flTryV;
						w = flTryW;
						vecWorldPosition = vecTryWorldPosition;
						nHitbox = 0;
						flBestPointGoodness = flPointGoodness;
					}
				} while ( nTryCnt-- );
				*( pPntsOut++ ) = vecWorldPosition;
				if ( pHitBoxRelativeCoordOut )
					( pHitBoxRelativeCoordOut++ )->Init( u, v, w );
				if ( pHitBoxIndexOut )
					*( pHitBoxIndexOut++ ) = nHitbox;
			}
		}

		s_BoneMutex.Unlock();
	}
#endif
	if (! bSucesss )
	{
		// don't have a model or am in editor or something - fill return with control point
		for( int i=0 ; i < nNumPtsOut; i++)
		{
			pPntsOut[i] = pParticles->ControlPoint( nControlPointNumber ).m_Position; // fallback if anything goes wrong
			
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;
			
			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}
}


int CParticleSystemQuery::GetControllingObjectHitBoxInfo(
	CParticleCollection *pParticles,
	int nControlPointNumber,
	int nBufSize,										// # of output slots available
	ModelHitBoxInfo_t *pHitBoxOutputBuffer )
{
	int nRet = 0;

#ifndef GAME_DLL
	s_BoneMutex.Lock();

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = NULL;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}

	if ( pMoveParent )
	{
		C_BaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();
		if ( pAnimating )
		{
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{
		
				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
				
				if ( pStudioHdr )
				{
					// Try to get the "effects" set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), "effects" );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
					
					if ( set )
					{
						for( int i=0 ; i < set->numhitboxes; i++ )
						{
							mstudiobbox_t *pBox = set->pHitbox( i );

							// E3 HACK - check for hitboxes at the origin and ignore those
							if ( fabs( (*hitboxbones[pBox->bone])[0][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[1][3] ) < POINT_AT_ORIGIN_EPSILON && fabs( (*hitboxbones[pBox->bone])[2][3] ) < POINT_AT_ORIGIN_EPSILON )
							{
								continue;
							}

							pHitBoxOutputBuffer[nRet].m_vecBoxMins.x = pBox->bbmin.x;
							pHitBoxOutputBuffer[nRet].m_vecBoxMins.y = pBox->bbmin.y;
							pHitBoxOutputBuffer[nRet].m_vecBoxMins.z = pBox->bbmin.z;

							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.x = pBox->bbmax.x;
							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.y = pBox->bbmax.y;
							pHitBoxOutputBuffer[nRet].m_vecBoxMaxes.z = pBox->bbmax.z;

							pHitBoxOutputBuffer[nRet].m_Transform = *hitboxbones[pBox->bone];

							nRet++;

							if ( nRet >= nBufSize )
							{
								break;
							}
						}
					}
				}
			}
		}
		if ( pMoveParent->IsBrushModel() )
		{
			Vector vecMin;
			Vector vecMax;
			matrix3x4_t matOrientation;
			pMoveParent->GetRenderBounds( vecMin, vecMax  );
			matOrientation = pMoveParent->EntityToWorldTransform();
			pHitBoxOutputBuffer[0].m_vecBoxMins = vecMin;
			pHitBoxOutputBuffer[0].m_vecBoxMaxes = vecMax;
			pHitBoxOutputBuffer[0].m_Transform = matOrientation;
			nRet = 1;
		}
	}
	s_BoneMutex.Unlock();
#endif
	return nRet;
}



bool CParticleSystemQuery::IsPointInControllingObjectHitBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, Vector vecPos, bool bBBoxOnly )
{
	bool bSuccess = false;
#ifndef GAME_DLL

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = NULL;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		s_BoneMutex.Lock();
		C_BaseAnimating *pAnimating = pMoveParent->GetBaseAnimating();

		bool bInBBox = false;
		Vector vecBBoxMin;
		Vector vecBBoxMax;
		Vector vecOrigin;

		vecBBoxMin = pMoveParent->CollisionProp()->OBBMins();
		vecBBoxMax = pMoveParent->CollisionProp()->OBBMaxs();

		Vector vecLocalPos = vecPos;

		if ( pMoveParent->CollisionProp()->IsBoundsDefinedInEntitySpace() )
		{
			matrix3x4_t matOrientation;
			matOrientation = pMoveParent->EntityToWorldTransform();
			VectorITransform( vecPos, matOrientation, vecLocalPos );
		}
		if ( IsPointInBox( vecLocalPos, vecBBoxMin, vecBBoxMax ) )
			bInBBox = true;

		if ( bInBBox && bBBoxOnly )
			bSuccess = true;
		else if ( pAnimating && bInBBox )
		{
			matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
			if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) )
			{

				studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );

				if ( pStudioHdr )
				{
					// Try to get the "effects" set first, otherwise use their current set
					int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), "effects" );
					mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );

					if ( set )
					{
						// do a point in solid test
						Ray_t ray;
						trace_t tr;
						ray.Init( vecPos, vecPos );
						enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
						if ( tr.startsolid )
							bSuccess = true;
					}
				}
			}
		}
		else if ( pMoveParent->IsBrushModel() && bInBBox )
		{
			// do a point in solid test
			Ray_t ray;
			trace_t tr;
			ray.Init( vecPos, vecPos );
			enginetrace->ClipRayToEntity( ray, MASK_ALL, pMoveParent, &tr );
			if ( tr.startsolid )
				bSuccess = true;
		}

		s_BoneMutex.Unlock();
	}
#endif
	return bSuccess;
}

void CParticleSystemQuery::GetControllingObjectOBBox( 
	CParticleCollection *pParticles,
	int nControlPointNumber, Vector vecMin, Vector vecMax )
{
	vecMin = vecMax = vec3_origin;
#ifndef GAME_DLL

	EHANDLE *phMoveParent = reinterpret_cast<EHANDLE *> ( pParticles->ControlPoint( nControlPointNumber ).m_pObject );
	CBaseEntity *pMoveParent = NULL;
	if ( phMoveParent )
	{
		pMoveParent = *( phMoveParent );
	}
	if ( pMoveParent )
	{
		vecMin = pMoveParent->CollisionProp()->OBBMins();
		vecMax = pMoveParent->CollisionProp()->OBBMaxs();
	}
#endif
}

extern CUtlVector< RayTracingEnvironment * > g_RayTraceEnvironments;

void CParticleSystemQuery::TraceAgainstRayTraceEnv( int envnumber, const FourRays &rays, fltx4 TMin, fltx4 TMax,
													  RayTracingResult *rslt_out, int32 skip_id ) const
{
#if defined( CLIENT_DLL )
	if ( g_RayTraceEnvironments.IsValidIndex( envnumber ) )
	{
		RayTracingEnvironment *RtEnv = g_RayTraceEnvironments.Element( envnumber );
		RtEnv->Trace4Rays( rays, TMin, TMax, rslt_out, skip_id );
	}
#endif
}



struct RayTraceEnvironmentNameRecord_t
{
	const char *m_pszGroupName;
	int m_nGroupID;
};


static RayTraceEnvironmentNameRecord_t s_RtEnvNameMap[]={
	{ "PRECIPITATION", 0 },
	{ "PRECIPITATIONBLOCKER", 1 },
};


int CParticleSystemQuery::GetRayTraceEnvironmentFromName( const char *pszRtEnvName )
{
	for(int i = 0; i < ARRAYSIZE( s_RtEnvNameMap ); i++ )
	{
		if ( ! stricmp( s_RtEnvNameMap[i].m_pszGroupName, pszRtEnvName ) )
			return s_RtEnvNameMap[i].m_nGroupID;
	}
	return 0;
}



struct CollisionGroupNameRecord_t
{
	const char *m_pszGroupName;
	int m_nGroupID;
};


static CollisionGroupNameRecord_t s_NameMap[]={
	{ "NONE", COLLISION_GROUP_NONE },
	{ "DEBRIS", COLLISION_GROUP_DEBRIS },
	{ "INTERACTIVE", COLLISION_GROUP_INTERACTIVE },
	{ "NPC", COLLISION_GROUP_NPC },
	{ "ACTOR", COLLISION_GROUP_NPC_ACTOR },
	{ "PASSABLE", COLLISION_GROUP_PASSABLE_DOOR },	

#if defined( SWARM_DLL )
	{ "SENTRYPROJ", ASW_COLLISION_GROUP_SENTRY_PROJECTILE },
#endif
};


int CParticleSystemQuery::GetCollisionGroupFromName( const char *pszCollisionGroupName )
{
	for(int i = 0; i < ARRAYSIZE( s_NameMap ); i++ )
	{
		if ( ! stricmp( s_NameMap[i].m_pszGroupName, pszCollisionGroupName ) )
			return s_NameMap[i].m_nGroupID;
	}
	return COLLISION_GROUP_NONE;
}

Vector CParticleSystemQuery::GetLocalPlayerPos( void )
{
#ifdef CLIENT_DLL
//	HACK_GETLOCALPLAYER_GUARD( "CParticleSystemQuery::GetLocalPlayerPos" );
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return vec3_origin;
	return pPlayer->WorldSpaceCenter();
#else
	CBasePlayer *pPlayer = AI_GetSinglePlayer();	
	if ( !pPlayer )
		return vec3_origin;
	return pPlayer->WorldSpaceCenter();
#endif
}

void CParticleSystemQuery::GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight, Vector *pUp )
{
#ifdef CLIENT_DLL
//	HACK_GETLOCALPLAYER_GUARD( "CParticleSystemQuery::GetLocalPlayerPos" );
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		*pForward = vec3_origin;
		*pRight = vec3_origin;
		*pUp = vec3_origin;
		return;
	}
	pPlayer->EyeVectors( pForward, pRight, pUp );
#else
	CBasePlayer *pPlayer = AI_GetSinglePlayer();	
	if ( !pPlayer )
	{
		*pForward = vec3_origin;
		*pRight = vec3_origin;
		*pUp = vec3_origin;
		return;
	}
	pPlayer->EyeVectors( pForward, pRight, pUp );
#endif
}

Vector CParticleSystemQuery::GetCurrentViewOrigin()
{
#ifdef CLIENT_DLL
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	return CurrentViewOrigin();
#else
	return vec3_origin;
#endif

}


float CParticleSystemQuery::GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale )
{
#ifdef CLIENT_DLL
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot );
	pixelvis_queryparams_t params;
	params.Init( vecOrigin, flScale, 1.0 );
	float flVisibility = PixelVisibility_FractionVisible( params, pQueryHandle );
	flVisibility = MAX( 0.0f, flVisibility );
	return flVisibility;
#else
	return 0.0f;
#endif
}

void CParticleSystemQuery::DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration )
{
	debugoverlay->AddLineOverlay( origin, target, r, g, b, noDepthTest, duration );
}


#include "tier3/mdlutils.h"


#ifdef CLIENT_DLL
static void SetBodygroup( studiohdr_t *pstudiohdr, int &body, int iGroup, int iValue )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if ( iGroup >= pstudiohdr->numbodyparts )
	{
		return;
	}

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if ( iValue >= pbodypart->nummodels )
	{
		return;
	}

	int iCurrent = ( body / pbodypart->base ) % pbodypart->nummodels;

	body = ( body - ( iCurrent * pbodypart->base ) + ( iValue * pbodypart->base ) );
}
#endif

// callback to draw models given abstract ptr
void CParticleSystemQuery::DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
									  int nAnimationSequence, float flAnimationRate, float r, float g, float b, float a )
{
#ifdef CLIENT_DLL
	model_t *pMDL = ( model_t * ) pModel;
	if ( pMDL )
	{
		MDLHandle_t hStudioHdr = modelinfo->GetCacheHandle( pMDL );
		studiohdr_t *pStudioHdr = mdlcache->GetStudioHdr( hStudioHdr );

		CMDL	MDL;
		MDL.SetMDL( hStudioHdr );

		SetBodygroup( pStudioHdr, MDL.m_nBody, nBodyPart, nSubModel );
		MDL.m_Color = Color( r * 255, g * 255, b * 255, a * 255 );

		MDL.m_nSequence = nAnimationSequence;
		MDL.m_flPlaybackRate = flAnimationRate;
		MDL.m_flTime = pParticles->m_flCurTime;

		if ( pStudioHdr )
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pStudioHdr->numbones );
			MDL.SetUpBones( DrawMatrix, pStudioHdr->numbones, rdBoneToWorld.Base() );
			MDL.Draw(DrawMatrix, rdBoneToWorld.Base(), STUDIORENDER_DRAW_NO_SHADOWS );
		}
		else
		{
			MDL.Draw(DrawMatrix);
		}

		//debugging
		//	debugoverlay->AddTextOverlay( origin, 0.1, "p", pMDL );
		//Vector myOrigin = Vector( DrawMatrix.m_flMatVal[0][3], DrawMatrix.m_flMatVal[1][3], DrawMatrix.m_flMatVal[2][3] );
		//Vector vecFwd, vecRight, vecUp;
		//MatrixVectors( pRenderable->m_DrawModelMatrix, &vecFwd, &vecRight, &vecUp );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecFwd, 255, 0, 0, true, 0.1 );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecUp, 0, 0, 255, true, 0.1 );
 		//debugoverlay->AddLineOverlay( myOrigin, myOrigin + 36 * vecRight, 0, 255, 0, true, 0.1 );
	}
#endif
}

void *CParticleSystemQuery::GetModel( char const *pMdlName )
{
#ifdef CLIENT_DLL
// 	int modelIndex = modelinfo->GetModelIndex( pMdlName );
// 	const model_t *pModel = modelinfo->GetModel( modOelIndex );

	CUtlString	ModelName = "models/";

	ModelName += pMdlName;

 	model_t *pModel = (model_t *)engine->LoadModel( ModelName ); //, true );
 	//pMdlName = "models/weapons/shells/shell_pistol.mdl";
	return ( void * )pModel;
#else
	return NULL;
#endif
}


void CParticleSystemQuery::PreSimulate( ) 
{ 
#if defined( CLIENT_DLL )
	for( int i = 0; i < m_ActiveProjectedInfos.Count(); i++ )
	{
		m_ActiveProjectedInfos[ i ]->m_bUsedThisFrame = false;
	}
#endif // #if defined( CLIENT_DLL )
}


void CParticleSystemQuery::PostSimulate( ) 
{
#if defined( CLIENT_DLL )
	TProjectedTextureInfo *pInfo;

	while( m_ProjectedInfoAdds.PopItem( &pInfo ) == true )
	{
		m_ActiveProjectedInfos.AddToTail( pInfo );
	}

	for( int i = 0; i < m_ActiveProjectedInfos.Count(); i++ )
	{
		if ( m_ActiveProjectedInfos[ i ]->m_bUsedThisFrame == false )
		{
			delete m_ActiveProjectedInfos[ i ]->m_pEntity;
			m_ActiveProjectedInfos.Remove( i );
			i--;
			continue;
		}
		if ( m_ActiveProjectedInfos[ i ]->m_pEntity == NULL )
		{
			m_ActiveProjectedInfos[ i ]->m_pEntity = C_EnvProjectedTexture::Create();
		}

		m_ActiveProjectedInfos[ i ]->m_pEntity->SetAbsOrigin( m_ActiveProjectedInfos[ i ]->m_vOrigin );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetMaterial( m_ActiveProjectedInfos[ i ]->m_pMaterial );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetLightColor( m_ActiveProjectedInfos[ i ]->m_r * 255, m_ActiveProjectedInfos[ i ]->m_g * 255, m_ActiveProjectedInfos[ i ]->m_b * 255, m_ActiveProjectedInfos[ i ]->m_a * 255 );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetSize( m_ActiveProjectedInfos[ i ]->m_flSize );
		m_ActiveProjectedInfos[ i ]->m_pEntity->SetRotation( m_ActiveProjectedInfos[ i ]->m_flRotation );
	}
#endif // #if defined( CLIENT_DLL )
}


void CParticleSystemQuery::UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar )
{
#if defined( CLIENT_DLL )
	TProjectedTextureInfo *pInfo = reinterpret_cast< TProjectedTextureInfo * >( pUserVar );
	if ( pInfo == NULL )
	{
		pUserVar = pInfo = new TProjectedTextureInfo;
		memset( pInfo, 0, sizeof( *pInfo ) );
		m_ProjectedInfoAdds.PushItem( pInfo );
	}

	pInfo->m_nParticleID = nParticleID;
	pInfo->m_pMaterial = pMaterial;
	pInfo->m_vOrigin = vOrigin;
	pInfo->m_flSize = flRadius;
	pInfo->m_flRotation = flRotation;
	pInfo->m_r = r;
	pInfo->m_g = g;
	pInfo->m_b = b;
	pInfo->m_a = a;
	pInfo->m_bUsedThisFrame = true;

//	ClientEntityList().AddNonNetworkableEntity( this );
#endif // #if defined( CLIENT_DLL )
}


int CParticleSystemQuery::GetActivityCount()
{
	return 0;
}
const char* CParticleSystemQuery::GetActivityNameFromIndex( int nActivityIndex )
{
	return 0;
}



