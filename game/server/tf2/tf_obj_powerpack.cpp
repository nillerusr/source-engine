//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Human's power pack
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_obj_powerpack.h"
#include "tf_func_resource.h"
#include "resource_chunk.h"
#include "techtree.h"
#include "sendproxy.h"
#include "vstdlib/random.h"
#include "tf_stats.h"
#include "rope.h"
#include "tf_shareddefs.h"
#include "VGuiScreen.h"
#include "hierarchy.h"

#define POWERPACK_MODEL	"models/objects/human_obj_powerpack.mdl"
#define POWERPACK_ASSEMBLING_MODEL	"models/objects/human_obj_powerpack_build.mdl"

IMPLEMENT_SERVERCLASS_ST( CObjectPowerPack, DT_ObjectPowerPack )
	SendPropInt( SENDINFO(m_iObjectsAttached), 3, SPROP_UNSIGNED ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_powerpack, CObjectPowerPack);
PRECACHE_REGISTER(obj_powerpack);

ConVar	obj_powerpack_health( "obj_powerpack_health","100", FCVAR_NONE, "Human powerpack health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectPowerPack::CObjectPowerPack()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectPowerPack::Spawn( void )
{
	SetModel( POWERPACK_MODEL );
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, POWERPACK_MINS, POWERPACK_MAXS);

	m_iHealth = obj_powerpack_health.GetInt();
	m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	SetType( OBJ_POWERPACK );
	m_hPoweredObjects.Purge();
	m_iFreeAttachments = 0;
	m_iObjectsAttached = 0;

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectPowerPack::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_power_pack";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectPowerPack::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	// Now tell all our objects we connected to, during placement, that they're really getting power
	// Walk backwards, because we might remove objects from our list that have somehow gained power
	// inbetween the time we placed and the time we finished building.
	int iSize = m_hPoweredObjects.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		if ( m_hPoweredObjects[i] )
		{
			if ( m_hPoweredObjects[i]->IsPowered() )
			{
				UnPowerObject( m_hPoweredObjects[i] );
			}
			else
			{
				m_hPoweredObjects[i]->SetPowerPack( this );
			}
		}
	}

	PowerNearbyObjects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectPowerPack::Precache()
{
	BaseClass::Precache();
	PrecacheModel( POWERPACK_MODEL );
	PrecacheModel( POWERPACK_ASSEMBLING_MODEL );
	PrecacheVGuiScreen( "screen_obj_power_pack" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectPowerPack::DestroyObject( void )
{
	// Remove power from all my objects (backwards because list will change)
	int iSize = m_hPoweredObjects.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		if ( m_hPoweredObjects[i] )
		{
			UnPowerObject( m_hPoweredObjects[i] );
		}
	}

	// Now tell all other powerpacks on this team to power nearby objects, in case they can cover for this one.
	if ( GetTFTeam() )
	{
		GetTFTeam()->UpdatePowerpacks( this, NULL );
	}

	BaseClass::DestroyObject();
}

//-----------------------------------------------------------------------------
// Purpose: Update power connections on the fly while placing
//-----------------------------------------------------------------------------
bool CObjectPowerPack::CalculatePlacement( CBaseTFPlayer *pPlayer )
{
	bool bReturn = BaseClass::CalculatePlacement( pPlayer );

	// First, disconnect any connections that should break
	int iSize = m_hPoweredObjects.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		if ( m_hPoweredObjects[i] )
		{
			EnsureObjectPower( m_hPoweredObjects[i] );
		}
	}

	// If we have any spare connections, look for nearby objects to power
	if ( m_hPoweredObjects.Count() < MAX_OBJECTS_PER_PACK )
	{
		PowerNearbyObjects( NULL, true );
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: Find nearby objects and provide them with power
//-----------------------------------------------------------------------------
void CObjectPowerPack::PowerNearbyObjects( CBaseObject *pObjectToTarget, bool bPlacing )
{
	if ( !GetTFTeam() )
		return;
	// Am I ready to power anything?
	if ( IsBuilding() || (!bPlacing && IsPlacing()) )
		return;

	// Am I already full?
	if ( m_hPoweredObjects.Count() >= MAX_OBJECTS_PER_PACK )
		return;

	// Do we have a specific target?
	if ( pObjectToTarget )
	{
		if ( !pObjectToTarget->CanPowerupNow(POWERUP_POWER) )
			return;

		if ( IsWithinPowerRange( pObjectToTarget ) )
		{
			PowerObject( pObjectToTarget );
		}
	}
	else
	{
		// Find nearby objects 
		for ( int i = 0; i < GetTFTeam()->GetNumObjects(); i++ )
		{
			CBaseObject *pObject = GetTFTeam()->GetObject(i);
			assert(pObject);
			if ( pObject == this || !pObject->CanPowerupNow(POWERUP_POWER) )
				continue;
			// We might be rechecking our power because one of our own objects is dying.
			// Make sure we don't re-attach the sucker.
			if ( pObject->IsDying() )
				continue;

			// Make sure it's within range
			if ( IsWithinPowerRange( pObject ) )
			{
				PowerObject( pObject, bPlacing );
			}

			// Am I now full?
			if ( m_hPoweredObjects.Count() >= MAX_OBJECTS_PER_PACK )
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Provide power to the specified object
//-----------------------------------------------------------------------------
void CObjectPowerPack::PowerObject( CBaseObject *pObject, bool bPlacing )
{
	// Make sure we're not already powering it
	ObjectHandle hObject;
	hObject = pObject;
	if ( m_hPoweredObjects.Find( hObject ) != m_hPoweredObjects.InvalidIndex() )
		return;

	// Add it to our list
	m_hPoweredObjects.AddToTail( hObject );
	m_iObjectsAttached = m_hPoweredObjects.Count();

	// Find a free attachment point
	int iPoint = 1;
	for ( int i = 0; i < MAX_OBJECTS_PER_PACK; i++ )
	{
		if ( !(m_iFreeAttachments & (1<<i)) )
		{
			m_iFreeAttachments |= (1<<i);
			iPoint = i+1;
			break;
		}
	}

	// Lookup the attachment point...
	int nAttachmentIndex = pObject->LookupAttachment("powerpoint");
	if (nAttachmentIndex < 0)
		nAttachmentIndex = 1;
							   
	// FIXME: Cache these off
	char sAttachment[32];
	Q_snprintf( sAttachment,sizeof(sAttachment), "cablepoint%d", iPoint );
	int nLocalAttachment = LookupAttachment( sAttachment );
	if ( nLocalAttachment > 0 )
	{
		// Throw a cable to it
		CRopeKeyframe *pRope = ConnectCableTo( pObject, nLocalAttachment, nAttachmentIndex );
		if ( pRope )
		{
			pRope->SetMaterial( "cable/human_powercable.vmt" );
		}
	}

	// If we're placing only, don't tell it we're supplying power yet
	if ( IsPlacing() )
		return;

	pObject->SetPowerPack( this );
}

//-----------------------------------------------------------------------------
// Purpose: Remove power to the specified object
//-----------------------------------------------------------------------------
void CObjectPowerPack::UnPowerObject( CBaseObject *pObject )
{
	// Make sure it's in our list
	ObjectHandle hObject;
	hObject = pObject;
	if ( m_hPoweredObjects.Find( hObject ) == m_hPoweredObjects.InvalidIndex() )
		return;

	// Remove it from our list
	m_hPoweredObjects.FindAndRemove( hObject );
	m_iObjectsAttached = m_hPoweredObjects.Count();

	// Remove our cable to it
	for ( int i = 0; i < m_aRopes.Count(); i++ )
	{
		if ( (m_aRopes[i] != NULL) && (m_aRopes[i]->GetEndPoint() == pObject) )
		{
			// Free up the attachment point
			m_iFreeAttachments &= ~(1 << (m_aRopes[i]->GetEndAttachment()-1));
			UTIL_Remove( m_aRopes[i] );
			m_aRopes.Remove(i);
			break;
		}
	}

	// Tell the object that it has lost power
	if ( pObject->GetPowerPack() == this )
	{
		pObject->SetPowerPack( NULL );
	}

	// If I'm not dying, immediately look for other things to power
	if ( !IsDying() )
	{
		PowerNearbyObjects();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make sure the specified object is still within powering range
//-----------------------------------------------------------------------------
void CObjectPowerPack::EnsureObjectPower( CBaseObject *pObject )
{
	if ( IsWithinPowerRange( pObject ) )
		return;

	// It's obscured, or out of range. Remove it.
	UnPowerObject( pObject );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object is powerable
//-----------------------------------------------------------------------------
bool CObjectPowerPack::IsWithinPowerRange( CBaseObject *pObject )
{
	// If this powerpack is built on an attachment, it'll only power objects in the same hierarchy
	if ( GetParentObject() )
	{
		if ( GetRootMoveParent() != pObject->GetRootMoveParent() )
			return false;
	}

	if ( (pObject->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < POWERPACK_RANGE )
	{
		// Can I see it?
		// Ignore things we're attached to
		trace_t tr;
		CTraceFilterWorldAndPropsOnly powerFilter;
		UTIL_TraceLine( WorldSpaceCenter(), pObject->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, &powerFilter, &tr );
		CBaseEntity *pEntity = tr.m_pEnt;
		if ( (tr.fraction == 1.0) || ( pEntity == pObject ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : act - 
//-----------------------------------------------------------------------------
void CObjectPowerPack::OnActivityChanged( Activity act )
{
	BaseClass::OnActivityChanged( act );

	switch ( act )
	{
	case ACT_OBJ_ASSEMBLING:
		SetModel( POWERPACK_ASSEMBLING_MODEL );
		break;
	default:
		SetModel( POWERPACK_MODEL );
		break;
	}
}
