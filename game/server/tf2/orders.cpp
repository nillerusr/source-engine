//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Order handling
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "orders.h"
#include "tf_player.h"
#include "tf_func_resource.h"
#include "tf_team.h"
#include "tf_obj_resourcepump.h"


IMPLEMENT_SERVERCLASS_ST(COrder, DT_Order)
	SendPropInt( SENDINFO(m_iOrderType), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_iTargetEntIndex), 16, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( tf_order, COrder );


COrder::COrder()
{
	m_iOrderType = 0;
	m_iTargetEntIndex = 0;
	m_hTarget = NULL;
	m_flDistanceToRemove = 0;
	m_hOwningPlayer = NULL;
	m_flDieTime = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COrder::UpdateOnRemove( void )
{
	DetachFromPlayer();
	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Transmit weapon data
//-----------------------------------------------------------------------------
int COrder::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	CBaseEntity* pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );

	// If this is a personal order, only send to it's owner
	if ( GetOwner() )
	{
		if ( GetOwner() == pRecipientEntity )
			return FL_EDICT_ALWAYS;

		return FL_EDICT_DONTSEND;
	}

	// Otherwise, only send to players on our team
	if ( InSameTeam( pRecipientEntity ) )
		return FL_EDICT_ALWAYS;

	return FL_EDICT_DONTSEND;
}


void COrder::DetachFromPlayer()
{
	// Detach from our owner.
	if ( m_hOwningPlayer )
	{
		m_hOwningPlayer->SetOrder( NULL );
		m_hOwningPlayer = NULL;

		if ( GetTeam() )
		{
			((CTFTeam*)GetTeam())->RemoveOrder( this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int COrder::GetType( void )
{
	return m_iOrderType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *COrder::GetTargetEntity( void )
{
	return m_hTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COrder::SetType( int iOrderType )
{
	m_iOrderType = iOrderType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COrder::SetTarget( CBaseEntity *pTarget )
{
	m_hTarget = pTarget;
	if ( m_hTarget )
	{
		m_iTargetEntIndex = m_hTarget->entindex();
	}
	else
	{
		m_iTargetEntIndex = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COrder::SetDistance( float flDistance )
{
	m_flDistanceToRemove = flDistance;
}


void COrder::SetLifetime( float flLifetime )
{
	m_flDieTime = gpGlobals->curtime + flLifetime;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: for updates on the order. Return true if the order should be removed.
//-----------------------------------------------------------------------------
bool COrder::Update( void )
{
	// Orders with no targets & no owners don't go away on their own
	if ( !GetOwner() )
		return false;

	// Has it timed out?
	if( gpGlobals->curtime > m_flDieTime )
		return true;

	// Check to make sure we're still within the correct distance
	if ( m_flDistanceToRemove )
	{
		CBaseEntity *pTarget = GetTargetEntity();
		if ( pTarget )
		{
			// Have the player and the target moved away from each other?
			if ( (m_hOwningPlayer->GetAbsOrigin() - pTarget->GetAbsOrigin()).Length() > (m_flDistanceToRemove * 1.25) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: An event for this order's target has arrived. Return true if this order should be removed.
//-----------------------------------------------------------------------------
bool COrder::UpdateOnEvent( COrderEvent_Base *pEvent )
{
	// Default behavior is to get rid of the order if the object we're referencing
	// gets destroyed.
	if ( pEvent->GetType() == ORDER_EVENT_OBJECT_DESTROYED )
	{
		COrderEvent_ObjectDestroyed *pObjDestroyed = (COrderEvent_ObjectDestroyed*)pEvent;
		if ( pObjDestroyed->m_pObject == GetTargetEntity() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTFPlayer *COrder::GetOwner( void )
{
	return m_hOwningPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COrder::SetOwner( CBaseTFPlayer *pPlayer )
{
	// Null out our m_hOwningPlayer so we don't recurse infinitely.
	CHandle<CBaseTFPlayer> hPlayer = m_hOwningPlayer;
	m_hOwningPlayer = 0;

	if ( hPlayer.Get() && (hPlayer != pPlayer) )
	{
		Assert( hPlayer->GetOrder() == this );
		hPlayer->SetOrder( NULL );	
	}
	
	m_hOwningPlayer = pPlayer;
}






