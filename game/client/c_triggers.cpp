//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_triggers.h"
#include "in_buttons.h"
#include "c_func_brush.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_BaseTrigger, DT_BaseTrigger, CBaseTrigger )
	RecvPropBool( RECVINFO( m_bClientSidePredicted ) ),
	RecvPropInt( RECVINFO( m_spawnflags ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Disables auto movement on players that touch it
//-----------------------------------------------------------------------------
class C_TriggerPlayerMovement : public C_BaseTrigger
{
	DECLARE_CLASS( C_TriggerPlayerMovement, C_BaseTrigger );
	DECLARE_CLIENTCLASS();
public:

	C_TriggerPlayerMovement();
	~C_TriggerPlayerMovement();

	void StartTouch( C_BaseEntity *pOther );
	void EndTouch( C_BaseEntity *pOther );

protected:

	virtual void UpdatePartitionListEntry();

public:
	C_TriggerPlayerMovement	*m_pNext;
};

IMPLEMENT_CLIENTCLASS_DT( C_TriggerPlayerMovement, DT_TriggerPlayerMovement, CTriggerPlayerMovement )
END_RECV_TABLE()

C_EntityClassList< C_TriggerPlayerMovement > g_TriggerPlayerMovementList;
C_TriggerPlayerMovement *C_EntityClassList<C_TriggerPlayerMovement>::m_pClassList = NULL;

C_TriggerPlayerMovement::C_TriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Insert( this );
}

C_TriggerPlayerMovement::~C_TriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Remove( this );
}

//-----------------------------------------------------------------------------
// Little enumeration class used to try touching all triggers
//-----------------------------------------------------------------------------
template< class T >
class CFastTouchTriggers
{
public:
	CFastTouchTriggers( C_BaseEntity *pEnt, T *pTriggers ) : m_pEnt( pEnt ), m_pTriggers( pTriggers )
	{
		m_pCollide = pEnt->GetCollideable();
		Assert( m_pCollide );

		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( m_pCollide, &vecMins, &vecMaxs );
		const Vector &vecStart = m_pCollide->GetCollisionOrigin();
		m_Ray.Init( vecStart, vecStart, vecMins, vecMaxs );
	}

	FORCEINLINE void CM_GetCollideableTriggerTestBox( ICollideable *pCollide, Vector *pMins, Vector *pMaxs )
	{
		if ( pCollide->GetSolid() == SOLID_BBOX )
		{
			*pMins = pCollide->OBBMins();
			*pMaxs = pCollide->OBBMaxs();
		}
		else
		{
			const Vector &vecStart = pCollide->GetCollisionOrigin();
			pCollide->WorldSpaceSurroundingBounds( pMins, pMaxs );
			*pMins -= vecStart;
			*pMaxs -= vecStart;
		}
	}

	FORCEINLINE void Check( T *pEntity )
	{
		// Hmmm.. everything in this list should be a trigger....
		ICollideable *pTriggerCollideable = pEntity->GetCollideable();
		if ( !m_pCollide->ShouldTouchTrigger(pTriggerCollideable->GetSolidFlags()) )
			return;

		if ( pTriggerCollideable->GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS )
		{
			Vector vecTriggerMins, vecTriggerMaxs;
			pTriggerCollideable->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
			if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, m_Ray ) )
			{
				return;
			}
		}
		else
		{
			trace_t tr;
			enginetrace->ClipRayToCollideable( m_Ray, MASK_SOLID, pTriggerCollideable, &tr );
			if ( !(tr.contents & MASK_SOLID) )
				return;
		}

		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (m_pEnt->GetAbsOrigin() + pEntity->GetAbsOrigin()) * 0.5;
		m_pEnt->PhysicsMarkEntitiesAsTouching( pEntity, tr );
	}

	FORCEINLINE void Run()
	{
		for ( T *trigger = m_pTriggers; trigger ; trigger = trigger->m_pNext )
		{
			if ( trigger->IsDormant() )
				continue;
			Check( trigger );
		}
	}

	Ray_t m_Ray;

private:
	C_BaseEntity			*m_pEnt;
	ICollideable			*m_pCollide;
	T						*m_pTriggers;
};

void TouchTriggerPlayerMovement( C_BaseEntity *pEntity )
{
	CFastTouchTriggers< C_TriggerPlayerMovement > helper( pEntity, g_TriggerPlayerMovementList.m_pClassList );
	helper.Run();
}


void C_TriggerPlayerMovement::UpdatePartitionListEntry()
{
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	partition->RemoveAndInsert( 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

void C_TriggerPlayerMovement::StartTouch( C_BaseEntity *pOther )
{	
	C_BasePlayer *pPlayer = ToBasePlayer( pOther );

	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->ForceButtons( IN_DUCK );
	}

	// UNDONE: Currently this is the only operation this trigger can do
	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = false;
	}
}

void C_TriggerPlayerMovement::EndTouch( C_BaseEntity *pOther )
{
	C_BasePlayer *pPlayer = ToBasePlayer( pOther );
	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->UnforceButtons( IN_DUCK );
	}

	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = true;
	}
}

