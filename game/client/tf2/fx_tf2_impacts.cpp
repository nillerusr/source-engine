//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx_impact.h"
#include "decals.h"
#include "IEffects.h"
#include "c_breakableprop.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "tf_shareddefs.h"
#include "fx.h"

void ImpactCreateHurtShards( Vector &vecOrigin, trace_t &tr, Vector &shotDir, int iMaterial, bool bLarge, bool bBlood );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MakeHurt( const CEffectData &data, bool bBlood )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Throw out shards to show the player he's hurting it
		ImpactCreateHurtShards( vecOrigin, tr, vecShotDir, iMaterial, false, bBlood );

		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	MakeHurt( data, true );
}

DECLARE_CLIENT_EFFECT( "Impact", ImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: Bloodless Impact for:
//				Bullets hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactNoBloodCallback( const CEffectData &data )
{
	MakeHurt( data, false );
}

DECLARE_CLIENT_EFFECT( "ImpactNoBlood", ImpactNoBloodCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting targets that CANNOT be hurt
//-----------------------------------------------------------------------------
void ImpactUnhurtCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, perform our custom effects and play the sound
	// Don't decal team members, but decal everything else

	bool bShouldDecal = !( pEntity && pEntity->IsPlayer() );
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, bShouldDecal ? 0 : IMPACT_NODECAL ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "ImpactUnhurt", ImpactUnhurtCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting player's handheld shields
//-----------------------------------------------------------------------------
void ImpactShieldCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	// Don't call Impact() because we don't want to decal the player
	Vector reflect = -vecShotDir;
	reflect[0] += random->RandomFloat( -0.5f, 0.5f );
	reflect[1] += random->RandomFloat( -0.5f, 0.5f );
	reflect[2] += random->RandomFloat( 0, 0.5f );
	FX_MetalSpark( vecOrigin, reflect, -vecShotDir, 3 );
}

DECLARE_CLIENT_EFFECT( "ImpactShield", ImpactShieldCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MakePlasmaHurt( const CEffectData &data, bool bBlood )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Throw out shards to show the player he's hurting it
		ImpactCreateHurtShards( vecOrigin, tr, vecShotDir, iMaterial, false, bBlood );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaCallback( const CEffectData &data )
{
	MakePlasmaHurt( data, true );
}

DECLARE_CLIENT_EFFECT( "PlasmaHurt", ImpactPlasmaCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaNoBloodCallback( const CEffectData &data )
{
	MakePlasmaHurt( data, false );
}

DECLARE_CLIENT_EFFECT( "PlasmaHurtNoBlood", ImpactPlasmaNoBloodCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CANNOT be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaUnhurtCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	
	bool bShouldDecal = !( pEntity && pEntity->IsPlayer() );
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, bShouldDecal ? 0 : IMPACT_NODECAL  ) )
	{
		g_pEffects->EnergySplash( vecOrigin, tr.plane.normal, false );
	}
}

DECLARE_CLIENT_EFFECT( "PlasmaUnhurt", ImpactPlasmaUnhurtCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting player's handheld shields
//-----------------------------------------------------------------------------
void ImpactPlasmaShieldCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	// Bounce sparks away from the shield
	// Don't call Impact() because we don't want to decal the player
	Vector offset = vecOrigin - ( vecShotDir * 1.0f );
	Vector vecDir = -vecShotDir + Vector(0,0,1.5);
	g_pEffects->Sparks( offset, 2, 2, &vecDir );
}

DECLARE_CLIENT_EFFECT( "PlasmaShield", ImpactPlasmaShieldCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Strider hitting anything
//-----------------------------------------------------------------------------
void ImpactStriderCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 5 );
	}
}

DECLARE_CLIENT_EFFECT( "Strider", ImpactStriderCallback );


//=====================================================================================
// SHARDS. 
// Models for small impact tempents (not vphysics simulated)
//--------------------------------------------------------------------
// WOOD
//--------------------------------------------------------------------
#define WOOD_SHARDS		5
const char *ImpactHurtShards_Wood_Small_Models[ WOOD_SHARDS ] = 
{
"models/props_debris/wood_shard_001.mdl",
"models/props_debris/wood_shard_002.mdl",
"models/props_debris/wood_shard_003.mdl",
"models/props_debris/wood_shard_004.mdl",
"models/props_debris/wood_shard_005.mdl",
};
// Model pointers for the above
model_t *ImpactHurtShards_Wood_Small[ WOOD_SHARDS ];

//--------------------------------------------------------------------
// FOLIAGE
//--------------------------------------------------------------------
#define FOLIAGE_SHARDS		5
const char *ImpactHurtShards_Foliage_Small_Models[ FOLIAGE_SHARDS ] = 
{
"models/props_debris/foliage_shard_001.mdl",
"models/props_debris/foliage_shard_002.mdl",
"models/props_debris/foliage_shard_003.mdl",
"models/props_debris/foliage_shard_004.mdl",
"models/props_debris/foliage_shard_005.mdl",
};
// Model pointers for the above
model_t *ImpactHurtShards_Foliage_Small[ FOLIAGE_SHARDS ];

//--------------------------------------------------------------------
// CONCRETE
//--------------------------------------------------------------------
#define CONCRETE_SHARDS		1
const char *ImpactHurtShards_Concrete_Small_Models[ CONCRETE_SHARDS ] = 
{
"models/props_debris/metal_shard_001.mdl",
};
// Model pointers for the above
model_t *ImpactHurtShards_Concrete_Small[ CONCRETE_SHARDS ];

//--------------------------------------------------------------------
// METAL
//--------------------------------------------------------------------
#define METAL_SHARDS		6
const char *ImpactHurtShards_Metal_Small_Models[ METAL_SHARDS ] = 
{
"models/props_debris/metal_shard_001.mdl",
"models/props_debris/metal_shard_002.mdl",
"models/props_debris/metal_shard_003.mdl",
"models/props_debris/metal_shard_004.mdl",
"models/props_debris/metal_shard_005.mdl",
"models/props_debris/metal_shard_006.mdl",
};
// Model pointers for the above
model_t *ImpactHurtShards_Metal_Small[ METAL_SHARDS ];

//-----------------------------------------------------------------------------
// Purpose: Precache all our shards
//-----------------------------------------------------------------------------
void PrecacheImpactShards(void *pUser)
{
	int i;

	// Wood
	for ( i = 0; i < WOOD_SHARDS; i++ )
	{
		const char *sFile = ImpactHurtShards_Wood_Small_Models[i];
		ImpactHurtShards_Wood_Small[i] = (model_t *)engine->LoadModel( sFile );
	}

	// Foliage
	for ( i = 0; i < FOLIAGE_SHARDS; i++ )
	{
		const char *sFile = ImpactHurtShards_Foliage_Small_Models[i];
		ImpactHurtShards_Foliage_Small[i] = (model_t *)engine->LoadModel( sFile );
	}

	// Concrete
	for ( i = 0; i < CONCRETE_SHARDS; i++ )
	{
		const char *sFile = ImpactHurtShards_Concrete_Small_Models[i];
		ImpactHurtShards_Concrete_Small[i] = (model_t *)engine->LoadModel( sFile );
	}

	// Metal
	for ( i = 0; i < METAL_SHARDS; i++ )
	{
		const char *sFile = ImpactHurtShards_Metal_Small_Models[i];
		ImpactHurtShards_Metal_Small[i] = (model_t *)engine->LoadModel( sFile );
	}
}
PRECACHE_REGISTER_FN(PrecacheImpactShards);

//-----------------------------------------------------------------------------
// Purpose: Throw out shards from the impact point to show we can hurt the target
//-----------------------------------------------------------------------------
void ImpactCreateHurtShards( Vector &vecOrigin, trace_t &tr, Vector &shotDir, int iMaterial, bool bLarge, bool bBlood )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;

	float flShardSpread = 0.5;

	Vector vecSpawnOrigin = vecOrigin;
	
	int iNumGibs = random->RandomInt( 1, 2 );
	for ( int i = 0; i < iNumGibs; i++ )
	{
		QAngle vecAngles;
		vecAngles[0] = random->RandomFloat(-255, 255);
		vecAngles[1] = random->RandomFloat(-255, 255);
		vecAngles[2] = random->RandomFloat(-255, 255);
		Vector vecForceDir;
		float spreadOfs = random->RandomFloat( 3.0f, 4.0f );
		vecForceDir[0] = -shotDir[0] + random->RandomFloat( -(flShardSpread*spreadOfs), (flShardSpread*spreadOfs) );
		vecForceDir[1] = -shotDir[1] + random->RandomFloat( -(flShardSpread*spreadOfs), (flShardSpread*spreadOfs) );
		vecForceDir[2] = -shotDir[2] + random->RandomFloat( -(flShardSpread*spreadOfs), (flShardSpread*spreadOfs) );
		// Add some extra vertical height
		vecForceDir[2] += random->RandomFloat( 0, 5 );
		vecForceDir *= random->RandomFloat( 30.0,60.0 );

		model_t *pModel = NULL;
		int iFlags = ( FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_GRAVITY );

		// Select the right chunk for the job
		switch ( iMaterial )
		{
		case CHAR_TEX_WOOD:
			iFlags |= FTENT_ROTATE;
			pModel = ImpactHurtShards_Wood_Small[ random->RandomInt(0,WOOD_SHARDS-1) ];
			break;

		case CHAR_TEX_FOLIAGE:
			pModel = ImpactHurtShards_Foliage_Small[ random->RandomInt(0,FOLIAGE_SHARDS-1) ];
			break;

		case CHAR_TEX_METAL:
		case CHAR_TEX_VENT:
		case CHAR_TEX_GRATE:

		case CHAR_TEX_CONCRETE:
			default:
			pModel = ImpactHurtShards_Metal_Small[ random->RandomInt(0,METAL_SHARDS-1) ];
			break;

		case CHAR_TEX_FLESH:
			// Spray some blood out
			if ( !bBlood )
				return;

			CEffectData	data;
			data.m_vOrigin = vecOrigin;
			data.m_vNormal = tr.plane.normal;
			data.m_flScale = 4;
			data.m_fFlags = FX_BLOODSPRAY_ALL;
			data.m_nEntIndex = tr.m_pEnt ?  tr.m_pEnt->entindex() : 0;
			DispatchEffect( "tf2blood", data );
			return;
			break;
		/*
		case CHAR_TEX_CONCRETE:
		default:
			pModel = ImpactHurtShards_Concrete_Small[ random->RandomInt(0,CONCRETE_SHARDS-1) ];
			break;
		*/
		}
		Assert( pModel );			

		// ROBIN: Removed until optimized
		return;

		// Throw it out
		tempents->SpawnTempModel( pModel, vecSpawnOrigin, vecAngles, vecForceDir, random->RandomFloat(0.5,1.5), iFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Throw out breakable, vphysics gibs from the surface we hit
//-----------------------------------------------------------------------------
void ImpactCreateHurtGibs( Vector &vecOrigin, trace_t &tr, Vector &shotDir, int iMaterial, bool bLarge )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;

	Vector vecSpawnOrigin = vecOrigin - (shotDir * 32);
	float flGibSpread = 0.8;

	// Custom TF2 impact effects
	if ( iMaterial == CHAR_TEX_FOLIAGE )
	{
		int iNumGibs = random->RandomInt( 1, 2 );
		for ( int i = 0; i < iNumGibs; i++ )
		{
			AngularImpulse angVel;
			angVel.Random( -400.0f, 400.0f );
			Vector vecForceDir;
			float spreadOfs = random->RandomFloat( 3.0f, 4.0f );
			vecForceDir[0] = -shotDir[0] + random->RandomFloat( -(flGibSpread*spreadOfs), (flGibSpread*spreadOfs) );
			vecForceDir[1] = -shotDir[1] + random->RandomFloat( -(flGibSpread*spreadOfs), (flGibSpread*spreadOfs) );
			vecForceDir[2] = -shotDir[2] + random->RandomFloat( -(flGibSpread*spreadOfs), (flGibSpread*spreadOfs) );
			// Add some extra vertical height
			vecForceDir[2] += 5;
			vecForceDir *= random->RandomFloat( 10.0,30.0 );

			//C_BreakableProp *pProp = new C_BreakableProp;
			//pProp->CreateClientsideProp( ImpactHurtGibs_Wood_Small[ random->RandomInt(0,NUM_WOOD_GIBS_SMALL-1) ], vecSpawnOrigin, vecForceDir, angVel );
		}
	}
	else 
	{
		PerformCustomEffects( vecOrigin, tr, shotDir, iMaterial, 1 );
	}
}
