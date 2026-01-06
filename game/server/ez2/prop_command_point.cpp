//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Marker for squad player commands
//
//=============================================================================//

#include "cbase.h"
#include "baseentity.h"

#define COMMAND_POINT_MODEL "models/effects/commandpoint.mdl"

class CPropCommandPoint : public CBaseAnimating
{
public:
	DECLARE_CLASS(CPropCommandPoint, CBaseAnimating );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );
};

LINK_ENTITY_TO_CLASS( prop_command_point, CPropCommandPoint);

const QAngle vec3_yaw(0, 1, 0);

void CPropCommandPoint::Spawn( void )
{
	char *szModel = COMMAND_POINT_MODEL;
	if (!szModel || !*szModel)
	{
		UTIL_Remove( this );
		return;
	}

	PrecacheModel( szModel );
	SetModel(szModel);

	SetMoveType(MOVETYPE_NOCLIP);

	BaseClass::Spawn();

	AddEffects( EF_NOSHADOW );

	SetSequence( 0 );
	SetPlaybackRate( 1.0f );
	SetLocalAngularVelocity(vec3_yaw * 15);
}

void CPropCommandPoint::Precache( void )
{
	BaseClass::Precache();
}

void CPropCommandPoint::OnRestore( void )
{
	BaseClass::OnRestore();

	if (gpGlobals->eLoadType == MapLoad_Transition)
	{
		// Left behind after a level transition
		UTIL_Remove( this );
	}
}
