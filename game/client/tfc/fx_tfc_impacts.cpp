//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_impact.h"


void TFCImpact( const CEffectData &data )
{
	Vector vOrigin, vStart, vDir;
	short nSurfaceProp;
	int iMaterial, iDamageType, iHitbox;
	C_BaseEntity *pEntity = ParseImpactData( data, &vOrigin, &vStart, &vDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	trace_t tr;
	if ( Impact( vOrigin, vStart, iMaterial, iDamageType, iHitbox, iEntIndex, tr ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vOrigin, tr, vDir, iMaterial, 1.0 );
	}

	PlayImpactSound( pEntity, tr, vOrigin, nSurfaceProp );
}


DECLARE_CLIENT_EFFECT( "Impact", TFCImpact );
