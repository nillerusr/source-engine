//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_asw_physics_prop_statue.h"
#include "debugoverlay_shared.h"
#include "c_asw_fx.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_ASWStatueProp, DT_ASWStatueProp, CASWStatueProp)

END_RECV_TABLE()

C_ASWStatueProp::C_ASWStatueProp()
{
	m_bShattered = false;
	m_flAimTargetRadius = 32;
}

void C_ASWStatueProp::Spawn( void )
{
	BaseClass::Spawn();

}

void C_ASWStatueProp::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	//CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;

	//if ( pBaseAnimating )
	//{
	//	pBaseAnimating->CollisionProp()->WorldSpaceSurroundingBounds( pVecWorldMins, pVecWorldMaxs );
	//}
}


void C_ASWStatueProp::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	Vector vecSurroundMins, vecSurroundMaxs;
	vecSurroundMins = CollisionProp()->OBBMins();
	vecSurroundMaxs = CollisionProp()->OBBMaxs();

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// for calculating the aim target radius
		m_flAimTargetRadius = (((vecSurroundMaxs.x - vecSurroundMins.x)+(vecSurroundMaxs.y - vecSurroundMins.y)+(vecSurroundMaxs.z - vecSurroundMins.z)) / 3) / 2;
	}

	//FIXME: shatter effects should call a function so derived classes can make their own
	if ( m_bShatter && !m_bShattered )
	{
		m_bShattered = true;

		float xSize = vecSurroundMaxs.x - vecSurroundMins.x;
		float ySize = vecSurroundMaxs.y - vecSurroundMins.y;
		float flSize = ( xSize + ySize ) / 218.0f; // this is the size of the drone BBox
		if ( flSize < 0.5f )
			flSize = 0.5f;
		else if ( flSize > 2.5f )
			flSize = 2.5f;

		//NDebugOverlay::Cross3D( m_vShatterPosition, 16, 255, 0, 0, true, 30 );
		CUtlReference<CNewParticleEffect> pEffect;
		pEffect = ParticleProp()->Create( "freeze_statue_shatter", PATTACH_ABSORIGIN_FOLLOW, -1, m_vShatterPosition - GetAbsOrigin() );
		pEffect->SetControlPoint( 1, Vector( flSize, 0, 0 ) );

		// NOT USED ANYMORE
		/*
		// copied directly from c_asw_clientragdoll
		KeyValues * modelKeyValues = new KeyValues("");
		if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
		{
			KeyValues *pkvMeshParticleEffect = modelKeyValues->FindKey("MeshParticles");
			if ( pkvMeshParticleEffect )
			{

				for ( KeyValues *pSingleEffect = pkvMeshParticleEffect->GetFirstSubKey(); pSingleEffect; pSingleEffect = pSingleEffect->GetNextKey() )
				{
					const char *pszParticleEffect = pSingleEffect->GetString( "effectName", "" );
					const char *pszModelName = pSingleEffect->GetString( "modelName", "" );
					const char *pszBoneName = pSingleEffect->GetString( "boneName", "" );
					//const char *pszBoneAxis = pSingleEffect->GetString( "boneAxis", "0 1 0" );

					Vector vGibOrigin;
					QAngle aGibAngles;

					int iBoneIdx = -1;
					if ( pszBoneName )
					{
						iBoneIdx = LookupBone( pszBoneName );	
					}

					// See if we can find the appropriate bone
					if ( iBoneIdx > -1 )
					{
						GetBonePosition( iBoneIdx, vGibOrigin, aGibAngles );
					}
					else
					{
						//if no bone was specified, pop a warning and then use the ragdoll's centre, or the worldspace origin...
						Warning("Failed to find the bone specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' on attachment named '%s'.  Spawning effect at origin of prop_statue.\n", GetModelName(), pszParticleEffect, pszBoneName );

						vGibOrigin = WorldSpaceCenter();
					}


					//rotate the direction vector by the angles of the joint so we emit in the right direction
					//Vector vBaseDirection = Vector( 0, 1, 0 );
					//Vector vDirection;
					//VectorRotate( vBaseDirection, aGibAngles, vDirection );

					Vector vecToTarget = vGibOrigin - m_vShatterPosition;
					VectorNormalize( vecToTarget );
					Vector vecForce = vecToTarget * 200 + Vector( 0, 0, 32 );

					//Vector vGibVelocity = info.GetDamageForce();

					FX_GibMeshEmitter( pszModelName, pszParticleEffect, vGibOrigin, vecForce, GetSkin(), 1.0f, true );
				}
			}

			modelKeyValues->deleteThis();
		}
		*/
	}
}

void C_ASWStatueProp::TakeDamage( const CTakeDamageInfo &info )
{
	UTIL_ASW_ClientFloatingDamageNumber( info );
}
