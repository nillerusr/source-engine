//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "fx_impact.h"
#include "engine/IEngineSound.h"
#include "c_tf_player.h"

//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;

	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	bool bImpact = ( data.m_nDamageType != pEntity->GetTeamNumber() || pEntity->GetTeamNumber() < FIRST_GAME_TEAM );

	if ( data.m_nDamageType != pEntity->GetTeamNumber() && pEntity->IsPlayer() )
	{	
		C_TFPlayer *pPlayer = ToTFPlayer( pEntity );

		// Don't impact spies disguised as the same team as this syringe/projectile
		if ( pPlayer && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			if ( pPlayer->m_Shared.GetDisguiseTeam() == data.m_nDamageType )
			{
				bImpact = false;
			}
		}
	}

	if ( bImpact )
	{	
		// If we hit, perform our custom effects and play the sound
		if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
		{
			// Check for custom effects based on the Decal index
			PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );

			//Play a ricochet sound some of the time
			if( random->RandomInt(1,10) <= 3 && (iDamageType == DMG_BULLET) )
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Bounce.Shrapnel", &vecOrigin );
			}
		}

		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
	}
}

DECLARE_CLIENT_EFFECT( "Impact", ImpactCallback );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFSplashCallbackHelper( const CEffectData &data, const char *pszEffectName )
{
	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, pszEffectName );
	if ( pEffect->IsValid() )
	{
		pEffect->SetSortOrigin( data.m_vOrigin );
		pEffect->SetControlPoint( 0, data.m_vOrigin );
		pEffect->SetControlPointOrientation( 0, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );
	}
}

void TFSplashCallback( const CEffectData &data )
{
	TFSplashCallbackHelper( data, "water_bulletsplash01" );
}
DECLARE_CLIENT_EFFECT( "tf_gunshotsplash", TFSplashCallback );

void TFSplashCallbackMinigun( const CEffectData &data )
{
	TFSplashCallbackHelper( data, "water_bulletsplash01_minigun" );
}
DECLARE_CLIENT_EFFECT( "tf_gunshotsplash_minigun", TFSplashCallbackMinigun );