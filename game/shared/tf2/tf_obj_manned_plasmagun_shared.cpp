//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj_manned_plasmagun_shared.h"
#include "tf_movedata.h"

ConVar mannedgun_usethirdperson( "mannedgun_usethirdperson", "1", FCVAR_REPLICATED, "Use third person view while in manned guns built on vehicles." );

#define MANNED_PLASMAGUN_AIMING_CONE_ANGLE	45.0f	// total angle of aiming
#define MANNED_PLASMAGUN_YAW_SPEED			1000.0f
#define MANNED_PLASMAGUN_MAX_PITCH			50.0f
#define MANNED_PLASMAGUN_BARREL_MAX_PITCH	30.0f

float CObjectMannedPlasmagunMovement::GetMaxYaw() const
{
	return MANNED_PLASMAGUN_AIMING_CONE_ANGLE;
}

float CObjectMannedPlasmagunMovement::GetMinYaw() const
{
	return -MANNED_PLASMAGUN_AIMING_CONE_ANGLE;
}

float CObjectMannedPlasmagunMovement::GetMaxPitch() const
{
	return MANNED_PLASMAGUN_MAX_PITCH;
}

void CObjectMannedPlasmagunMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	CTFMoveData *pMoveData = (CTFMoveData*)pMove; 
	Assert( sizeof(MannedPlasmagunData_t) <= pMoveData->VehicleDataMaxSize() );

	MannedPlasmagunData_t *pVehicleData = (MannedPlasmagunData_t*)pMoveData->VehicleData();

	bool bSimple = (pVehicleData->m_nMoveStyle == MOVEMENT_STYLE_SIMPLE);
	CBaseTFVehicle *pVehicle = pVehicleData->m_pVehicle;

	// Flush caches since bone controllers might be wrong since they are not in the cache yet
	pVehicle->InvalidateBoneCache();

	// Get the view direction *in world coordinates*
	Vector vPlayerEye = pPlayer->EyePosition();
	QAngle angEyeAngles = pPlayer->LocalEyeAngles();
	Vector vPlayerForward;
	AngleVectors( angEyeAngles, &vPlayerForward, NULL, NULL );


	// Now figure out the pitch. This is done by casting a ray to see where the player's aiming reticle is pointing.
	// Then we do some trig to find out what angle the turret should point so it can see the target.
	// NOTE: this is done in the tank's local space so it works when the tank is banked.
	VMatrix mGunToWorld = SetupMatrixTranslation(pVehicle->GetAbsOrigin()) * SetupMatrixAngles(pVehicle->GetAbsAngles());
	VMatrix mWorldToGun = mGunToWorld.InverseTR();

	// First trace only on the world..
	Vector start = vPlayerEye;
	Vector end   = start + vPlayerForward * 5000.0f;

	Vector vTarget;
	if ( bSimple )
	{
		vTarget = end;
	}
	else
	{
		trace_t trace;
		UTIL_TraceLine(start, end, MASK_SOLID_BRUSHONLY, pVehicle, COLLISION_GROUP_NONE, &trace);
		vTarget = trace.endpos;
		if(trace.fraction == 1)
		{
			// It didn't hit the world, so trace on ents.
			UTIL_TraceLine(start, end, MASK_PLAYERSOLID|MASK_NPCSOLID, pVehicle, COLLISION_GROUP_NONE, &trace);
			vTarget = trace.endpos;
			if(trace.fraction == 1)
			{
				// Didn't hit any ents.. just assume it's way out in front of the player's view.
				vTarget = end;
			}
		}
	}

	// Transform the world position into gun space.
	vTarget = mWorldToGun * vTarget;


	// Compute the position of the barrel pivot point as measured in the coordinate system of the gun
	Vector vTurretBase;
	QAngle vTurretBaseAngles;
	pVehicle->GetAttachment(pVehicleData->m_nBarrelPivotAttachment, vTurretBase, vTurretBaseAngles);

	vTurretBase = mWorldToGun * vTurretBase;

	// Make everything be relative to the pivot...
	vTarget -= vTurretBase;


	// Now we've got the target vector in local space. Now just figure out what 
	// the gun angles need to be to hit the target.
	QAngle vWantedAngles;
	VectorAngles( vTarget, vWantedAngles );

	pVehicleData->m_flGunPitch = vWantedAngles[PITCH];
	pVehicleData->m_flGunYaw = vWantedAngles[YAW];


	// Place the player at the feet of the vehicle
	Vector vStandAngles;

	pVehicle->GetAttachmentLocal(pVehicleData->m_nStandAttachment, pMove->m_vecAbsOrigin, pMove->m_vecAngles);
}


