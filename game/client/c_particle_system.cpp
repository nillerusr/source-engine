//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "particles/particles.h"
#include "c_te_effect_dispatch.h"
#include "particles_new.h"
#include "networkstringtable_clientdll.h"
#include "tier0/vprof.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
class C_ParticleSystem : public C_BaseEntity
{
	DECLARE_CLASS( C_ParticleSystem, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_ParticleSystem( void );

	virtual void PreDataUpdate( DataUpdateType_t updateType );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void ClientThink( void );

protected:
	C_ParticleSystem::~C_ParticleSystem( void );

	int			m_iEffectIndex;
	int			m_nStopType;
	bool		m_bActive;
	bool		m_bOldActive;
	float		m_flStartTime;	// Time at which the effect started
	char		m_szSnapshotFileName[ MAX_PATH ];

	//server controlled control points (variables in particle effects instead of literal follow points)
	Vector		m_vServerControlPoints[4];
	uint8		m_iServerControlPointAssignments[4];

	CUtlReference< CNewParticleEffect > m_pEffect;
	CParticleSnapshot *m_pSnapshot;

	enum { kMAXCONTROLPOINTS = 63 }; ///< actually one less than the total number of cpoints since 0 is assumed to be me

	// stop types
	enum 
	{
		STOP_NORMAL = 0,
		STOP_DESTROY_IMMEDIATELY,
		STOP_PLAY_ENDCAP,
		NUM_STOP_TYPES
	};
	
	EHANDLE		m_hControlPointEnts[kMAXCONTROLPOINTS];
	//	SendPropArray3( SENDINFO_ARRAY3(m_iControlPointParents), SendPropInt( SENDINFO_ARRAY(m_iControlPointParents), 3, SPROP_UNSIGNED ) ),
	unsigned char m_iControlPointParents[kMAXCONTROLPOINTS];
};

IMPLEMENT_CLIENTCLASS(C_ParticleSystem, DT_ParticleSystem, CParticleSystem);

BEGIN_RECV_TABLE_NOBASE( C_ParticleSystem, DT_ParticleSystem )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropEHandle( RECVINFO(m_hOwnerEntity) ),
	RecvPropInt( RECVINFO_NAME(m_hNetworkMoveParent, moveparent), 0, RecvProxy_IntToMoveParent ),
	RecvPropInt( RECVINFO( m_iParentAttachment ) ),
	RecvPropQAngles( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),

	RecvPropInt( RECVINFO( m_iEffectIndex ) ),
	RecvPropBool( RECVINFO( m_bActive ) ),
	RecvPropInt( RECVINFO( m_nStopType ) ),
	RecvPropFloat( RECVINFO( m_flStartTime ) ),
	RecvPropString( RECVINFO( m_szSnapshotFileName ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_vServerControlPoints), RecvPropVector( RECVINFO( m_vServerControlPoints[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iServerControlPointAssignments), RecvPropInt( RECVINFO(m_iServerControlPointAssignments[0]))), 

	RecvPropArray3( RECVINFO_ARRAY(m_hControlPointEnts), RecvPropEHandle( RECVINFO( m_hControlPointEnts[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_iControlPointParents), RecvPropInt( RECVINFO(m_iControlPointParents[0]))), 
END_RECV_TABLE();

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ParticleSystem::C_ParticleSystem( void )
 :	m_pSnapshot( NULL )
{
	memset( m_szSnapshotFileName, 0, sizeof( m_szSnapshotFileName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ParticleSystem::~C_ParticleSystem( void )
{
	if ( m_pSnapshot )
	{
		delete m_pSnapshot;
		m_pSnapshot = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ParticleSystem::PreDataUpdate( DataUpdateType_t updateType )
{
	m_bOldActive = m_bActive;

	BaseClass::PreDataUpdate( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ParticleSystem::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// Always restart if just created and updated
	// FIXME: Does this play fairly with PVS?
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// TODO: !!HACK HACK HACK!! .PSF files should be loaded/refcounted through the CParticleSystemMgr (ala .PCFs).
		//       The current code will duplicate a given .PSF file in memory for every info_particle_system that uses it!
		if ( m_szSnapshotFileName[0] )
		{
			m_pSnapshot = new CParticleSnapshot();
			if ( !m_pSnapshot->Unserialize( CFmtStr( "particles/%s.psf", m_szSnapshotFileName ) ) )
			{
				delete m_pSnapshot;
				m_pSnapshot = NULL;
			}
		}

		if ( m_bActive )
		{
			// Delayed here so that we don't get invalid abs queries on level init with active particle systems
			SetNextClientThink( gpGlobals->curtime );
		}
	}
	else
	{
		if ( m_bOldActive != m_bActive )
		{
			if ( m_bActive )
			{
				// Delayed here so that we don't get invalid abs queries on level init with active particle systems
				SetNextClientThink( gpGlobals->curtime );
			}
			else
			{
				switch( m_nStopType )
				{
				case STOP_NORMAL:
					{
						ParticleProp()->StopEmission();
					}
					break;
				case STOP_DESTROY_IMMEDIATELY:
					{
						ParticleProp()->StopEmissionAndDestroyImmediately();
					}
					break;
				case STOP_PLAY_ENDCAP:
					{
						ParticleProp()->StopEmission( NULL, false, false, false, true);
					}
					break;
				}
			}
		}

		if( m_bActive && ParticleProp()->IsValidEffect( m_pEffect ) )
		{
			//server controlled control points (variables in particle effects instead of literal follow points)
			for( int i = 0; i != ARRAYSIZE( m_iServerControlPointAssignments ); ++i )
			{
				if( m_iServerControlPointAssignments[i] != 255 )
				{
					m_pEffect->SetControlPoint( m_iServerControlPointAssignments[i], m_vServerControlPoints[i] );
				}
				else
				{
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ParticleSystem::ClientThink( void )
{
	if ( m_bActive )
	{
		const char *pszName = GetParticleSystemNameFromIndex( m_iEffectIndex );
		if ( pszName && pszName[0] )
		{
			CNewParticleEffect *pEffect = ParticleProp()->Create( pszName, PATTACH_ABSORIGIN_FOLLOW );
			m_pEffect = pEffect;
	
			if (pEffect)
			{
				for ( int i = 0 ; i < kMAXCONTROLPOINTS ; ++i )
				{
					CBaseEntity *pOnEntity = m_hControlPointEnts[i].Get();
					if ( pOnEntity )
					{
						ParticleProp()->AddControlPoint( pEffect, i + 1, pOnEntity, PATTACH_ABSORIGIN_FOLLOW );
					}

					AssertMsg2( m_iControlPointParents[i] >= 0 && m_iControlPointParents[i] <= kMAXCONTROLPOINTS ,
						"Particle system specified bogus control point parent (%d) for point %d.",
						m_iControlPointParents[i], i );

					if (m_iControlPointParents[i] != 0)
					{
						pEffect->SetControlPointParent(i+1, m_iControlPointParents[i]);
					}
				}

				//server controlled control points (variables in particle effects instead of literal follow points)
				for( int i = 0; i != ARRAYSIZE( m_iServerControlPointAssignments ); ++i )
				{
					if( m_iServerControlPointAssignments[i] != 255 )
					{
						pEffect->SetControlPoint( m_iServerControlPointAssignments[i], m_vServerControlPoints[i] );
					}
					else
					{
						break;
					}
				}

				// Attach our particle snapshot if we have one
				Assert( m_pSnapshot || !m_szSnapshotFileName[0] ); // m_szSnapshotFileName shouldn't change after the create update
				if ( m_pSnapshot )
				{
					pEffect->SetControlPointSnapshot( 0, m_pSnapshot );
				}

				// NOTE: What we really want here is to compare our lifetime and that of our children and see if this delta is
				//		 already past the end of it, denoting that we're finished.  In that case, just destroy us and be done. -- jdw

				// TODO: This can go when the SkipToTime code below goes
				ParticleProp()->OnParticleSystemUpdated( pEffect, 0.0f );

				// Skip the effect ahead if we're restarting it
				float flTimeDelta = gpGlobals->curtime - m_flStartTime;
				if ( flTimeDelta > 0.01f )
				{
					VPROF_BUDGET( "C_ParticleSystem::ClientThink SkipToTime", "Particle Simulation" );
					pEffect->SkipToTime( flTimeDelta );
				}
			}
		}
	}
}


//======================================================================================================================
// PARTICLE SYSTEM DISPATCH EFFECT
//======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot /*= -1*/ )
{
	// this needs to be before using data.m_nHitBox, 
	// since that may be a serialized value that's past the end of the current particle system string table
	if ( SuppressingParticleEffects() )
		return; 

	// Don't crash if we're passed an invalid particle system
	if ( data.m_nHitBox == 0 )
		return;

	if ( data.m_fFlags & PARTICLE_DISPATCH_FROM_ENTITY )
	{
		if ( data.m_hEntity.Get() )
		{
			C_BaseEntity *pEnt = C_BaseEntity::Instance( data.m_hEntity );
			// commented out assert. dormant entities have their particle system spawns stopped.
			//Assert( pEnt && !pEnt->IsDormant() );
			if ( pEnt && !pEnt->IsDormant() )
			{
				if ( data.m_fFlags & PARTICLE_DISPATCH_RESET_PARTICLES )
				{
					pEnt->ParticleProp()->StopEmission();
				}

				CUtlReference<CNewParticleEffect> pEffect = pEnt->ParticleProp()->CreatePrecached( data.m_nHitBox, (ParticleAttachment_t)data.m_nDamageType, data.m_nAttachmentIndex );

				if ( pEffect.IsValid() && pEffect->IsValid() )
				{
					if ( (ParticleAttachment_t)data.m_nDamageType == PATTACH_CUSTOMORIGIN || (ParticleAttachment_t)data.m_nDamageType == PATTACH_CUSTOMORIGIN_FOLLOW )
					{
						pEffect->SetDrawOnlyForSplitScreenUser( nSplitScreenPlayerSlot );
						pEffect->SetSortOrigin( data.m_vOrigin );
						if ( (ParticleAttachment_t)data.m_nDamageType == PATTACH_CUSTOMORIGIN_FOLLOW )
						{
							Vector vecCtrl1 = (data.m_vStart - pEnt->GetAbsOrigin() );
							pEffect->SetControlPoint( 1, vecCtrl1 );
							pEffect->SetControlPointEntity( 1, pEnt );
							Vector vecCtrl0 = (data.m_vOrigin - pEnt->GetAbsOrigin() );
							matrix3x4_t mat;
							AngleMatrix( data.m_vAngles, mat );
							pEnt->ParticleProp()->AddControlPoint( pEffect, 0, pEnt, PATTACH_CUSTOMORIGIN_FOLLOW, NULL, vecCtrl0, &mat );
						}
						else
						{
							pEffect->SetControlPoint( 0, data.m_vOrigin );
							pEffect->SetControlPoint( 1, data.m_vStart );
							Vector vecForward, vecRight, vecUp;
							AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
							pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
						}
					}
					else if ( data.m_nOtherEntIndex > 0 )
					{
						C_BaseEntity *pOtherEnt = ClientEntityList().GetEnt( data.m_nOtherEntIndex );
					
						if ( pOtherEnt )
						{
							pEnt->ParticleProp()->AddControlPoint( pEffect, 1, pOtherEnt, PATTACH_ABSORIGIN_FOLLOW, NULL, Vector( 0, 0, 50 ) );
						}
					}
				}
			}
		}
	}	
	else
	{
		CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( data.m_nHitBox );
		if ( pDef )
		{
			CUtlReference<CNewParticleEffect> pEffect = CNewParticleEffect::CreateOrAggregate( NULL, pDef, data.m_vOrigin, NULL, nSplitScreenPlayerSlot );
			if ( pEffect.IsValid() && pEffect->IsValid() )
			{
				pEffect->SetSortOrigin( data.m_vOrigin );
				pEffect->SetControlPoint( 0, data.m_vOrigin );
				pEffect->SetControlPoint( 1, data.m_vStart );
				Vector vecForward, vecRight, vecUp;
				AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
				pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
			}
		}
		else
		{
			Warning( "StartParticleEffect:  Failed to find precached particle system for %d!!\n", data.m_nHitBox );
		}
	}
}

void ParticleEffectCallback( const CEffectData &data )
{
	// NOTE: This is because this effect doesn't need to participate
	// in the precache validation tests. Particle systems used with this
	// effect must already be precached.
	g_pPrecacheSystem->EndLimitedResourceAccess( );

	// From networking always go draw for all local players
	StartParticleEffect( data, -1 );
}

DECLARE_CLIENT_EFFECT( ParticleEffect, ParticleEffectCallback )


//======================================================================================================================
// PARTICLE SYSTEM STOP EFFECT
//======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParticleEffectStopCallback( const CEffectData &data )
{
	if ( data.m_hEntity.Get() )
	{
		C_BaseEntity *pEnt = C_BaseEntity::Instance( data.m_hEntity );
		if ( pEnt )
		{
			if ( data.m_nHitBox > 0 )
			{
				if ( pEnt->IsWorld() )
				{
					if ( data.m_nHitBox > 0 ) 
					{
						CNewParticleEffect::RemoveParticleEffect( data.m_nHitBox );
					}
				}
				else
				{
					CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindPrecachedParticleSystem( data.m_nHitBox );

					if ( pDef )
					{
						pEnt->ParticleProp()->StopParticlesNamed( pDef->GetName(), true );
					}
				}
			}
			else
			{
				pEnt->ParticleProp()->StopEmission();
			}
		}
	}
}

DECLARE_CLIENT_EFFECT( ParticleEffectStop, ParticleEffectStopCallback );
