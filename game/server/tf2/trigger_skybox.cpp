//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource collection entity
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "env_meteor.h "

//-----------------------------------------------------------------------------
// 3DSkybox to World Transition Trigger Class
//-----------------------------------------------------------------------------
class CTrigger3DSkyboxToWorld : public CBaseTrigger
{
	DECLARE_CLASS( CTrigger3DSkyboxToWorld, CBaseTrigger );

public:

	CTrigger3DSkyboxToWorld();

	DECLARE_DATADESC();

	void Spawn( void );
	void ImpactTouch( CBaseEntity *pOther );
};

BEGIN_DATADESC( CTrigger3DSkyboxToWorld )

	// Function Pointers
	DEFINE_FUNCTION( ImpactTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( trigger_skybox2world, CTrigger3DSkyboxToWorld );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTrigger3DSkyboxToWorld::CTrigger3DSkyboxToWorld()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTrigger3DSkyboxToWorld::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();
//	SetTouch( ImpactTouch );
}

//-----------------------------------------------------------------------------
// Purpose: Not handling transitions with a touch function currently!!
//-----------------------------------------------------------------------------
void CTrigger3DSkyboxToWorld::ImpactTouch( CBaseEntity *pOther )
{
#if 0
	if ( FClassnameIs( pOther, "meteor" ) )
	{
	}
#endif
}
