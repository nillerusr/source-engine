//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map entity that allows players to build objects on it
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "EntityOutput.h"
#include "EntityList.h"
#include "tf_team.h"
#include "baseentity.h"
#include "info_buildpoint.h"
#include "tf_gamerules.h"

// Spawnflags
const int SF_BUILDPOINT_ALLOW_ALL_GUNS		= 0x01;	// Allow all manned guns to be built on this point
const int SF_BUILDPOINT_ALLOW_VEHICLES		= 0x02;	// Allow all vehicles to be built on this point

BEGIN_DATADESC( CInfoBuildPoint )

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iszAllowedObject, FIELD_STRING, "AllowedObject" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_buildpoint, CInfoBuildPoint );

// List of buildpoints
CUtlVector<CInfoBuildPoint*> g_MapDefinedBuildPoints;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoBuildPoint::Spawn( void )
{
	g_MapDefinedBuildPoints.AddToTail( this );

	m_iAllowedObjectType = -1;
	if ( m_iszAllowedObject != NULL_STRING )
	{
		for ( int i = 0; i < OBJ_LAST; i++ )
		{
			if ( !Q_strcmp( STRING(m_iszAllowedObject), GetObjectInfo(i)->m_pClassName ) )
			{
				m_iAllowedObjectType = i;
				break;
			}
		}
	}

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoBuildPoint::UpdateOnRemove( void )
{
	g_MapDefinedBuildPoints.FindAndRemove( this );
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Tell me how many build points you have
//-----------------------------------------------------------------------------
int	CInfoBuildPoint::GetNumBuildPoints( void ) const
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Give me the origin & angles of the specified build point
//-----------------------------------------------------------------------------
bool CInfoBuildPoint::GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles )
{
	ASSERT( iPoint <= GetNumBuildPoints() );

	vecOrigin = GetAbsOrigin();
	vecAngles = GetAbsAngles();
	return true;
}

int CInfoBuildPoint::GetBuildPointAttachmentIndex( int iPoint ) const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Can I build the specified object on the specified build point?
//-----------------------------------------------------------------------------
bool CInfoBuildPoint::CanBuildObjectOnBuildPoint( int iPoint, int iObjectType )
{
	ASSERT( iPoint <= GetNumBuildPoints() );
	if ( m_hObjectBuiltOnMe )
		return false;

	// Manned guns?
	if ( m_spawnflags & SF_BUILDPOINT_ALLOW_ALL_GUNS )
	{
		if ( (iObjectType == OBJ_MANNED_PLASMAGUN) || 
			 (iObjectType == OBJ_MANNED_MISSILELAUNCHER) || 
			 (iObjectType == OBJ_MANNED_SHIELD) ||  
			 (iObjectType == OBJ_SENTRYGUN_PLASMA) )
			return true;
	}

	// Vehicles
	if ( m_spawnflags & SF_BUILDPOINT_ALLOW_VEHICLES )
	{
		if ( IsObjectAVehicle(iObjectType) )
			return true;
	}

	// Check our unique
	if ( m_iAllowedObjectType >= 0 && m_iAllowedObjectType == iObjectType )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: I've finished building the specified object on the specified build point
//-----------------------------------------------------------------------------
void CInfoBuildPoint::SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject )
{
	ASSERT( iPoint <= GetNumBuildPoints() );
	m_hObjectBuiltOnMe = pObject;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of objects build on this entity
//-----------------------------------------------------------------------------
int	CInfoBuildPoint::GetNumObjectsOnMe( void )
{
	if ( m_hObjectBuiltOnMe )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get the first object that's built on me
//-----------------------------------------------------------------------------
CBaseEntity	*CInfoBuildPoint::GetFirstObjectOnMe( void )
{
	return m_hObjectBuiltOnMe;
}

//-----------------------------------------------------------------------------
// Purpose: Get the first object of type, return NULL if no such type available
//-----------------------------------------------------------------------------
CBaseObject *CInfoBuildPoint::GetObjectOfTypeOnMe( int iObjectType )
{
	if ( m_hObjectBuiltOnMe )
	{
		if ( m_hObjectBuiltOnMe->ObjectType() == iObjectType )
			return m_hObjectBuiltOnMe;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Remove all objects built on me
//-----------------------------------------------------------------------------
void CInfoBuildPoint::RemoveAllObjects( void )
{
	UTIL_Remove( m_hObjectBuiltOnMe );
}

//-----------------------------------------------------------------------------
// Purpose: Return the maximum distance that this entity's build points can be snapped to
//-----------------------------------------------------------------------------
float CInfoBuildPoint::GetMaxSnapDistance( int iPoint )
{
	return 64;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if it's possible that build points on this entity may move in local space (i.e. due to animation)
//-----------------------------------------------------------------------------
bool CInfoBuildPoint::ShouldCheckForMovement( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: I've finished building the specified object on the specified build point
//-----------------------------------------------------------------------------
int	CInfoBuildPoint::FindObjectOnBuildPoint( CBaseObject *pObject )
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Returns an exit point for a vehicle built on a build point...
//-----------------------------------------------------------------------------
void CInfoBuildPoint::GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	// FIXME: In future, we may well want to use specific exit attachments here...
	GetBuildPoint( iPoint, *pAbsOrigin, *pAbsAngles );

	// Move back along the forward direction a bit...
	Vector vecForward;
	AngleVectors( *pAbsAngles, &vecForward );
	*pAbsOrigin -= vecForward * 60;

	// Now select a good spot to drop onto
	Vector vNewPos;
	if ( !EntityPlacementTest(pPlayer, *pAbsOrigin, vNewPos, true) )
	{
		Warning("Can't find valid place to exit object.\n");
		return;
	}

	*pAbsOrigin = vNewPos;
}
