//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "baseobject_shared.h"
#include <KeyValues.h>
#include "tf_shareddefs.h"
#include "engine/ivmodelinfo.h"

//-----------------------------------------------------------------------------
// Purpose: Parse our model and create the buildpoints in it
//-----------------------------------------------------------------------------
void CBaseObject::CreateBuildPoints( void )
{
	// Clear out any existing build points
	m_BuildPoints.RemoveAll();

	KeyValues * modelKeyValues = new KeyValues("");
	if ( !modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		return;
	}

	// Do we have a build point section?
	KeyValues *pkvAllBuildPoints = modelKeyValues->FindKey("build_points");
	if ( pkvAllBuildPoints )
	{
		KeyValues *pkvBuildPoint = pkvAllBuildPoints->GetFirstSubKey();
		while ( pkvBuildPoint )
		{
			// Find the attachment first
			const char *sAttachment = pkvBuildPoint->GetName();
			int iAttachmentNumber = LookupAttachment( sAttachment );
			if ( iAttachmentNumber )
			{
				AddAndParseBuildPoint( iAttachmentNumber, pkvBuildPoint );
			}
			else
			{
				Msg( "ERROR: Model %s specifies buildpoint %s, but has no attachment named %s.\n", STRING(GetModelName()), pkvBuildPoint->GetString(), pkvBuildPoint->GetString() );
			}

			pkvBuildPoint = pkvBuildPoint->GetNextKey();
		}
	}

	// Any virtual build points (build points that aren't on an attachment)?
	pkvAllBuildPoints = modelKeyValues->FindKey("virtual_build_points");
	if ( pkvAllBuildPoints )
	{
		KeyValues *pkvBuildPoint = pkvAllBuildPoints->GetFirstSubKey();
		while ( pkvBuildPoint )
		{
			AddAndParseBuildPoint( -1, pkvBuildPoint );
			pkvBuildPoint = pkvBuildPoint->GetNextKey();
		}
	}

	modelKeyValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::AddAndParseBuildPoint( int iAttachmentNumber, KeyValues *pkvBuildPoint )
{
	int iPoint = AddBuildPoint( iAttachmentNumber );

	
	m_BuildPoints[iPoint].m_bPutInAttachmentSpace = (pkvBuildPoint->GetInt( "PutInAttachmentSpace", 0 ) != 0);

	// Now see if we've got a set of valid objects specified
	KeyValues *pkvValidObjects = pkvBuildPoint->FindKey( "valid_objects" );
	if ( pkvValidObjects )
	{
		KeyValues *pkvObject = pkvValidObjects->GetFirstSubKey();
		while ( pkvObject )
		{
			const char *pSpecifiedObject = pkvObject->GetName();
			int iLenObjName = Q_strlen( pSpecifiedObject );

			// Find the object index for the name
			for ( int i = 0; i < OBJ_LAST; i++ )
			{
				if ( !Q_strncmp( GetObjectInfo( i )->m_pClassName, pSpecifiedObject, iLenObjName) )
				{
					AddValidObjectToBuildPoint( iPoint, i );
					break;
				}
			}

			pkvObject = pkvObject->GetNextKey();
		}
	}

	SetBuildPointPassenger( iPoint, pkvBuildPoint->GetInt( "passenger", -1 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Add a new buildpoint to my list of buildpoints
//-----------------------------------------------------------------------------
int CBaseObject::AddBuildPoint( int iAttachmentNum )
{
	// Make a new buildpoint
	BuildPoint_t sNewPoint;
	sNewPoint.m_hObject = NULL;
	sNewPoint.m_iAttachmentNum = iAttachmentNum;
	sNewPoint.m_iPassenger = -1;
	sNewPoint.m_bPutInAttachmentSpace = false;
	Q_memset( sNewPoint.m_bValidObjects, 0, sizeof( sNewPoint.m_bValidObjects ) );

	// Insert it into our list
	return m_BuildPoints.AddToTail( sNewPoint );
}

//-----------------------------------------------------------------------------
// Purpose: Indicate which passenger position this build point is associated with
//-----------------------------------------------------------------------------
void CBaseObject::SetBuildPointPassenger( int iPoint, int iPassenger )
{
	m_BuildPoints[iPoint].m_iPassenger = iPassenger;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseObject::GetBuildPointPassenger( int iPoint ) const
{
	return m_BuildPoints[iPoint].m_iPassenger;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::AddValidObjectToBuildPoint( int iPoint, int iObjectType )
{
	Assert( iPoint <= GetNumBuildPoints() );
	m_BuildPoints[iPoint].m_bValidObjects[ iObjectType ] = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseObject::GetNumBuildPoints( void ) const
{
	return m_BuildPoints.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject* CBaseObject::GetBuildPointObject( int iPoint )
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );

	return m_BuildPoints[iPoint].m_hObject;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified object type can be built on this point
//-----------------------------------------------------------------------------
bool CBaseObject::CanBuildObjectOnBuildPoint( int iPoint, int iObjectType )
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );

	// Allowed to build here?
	if ( !m_BuildPoints[iPoint].m_bValidObjects[ iObjectType ] )
		return false;

	// Buildpoint empty?
	return ( m_BuildPoints[iPoint].m_hObject == NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseObject::GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles )
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );

	int iAttachmentNum = m_BuildPoints[iPoint].m_iAttachmentNum;
	if ( iAttachmentNum == -1 )
	{
		vecOrigin = GetAbsOrigin();
		vecAngles = GetAbsAngles();
		return true;
	}
	else
	{
		return GetAttachment( m_BuildPoints[iPoint].m_iAttachmentNum, vecOrigin, vecAngles );
	}
}


int CBaseObject::GetBuildPointAttachmentIndex( int iPoint ) const
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );

	if ( m_BuildPoints[iPoint].m_bPutInAttachmentSpace )
	{
		return m_BuildPoints[iPoint].m_iAttachmentNum;
	}
	else
	{
		return 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseObject::SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject )
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );
	m_BuildPoints[iPoint].m_hObject = pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBaseObject::GetMaxSnapDistance( int iPoint )
{
	Assert( iPoint >= 0 && iPoint <= GetNumBuildPoints() );

	if ( m_BuildPoints[iPoint].m_iAttachmentNum == -1 )
	{
		// Virtual build points need some more space since they represent an upgrade to the whole object.
		return 128;
	}
	else
	{
		return 128;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of objects on my build points
//-----------------------------------------------------------------------------
int CBaseObject::GetNumObjectsOnMe( void )
{
	int iObjects = 0;
	for ( int i = 0; i < GetNumBuildPoints(); i++ )
	{
		if ( m_BuildPoints[i].m_hObject )
		{
			iObjects++;
		}
	}

	return iObjects;
}

//-----------------------------------------------------------------------------
// Purpose: Return the first object build on this object
//-----------------------------------------------------------------------------
CBaseEntity *CBaseObject::GetFirstObjectOnMe( void )
{
	for ( int i = 0; i < GetNumBuildPoints(); i++ )
	{
		if ( m_BuildPoints[i].m_hObject )
			return m_BuildPoints[i].m_hObject;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// I've finished building the specified object on the specified build point
//-----------------------------------------------------------------------------
int CBaseObject::FindObjectOnBuildPoint( CBaseObject *pObject )
{
	for (int i = m_BuildPoints.Count(); --i >= 0; )
	{
		if (m_BuildPoints[i].m_hObject == pObject)
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject *CBaseObject::GetObjectOfTypeOnMe( int iObjectType )
{
	for ( int iObject = 0; iObject < GetNumObjectsOnMe(); ++iObject )
	{
		CBaseObject *pObject = dynamic_cast<CBaseObject*>( m_BuildPoints[iObject].m_hObject.Get() );
		if ( pObject )
		{
			if ( pObject->GetType() == iObjectType )
				return pObject;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::RemoveAllObjects( void )
{
	for ( int i = 0; i < GetNumBuildPoints(); i++ )
	{
		if ( m_BuildPoints[i].m_hObject )
		{
#ifndef CLIENT_DLL
			UTIL_Remove( m_BuildPoints[i].m_hObject );
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject	*CBaseObject::GetParentObject( void )
{
	if ( GetMoveParent() )
		return dynamic_cast<CBaseObject*>(GetMoveParent());

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if I ever accept this powerup type
//-----------------------------------------------------------------------------
bool CBaseObject::CanPowerupEver( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	// Un-repairable objects can't be boosted
	switch( iPowerup )
	{
#ifndef CLIENT_DLL
	case POWERUP_BOOST:
		if ( !m_flRepairMultiplier )
			return false;
		break;
#endif

	case POWERUP_POWER:
		// Do we use power?
		if ( m_fObjectFlags & OF_DOESNT_NEED_POWER )
			return false;

		// Objects on vehicles never need power
		if ( IsBuiltOnAttachment() )
		{
			if ( GetParentObject() && GetParentObject()->IsAVehicle() )
				return false;
		}

		// Ok, I'll be needing some juice
		return true;

	default:
		break;
	}

	// Don't accept any powerups if we're placing
	if ( IsPlacing() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if I can be powered by this powerup right now
//-----------------------------------------------------------------------------
bool CBaseObject::CanPowerupNow( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	if ( !CanPowerupEver(iPowerup) )
		return false;

	// Un-repairable objects can't be boosted
	switch( iPowerup )
	{
	case POWERUP_POWER:
		// If I have power, I don't need it
		if ( IsPowered() )
			return false;
		return true;

	default:
		break;
	}

	// Don't accept any powerups if we're placing
	if ( IsPlacing() )
		return false;

#ifdef CLIENT_DLL
	return true;
#else
	return BaseClass::CanPowerupEver( iPowerup );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object has power
//-----------------------------------------------------------------------------
bool CBaseObject::IsPowered( void )
{
	if ( !CanPowerupEver( POWERUP_POWER ) )
		return true;

#ifdef CLIENT_DLL
	return ( HasPowerup(POWERUP_POWER) );
#else
	return ( HasPowerup(POWERUP_POWER) || m_hPowerPack );
#endif
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBaseObject::GetSapperAttachTime( void )
{
	return GetObjectInfo( GetType() )->m_flSapperAttachTime;
}

static ConVar sv_ignore_hitboxes( "sv_ignore_hitboxes", "0", FCVAR_REPLICATED, "Disable hitboxes" );

bool CBaseObject::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	bool bReturn = BaseClass::TestHitboxes( ray, fContentsMask, tr );

	if( !sv_ignore_hitboxes.GetBool() )
		return bReturn;


	if( !bReturn )
	{
		return false;
	}

	if( tr.fraction == 1.f  && !tr.allsolid && !tr.startsolid )
	{
		return false;
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object should be active
//-----------------------------------------------------------------------------
bool CBaseObject::ShouldBeActive( void )
{
	// Placing and/or constructing objects shouldn't be active
	if ( IsPlacing() || IsBuilding() )
		return false;

	// Powered? Or don't need it
	if ( CanPowerupEver(POWERUP_POWER) && !HasPowerup( POWERUP_POWER ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the object's type
//-----------------------------------------------------------------------------
void CBaseObject::SetType( int iObjectType )
{
	m_iObjectType = iObjectType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : act - 
//-----------------------------------------------------------------------------
void CBaseObject::SetActivity( Activity act ) 
{ 
	// Allow any model swapping, etc. to occur
	OnActivityChanged( act );

	// Hrm, it's not actually a studio model...
	if ( !GetModelPtr() )
		return;

	int sequence = SelectWeightedSequence( act ); 
	if ( sequence != ACTIVITY_NOT_AVAILABLE )
	{
		m_Activity = act; 
		SetObjectSequence( sequence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBaseObject::GetActivity( ) const
{ 
	return m_Activity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : act - 
//-----------------------------------------------------------------------------
void CBaseObject::OnActivityChanged( Activity act )
{
	// Nothing
}

//-----------------------------------------------------------------------------
// Purpose: Thin wrapper over CBaseAnimating::SetSequence to do bookkeeping.
// Input  : sequence - 
//-----------------------------------------------------------------------------
void CBaseObject::SetObjectSequence( int sequence )
{
	ResetSequence( sequence );
	SetCycle( 0 );

#if !defined( CLIENT_DLL )
	if ( IsUsingClientSideAnimation() )
	{
		ResetClientsideFrame();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::AttemptToGoActive( void )
{
	// Go active if we can
	if ( ShouldBeActive() )
	{
		OnGoActive();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::OnGoActive( void )
{
#ifndef CLIENT_DLL
	// Play startup animation
	PlayStartupAnimation();

	// Switch to the on state
	if ( GetModelPtr() )
	{
		int index = FindBodygroupByName( "powertoggle" );
		if ( index >= 0 )
		{
			SetBodygroup( index, 1 );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::OnGoInactive( void )
{
#ifndef CLIENT_DLL
	if ( GetModelPtr() )
	{
		// Switch to the off state
		int index = FindBodygroupByName( "powertoggle" );
		if ( index >= 0 )
		{
			SetBodygroup( index, 0 );
		}
	}
#endif
}