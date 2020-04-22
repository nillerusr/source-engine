//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_gasoline_blob.h"
#include "gasoline_shared.h"
#include "engine/IEngineSound.h"
#include "clienteffectprecachesystem.h"

static CUtlLinkedList<C_GasolineBlob*, int> g_GasolineBlobs;

// If multiple blobs are within this distance to each other, then only one will
// play a sound.
#define BLOB_SOUND_RELATED_DISTANCE	600


#define PUDDLE_START_SIZE	35
#define PUDDLE_END_SIZE		65
#define PUDDLE_GROW_TIME	0.5

#define PUDDLE_FADE_TIME	1.0


CLIENTEFFECT_REGISTER_BEGIN( PrecacheGasolineBlob )
CLIENTEFFECT_MATERIAL( "decals/puddle" )
CLIENTEFFECT_REGISTER_END()

// ------------------------------------------------------------------------------------------------ //
// CGasolineEmitter.
// ------------------------------------------------------------------------------------------------ //

CSmartPtr<CGasolineEmitter> CGasolineEmitter::Create( C_GasolineBlob *pBlob )
{
	CGasolineEmitter *pEmitter = new CGasolineEmitter;
	
	pEmitter->m_pBlob = pBlob;

	pEmitter->m_hFireMaterial = pEmitter->GetPMaterial( "particle/fire" );
	pEmitter->m_hUnlitMaterial = pEmitter->GetPMaterial( "sprites/env_particles" );
	
	pEmitter->m_Timer.Init( 40 );

	return pEmitter;
}


void CGasolineEmitter::UpdateFire( float frametime )
{
	float flLifetime = gpGlobals->curtime - m_pBlob->m_flCreateTime;

	float litPercent = 1;
	if ( m_pBlob->IsLit() )
	{
		litPercent = 1 - (flLifetime / m_pBlob->m_flMaxLifetime);
		if ( litPercent <= 0 )
			return;
	}
	else
	{
		return;
	}

	// Don't show a burn effect for a blob that hasn't hit anything yet.
	// If you do, it tends to make the flamethrower effect look weird.
	if ( !m_pBlob->IsStopped() )
		return;

	// Make a coordinate system in which to spawn the particles. It 
	Vector vUp, vRight;
	vUp.Init();
	vRight.Init();
	if ( m_pBlob->IsStopped() )
	{
		QAngle angles;
		VectorAngles( m_pBlob->GetSurfaceNormal(), angles );
		AngleVectors( angles, NULL, &vRight, &vUp );
	}
	
	PMaterialHandle hMaterial = m_hFireMaterial;
	float flParticleLifetime = 1;
	float flRadius = 7;
	unsigned char uchColor[4] = { 255, 128, 0, 128 };
	float flMaxZVel = 29;


	float curDelta = frametime;
	while ( m_Timer.NextEvent( curDelta ) )
	{
		// Based on how close we are to expiring, show less particles.
		if ( RandomFloat( 0, 1 ) > litPercent )
			continue;

		Vector vPos = m_pBlob->GetAbsOrigin();
		if ( m_pBlob->IsStopped() )
		{
			float flAngle = RandomFloat( 0, M_PI * 2 );
			float flDist = RandomFloat( 0, GASOLINE_BLOB_RADIUS );
			vPos += vRight * (cos( flAngle ) * flDist);
			vPos += vUp * ( sin( flAngle ) * flDist );
		}
		else
		{
			vPos += RandomVector( -GASOLINE_BLOB_RADIUS, GASOLINE_BLOB_RADIUS );
		}
		
		SimpleParticle *pParticle = AddSimpleParticle( hMaterial, vPos, flParticleLifetime, flRadius );
		if ( pParticle )
		{
			pParticle->m_uchColor[0] = uchColor[0];
			pParticle->m_uchColor[1] = uchColor[1];
			pParticle->m_uchColor[2] = uchColor[2];

			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartAlpha = uchColor[3];

			pParticle->m_vecVelocity.x = RandomFloat( -2, 2 );
			pParticle->m_vecVelocity.y = RandomFloat( -2, 2 );
			pParticle->m_vecVelocity.z = RandomFloat( 3, flMaxZVel );
		}
	}
}


// ------------------------------------------------------------------------------------------------ //
// C_GasolineBlob.
// ------------------------------------------------------------------------------------------------ //

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_GasolineBlob, DT_GasolineBlob, CGasolineBlob )
	RecvPropInt( RECVINFO( m_BlobFlags ) ),
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropInt( RECVINFO_NAME(m_hNetworkMoveParent, moveparent), 0, RecvProxy_IntToMoveParent ),
	RecvPropFloat( RECVINFO( m_flLitStartTime ) ),
	RecvPropFloat( RECVINFO( m_flCreateTime ) ),
	RecvPropFloat( RECVINFO( m_flMaxLifetime ) ),
	RecvPropInt( RECVINFO( m_iTeamNum ) ),
	RecvPropVector( RECVINFO( m_vSurfaceNormal ) )
END_RECV_TABLE()


C_GasolineBlob::C_GasolineBlob()
{
	m_pEmitter = CGasolineEmitter::Create( this );
	m_vSurfaceNormal.Init();
	m_flLitStartTime = 0;
	m_bSoundOn = false;
	g_GasolineBlobs.AddToTail( this );
	m_flPuddleSize = PUDDLE_START_SIZE;
	m_flPuddleFade = 1;
}


C_GasolineBlob::~C_GasolineBlob()
{
	g_GasolineBlobs.FindAndRemove( this );
	StopSound();

	// If a bunch of nearby blobs weren't playing a sound because we were, have them start their sound now.
	FOR_EACH_LL( g_GasolineBlobs, i )
	{
		C_GasolineBlob *pBlob = g_GasolineBlobs[i];

		if ( pBlob->IsSoundRelatedTo( this ) )
			pBlob->CheckStartSound();
	}
}


bool C_GasolineBlob::IsLit() const
{
	return (m_BlobFlags & BLOBFLAG_LIT) != 0;
}


bool C_GasolineBlob::IsStopped() const
{
	return (m_BlobFlags & BLOBFLAG_STOPPED) != 0;
}


const Vector& C_GasolineBlob::GetSurfaceNormal() const
{
	return m_vSurfaceNormal;
}


float C_GasolineBlob::GetLitStartTime() const
{
	return m_flLitStartTime;
}


void C_GasolineBlob::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	
	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	CheckStartSound();
}


void C_GasolineBlob::ClientThink()
{
	if ( m_pEmitter.IsValid() )
		m_pEmitter->UpdateFire( gpGlobals->frametime );

	// Grow the puddle a little.
	if ( IsStopped() )
	{
		if ( IsLit() )
		{
			// Fade out after we get lit.
			m_flPuddleFade -= gpGlobals->frametime / PUDDLE_FADE_TIME;
			m_flPuddleFade = MAX( m_flPuddleFade, 0 );
		}
		else
		{
			// Grow the puddle until it's at its max size.
			m_flPuddleSize += gpGlobals->frametime * ( PUDDLE_END_SIZE - PUDDLE_START_SIZE ) / PUDDLE_GROW_TIME;
			m_flPuddleSize = MIN( m_flPuddleSize, PUDDLE_END_SIZE );
		}
	}
}


bool C_GasolineBlob::ShouldDraw()
{
	return IsStopped() && (m_flPuddleFade > 0);
}


int C_GasolineBlob::DrawModel( int flags )
{
	// Generate a basis.
	QAngle angles;
	VectorAngles( m_vSurfaceNormal, angles );

	Vector vRight, vUp;
	AngleVectors( angles, NULL, &vRight, &vUp );

	
	float flAlpha = m_flPuddleFade * RemapVal( m_flPuddleSize, PUDDLE_START_SIZE, PUDDLE_END_SIZE, 0, 1 );
	if ( flAlpha <= 0 )
		return 0;


	// Draw the puddle.
	IMaterial *pMat = materials->FindMaterial( "decals/puddle", TEXTURE_GROUP_DECAL );
	IMesh *pMesh = materials->GetDynamicMesh( true, NULL, NULL, pMat );
	CMeshBuilder mb;
	mb.Begin( pMesh, MATERIAL_QUADS, 1 );
		
		Vector v;
		
		v = GetAbsOrigin() + m_vSurfaceNormal + vRight*m_flPuddleSize - vUp*m_flPuddleSize;
		mb.Position3f( v.x, v.y, v.z );
		mb.Color4f( 1, 1, 1, flAlpha );
		mb.Normal3f( VectorExpand( m_vSurfaceNormal ) );
		mb.TexCoord2f( 0, 1, 0 );
		mb.AdvanceVertex();

		v = GetAbsOrigin() + m_vSurfaceNormal + vRight*m_flPuddleSize + vUp*m_flPuddleSize;
		mb.Position3f( v.x, v.y, v.z );
		mb.Color4f( 1, 1, 1, flAlpha );
		mb.Normal3f( VectorExpand( m_vSurfaceNormal ) );
		mb.TexCoord2f( 0, 1, 1 );
		mb.AdvanceVertex();

		v = GetAbsOrigin() + m_vSurfaceNormal - vRight*m_flPuddleSize + vUp*m_flPuddleSize;
		mb.Position3f( v.x, v.y, v.z );
		mb.Color4f( 1, 1, 1, flAlpha );
		mb.Normal3f( VectorExpand( m_vSurfaceNormal ) );
		mb.TexCoord2f( 0, 0, 1 );
		mb.AdvanceVertex();

		v = GetAbsOrigin() + m_vSurfaceNormal - vRight*m_flPuddleSize - vUp*m_flPuddleSize;
		mb.Position3f( v.x, v.y, v.z );
		mb.Color4f( 1, 1, 1, flAlpha );
		mb.Normal3f( VectorExpand( m_vSurfaceNormal ) );
		mb.TexCoord2f( 0, 0, 0 );
		mb.AdvanceVertex();

	mb.End( false, true );		

	return 0;
}


bool C_GasolineBlob::IsSoundRelatedTo( const C_GasolineBlob *pBlob ) const
{
	return pBlob->GetAbsOrigin().DistTo( GetAbsOrigin() ) < BLOB_SOUND_RELATED_DISTANCE;
}


bool C_GasolineBlob::IsPlayingBurningSound() const
{
	return m_bSoundOn;
}


void C_GasolineBlob::CheckStartSound()
{
	if ( IsPlayingBurningSound() || (m_BlobFlags & BLOBFLAG_STOPPED) == 0 || !IsLit() )
		return;


	// First, make sure no nearby blob is playing the sound.
	FOR_EACH_LL( g_GasolineBlobs, i )
	{
		C_GasolineBlob *pBlob = g_GasolineBlobs[i];
		
		if ( pBlob != this && pBlob->IsSoundRelatedTo( this ) )
		{
			// If it's already playing a sound, then don't start our sound.
			if ( pBlob->IsPlayingBurningSound() )
				return;
		}
	}		

	
	StartSound();
}


void C_GasolineBlob::StartSound()
{
	if ( !m_bSoundOn )
	{
		EmitSound( "GasolineBlob.FlameSound" );

		m_bSoundOn = true;
	}
}


void C_GasolineBlob::StopSound()
{
	if ( m_bSoundOn )
	{
		BaseClass::StopSound( "GasolineBlob.FlameSound" );
		m_bSoundOn = false;
	}
}

