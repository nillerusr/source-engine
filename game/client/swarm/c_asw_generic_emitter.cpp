#include "cbase.h"
#include "c_asw_mesh_emitter_entity.h"
#include "c_asw_generic_emitter.h"
#include "KeyValues.h"
#include <filesystem.h>
#include "gamestringpool.h"
#include "c_tracer.h"
#include "precache_register.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "datacache/imdlcache.h"
#include "engine/IVDebugOverlay.h"
#include "soundemittersystem/isoundemittersystembase.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_mesh_emitter_draw("asw_mesh_emitter_draw", "1", FCVAR_CHEAT, "Draw meshes from mesh emitters");
ConVar asw_emitter_min_collision_speed("asw_emitter_min_collision_speed", "50", FCVAR_CHEAT, "Minimum speed to make a sound");
ConVar asw_emitter_max_collision_speed("asw_emitter_max_collision_speed", "80", FCVAR_CHEAT, "Maximum speed that makes a full volume");

CASWGenericEmitter::CASWGenericEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
	m_iResetEmitter = 0;
	m_ParticlesPerSecond = 1;
	m_tParticleTimer.Init( m_ParticlesPerSecond );
	m_CurrentParticlesPerSecond = m_ParticlesPerSecond;
	m_hMaterial = GetPMaterial( "effects/yellowflare" );
	m_hGlowMaterial = NULL;
	m_bEmit = true;
	m_szGlowMaterialName[0] = '\0';
	m_fGlowScale = 1.7f;
	Q_snprintf(m_szMaterialName, sizeof(m_szTemplateName), "effects/yellowflare");
	Q_snprintf(m_szTemplateName, sizeof(m_szTemplateName), "None");
	Q_snprintf(m_szCollisionTemplateName, sizeof(m_szCollisionTemplateName), "None");
	Q_snprintf(m_szDropletTemplateName, sizeof(m_szDropletTemplateName), "None");

	m_Colors[0].bUse = true;
	m_Colors[0].fTime = 0;
	m_Colors[0].fBandLength = 100;
	m_Colors[0].Color.r = 255;
	m_Colors[0].Color.g = 255;
	m_Colors[0].Color.b = 255;

	m_Scales[0].bUse = true;
	m_Scales[0].fTime = 0;
	m_Scales[0].fBandLength = 100;
	m_Scales[0].fScale = 30;

	m_Alphas[0].bUse = true;
	m_Alphas[0].fTime = 0;
	m_Alphas[0].fBandLength = 100;
	m_Alphas[0].fAlpha = 255;

	for (int i=1;i<5;i++)
	{
		m_Colors[i].bUse = false;
		m_Colors[i].fTime = 0;
		m_Colors[i].fBandLength = 0;
		m_Colors[i].Color.r = 0;
		m_Colors[i].Color.g = 0;
		m_Colors[i].Color.b = 0;

		m_Scales[i].bUse = false;
		m_Scales[i].fTime = 0;
		m_Scales[i].fBandLength = 0;
		m_Scales[i].fScale = 0;

		m_Alphas[i].bUse = false;
		m_Alphas[i].fTime = 0;
		m_Alphas[i].fBandLength = 0;
		m_Alphas[i].fAlpha = 0;
	}		
	m_fParticleLifeMin = 3;
	m_fParticleLifeMax = 5;
	m_fPresimulateTime = 0;
	velocityMin.Init(-30,-30,0);
	velocityMax.Init(30,30,0);
	positionMin.Init();
	positionMax.Init();
	accelerationMin.Init();
	accelerationMax.Init();
	fGravity = 0;
	fRollMin = 0;
	fRollMax = 0;
	fRollDeltaMin = 0;
	fRollDeltaMax = 0;
	m_fEmitterScale = 1.0f;
	m_bWrapParticlesToSpawnBounds = false;
	m_bLocalCoordSpace = false;	// if true, particles are stored in local space and transformed to the position of the emitter when rendered
	m_vecPosition.Init();
	m_vecEmitterPositionDelta.Init();
	m_vecLastSimulatePosition.Init();
	m_angFacing.Init();
	m_UseCollision = aswpc_none;
	m_hCollisionIgnoreEntity = NULL;
	m_fCollisionDampening = 50.0f;
	m_hCollisionEmitter = NULL;
	m_hDropletEmitter = NULL;
	m_DrawType = aswpdt_sprite;
	m_fBeamLength = 1.0f;
	m_bScaleBeamByVelocity = true;
	m_bScaleBeamByLifeLeft = true;
	m_iBeamPosition = ASW_EMITTER_BEAM_POS_BEHIND;
	m_fDropletChance = 40.f;
	m_fLargestParticleSize = 0;
	m_iParticleSupply = -1;
	m_iInitialParticleSupply = -1;
	m_fDieTime = 0;
	m_fLifeLostOnCollision= 0;

	// lighting
	m_iLightingType = 0;
	m_fLightApply = 0.5f;
	m_vecLighting.Init();

	// custom collision
	m_bUseCustomCollisionMask = false;
	m_bUseCustomCollisionGroup = false;
	m_CustomCollisionMask = MASK_SOLID;
	m_CustomCollisionGroup = COLLISION_GROUP_NONE;

	m_hMeshEmitter = NULL;
	m_vecTraceMins = Vector(-10, -10, -10);
	m_vecTraceMaxs = Vector(10, 10, 10);
	m_bHullTraces = false;
	m_fReduceRollRateOnCollision = 1.0f;
	m_szCollisionSoundName[0] = '\0';
	m_szCollisionDecalName[0] = '\0';

	m_fParticleLocal = 0;
}

CSmartPtr<CASWGenericEmitter> CASWGenericEmitter::Create( const char *pDebugName )
{
	CASWGenericEmitter *pRet = new CASWGenericEmitter( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

// returns the desired alpha for this particle (based upon its lifetime, dietime and our alpha nodes)
float	CASWGenericEmitter::UpdateAlpha( const SimpleParticle *pParticle )
{
	float fLightingScale = m_iLightingType < 2 ?
		1.0f :		// no scaling alpha by lighting
	((m_vecLighting.x + m_vecLighting.y + m_vecLighting.z) / 3.0f);	// scale alpha by average of rgb lighting
	// check for only applying some fraction of the shading
	fLightingScale += (1.0f - fLightingScale) * (1.0f - m_fLightApply);

	// find which band we're in
	float fTime = (pParticle->m_flLifetime / pParticle->m_flDieTime) * 100.0f;
	if (fTime < m_Alphas[0].fTime)
	{
		return (m_Alphas[0].fAlpha / 255.0f) * fLightingScale;
	}
	for (unsigned int i=0; i<5; i++)
	{
		if (fTime >= m_Alphas[i].fTime && (fTime - m_Alphas[i].fTime) <= m_Alphas[i].fBandLength)
		{
			if (i==4 || !m_Alphas[i+1].bUse)		// last band, use the solid alpha
				return (m_Alphas[i].fAlpha / 255.0f) * fLightingScale;	
			// transition = into band / band length
			float fTransition = (fTime - m_Alphas[i].fTime) / m_Alphas[i].fBandLength;
			// transition is how far to go between this band's alpha and the next band's alpha			
			float fDifference = m_Alphas[i+1].fAlpha - m_Alphas[i].fAlpha;
			return ((m_Alphas[i].fAlpha + fDifference * fTransition) / 255.0f) * fLightingScale;
		}
	}
	return (m_Alphas[0].fAlpha / 255.0f) * fLightingScale;
}
float	CASWGenericEmitter::ASWUpdateScale( const ASWParticle *pParticle )
{
	// find which band we're in
	float fTime = (pParticle->m_flLifetime / pParticle->m_flDieTime) * 100.0f;
	bool bGlow = (pParticle->m_ParticleType == aswpt_glow);

	if (fTime < m_Scales[0].fTime)
	{
		if (bGlow)
			return m_Scales[0].fScale * m_fEmitterScale * m_fGlowScale;

		return m_Scales[0].fScale * m_fEmitterScale;
	}
	for (unsigned int i=0; i<5; i++)
	{
		if (fTime >= m_Scales[i].fTime && (fTime - m_Scales[i].fTime) <= m_Scales[i].fBandLength)
		{
			if (i==4 || !m_Scales[i+1].bUse)		// last band, use the solid fScale
				return m_Scales[i].fScale * m_fEmitterScale;	
			// transition = into band / band length
			float fTransition = (fTime - m_Scales[i].fTime) / m_Scales[i].fBandLength;
			// transition is how far to go between this band's fScale and the next band's fScale			
			float fDifference = m_Scales[i+1].fScale - m_Scales[i].fScale;
			if (bGlow)
				return (m_Scales[i].fScale + fDifference * fTransition) * m_fEmitterScale * m_fGlowScale;
			return (m_Scales[i].fScale + fDifference * fTransition) * m_fEmitterScale;
		}
	}
	if (bGlow)
		return m_Scales[0].fScale * m_fEmitterScale * m_fGlowScale;
	return m_Scales[0].fScale * m_fEmitterScale;
}

Vector	CASWGenericEmitter::UpdateColor( const SimpleParticle *pParticle )
{
	bool bUseLighting = (m_iLightingType == 1) || (m_iLightingType == 3);
	float fLightingScaleR = bUseLighting ? m_vecLighting.x : 1.0f;
	float fLightingScaleG = bUseLighting ? m_vecLighting.y : 1.0f;
	float fLightingScaleB = bUseLighting ? m_vecLighting.z : 1.0f;
	fLightingScaleR += (1.0f - fLightingScaleR) * (1.0f - m_fLightApply);
	fLightingScaleG += (1.0f - fLightingScaleG) * (1.0f - m_fLightApply);
	fLightingScaleB += (1.0f - fLightingScaleB) * (1.0f - m_fLightApply);

	// find which band we're in
	float fTime = (pParticle->m_flLifetime / pParticle->m_flDieTime) * 100.0f;
	if (fTime < m_Colors[0].fTime)
	{
		Vector result;
		result[0] =  (m_Colors[0].Color.r / 255.0f) * fLightingScaleR;
		result[1] =  (m_Colors[0].Color.g / 255.0f) * fLightingScaleG;
		result[2] =  (m_Colors[0].Color.b / 255.0f) * fLightingScaleB;
		return result;
	}
	for (unsigned int i=0; i<5; i++)
	{
		if (fTime >= m_Colors[i].fTime && (fTime - m_Colors[i].fTime) <= m_Colors[i].fBandLength)
		{
			if (i==4 || !m_Colors[i+1].bUse)		// last band, use the solid fScale
			{
				Vector result;
				result[0] =  (m_Colors[i].Color.r / 255.0f) * fLightingScaleR;
				result[1] =  (m_Colors[i].Color.g / 255.0f) * fLightingScaleG;
				result[2] =  (m_Colors[i].Color.b / 255.0f) * fLightingScaleB;
				return result;
			}
			// transition = into band / band length
			float fTransition = (fTime - m_Colors[i].fTime) / m_Colors[i].fBandLength;
			// transition is how far to go between this band's fScale and the next band's fScale			
			float fDifferenceR = m_Colors[i+1].Color.r - m_Colors[i].Color.r;
			float fDifferenceG = m_Colors[i+1].Color.g - m_Colors[i].Color.g;
			float fDifferenceB = m_Colors[i+1].Color.b - m_Colors[i].Color.b;

			Vector result;
			result[0] = ((m_Colors[i].Color.r + fDifferenceR * fTransition) / 255.0f) * fLightingScaleR;
			result[1] = ((m_Colors[i].Color.g + fDifferenceG * fTransition) / 255.0f) * fLightingScaleG;
			result[2] = ((m_Colors[i].Color.b + fDifferenceB * fTransition) / 255.0f) * fLightingScaleB;
			return result;
		}
	}
	Vector result;
	result[0] =  (m_Colors[0].Color.r / 255.0f) * fLightingScaleR;
	result[1] =  (m_Colors[0].Color.g / 255.0f) * fLightingScaleG;
	result[2] =  (m_Colors[0].Color.b / 255.0f) * fLightingScaleB;
	return result;
}

// precalcuate each band length (where a band is one straight line on the node graph)
void CASWGenericEmitter::CalcBandLengths()
{
	float fCurrentTime = 0;
	for (unsigned int i=0; i<5; i++)
	{
		if (!m_Colors[i].bUse)
			break;

		if (i==4 || !m_Colors[i+1].bUse)	// last band
		{
			m_Colors[i].fBandLength = 100.0f - fCurrentTime;
		}
		else
		{
			m_Colors[i].fBandLength = m_Colors[i+1].fTime - fCurrentTime;
		}
		fCurrentTime += m_Colors[i].fBandLength;
	}

	fCurrentTime = 0;
	for (unsigned int i=0; i<5; i++)
	{
		if (!m_Scales[i].bUse)
			break;

		if (i==4 || !m_Scales[i+1].bUse)	// last band
		{
			m_Scales[i].fBandLength = 100.0f - fCurrentTime;
		}
		else
		{
			m_Scales[i].fBandLength = m_Scales[i+1].fTime - fCurrentTime;
		}
		fCurrentTime += m_Scales[i].fBandLength;
	}

	fCurrentTime = 0;
	for (unsigned int i=0; i<5; i++)
	{
		if (!m_Alphas[i].bUse)
			break;

		if (i==4 || !m_Alphas[i+1].bUse)	// last band
		{
			m_Alphas[i].fBandLength = 100.0f - fCurrentTime;
		}
		else
		{
			m_Alphas[i].fBandLength = m_Alphas[i+1].fTime - fCurrentTime;
		}
		fCurrentTime += m_Alphas[i].fBandLength;
	}
}

void CASWGenericEmitter::StartRender( VMatrix &effectMatrix )
{
	BaseClass::StartRender(effectMatrix);
	if (m_iResetEmitter > 0)		// this gets set to 2 when someone wants the emitter reset.  It ticks down once here, then all the particles are removed in the subsequent simulate, then it gets ticked down to 0 again next time.
		m_iResetEmitter--;
}

void CASWGenericEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	// work out how much the emitter moved since the last simulate
	if (m_vecLastSimulatePosition != vec3_origin)
		m_vecEmitterPositionDelta = m_vecPosition - m_vecLastSimulatePosition;
	else
		m_vecEmitterPositionDelta = vec3_origin;

	ASWParticle *pParticle = (ASWParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		if (SimulateParticle(pParticle, timeDelta))
		{
			if (pParticle->m_pPartner)
				pParticle->m_pPartner->m_pPartner = NULL;
			pIterator->RemoveParticle(pParticle);
		}

		pParticle = (ASWParticle*)pIterator->GetNext();
	}

	m_vecLastSimulatePosition = m_vecPosition;
}

bool CASWGenericEmitter::SimulateParticle(ASWParticle* pParticle, float timeDelta)
{
	timeDelta += pParticle->m_fExtraSimulateTime;
	pParticle->m_fExtraSimulateTime = 0;		

	if (pParticle->m_ParticleType == aswpt_glow)	// glow particles move in a special way, so as to stay linked to their main particle
	{
		// advance the life of the particle
		pParticle->m_flLifetime += timeDelta;
		// position it relative to its parent based on its lifetime and constant velocity
		if (pParticle->m_pPartner && pParticle->m_pPartner->m_pPartner == pParticle)	// check it's a valid partner
		{
			pParticle->m_Pos = pParticle->m_pPartner->m_Pos + pParticle->m_vecVelocity * pParticle->m_flLifetime;
		}
		else
		{
			pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;
		}
	}
	else
	{
		//Update velocity
		ASWUpdateVelocity( pParticle, timeDelta );

		Vector newPos = pParticle->m_Pos + pParticle->m_vecVelocity * timeDelta;

		// move the particle along with the emitter origin?
		newPos += m_vecEmitterPositionDelta * (m_fParticleLocal / 100.0f);

		// check for wrapping spawn bounds (used primarily for particle emitters attached to the camera)
		if (m_bWrapParticlesToSpawnBounds && !m_bLocalCoordSpace)
		{
			for (int i=0;i<2;i++)
			{
				float width = positionMax[i] - positionMin[i];
				while (newPos[i] > m_vecPosition[i] + positionMax[i])
				{
					newPos[i] -= width;
				}
				while (newPos[i] < m_vecPosition[i] + positionMin[i])
				{
					newPos[i] += width;
				}
			}
		}

		if (m_UseCollision != aswpc_none)
		{
			trace_t trace;
			int mask = MASK_SOLID_BRUSHONLY;
			if (m_UseCollision == aswpc_all)
				mask = MASK_SOLID;
			if (m_bUseCustomCollisionMask)
				mask = m_CustomCollisionMask;
			int col_group = COLLISION_GROUP_NONE;
			if (m_bUseCustomCollisionGroup)
				col_group = m_CustomCollisionGroup;

			if (m_bHullTraces)
			{				
				UTIL_TraceHull(pParticle->m_Pos, newPos, m_vecTraceMins, m_vecTraceMaxs, mask, m_hCollisionIgnoreEntity.Get(), col_group, &trace);
			}
			else
				UTIL_TraceLine(pParticle->m_Pos, newPos, mask, m_hCollisionIgnoreEntity.Get(), col_group, &trace);
			if ( trace.fraction >= 1.0f )
			{
				pParticle->m_Pos = newPos;
			}
			else
			{
				// if we're going fast enough to do a sound
				if (m_szCollisionSoundName[0] != '\0')
				{
					float speed = pParticle->m_vecVelocity.Length();
					if (speed  > asw_emitter_min_collision_speed.GetFloat())
					{
						float fVolume = 1.0f;
						if (speed < asw_emitter_max_collision_speed.GetFloat())
						{
							fVolume = (speed - asw_emitter_min_collision_speed.GetFloat()) / 
								(asw_emitter_max_collision_speed.GetFloat() - asw_emitter_min_collision_speed.GetFloat());
						}						
						CLocalPlayerFilter filter;						
						CSoundParameters params;

						if ( C_BaseEntity::GetParametersForSound( m_szCollisionSoundName, params, NULL ) )
						{
							EmitSound_t ep( params );

							ep.m_flVolume = fVolume;
							ep.m_nChannel = CHAN_AUTO;
							ep.m_pOrigin = &pParticle->m_Pos;

							C_BaseEntity::EmitSound( filter, 0, ep );
						}						
					}
				}
				// if we're going fast enough to do a decal
				if ( m_szCollisionDecalName[0] != '\0' && !pParticle->bPlacedDecal )
				{
					float speed = pParticle->m_vecVelocity.Length();
					if (speed  > asw_emitter_min_collision_speed.GetFloat())
					{
						Vector diff = newPos - pParticle->m_Pos;
						trace_t tr;
						UTIL_TraceLine( pParticle->m_Pos, pParticle->m_Pos + diff * 64.0f, MASK_SOLID, m_hCollisionIgnoreEntity.Get(), COLLISION_GROUP_NONE, &tr );
						UTIL_DecalTrace( &tr, m_szCollisionDecalName );
						pParticle->bPlacedDecal = true;
					}
				}

				// reflect off the surface
				float proj = (pParticle->m_vecVelocity).Dot(trace.plane.normal);
				VectorMA( pParticle->m_vecVelocity, -proj*2, trace.plane.normal, pParticle->m_vecVelocity );

				proj = (pParticle->m_vecAccn).Dot(trace.plane.normal);
				VectorMA( pParticle->m_vecAccn, -proj*2, trace.plane.normal, pParticle->m_vecAccn );

				// dampen
				pParticle->m_vecAccn *= m_fCollisionDampening * 0.01f;
				pParticle->m_vecVelocity *= m_fCollisionDampening * 0.01f;

				// dampen roll rate
				pParticle->m_flRollDelta *= m_fReduceRollRateOnCollision;

				// reduce lifespan
				pParticle->m_flLifetime += m_fLifeLostOnCollision;
				if (pParticle->m_flLifetime > pParticle->m_flDieTime)
					pParticle->m_flLifetime = pParticle->m_flDieTime;

				// if we're dropping particles, drop a bunch more when we collide
				if (pParticle->m_ParticleType == aswpt_normal)
				{						
					// if we have a collide emitter, make it spit out a particle on collision
					if (m_hCollisionEmitter.IsValid())
					{
						m_hCollisionEmitter->SpawnParticle(pParticle->m_Pos, QAngle(0,0,0));
					}
				}
			}
		}
		else
		{
			pParticle->m_Pos = newPos;
		}

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
	}


	if (m_iResetEmitter > 0)
	{
		pParticle->m_flLifetime = pParticle->m_flDieTime;
	}

	UpdateRoll( pParticle, timeDelta );

	// Droplet Particles?
	if (m_hDropletEmitter.IsValid() && pParticle->m_fDropTime > 0 && pParticle->m_fDropTime <= pParticle->m_flLifetime)
	{
		if (m_fDropletChance < 0)	// negative droplet chance indicates we want a droplet every interval, instead of 1 random droplet
		{
			pParticle->m_fDropTime += -m_fDropletChance;	// spawn another droplet next interval
		}
		else
			pParticle->m_fDropTime = 0;
		m_hDropletEmitter->SpawnParticle(pParticle->m_Pos, QAngle(0,0,0));
	}

	return ( pParticle->m_flLifetime >= pParticle->m_flDieTime );
}

ASWParticle*	CASWGenericEmitter::AddASWParticle(
	PMaterialHandle hMaterial,
	const Vector &vOrigin,
	float flDieTime,
	unsigned char uchSize )
{
	ASWParticle *pRet = (ASWParticle*)AddParticle( sizeof( ASWParticle ), hMaterial, vOrigin );
	if ( pRet )
	{
		pRet->m_Pos = vOrigin;
		pRet->m_vecVelocity.Init();
		pRet->m_vecAccn.Init();
		pRet->m_flRoll = 0;
		pRet->m_flRollDelta = 0;
		pRet->m_flLifetime = 0;
		pRet->m_flDieTime = flDieTime;
		pRet->m_uchColor[0] = pRet->m_uchColor[1] = pRet->m_uchColor[2] = 0;
		pRet->m_uchStartAlpha = pRet->m_uchEndAlpha = 255;
		pRet->m_uchStartSize = pRet->m_uchEndSize = uchSize;
		pRet->m_iFlags = 0;		
		pRet->m_fExtraSimulateTime = 0;
		pRet->m_ParticleType = aswpt_normal;
		pRet->m_fDropTime = 0;
		pRet->m_pPartner = NULL;
		pRet->bPlacedDecal = false;
	}

	return pRet;
}

void	CASWGenericEmitter::ASWUpdateVelocity( ASWParticle *pParticle, float timeDelta )
{
	// add particle acceleration to its velocity
	pParticle->m_vecVelocity += pParticle->m_vecAccn * timeDelta;
	// add gravity
	pParticle->m_vecVelocity.z += fGravity * timeDelta;

	BaseClass::UpdateVelocity(pParticle, timeDelta);
}

ASWParticle* CASWGenericEmitter::SpawnParticle(const Vector& Position, const QAngle& Angle)
{
	ASWParticle *pParticle;

	matrix3x4_t matrix;
	AngleMatrix( Angle, matrix );

	Vector v, pos;
	v.x = (positionMin.x + (positionMax.x - positionMin.x) * random->RandomFloat()) * m_fEmitterScale;
	v.y = (positionMin.y + (positionMax.y - positionMin.y) * random->RandomFloat()) * m_fEmitterScale;
	v.z = (positionMin.z + (positionMax.z - positionMin.z) * random->RandomFloat()) * m_fEmitterScale;
	if (m_bLocalCoordSpace)
	{
		pos = v;
	}
	else
	{
		VectorTransform(v, matrix, pos);
		pos += Position;
	}

	// Create the particle
	pParticle = AddASWParticle( m_hMaterial, pos );

	if ( pParticle == NULL )
		return NULL;

	// Setup our roll
	pParticle->m_flRoll = DEG2RAD(fRollMin) + (DEG2RAD(fRollMax) - DEG2RAD(fRollMin)) * random->RandomFloat();
	pParticle->m_flRollDelta = DEG2RAD(fRollDeltaMin) + (DEG2RAD(fRollDeltaMax) - DEG2RAD(fRollDeltaMin)) * random->RandomFloat();



	// Set our velocity
	v.x = (velocityMin.x + (velocityMax.x - velocityMin.x) * random->RandomFloat()) * m_fEmitterScale;
	v.y = (velocityMin.y + (velocityMax.y - velocityMin.y) * random->RandomFloat()) * m_fEmitterScale;
	v.z = (velocityMin.z + (velocityMax.z - velocityMin.z) * random->RandomFloat()) * m_fEmitterScale;
	VectorTransform(v, matrix, pParticle->m_vecVelocity);

	// set our acceleration
	v.x = (accelerationMin.x + (accelerationMax.x - accelerationMin.x) * random->RandomFloat()) * m_fEmitterScale;
	v.y = (accelerationMin.y + (accelerationMax.y - accelerationMin.y) * random->RandomFloat()) * m_fEmitterScale;
	v.z = (accelerationMin.z + (accelerationMax.z - accelerationMin.z) * random->RandomFloat()) * m_fEmitterScale;		
	VectorTransform(v, matrix, pParticle->m_vecAccn);

	// Die in a short range of time
	pParticle->m_flDieTime = m_fParticleLifeMin + (m_fParticleLifeMax - m_fParticleLifeMin) * random->RandomFloat();

	// alpha, color, scale are set before rendering from the emitter calcs
	return pParticle;
}

ASWParticle* CASWGenericEmitter::SpawnGlowParticle(const Vector& Position, const QAngle& Angle, ASWParticle* pParent)
{
	if (!pParent || *GetGlowMaterial()==NULL)
		return NULL;

	ASWParticle *pParticle;

	// Create the particle
	pParticle = AddASWParticle( m_hGlowMaterial, pParent->m_Pos );

	if ( pParticle == NULL )
		return NULL;

	pParticle->m_ParticleType = aswpt_glow;
	// Setup our roll
	pParticle->m_flRoll = pParent->m_flRoll;
	pParticle->m_flRollDelta = pParent->m_flRollDelta;

	// Set our velocity/accn/etc
	float f = pParent->m_vecVelocity.Length() * (m_fGlowDeviation / 100); // 0.05f;
	pParticle->m_vecVelocity = RandomVector(-f, f);
	pParticle->m_vecAccn.Init();// = pParent->m_vecAccn;
	pParticle->m_flDieTime = pParent->m_flDieTime;

	// link the glow particle to its parent
	pParticle->m_pPartner = pParent;
	pParent->m_pPartner = pParticle;

	// alpha, color, scale are set before rendering from the emitter calcs
	return pParticle;
}

// simulate the emitter running for x seconds, instantly
void CASWGenericEmitter::DoPresimulate(const Vector& Position, const QAngle& Angle)
{
	float fTimeLeft = m_fPresimulateTime;

	while (fTimeLeft > 0)
	{
		fTimeLeft -= 1.0f / m_ParticlesPerSecond;
		if (fTimeLeft > 0 && (m_iParticleSupply == -1 || m_iParticleSupply > 0))
		{
			ASWParticle* pParticle = SpawnParticle(Position, Angle);
			if (pParticle)
			{
				pParticle->m_fExtraSimulateTime = fTimeLeft;
				if (m_iParticleSupply > 0)
					m_iParticleSupply--;

				if (*GetGlowMaterial()!=NULL && pParticle)
				{
					ASWParticle* pGlowParticle = SpawnGlowParticle(Position, Angle, pParticle);
					if (pGlowParticle)
						pGlowParticle->m_fExtraSimulateTime = fTimeLeft;
				}
			}			
		}
	}

	m_tParticleTimer.Init( m_ParticlesPerSecond );
}

// Change the material given to particles spat out by this emitter
void CASWGenericEmitter::SetMaterial(const char* materialname )
{
	strcpy(m_szMaterialName, materialname);
	m_hMaterial = GetPMaterial( materialname );
	ResetEmitter();
}

void CASWGenericEmitter::SetGlowMaterial(const char* materialname )
{
	if (!stricmp(materialname, "None"))
	{
		m_szGlowMaterialName[0]='\0';
		m_hGlowMaterial = NULL;
	}
	else
	{
		strcpy(m_szGlowMaterialName, materialname);
		m_hGlowMaterial = GetPMaterial( materialname );
	}
	ResetEmitter();
}

void CASWGenericEmitter::SetCustomCollisionMask(int iMask)
{
	m_bUseCustomCollisionMask = true;
	m_CustomCollisionMask = iMask;	
}

void CASWGenericEmitter::SetCustomCollisionGroup(int iColGroup)
{
	m_bUseCustomCollisionGroup = true;
	m_CustomCollisionGroup = iColGroup;
}	

// Makes all the particles get destroyed in the next simulate, and triggers the presimulate
void CASWGenericEmitter::ResetEmitter()
{
	if (m_hCollisionEmitter.IsValid())
	{
		m_hCollisionEmitter = NULL;
	}
	if (m_hDropletEmitter.IsValid())
	{
		m_hDropletEmitter = NULL;
	}
	// if we have a collision template, instantiate that emitter too
	if (stricmp(m_szCollisionTemplateName, "None") && m_szCollisionTemplateName[0]!=0)
	{
		m_hCollisionEmitter = CASWGenericEmitter::Create("CollisionTemplate");
		m_hCollisionEmitter->UseTemplate(m_szCollisionTemplateName);
		m_hCollisionEmitter->SetActive(false);
		m_hCollisionEmitter->m_ParticlesPerSecond = 0;	// stop it producing particles over time
	}
	if (stricmp(m_szDropletTemplateName, "None") && m_szDropletTemplateName[0]!=0)
	{
		m_hDropletEmitter = CASWGenericEmitter::Create("DropletTemplate");
		m_hDropletEmitter->UseTemplate(m_szDropletTemplateName);
		m_hDropletEmitter->SetActive(false);
		m_hDropletEmitter->m_ParticlesPerSecond = 0;	// stop it producing particles over time
	}
	// set particle cull radius
	m_fLargestParticleSize = 1.0f;
	for (int i=0;i<5;i++)
	{
		if (m_Scales[i].bUse && m_Scales[0].fScale > m_fLargestParticleSize)
			m_fLargestParticleSize = m_Scales[0].fScale;
	}	
	SetParticleCullRadius(m_fLargestParticleSize * m_fEmitterScale);

	m_iResetEmitter = 2;
	m_iParticleSupply = m_iInitialParticleSupply;
}

// this should be called when something else changes our variables
void CASWGenericEmitter::Update()
{
	CalcBandLengths();
}

void CASWGenericEmitter::Think(float deltaTime, const Vector& Position, const QAngle& Angle)
{
	VPROF_BUDGET( "CASWGenericEmitter::Think", VPROF_BUDGETGROUP_ASW_CLIENT );

	// store position and facing (used by rendering when particles are stored in local space)
	m_vecPosition = Position;
	m_angFacing = Angle;

	// calculate the max bbox for this emitter
	// todo: rotate by angle, take into account gravity + acceleration
	//Vector PosMax = Position + velocityMax * m_fParticleLifeMax + positionMax;
	//Vector PosMin = Position + velocityMin * m_fParticleLifeMax + positionMin;	
	//m_ParticleEffect.SetBBox( PosMin, PosMax );

	if (m_iResetEmitter && GetBinding().GetNumActiveParticles()==0)
	{
		m_iResetEmitter = 0;
		// calc lighting
		if (m_iLightingType > 0)
		{
			m_vecLighting = engine->GetLightForPoint(Position, true);
			m_vecLighting.x = LinearToTexture( m_vecLighting.x ) / 255.0f;
			m_vecLighting.y = LinearToTexture( m_vecLighting.y ) / 255.0f;
			m_vecLighting.z = LinearToTexture( m_vecLighting.z ) / 255.0f;
			//Msg("Baked lighting for emitter: %f, %f, %f\n", m_vecLighting.x, m_vecLighting.y, m_vecLighting.z);
		}
		DoPresimulate(Position, Angle);
	}

	float curTime = gpGlobals->frametime;

	// Add as many particles as required this frame
	while ( m_tParticleTimer.NextEvent( curTime ) )
	{
		if (m_bEmit && (m_iParticleSupply == -1 || m_iParticleSupply > 0))
		{
			ASWParticle* pParticle = SpawnParticle(Position, Angle);
			if (pParticle)
			{
				if (m_iParticleSupply > 0)
					m_iParticleSupply--;
				if (curTime > 0)
					pParticle->m_fExtraSimulateTime = curTime;
				if (*GetGlowMaterial()!=NULL)
				{
					ASWParticle* pGlow = SpawnGlowParticle(Position, Angle, pParticle);
					if (pGlow)
						pGlow->m_fExtraSimulateTime = pParticle->m_fExtraSimulateTime;
				}
				if (m_hDropletEmitter.IsValid())
				{
					if (m_fDropletChance < 0)	// negative droplet chance indicates we want a droplet every interval, instead of 1 random droplet
					{
						pParticle->m_fDropTime = -m_fDropletChance;
					}
					else if (random->RandomFloat() < (m_fDropletChance / 100.0f))
						pParticle->m_fDropTime = random->RandomFloat(0.25f, 1.0f) * pParticle->m_flDieTime;
				}
			}
		}
	}
	if (m_CurrentParticlesPerSecond != m_ParticlesPerSecond)
	{
		m_tParticleTimer.Init( m_ParticlesPerSecond );
		m_CurrentParticlesPerSecond = m_ParticlesPerSecond;
	}
	if (m_hCollisionEmitter.IsValid())
	{
		m_hCollisionEmitter->Think(deltaTime, Position, Angle);
	}
	if (m_hDropletEmitter.IsValid())
	{
		m_hDropletEmitter->Think(deltaTime, Position, Angle);
	}
	if (m_fDieTime > 0 && m_fDieTime < gpGlobals->curtime)
	{
		m_bEmit = false;
		// ASWTODO - find out a good way to destroy this CParticleEffect
		//Release();
	}
}

void CASWGenericEmitter::SetActive(bool b)
{
	if (!IsReleased())	// don't change our active state if we're dying
		m_bEmit = b;
}

void CASWGenericEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	/*
	if (asw_mesh_emitter_draw.GetBool())
	{
	// check for drawing by some parent entity mesh - is this going to mess up draw order and things?
	if ( m_DrawType == aswpdt_mesh && m_hMeshEmitter.Get())
	{
	MDLCACHE_CRITICAL_SECTION();
	if (m_hMeshEmitter->PrepareToDraw())
	{			

	const ASWParticle *pParticle = (const ASWParticle *)pIterator->GetFirst();

	int iDrawn = 0;
	while ( pParticle )
	{
	//Render
	Vector	tPos;
	Vector vecParticlePos = pParticle->m_Pos;

	// if particles are stored in local coord space, then transform them to the particle system's last known coords
	//if (m_bLocalCoordSpace)
	//{
	//matrix3x4_t matrix;
	//AngleMatrix( m_angFacing, matrix );
	//VectorTransform(pParticle->m_Pos, matrix, vecParticlePos);
	//vecParticlePos += m_vecPosition;
	//}

	debugoverlay->AddLineOverlay( vecParticlePos, vecParticlePos+Vector(3, 3, 3),
	0, 0, 255, true, 0.01f );
	iDrawn += m_hMeshEmitter->DrawParticle(vecParticlePos);

	TransformParticle( ParticleMgr()->GetModelView(), vecParticlePos, tPos );
	float sortKey = (int) tPos.z;									

	pParticle = (const ASWParticle *)pIterator->GetNext( sortKey );
	}
	Msg("Drew mesh particles: %d\n", iDrawn);
	}

	return;
	}
	}*/

	const ASWParticle *pParticle = (const ASWParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		//Render
		Vector	tPos;
		Vector vecParticlePos = pParticle->m_Pos;

		// if particles are stored in local coord space, then transform them to the particle system's last known coords
		if (m_bLocalCoordSpace)
		{
			matrix3x4_t matrix;
			AngleMatrix( m_angFacing, matrix );
			VectorTransform(pParticle->m_Pos, matrix, vecParticlePos);
			vecParticlePos += m_vecPosition;
		}

		TransformParticle( ParticleMgr()->GetModelView(), vecParticlePos, tPos );
		float sortKey = (int) tPos.z;

		if (m_DrawType == aswpdt_sprite)
		{
			//Render it as a normal sprite
			Vector col = UpdateColor( pParticle );
			RenderParticle_ColorSizeAngle(
				pIterator->GetParticleDraw(),
				tPos,
				col,
				//UpdateColor( pParticle ),
				UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax ),
				ASWUpdateScale( pParticle ),
				pParticle->m_flRoll
				);			
		}
		else if (m_DrawType == aswpdt_tracer)	// beam drawing
		{
			// render the particle as a tracer beam
			float fSize = ASWUpdateScale( pParticle );
			float lifePerc = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime  );
			float scale = fSize * 0.1;
			if (m_bScaleBeamByLifeLeft)
				scale *= lifePerc;

			if ( scale < 0.01f )
				scale = 0.01f;

			Vector delta;
			Vector3DMultiply( ParticleMgr()->GetModelView(), pParticle->m_vecVelocity, delta );

			float	flLength = (pParticle->m_vecVelocity * scale).Length();//( delta - pos ).Length();
			float	flWidth	 = ( flLength < fSize ) ? flLength : fSize;

			float color[4];
			Vector col = UpdateColor( pParticle );
			color[0] = col[0];
			color[1] = col[1];
			color[2] = col[2];
			color[3] = UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax );
			if (!m_bScaleBeamByVelocity)
			{
				VectorNormalize(delta);
			}
			if (m_iBeamPosition == ASW_EMITTER_BEAM_POS_FRONT)
			{
				tPos += (delta*scale)*m_fBeamLength;
			}
			else if (m_iBeamPosition == ASW_EMITTER_BEAM_POS_CENTER)
			{
				tPos += (delta*scale)*m_fBeamLength*0.5f;
			}			
			Tracer_Draw( pIterator->GetParticleDraw(), tPos, -(delta*scale)*m_fBeamLength, flWidth, color );
		}

		pParticle = (const ASWParticle *)pIterator->GetNext( sortKey );
	}
}

void CASWGenericEmitter::SetCollisionTemplate(const char* templatename )
{
	strcpy(m_szCollisionTemplateName, templatename);
	ResetEmitter();
}

void CASWGenericEmitter::SetDropletTemplate(const char* templatename )
{
	strcpy(m_szDropletTemplateName, templatename);
	ResetEmitter();
}

void CASWGenericEmitter::SetCollisionSound(const char* szSoundName )
{
	strcpy(m_szCollisionSoundName, szSoundName);
}

void CASWGenericEmitter::SetCollisionDecal(const char* szDecalName )
{
	strcpy(m_szCollisionDecalName, szDecalName);
}

void CASWGenericEmitter::SetDieTime(float fTime)
{
	m_fDieTime = fTime;
}

// load our emitter settings from a particular template
void CASWGenericEmitter::UseTemplate(const char* templatename, bool bReset, bool bLoadFromCache)
{	
	char buf[MAX_PATH];
	Q_snprintf(buf, sizeof(buf), "resource/particletemplates/%s.ptm", templatename);

	KeyValues* kv = NULL;
	if (bLoadFromCache)	
		kv = g_ASWGenericEmitterCache.FindTemplate(templatename);
	else
	{
		Msg("UseTemplate uncache force\n");
		kv = new KeyValues("UncachedParticleEmitter");
		if ( !kv->LoadFromFile(filesystem, buf, "GAME") )
		{
			DevMsg( 1, "C_ASW_Emitter::UseTemplate: couldn't load file: %s\n", buf );
			return;
		}
	}

	// couldn't find the template
	if ( !kv )
	{
		DevMsg( 1, "Couldn't load emitter template from cache. %s\n", buf );
		return;
	}

	strcpy(m_szTemplateName, templatename);
	strcpy(m_szMaterialName, kv->GetString("Material"));
	m_hMaterial = GetPMaterial( m_szMaterialName );
	strcpy(m_szGlowMaterialName, kv->GetString("GlowMaterial"));
	if (Q_strlen(m_szGlowMaterialName) <= 0)
		m_hGlowMaterial = NULL;
	else
		m_hGlowMaterial = GetPMaterial( m_szGlowMaterialName );
	m_ParticlesPerSecond = kv->GetFloat("ParticlesPerSecond");
	m_fParticleLifeMin = kv->GetFloat("ParticleLifeMin");
	m_fParticleLifeMax = kv->GetFloat("ParticleLifeMax");	
	m_fPresimulateTime = kv->GetFloat("PresimulateTime");
	fRollMin = kv->GetFloat("RollMin");
	fRollMax = kv->GetFloat("RollMax");
	fRollDeltaMin = kv->GetFloat("RollDeltaMin");
	fRollDeltaMax = kv->GetFloat("RollDeltaMax");
	velocityMin.x = kv->GetFloat("VelocityMinX");
	velocityMin.y = kv->GetFloat("VelocityMinY");
	velocityMin.z = kv->GetFloat("VelocityMinZ");
	velocityMax.x = kv->GetFloat("VelocityMaxX");
	velocityMax.y = kv->GetFloat("VelocityMaxY");
	velocityMax.z = kv->GetFloat("VelocityMaxZ");
	positionMin.x = kv->GetFloat("PositionMinX");
	positionMin.y = kv->GetFloat("PositionMinY");
	positionMin.z = kv->GetFloat("PositionMinZ");
	positionMax.x = kv->GetFloat("PositionMaxX");
	positionMax.y = kv->GetFloat("PositionMaxY");
	positionMax.z = kv->GetFloat("PositionMaxZ");
	accelerationMin.x = kv->GetFloat("AccnMinX");
	accelerationMin.y = kv->GetFloat("AccnMinY");
	accelerationMin.z = kv->GetFloat("AccnMinZ");
	accelerationMax.x = kv->GetFloat("AccnMaxX");
	accelerationMax.y = kv->GetFloat("AccnMaxY");
	accelerationMax.z = kv->GetFloat("AccnMaxZ");
	fGravity = kv->GetFloat("Gravity");
	m_fCollisionDampening = kv->GetFloat("CollisionDampening", 50.0f);
	m_fBeamLength = kv->GetFloat("BeamLength", 1.0f);
	m_bScaleBeamByVelocity = kv->GetInt("ScaleBeamByVelocity", 1) != 0;
	m_bScaleBeamByLifeLeft = kv->GetInt("ScaleBeamByLifeLeft", 1) != 0;
	m_iBeamPosition = kv->GetInt("BeamPosition");
	m_fGlowScale = kv->GetFloat("GlowScale", 1.7f);
	m_fGlowDeviation = kv->GetFloat("GlowDeviation", 5);	
	m_fDropletChance = kv->GetFloat("DropletChance");
	m_fParticleLocal = kv->GetFloat("ParticleLocal");
	m_fLifeLostOnCollision = kv->GetFloat("LifeLostOnCollision");
	m_iParticleSupply = m_iInitialParticleSupply = kv->GetInt("ParticleSupply", -1);
	m_DrawType = (ASWParticleDrawType) kv->GetInt("DrawType");
	m_iLightingType = kv->GetInt("Lighting");
	m_fLightApply = kv->GetFloat("LightApply");
	strcpy(m_szCollisionTemplateName, kv->GetString("CollisionTemplate"));
	strcpy(m_szDropletTemplateName, kv->GetString("DropletTemplate"));
	// save color nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Color%dUse", i);		m_Colors[i].bUse = kv->GetBool(buf);
		Q_snprintf(buf, 64, "Color%dTime", i);		m_Colors[i].fTime = kv->GetFloat(buf);
		Q_snprintf(buf, 64, "Color%dRed", i);		m_Colors[i].Color.r = kv->GetInt(buf);
		Q_snprintf(buf, 64, "Color%dGreen", i);		m_Colors[i].Color.g = kv->GetInt(buf);
		Q_snprintf(buf, 64, "Color%dBlue", i);		m_Colors[i].Color.b = kv->GetInt(buf);
	}
	// save scale nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Scale%dUse", i);		m_Scales[i].bUse = kv->GetBool(buf);
		Q_snprintf(buf, 64, "Scale%dTime", i);		m_Scales[i].fTime = kv->GetFloat(buf);
		Q_snprintf(buf, 64, "Scale%dValue", i);		m_Scales[i].fScale = kv->GetFloat(buf);
	}
	// save alpha nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Alpha%dUse", i);		m_Alphas[i].bUse = kv->GetBool(buf);
		Q_snprintf(buf, 64, "Alpha%dTime", i);		m_Alphas[i].fTime = kv->GetFloat(buf);
		Q_snprintf(buf, 64, "Alpha%dValue", i);		m_Alphas[i].fAlpha = kv->GetFloat(buf);
	}
	// collision
	m_UseCollision = (ASWParticleCollision) kv->GetInt("Collision");
	strcpy(m_szCollisionSoundName, kv->GetString("CollisionSound"));
	strcpy(m_szCollisionDecalName, kv->GetString("CollisionDecal"));

	Update();

	if (bReset)
		ResetEmitter();
}

void CASWGenericEmitter::SetMeshEmitter(C_ASW_Mesh_Emitter *pMeshEmitter)
{
	m_hMeshEmitter = pMeshEmitter;
	m_DrawType = aswpdt_mesh;
}


// save our emitter settings to a template
void CASWGenericEmitter::SaveTemplateAs(const char* templatename)
{
	char filename[MAX_PATH];
	Q_snprintf(filename, sizeof(filename), "resource/particletemplates/%s.ptm", templatename);

	strcpy(m_szTemplateName, templatename);

	KeyValues* kv = new KeyValues( "ParticleTemplate" );

	kv->SetString("Material", m_szMaterialName);
	kv->SetString("GlowMaterial", m_szGlowMaterialName);
	kv->SetFloat("ParticlesPerSecond", m_ParticlesPerSecond);
	kv->SetFloat("ParticleLifeMin", m_fParticleLifeMin);
	kv->SetFloat("ParticleLifeMax", m_fParticleLifeMax);	
	kv->SetFloat("PresimulateTime", m_fPresimulateTime);
	kv->SetFloat("RollMin", fRollMin);
	kv->SetFloat("RollMax", fRollMax);
	kv->SetFloat("RollDeltaMin", fRollDeltaMin);
	kv->SetFloat("RollDeltaMax", fRollDeltaMax);
	kv->SetFloat("VelocityMinX", velocityMin.x);
	kv->SetFloat("VelocityMinY", velocityMin.y);
	kv->SetFloat("VelocityMinZ", velocityMin.z);
	kv->SetFloat("VelocityMaxX", velocityMax.x);
	kv->SetFloat("VelocityMaxY", velocityMax.y);
	kv->SetFloat("VelocityMaxZ", velocityMax.z);
	kv->SetFloat("PositionMinX", positionMin.x);
	kv->SetFloat("PositionMinY", positionMin.y);
	kv->SetFloat("PositionMinZ", positionMin.z);
	kv->SetFloat("PositionMaxX", positionMax.x);
	kv->SetFloat("PositionMaxY", positionMax.y);
	kv->SetFloat("PositionMaxZ", positionMax.z);
	kv->SetFloat("AccnMinX", accelerationMin.x);
	kv->SetFloat("AccnMinY", accelerationMin.y);
	kv->SetFloat("AccnMinZ", accelerationMin.z);
	kv->SetFloat("AccnMaxX", accelerationMax.x);
	kv->SetFloat("AccnMaxY", accelerationMax.y);
	kv->SetFloat("AccnMaxZ", accelerationMax.z);
	kv->SetFloat("Gravity", fGravity);
	kv->SetFloat("CollisionDampening", m_fCollisionDampening);
	kv->SetFloat("GlowScale", m_fGlowScale);
	kv->SetFloat("GlowDeviation", m_fGlowDeviation);
	kv->SetFloat("BeamLength", m_fBeamLength);
	kv->SetBool("ScaleBeamByVelocity", m_bScaleBeamByVelocity);
	kv->SetBool("ScaleBeamByLifeLeft", m_bScaleBeamByLifeLeft);
	kv->SetInt("BeamPosition", m_iBeamPosition);		 	
	kv->SetInt("Lighting", m_iLightingType);
	kv->SetFloat("LightApply", m_fLightApply);
	kv->SetFloat("DropletChance", m_fDropletChance);
	kv->SetFloat("ParticleLocal", m_fParticleLocal);
	kv->SetFloat("LifeLostOnCollision", m_fLifeLostOnCollision);
	kv->SetInt("ParticleSupply", m_iInitialParticleSupply);
	kv->SetInt("DrawType", m_DrawType);
	kv->SetString("CollisionTemplate", m_szCollisionTemplateName);
	kv->SetString("DropletTemplate", m_szDropletTemplateName);	

	// save color nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Color%dUse", i);		kv->SetInt(buf, m_Colors[i].bUse);
		Q_snprintf(buf, 64, "Color%dTime", i);		kv->SetFloat(buf, m_Colors[i].fTime);
		Q_snprintf(buf, 64, "Color%dRed", i);		kv->SetInt(buf, m_Colors[i].Color.r);
		Q_snprintf(buf, 64, "Color%dGreen", i);		kv->SetInt(buf, m_Colors[i].Color.g);
		Q_snprintf(buf, 64, "Color%dBlue", i);		kv->SetInt(buf, m_Colors[i].Color.b);
	}
	// save scale nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Scale%dUse", i);		kv->SetInt(buf, m_Scales[i].bUse);
		Q_snprintf(buf, 64, "Scale%dTime", i);		kv->SetFloat(buf, m_Scales[i].fTime);
		Q_snprintf(buf, 64, "Scale%dValue", i);		kv->SetFloat(buf, m_Scales[i].fScale);
	}
	// save alpha nodes
	for (int i=0;i<5;i++)
	{
		char buf[64];
		Q_snprintf(buf, 64, "Alpha%dUse", i);		kv->SetInt(buf, m_Alphas[i].bUse);
		Q_snprintf(buf, 64, "Alpha%dTime", i);		kv->SetFloat(buf, m_Alphas[i].fTime);
		Q_snprintf(buf, 64, "Alpha%dValue", i);		kv->SetFloat(buf, m_Alphas[i].fAlpha);
	}
	// collision
	kv->SetInt("Collision", m_UseCollision);
	kv->SetString("CollisionSound", m_szCollisionSoundName);	
	kv->SetString("CollisionDecal", m_szCollisionDecalName);	

	// save the template
	kv->SaveToFile( filesystem, filename );	
}

KeyValues* CASWGenericEmitterCache::FindTemplate(const char* szTemplateName)
{	
	//Msg("CASWGenericEmitterCache::FindTemplate %s\n", szTemplateName);
	// check if it's in our cache of templates already

	int k = m_TemplateNames.Count();
	for (int i=0;i<k;i++)
	{
		if (!Q_strcmp(m_TemplateNames[i], szTemplateName))
		{
			//Msg("  cache hit!\n");
			return m_Templates[i];
		}
	}

	// otherwise, load it;
	KeyValues* kv = new KeyValues( "ParticleEmitters" );
	char buf[MAX_PATH];
	Q_snprintf(buf, sizeof(buf), "resource/particletemplates/%s.ptm", szTemplateName);
	if (!kv->LoadFromFile( filesystem, buf, "GAME" ))
	{
		Warning("Failed to load particle emitter: %s\n", szTemplateName);
		return NULL;
	}
	// add it to the cache
	m_Templates.AddToTail(kv);

	int length = Q_strlen(szTemplateName)+1;
	char *pNewString = new char[length];
	Q_memcpy( pNewString, szTemplateName, length );
	m_TemplateNames.AddToTail(pNewString);	

	//Msg("  cache miss (now have %d templates loaded)\n", m_Templates.Count());

	return kv;
}

CASWGenericEmitterCache::~CASWGenericEmitterCache()
{
	// free up all the keyvalues we allocated
	int k=m_Templates.Count();
	for (int i=0;i<k;i++)
	{
		KeyValues* kv = m_Templates[i];
		kv->deleteThis();
		// also free the template name strings we allocated
		delete[] m_TemplateNames[i];
	}	
}

void CASWGenericEmitterCache::ListCachedEmitters()
{
	int k=m_Templates.Count();
	for (int i=0;i<k;i++)
	{
		KeyValues* kv = m_Templates[i];
		Msg("Template %d = %s (addr:%d)\n", i, STRING(m_TemplateNames[i]), kv);
	}
}

void CASWGenericEmitterCache::PrecacheTemplates()
{
	// precache our standard templates
	FindTemplate("snow2");
	FindTemplate("snow3");
	FindTemplate("snowclouds");
	FindTemplate("sollyopenfire");
	FindTemplate("radjet1");
	FindTemplate("radcloud1");
	FindTemplate("envfiresol");
	FindTemplate("envfireslow");
	FindTemplate("envfire3");
	FindTemplate("incendiary");
	FindTemplate("flamer5");
	FindTemplate("flamerstream1");
	FindTemplate("fireextinguisher2");
	FindTemplate("healfast");
	FindTemplate("doorsmoke1");
	FindTemplate("fireextinguisherdisperse1");
	FindTemplate("flamerdroplets1");
	FindTemplate("dronebloodjet");
	FindTemplate("dronebloodburst");
	FindTemplate("droneblooddroplets");
	FindTemplate("piercespark");
	FindTemplate("fireextinguisherself");
	FindTemplate("railgunsmoke");
	FindTemplate("railgunspray");
	FindTemplate("railgunspray");
	FindTemplate("fireburst");
	FindTemplate("incendiarycloud1");
	FindTemplate("shotgunsmoke");
	FindTemplate("queendie");
	FindTemplate("queenburst");
	FindTemplate("queenspit");
	FindTemplate("dronegib1");
	FindTemplate("dronegib2");
	FindTemplate("dronegib3");
	FindTemplate("dronegibfire1");
	FindTemplate("eggfluid");
	FindTemplate("egggib1");
	FindTemplate("fireextinguisherdisperse");
	FindTemplate("flamerdroplets1");
	FindTemplate("flamersparks1");
	FindTemplate("grubgib1");
	FindTemplate("grubgibfire1");
	FindTemplate("parasitegib1");
	FindTemplate("parasitegib1quiet");
	FindTemplate("parasitegib2");
	FindTemplate("parasitegib2quiet");
	FindTemplate("parasitegibfire1");
	FindTemplate("parasitegibfire1quiet");
}

CASWGenericEmitterCache g_ASWGenericEmitterCache;



void asw_list_cached_emitters_f()
{	
	g_ASWGenericEmitterCache.ListCachedEmitters();	
}

static ConCommand asw_list_cached_emitters("asw_list_cached_emitters", asw_list_cached_emitters_f, "Lists all emitter templates currently loaded", FCVAR_CHEAT);