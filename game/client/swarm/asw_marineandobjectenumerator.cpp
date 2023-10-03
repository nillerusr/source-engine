#include "cbase.h"
#include "asw_marineandobjectenumerator.h"
#include "c_ai_basenpc.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Enumator class for finding other marines and objects close to the
//  local player's marine
CASW_MarineAndObjectEnumerator::CASW_MarineAndObjectEnumerator( float radius )
{
	m_flRadiusSquared		= radius * radius;
	m_Objects.RemoveAll();
	m_pLocal = C_ASW_Player::GetLocalASWPlayer();
}

int	CASW_MarineAndObjectEnumerator::GetObjectCount()
{
	return m_Objects.Count();
}

C_BaseEntity *CASW_MarineAndObjectEnumerator::GetObject( int index )
{
	if ( index < 0 || index >= GetObjectCount() )
		return NULL;

	return m_Objects[ index ];
}

// Actual work code
IterationRetval_t CASW_MarineAndObjectEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	if ( !m_pLocal )
		return ITERATION_STOP;

	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
	if ( pEnt == NULL )
		return ITERATION_CONTINUE;

	if ( pEnt == m_pLocal )
		return ITERATION_CONTINUE;

	if ( !pEnt->IsPlayer() &&
		 !pEnt->IsNPC() )
	{
		return ITERATION_CONTINUE;
	}

	if ( pEnt->IsNPC() )
	{
		C_AI_BaseNPC *pNPC = (C_AI_BaseNPC *)pEnt;

		if ( !pNPC->ShouldAvoidObstacle() )
			 return ITERATION_CONTINUE;
	}

	// Ignore vehicles, since they have vcollide collisions that's push me away
	if ( pEnt->GetCollisionGroup() == COLLISION_GROUP_VEHICLE )
		return ITERATION_CONTINUE;

#ifdef TF2_CLIENT_DLL
	// If it's solid to player movement, don't steer around it since we'll just bump into it
	if ( pEnt->GetCollisionGroup() == TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT )
		return ITERATION_CONTINUE;
#endif

	C_ASW_Marine *pMarine = m_pLocal->GetMarine();
	if (!pMarine)
		return ITERATION_CONTINUE;

	Vector	deltaPos = pEnt->GetAbsOrigin() - pMarine->GetAbsOrigin();
	//if ( deltaPos.LengthSqr() > m_flRadiusSquared )
		//return ITERATION_CONTINUE;

	CHandle< C_BaseEntity > h;
	h = pEnt;
	m_Objects.AddToTail( h );

	return ITERATION_CONTINUE;
}