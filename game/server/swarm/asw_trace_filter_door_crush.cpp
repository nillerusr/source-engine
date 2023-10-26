#include "cbase.h"
#include "asw_trace_filter_door_crush.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "iservervehicle.h"
#include "player_pickup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHandleEntity - 
//			contentsMask - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Trace_Filter_Door_Crush::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		return false;

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	
	if ( pEntity )
	{
		//Msg("%f CASW_Trace_Filter_Door_Crush::ShouldHitEntity %s\n", gpGlobals->curtime, pEntity->GetClassname());
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		if ( pEntity->Classify() == CLASS_ASW_DOOR )
			return false;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			return false;

		// Translate the vehicle into its driver for damage
		if ( pEntity->GetServerVehicle() != NULL )
		{
			CBaseEntity *pDriver = pEntity->GetServerVehicle()->GetPassenger();

			if ( pDriver != NULL )
			{
				pEntity = pDriver;
			}
		}

		Vector	attackDir = pEntity->WorldSpaceCenter() - m_dmgInfo->GetAttacker()->WorldSpaceCenter();
		VectorNormalize( attackDir );
	
		pEntity->TakeDamage( *m_dmgInfo );

		//CalculateMeleeDamageForce( &info, attackDir, info.GetAttacker()->WorldSpaceCenter(), m_flForceScale );
		return true;
	}

	return false;
}