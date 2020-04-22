//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"

#ifdef CLIENT_DLL
#include "c_basedoor.h"
#endif

CUtlVector<CProp_Portal *> CProp_Portal_Shared::AllPortals;

const Vector CProp_Portal_Shared::vLocalMins( 0.0f, -PORTAL_HALF_WIDTH, -PORTAL_HALF_HEIGHT );
const Vector CProp_Portal_Shared::vLocalMaxs( 64.0f, PORTAL_HALF_WIDTH, PORTAL_HALF_HEIGHT );

void CProp_Portal_Shared::UpdatePortalTransformationMatrix( const matrix3x4_t &localToWorld, const matrix3x4_t &remoteToWorld, VMatrix *pMatrix )
{
	VMatrix matPortal1ToWorldInv, matPortal2ToWorld, matRotation;

	//inverse of this
	MatrixInverseTR( localToWorld, matPortal1ToWorldInv );

	//180 degree rotation about up
	matRotation.Identity();
	matRotation.m[0][0] = -1.0f;
	matRotation.m[1][1] = -1.0f;

	//final
	matPortal2ToWorld = remoteToWorld;	
	*pMatrix = matPortal2ToWorld * matRotation * matPortal1ToWorldInv;
}

static char *g_pszPortalNonTeleportable[] = 
{ 
	"func_door", 
	"func_door_rotating", 
	"prop_door_rotating",
	"func_tracktrain",
	//"env_ghostanimating",
	"physicsshadowclone"
};

bool CProp_Portal_Shared::IsEntityTeleportable( CBaseEntity *pEntity )
{

	do
	{

#ifdef CLIENT_DLL
		//client
	
		if( dynamic_cast<C_BaseDoor *>(pEntity) != NULL )
			return false;

#else
		//server
		
		for( int i = 0; i != ARRAYSIZE(g_pszPortalNonTeleportable); ++i )
		{
			if( FClassnameIs( pEntity, g_pszPortalNonTeleportable[i] ) )
				return false;
		}

#endif

		Assert( pEntity != pEntity->GetMoveParent() );
		pEntity = pEntity->GetMoveParent();
	} while( pEntity );

	return true;
}




