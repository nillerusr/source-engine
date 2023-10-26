#include "cbase.h"
#include "asw_trace_filter_shot.h"
#include "asw_marine_profile.h"
#include "asw_melee_system.h"
#ifdef GAME_DLL
#include "asw_marine.h"
#include "asw_flamer_projectile.h"
#include "asw_extinguisher_projectile.h"
#else
#include "c_asw_marine.h"
#include "c_asw_flamer_projectile.h"
#include "c_asw_extinguisher_projectile.h"
#define CASW_Flamer_Projectile C_ASW_Flamer_Projectile
#define CASW_Extinguisher_Projectile C_ASW_Extinguisher_Projectile
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Trace filter that skips two entities
//-----------------------------------------------------------------------------
CASWTraceFilterShot::CASWTraceFilterShot( const IHandleEntity *passentity, const IHandleEntity *passentity2, int collisionGroup ) :
	BaseClass( passentity, collisionGroup ), m_pPassEnt2(passentity2)
{
	m_bSkipMarines = true;
	m_bSkipRollingMarines = false;
	m_bSkipMarinesReflectingProjectiles = false;
	m_bSkipAliens = false;
}

bool CASWTraceFilterShot::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	Assert( pHandleEntity );
	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt2 ) )
		return false;
	
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

	// don't collide with other projectiles
	if ( dynamic_cast<CASW_Flamer_Projectile*>( pEntity ) != NULL )
		return false;

	if ( dynamic_cast<CASW_Extinguisher_Projectile*>( pEntity ) != NULL )
		return false;

	if ( pEntity && pEntity->Classify() == CLASS_ASW_MARINE )
	{
		if ( m_bSkipMarines )
			return false;

		CASW_Marine *pMarine = assert_cast<CASW_Marine*>( pEntity );
		if ( m_bSkipRollingMarines && pMarine->GetCurrentMeleeAttack() && pMarine->GetCurrentMeleeAttack()->m_nAttackID == CASW_Melee_System::s_nRollAttackID )
			return false;

		if ( m_bSkipMarinesReflectingProjectiles && pMarine->IsReflectingProjectiles() )
			return false;
	}

	if ( m_bSkipAliens && pEntity && IsAlienClass( pEntity->Classify() ) )
		return false;

	return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
}