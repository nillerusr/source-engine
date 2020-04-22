//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================
#include "cbase.h"
#include "decals.h"
#include "iefx.h"
#include "fx_impact.h"
#include "tempent.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"

void KnifeSlash( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;

	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if( pEntity == NULL )
		return;

	int decalNumber = decalsystem->GetDecalIndexForName( GetImpactDecal( pEntity, iMaterial, iDamageType ) );
	if ( decalNumber == -1 )
		return;

	// vector perpendicular to the slash direction
	// so we can align the slash decal to that
	Vector vecPerp;
	AngleVectors( data.m_vAngles, NULL, &vecPerp, NULL );

	const ConVar *decals =cvar->FindVar( "r_decals" );

	if ( decals && decals->GetInt() )
	{
		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			// Setup our shot information
			Vector shotDir = vecOrigin - vecStart;
			float flLength = VectorNormalize( shotDir );
			Vector traceExt;
			VectorMA( vecStart, flLength + 8.0f, shotDir, traceExt );

			// Special case for world entity with hitbox (that's a static prop):
			// In this case, we've hit a static prop. Decal it!
			staticpropmgr->AddDecalToStaticProp( vecStart, traceExt, iHitbox - 1, decalNumber, true, tr );
		}
		else
		{
			effects->DecalShoot( decalNumber,
				pEntity->entindex(),
				pEntity->GetModel(),
				pEntity->GetAbsOrigin(),
				pEntity->GetAbsAngles(),
				vecOrigin,
				&vecPerp,
				0 );
		}

	}

	if( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, data.m_fFlags ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );	
	}
}

DECLARE_CLIENT_EFFECT( "KnifeSlash",		KnifeSlash );
