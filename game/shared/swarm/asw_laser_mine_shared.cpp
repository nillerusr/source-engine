#include "cbase.h"
#ifdef CLIENT_DLL
	#include "c_asw_laser_mine.h"
	#define CASW_Laser_Mine C_ASW_Laser_Mine
#else
	#include "asw_laser_mine.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
extern ConVar asw_debug_mine;
#endif

void CASW_Laser_Mine::UpdateLaser()
{
	// trace along the laser to see if anything's there
	const float ASW_LASER_DETECTION_RANGE = 200.0f;
	const float ASW_LASER_START_DIST = 14.0f;
	Vector vecLaserDir, vecForward;
	QAngle angAimOffset = m_angLaserAim.Get();

	matrix3x4_t facingMatrix;
	AngleMatrix( GetAbsAngles(), facingMatrix );

	QAngle angRotateMine( 0, 90, 90 );
	matrix3x4_t fRotateMatrix;
	AngleMatrix( angRotateMine, fRotateMatrix );
	matrix3x4_t fRotateMatrixInv;
	MatrixInvert( fRotateMatrix, fRotateMatrixInv );

	matrix3x4_t finalMatrix;
	QAngle angMine;
	ConcatTransforms( facingMatrix, fRotateMatrixInv, finalMatrix );
	MatrixAngles( finalMatrix, angMine );

	AngleVectors( angMine, &vecForward );
	VectorRotate( vecForward, angAimOffset, vecLaserDir );

	Vector vecSrc = WorldSpaceCenter() + vecLaserDir * ASW_LASER_START_DIST;
	Vector vecDest = vecSrc + vecLaserDir * ASW_LASER_DETECTION_RANGE;

#ifdef CLIENT_DLL
	if ( m_bMineActive.Get() )
	{
		if ( !m_pLaserEffect )
		{
			CreateLaserEffect();
		}

		if ( m_pLaserEffect )
		{
			trace_t tr;
			UTIL_TraceLine( vecSrc, vecDest, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			m_pLaserEffect->SetControlPoint( 1, vecSrc );
			Vector vecLaserEnd = vecDest;
			if ( tr.startsolid )
			{
				vecLaserEnd = vecSrc;
			}
			else if ( tr.DidHit() )
			{
				vecLaserEnd = tr.endpos;
			}
			m_pLaserEffect->SetControlPoint( 2, vecLaserEnd );
			m_pLaserEffect->SetControlPointForwardVector ( 1, vecLaserDir );
			Vector vecImpactY, vecImpactZ;
			VectorVectors( tr.plane.normal, vecImpactY, vecImpactZ ); 
			vecImpactY *= -1.0f;
			m_pLaserEffect->SetControlPointOrientation( 2, vecImpactY, vecImpactZ, tr.plane.normal );
			float alpha = 1.0f;	// TODO: fade this up as laser mine charges up
			m_pLaserEffect->SetControlPoint( 3, Vector( alpha, 0, 0 ) );
		}
	}
	else
	{
		if ( m_pLaserEffect )
		{
			RemoveLaserEffect();
		}
	}
#else
	Ray_t ray;
	ray.Init( vecSrc, vecDest, Vector( -10.0f, -10.0f, -10.0f ), Vector( 10.0f, 10.0f, 10.0f ) );

	CBaseEntity *(pEntities[ 16 ]);
	CHurtableEntitiesEnum hurtableEntities( pEntities, 16 );
	partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS | PARTITION_ENGINE_SOLID_EDICTS, ray, false, &hurtableEntities );

	trace_t	tr;

	for ( int i = 0; i < hurtableEntities.GetCount(); ++i )
	{
		if ( ValidMineTarget( pEntities[ i ] ) )
		{
			if ( asw_debug_mine.GetBool() )
			{
				NDebugOverlay::Line( vecSrc, tr.endpos, 255, 0, 0, false, 0.1f );
			}
			Explode();

			break;
		}
	}
#endif
}