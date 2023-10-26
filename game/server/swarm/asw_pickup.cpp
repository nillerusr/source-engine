#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "ammodef.h"
#include "eventlist.h"
#include "npcevent.h"
#include "asw_pickup.h"
#include "asw_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_SERVERCLASS_ST(CASW_Pickup, DT_ASW_Pickup)
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Pickup )
	DEFINE_KEYFIELD(m_bFreezePickup, FIELD_BOOLEAN, "FreezePickup")
END_DATADESC()

#define ITEM_PICKUP_BOX_BLOAT		24

// as CItem, but we don't install the touch function
void CASW_Pickup::Spawn( void )
{
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );
	SetBlocksLOS( false );
	AddEFlags( EFL_NO_ROTORWASH_PUSH );
	
	// This will make them not collide with the player, but will collide
	// against other items + weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT );
	//SetTouch(&CItem::ItemTouch);

	if ( CreateItemVPhysicsObject() == false )
		 return;
	
	m_takedamage = DAMAGE_EVENTS_ONLY;

#ifdef HL2MP
	SetThink( &CItem::FallThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
#endif
	
	if ( m_bFreezePickup )
	{
		IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
		if ( pPhysicsObject != NULL )
		{
			pPhysicsObject->EnableMotion( false );
		}
	}
}

void CASW_Pickup::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == 1 /*ASW_USE_HOLD_START*/ )
		return;

	// player has used this item
	Msg("Player has activated a pickup\n");
}

bool CASW_Pickup::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}