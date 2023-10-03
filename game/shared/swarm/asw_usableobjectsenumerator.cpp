#include "cbase.h"
#include "asw_usableobjectsenumerator.h"

#ifdef CLIENT_DLL
	#include "c_ai_basenpc.h"
	#include "c_asw_marine.h"
	#include "c_asw_player.h"
	#include "c_asw_use_area.h"
	#include "c_asw_pickup.h"
	#include "c_asw_sentry_base.h"
	#include "iasw_client_vehicle.h"
	#include "iasw_client_usable_entity.h"
	#include "engine/IVDebugOverlay.h"
#else
	#include "ai_basenpc.h"
	#include "asw_marine.h"
	#include "asw_player.h"
	#include "asw_use_area.h"
	#include "asw_pickup.h"
	#include "asw_sentry_base.h"
	#include "iasw_vehicle.h"
	#include "iasw_server_usable_entity.h"
	#define C_ASW_Use_Area CASW_Use_Area
	#define C_ASW_Pickup CASW_Pickup
	#define C_ASW_Sentry_Base CASW_Sentry_Base
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
extern ConVar asw_debug_hud;
#endif

// Enumator class for ragdolls being affected by explosive forces
CASW_UsableObjectsEnumerator::CASW_UsableObjectsEnumerator( float radius, C_ASW_Player *pPlayer )
{
	m_flRadiusSquared		= radius * radius;
	m_Objects.RemoveAll();
	m_pLocal = pPlayer;
}

int	CASW_UsableObjectsEnumerator::GetObjectCount()
{
	return m_Objects.Count();
}

C_BaseEntity *CASW_UsableObjectsEnumerator::GetObject( int index )
{
	if ( index < 0 || index >= GetObjectCount() )
		return NULL;

	return m_Objects[ index ];
}

// Actual work code
IterationRetval_t CASW_UsableObjectsEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	if ( !m_pLocal )
		return ITERATION_STOP;

#ifndef CLIENT_DLL
		CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#else
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
#endif
	
	if ( pEnt == NULL )
		return ITERATION_CONTINUE;

	if ( pEnt == m_pLocal )
		return ITERATION_CONTINUE;

	C_ASW_Marine *pMarine = m_pLocal->GetMarine();
	if (!pMarine)
		return ITERATION_CONTINUE;

#ifdef CLIENT_DLL
	IASW_Client_Usable_Entity *pUsable = dynamic_cast<IASW_Client_Usable_Entity*>(pEnt);
#else
	IASW_Server_Usable_Entity *pUsable = dynamic_cast<IASW_Server_Usable_Entity*>(pEnt);
#endif

	if (!pUsable || !pUsable->IsUsable(pMarine))
		return ITERATION_CONTINUE;	

	if (pUsable->NeedsLOSCheck())
	{
		trace_t tr;
		Vector vecSrc = pMarine->WorldSpaceCenter();
		Vector vecDest = pEnt->WorldSpaceCenter();
		UTIL_TraceLine(vecSrc, vecDest, MASK_SOLID_BRUSHONLY, pMarine, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction < 1.0f && tr.m_pEnt != pEnt)	// didn't hit our target
		{
#ifdef CLIENT_DLL
			if (asw_debug_hud.GetBool())
				debugoverlay->AddLineOverlay(vecSrc, tr.endpos, 255, 0, 0, true, 0.1f);
#endif
			float dist = (vecSrc - vecDest).Length2D();
			if (dist > 30)	// and too far away
				return ITERATION_CONTINUE;
		}
#ifdef CLIENT_DLL
		if (asw_debug_hud.GetBool())
			debugoverlay->AddLineOverlay(vecSrc, tr.endpos, 0, 255, 0, true, 0.1f);
#endif
	}

	CHandle< C_BaseEntity > h;
	h = pEnt;
	m_Objects.AddToTail( h );

	return ITERATION_CONTINUE;
}