//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for object upgrading objects
//
//=============================================================================//
#include "cbase.h"
#include "baseobject_shared.h"
#include "tf_obj_baseupgrade_shared.h"

IMPLEMENT_NETWORKCLASS_ALIASED( BaseObjectUpgrade, DT_BaseObjectUpgrade )

BEGIN_NETWORK_TABLE( CBaseObjectUpgrade, DT_BaseObjectUpgrade )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObjectUpgrade::CBaseObjectUpgrade()
{
#if !defined( CLIENT_DLL )
	UseClientSideAnimation();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObjectUpgrade::Spawn()
{
	BaseClass::Spawn();

#if !defined( CLIENT_DLL )
	m_fObjectFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ | OF_ALLOW_REPEAT_PLACEMENT | 
					  OF_DOESNT_NEED_POWER | OF_MUST_BE_BUILT_ON_ATTACHMENT; 

	// Prevent anyone shooting / emping / buffing me
	SetSolid( SOLID_NONE );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Prevent Team Damage
//-----------------------------------------------------------------------------
int CBaseObjectUpgrade::OnTakeDamage( const CTakeDamageInfo &info )
{
#if !defined( CLIENT_DLL )
	// Check teams
	if ( info.GetDamageType() & DMG_BLAST )
	{
		return 0;
	}

	return BaseClass::OnTakeDamage( info );
#else
	return 0;
#endif
}