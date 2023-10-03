//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "particle_property.h"
#include "utlvector.h"

#ifdef CLIENT_DLL

#include "c_baseentity.h"
#include "c_baseanimating.h"
#include "recvproxy.h"
#include "particles_new.h"
#include "engine/ivdebugoverlay.h"

#else

#include "baseentity.h"
#include "baseanimating.h"
#include "sendproxy.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
BEGIN_DATADESC_NO_BASE( CParticleProperty )
	//		DEFINE_FIELD( m_pOuter, FIELD_CLASSPTR ),
END_DATADESC()

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
BEGIN_PREDICTION_DATA_NO_BASE( CParticleProperty )
	//DEFINE_PRED_FIELD( m_vecMins, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CParticleProperty, DT_ParticleProperty )
#ifdef CLIENT_DLL
//RecvPropVector( RECVINFO(m_vecMins), 0, RecvProxy_OBBMins ),
#else
//SendPropVector( SENDINFO(m_vecMins), 0, SPROP_NOSCALE),
#endif
END_NETWORK_TABLE()


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CParticleProperty::CParticleProperty()
{
	Init( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CParticleProperty::~CParticleProperty()
{
	// We're being removed. Call StopEmission() on any particle system
	// that has an unlimited number of particles to emit.
	StopEmission( NULL, false, true, false, true );
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CParticleProperty::Init( CBaseEntity *pEntity )
{
	m_pOuter = pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CParticleProperty::GetParticleAttachment( C_BaseEntity *pEntity, const char *pszAttachmentName, const char *pszParticleName )
{
	Assert( pEntity && pEntity->GetBaseAnimating() );
	if ( !pEntity || !pEntity->GetBaseAnimating() )
		return -1;

	// Find the attachment point index
	int iAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
	if ( iAttachment == -1 )
	{
		Warning("Model '%s' doesn't have attachment '%s' to attach particle system '%s' to.\n", STRING(pEntity->GetBaseAnimating()->GetModelName()), pszAttachmentName, pszParticleName );
	}

	return iAttachment;
}

//-----------------------------------------------------------------------------
// Purpose: Get's a list of all renderables used for this particle property
//-----------------------------------------------------------------------------
int CParticleProperty::GetAllParticleEffectRenderables( IClientRenderable **pOutput, int iMaxOutput )
{
	if( iMaxOutput == 0 )
		return 0;

	int iReturnedRenderables = 0;
	int iParticleEffectListCount = m_ParticleEffects.Count();

	for( int i = 0; i != iParticleEffectListCount; ++i )
	{
		if( m_ParticleEffects[i].pParticleEffect.IsValid() )
		{
			pOutput[iReturnedRenderables++] = m_ParticleEffects[i].pParticleEffect.GetObject();
			if( iReturnedRenderables == iMaxOutput )
				break;
		}
	}

	return iReturnedRenderables;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new particle system and attach it to our owner
//-----------------------------------------------------------------------------
CNewParticleEffect *CParticleProperty::Create( const char *pszParticleName, ParticleAttachment_t iAttachType, const char *pszAttachmentName )
{
	int iAttachment = GetParticleAttachment( GetOuter(), pszAttachmentName, pszParticleName );
	if ( iAttachment == -1 )
		return NULL;

	// Create the system
	return Create( pszParticleName, iAttachType, iAttachment );
}
	  
//-----------------------------------------------------------------------------
// Purpose: Create a new particle system and attach it to our owner
//-----------------------------------------------------------------------------
static ConVar cl_particle_batch_mode( "cl_particle_batch_mode", "1" );

CNewParticleEffect *CParticleProperty::Create( CParticleSystemDefinition *pDef, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	int nBatchMode = cl_particle_batch_mode.GetInt();
	bool bRequestedBatch = ( nBatchMode == 2 ) || ( ( nBatchMode == 1 ) && pDef && pDef->ShouldBatch() ); 
	if ( ( iAttachType == PATTACH_CUSTOMORIGIN ) && bRequestedBatch )
	{
		int iIndex = FindEffect( pDef->GetName() );
		if ( iIndex >= 0 )
		{
			CNewParticleEffect *pEffect = m_ParticleEffects[iIndex].pParticleEffect.GetObject();
			pEffect->Restart();
			return pEffect;
		}
	}

	int iIndex = m_ParticleEffects.AddToTail();
	ParticleEffectList_t *newEffect = &m_ParticleEffects[iIndex];
	newEffect->pParticleEffect = CNewParticleEffect::Create( m_pOuter, pDef, pDef->GetName() );

	if ( !newEffect->pParticleEffect->IsValid() )
	{
		// Caused by trying to spawn an unregistered particle effect. Remove it.
		ParticleMgr()->RemoveEffect( newEffect->pParticleEffect.GetObject() );
		return NULL;
	}

	AddControlPoint( iIndex, 0, GetOuter(), iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );

	if ( m_pOuter )
	{
		m_pOuter->OnNewParticleEffect( pDef->GetName(), newEffect->pParticleEffect.GetObject() );
	}

	return newEffect->pParticleEffect.GetObject();
}

CNewParticleEffect *CParticleProperty::CreatePrecached( int nPrecacheIndex, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( nPrecacheIndex );
	if ( !pDef )
	{
		AssertMsg( 0, "Attempting to create unknown particle system" );
		return NULL;
	}
	return Create( pDef, iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );
}

CNewParticleEffect *CParticleProperty::Create( const char *pszParticleName, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pszParticleName );
	if ( !pDef )
	{
		AssertMsg( 0, "Attempting to create unknown particle system" );
		Warning( "Attempting to create unknown particle system '%s' \n", pszParticleName );
		return NULL;
	}
	return Create( pDef, iAttachType, iAttachmentPoint, vecOriginOffset, matOffset );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::AddControlPoint( CNewParticleEffect *pEffect, int iPoint, C_BaseEntity *pEntity, ParticleAttachment_t iAttachType, const char *pszAttachmentName, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	int iAttachment = -1;
	if ( pszAttachmentName )
	{
		iAttachment = GetParticleAttachment( pEntity, pszAttachmentName, pEffect->GetEffectName() );
	}

	for ( int i = 0; i < m_ParticleEffects.Count(); i++ )
	{
		if ( m_ParticleEffects[i].pParticleEffect == pEffect )
		{
			AddControlPoint( i, iPoint, pEntity, iAttachType, iAttachment, vecOriginOffset, matOffset );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::AddControlPoint( int iEffectIndex, int iPoint, C_BaseEntity *pEntity, ParticleAttachment_t iAttachType, int iAttachmentPoint, Vector vecOriginOffset, matrix3x4_t *matOffset )
{
	Assert( iEffectIndex >= 0 && iEffectIndex < m_ParticleEffects.Count() );
	ParticleEffectList_t *pEffect = &m_ParticleEffects[iEffectIndex];
	Assert( pEffect->pControlPoints.Count() < MAX_PARTICLE_CONTROL_POINTS );

	int iIndex = pEffect->pControlPoints.AddToTail();
	ParticleControlPoint_t *pNewPoint = &pEffect->pControlPoints[iIndex];
	pNewPoint->iControlPoint = iPoint;
	pNewPoint->hEntity = pEntity;
	pNewPoint->iAttachType = iAttachType;
	pNewPoint->iAttachmentPoint = iAttachmentPoint;
	pNewPoint->vecOriginOffset = vecOriginOffset;
	if ( matOffset )
		pNewPoint->matOffset = *matOffset;
	else
		pNewPoint->matOffset.Init( Vector(1,0,0), Vector(0,1,0), Vector(0,0,1), Vector(0,0,0) );

	UpdateParticleEffect( pEffect, true, iIndex );
}


//-----------------------------------------------------------------------------
// Used to replace a particle effect with a different one; attaches the control point updating to the new one
//-----------------------------------------------------------------------------
void CParticleProperty::ReplaceParticleEffect( CNewParticleEffect *pOldEffect, CNewParticleEffect *pNewEffect )
{
	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pOldEffect != m_ParticleEffects[i].pParticleEffect.GetObject() )
			continue;

		m_ParticleEffects[i].pParticleEffect = pNewEffect;
		UpdateParticleEffect( &m_ParticleEffects[i], true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the parent of a given control point to the index of some other
//			control point.
//-----------------------------------------------------------------------------
void CParticleProperty::SetControlPointParent( int iEffectIndex, int whichControlPoint, int parentIdx )
{

}

//-----------------------------------------------------------------------------
// Purpose: Stop effects from emitting more particles. If no effect is 
//			specified, all effects attached to this entity are stopped.
//-----------------------------------------------------------------------------
void CParticleProperty::StopEmission( CNewParticleEffect *pEffect, bool bWakeOnStop, bool bDestroyAsleepSystems, bool bForceRemoveInstantly, bool bPlayEndCap )
{
	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);

	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	if ( pEffect )
	{
		if ( FindEffect( pEffect ) != -1 )
		{
			pEffect->StopEmission( false, bRemoveInstantly, bWakeOnStop, bPlayEndCap );
		}
	}
	else
	{
		// Stop all effects
		float flNow = g_pParticleSystemMgr->GetLastSimulationTime();
		int nCount = m_ParticleEffects.Count();
		for ( int i = nCount-1; i >= 0; i-- )
		{
			CNewParticleEffect *pTmp = m_ParticleEffects[i].pParticleEffect.GetObject();
			bool bRemoveSystem = bRemoveInstantly || ( bDestroyAsleepSystems && ( flNow >= pTmp->m_flNextSleepTime ) );
			if ( bRemoveSystem )
			{
				m_ParticleEffects.Remove( i );
				pTmp->SetOwner( NULL );
			}
			pTmp->StopEmission( false, bRemoveSystem, !bRemoveSystem && bWakeOnStop, bPlayEndCap );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove effects immediately, including all current particles. If no
// effect is specified, all effects attached to this entity are removed.
//-----------------------------------------------------------------------------
void CParticleProperty::StopEmissionAndDestroyImmediately( CNewParticleEffect *pEffect )
{
	if ( pEffect )
	{
		int iIndex = FindEffect( pEffect );
		Assert( iIndex != -1 );
		if ( iIndex != -1 )
		{
			m_ParticleEffects.Remove( iIndex );

			// Clear the owner so it doesn't try to call back to us on deletion
			pEffect->SetOwner( NULL );
			pEffect->StopEmission( false, true );
		}
	}
	else
	{
		// Immediately destroy all effects
		int nCount = m_ParticleEffects.Count();
		for ( int i = nCount-1; i >= 0; i-- )
		{
			CNewParticleEffect *pTmp = m_ParticleEffects[i].pParticleEffect.GetObject();
			m_ParticleEffects.Remove( i );

			// Clear the owner so it doesn't try to call back to us on deletion
			pTmp->SetOwner( NULL );
			pTmp->StopEmission( false, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop all effects that have  a control point associated with the given
//          entity.
//-----------------------------------------------------------------------------
void CParticleProperty::StopParticlesInvolving( CBaseEntity *pEntity, bool bForceRemoveInstantly /* =false */ )
{
	Assert( pEntity );

	EHANDLE entHandle(pEntity);

	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);
	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		ParticleEffectList_t &part = m_ParticleEffects[i];
		// look through all the control points to see if any mention the given object
		int cpCount = part.pControlPoints.Count();
		for (int j = 0; j < cpCount ; ++j )
		{
			// if any control points respond to the given handle...
			if (part.pControlPoints[j].hEntity == entHandle)
			{
				part.pParticleEffect->StopEmission( false, bRemoveInstantly );
				break; // break out of the inner loop (to where it says BREAK TO HERE)
			}
		}
		// BREAK TO HERE
	}
}


//-----------------------------------------------------------------------------
// Purpose: Stop all effects that were created using the given definition
//			name.
//-----------------------------------------------------------------------------
void CParticleProperty::StopParticlesNamed( const char *pszEffectName, bool bForceRemoveInstantly /* =false */, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pszEffectName );
	AssertMsg1(pDef, "Could not find particle definition %s", pszEffectName );
	if (!pDef)
		return;


	// If we return from dormancy and are then told to stop emitting,
	// we should have died while dormant. Remove ourselves immediately.
	bool bRemoveInstantly = (m_iDormancyChangedAtFrame == gpGlobals->framecount);
	// force remove particles instantly if caller specified
	bRemoveInstantly |= bForceRemoveInstantly;

	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		CNewParticleEffect *pParticleEffect = m_ParticleEffects[i].pParticleEffect.GetObject();
		if (pParticleEffect->m_pDef() == pDef)
		{
			if ( nSplitScreenPlayerSlot != -1 )
			{
				if ( !pParticleEffect->ShouldDrawForSplitScreenUser( nSplitScreenPlayerSlot ) )
					continue;
			}
			pParticleEffect->StopEmission( false, bRemoveInstantly );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::OnParticleSystemUpdated( CNewParticleEffect *pEffect, float flTimeDelta )
{
	int iIndex = FindEffect( pEffect );
	Assert( iIndex != -1 );
	if ( iIndex == -1 )
		return;

	UpdateParticleEffect( &m_ParticleEffects[iIndex] );

	/*
	// Display the bounding box of the particle effect
	Vector vecMins, vecMaxs;
	pEffect->GetRenderBounds( vecMins, vecMaxs );
	debugoverlay->AddBoxOverlay( pEffect->GetRenderOrigin(), vecMins, vecMaxs, QAngle( 0, 0, 0 ), 0, 255, 255, 0, 0 );
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::OnParticleSystemDeleted( CNewParticleEffect *pEffect )
{
	int iIndex = FindEffect( pEffect );
	if ( iIndex == -1 )
		return;

	if ( m_pOuter )
	{
		m_pOuter->OnParticleEffectDeleted( pEffect );
	}

	m_ParticleEffects[iIndex].pParticleEffect.MarkDeleted();
	m_ParticleEffects.Remove( iIndex );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: The entity we're attached to has change dormancy state on our client
//-----------------------------------------------------------------------------
void CParticleProperty::OwnerSetDormantTo( bool bDormant )
{
	m_iDormancyChangedAtFrame = gpGlobals->framecount;

	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; i++ )
	{
		//m_ParticleEffects[i].pParticleEffect->SetShouldSimulate( !bDormant );
		m_ParticleEffects[i].pParticleEffect->SetDormant( bDormant );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CParticleProperty::FindEffect( CNewParticleEffect *pEffect )
{
	for ( int i = 0; i < m_ParticleEffects.Count(); i++ )
	{
		if ( m_ParticleEffects[i].pParticleEffect == pEffect )
			return i;
	}

	return -1;
}

int CParticleProperty::FindEffect( const char *pEffectName )
{
	for ( int i = 0; i < m_ParticleEffects.Count(); i++ )
	{
		if ( !Q_stricmp( m_ParticleEffects[i].pParticleEffect->GetName(), pEffectName ) )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::UpdateParticleEffect( ParticleEffectList_t *pEffect, bool bInitializing, int iOnlyThisControlPoint )
{
	if ( iOnlyThisControlPoint != -1 )
	{
		UpdateControlPoint( pEffect, iOnlyThisControlPoint, bInitializing );
		return;
	}

	// Loop through our control points and update them all
	for ( int i = 0; i < pEffect->pControlPoints.Count(); i++ )
	{
		UpdateControlPoint( pEffect, i, bInitializing );
	}
}

extern void FormatViewModelAttachment( C_BasePlayer *pPlayer, Vector &vOrigin, bool bInverse );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleProperty::UpdateControlPoint( ParticleEffectList_t *pEffect, int iPoint, bool bInitializing )
{

	ParticleControlPoint_t *pPoint = &pEffect->pControlPoints[iPoint];

	if ( pEffect->pParticleEffect->m_pDef->IsScreenSpaceEffect() && iPoint == 0 )
	{
		pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );
		pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, vec3_origin );
		return;
	}

	if ( !pPoint->hEntity.Get() )
	{
		if ( pPoint->iAttachType == PATTACH_WORLDORIGIN && bInitializing )
		{
			pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );
			pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, pPoint->vecOriginOffset );
			pEffect->pParticleEffect->SetSortOrigin( pPoint->vecOriginOffset );
		}

		pEffect->pParticleEffect->SetControlPointEntity( pPoint->iControlPoint, NULL );
		return;
	}

	// Only update non-follow particles when we're initializing, 
	if ( !bInitializing && (pPoint->iAttachType == PATTACH_ABSORIGIN || pPoint->iAttachType == PATTACH_POINT ) )
		return;

	if ( pPoint->iAttachType == PATTACH_CUSTOMORIGIN )
		return;

	Vector vecOrigin, vecForward, vecRight, vecUp;

	switch ( pPoint->iAttachType )
	{
	case PATTACH_POINT:
	case PATTACH_POINT_FOLLOW:
		{
			C_BaseAnimating *pAnimating = pPoint->hEntity->GetBaseAnimating();

			bool bValid = false;
			Assert( pAnimating );
			if ( pAnimating )
			{
				matrix3x4_t attachmentToWorld;

				if ( pAnimating->IsViewModel() )
				{
					C_BasePlayer *pPlayer = ToBasePlayer( ((C_BaseViewModel *)pAnimating)->GetOwner() );
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

					if ( pAnimating->GetAttachment( pPoint->iAttachmentPoint, attachmentToWorld ) )
					{
						bValid = true;
						MatrixVectors( attachmentToWorld, &vecForward, &vecRight, &vecUp );
						MatrixPosition( attachmentToWorld, vecOrigin );

						if ( pEffect->pParticleEffect->m_pDef->IsViewModelEffect() )
						{
							FormatViewModelAttachment( pPlayer, vecOrigin, true );
						}
					}
				}
				else
				{
					// HACK_GETLOCALPLAYER_GUARD( "CParticleProperty::UpdateControlPoint" );

					if ( pAnimating->GetAttachment( pPoint->iAttachmentPoint, attachmentToWorld ) )
					{
						bValid = true;
						MatrixVectors( attachmentToWorld, &vecForward, &vecRight, &vecUp );
#ifdef _DEBUG
						float flTests[3] = {vecForward.Dot( vecRight ), vecRight.Dot( vecUp ), vecUp.Dot( vecForward )};
						static float s_flMaxTest = 0.001f;
						Assert( fabs( flTests[0] ) + fabs( flTests[1] ) + fabs( flTests[2] ) < s_flMaxTest );
#endif
						MatrixPosition( attachmentToWorld, vecOrigin );

						if ( pEffect->pParticleEffect->m_pDef->IsViewModelEffect() )
						{
							HACK_GETLOCALPLAYER_GUARD( "CParticleProperty::UpdateControlPoint" );

							FormatViewModelAttachment( NULL, vecOrigin, true );
						}
					}
				}
			}

			if ( !bValid )
			{
				static bool bWarned = false;
				if ( !bWarned )
				{
					bWarned = true;
					DevWarning( "Attempted to attach particle effect %s to an unknown attachment on entity %s\n",
						pEffect->pParticleEffect->m_pDef->GetName(), pAnimating->GetClassname() );
				}
			}
			if ( !bValid )
			{
				AssertOnce( 0 );
				return;
			}
		}
		break;

	case PATTACH_ABSORIGIN:
	case PATTACH_ABSORIGIN_FOLLOW:
	default:
		{
			vecOrigin = pPoint->hEntity->GetAbsOrigin() + pPoint->vecOriginOffset;
			pPoint->hEntity->GetVectors( &vecForward, &vecRight, &vecUp );
		}
		break;

	case PATTACH_EYES_FOLLOW:
		{
			C_BaseEntity *pEnt = pPoint->hEntity;

			if ( !pEnt->IsPlayer() )
				return;

			C_BasePlayer *pPlayer = assert_cast< C_BasePlayer* >( pEnt );

			bool bValid = false;
			Assert( pPlayer );
			if ( pPlayer )
			{
				bValid = true;
				vecOrigin = pPlayer->EyePosition() + pPoint->vecOriginOffset;
				pPlayer->EyeVectors( &vecForward, &vecRight, &vecUp );
			}
			if ( !bValid )
			{
				AssertOnce( 0 );
				return;
			}
		}
		break;

	case PATTACH_CUSTOMORIGIN_FOLLOW:
		{
			matrix3x4_t mat;
			MatrixMultiply( pPoint->hEntity->RenderableToWorldTransform(), pPoint->matOffset, mat );
			MatrixVectors( mat, &vecForward, &vecRight, &vecUp );
			vecOrigin = pPoint->hEntity->GetAbsOrigin() + pPoint->vecOriginOffset;
		}
		break;
	}
	pEffect->pParticleEffect->SetControlPointOrientation( pPoint->iControlPoint, vecForward, vecRight, vecUp );
	pEffect->pParticleEffect->SetControlPointEntity( pPoint->iControlPoint, pPoint->hEntity );
	pEffect->pParticleEffect->SetControlPoint( pPoint->iControlPoint, vecOrigin );
	pEffect->pParticleEffect->SetSortOrigin( vecOrigin );
}

//-----------------------------------------------------------------------------
// Purpose: Output all active effects
//-----------------------------------------------------------------------------
void CParticleProperty::DebugPrintEffects( void )
{
	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// for each effect...
		CNewParticleEffect *pParticleEffect = m_ParticleEffects[i].pParticleEffect.GetObject();

		if ( !pParticleEffect )
			continue;
	
		Msg( "(%d)  EffectName \"%s\"  Dormant? %s  Emission Stopped? %s \n",
			i,
			pParticleEffect->GetEffectName(),
			( pParticleEffect->m_bDormant ) ? "yes" : "no",
			( pParticleEffect->m_bEmissionStopped ) ? "yes" : "no" );
	}
}

bool CParticleProperty::IsValidEffect( const CNewParticleEffect *pEffect )
{
	if( pEffect == NULL )
		return false;

	int nCount = m_ParticleEffects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if( pEffect == m_ParticleEffects[i].pParticleEffect.GetObject() )
			return true;
	}

	return false;
}
