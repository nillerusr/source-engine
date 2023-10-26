#include "cbase.h"
#ifdef CLIENT_DLL
	#include "c_asw_shieldbug.h"
	#define CASW_Shieldbug C_ASW_Shieldbug
#else
	#include "asw_shieldbug.h"
	#include "asw_fail_advice.h"
#endif
#include "takedamageinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
extern ConVar asw_debug_shieldbug;
#endif

bool CASW_Shieldbug::BlockedDamage( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
// 	if (IsMeleeAttacking())
// 	{
// 		if (asw_debug_shieldbug.GetBool())
// 			Msg("Not blocking as I'm meleeing\n");
// 		return false;
// 	}

	if ( ptr->hitgroup == HITGROUP_BONE_SHIELD )
		return true;

#if 0
	Vector sparkNormal;
	Vector vecDamagePos = info.GetDamagePosition();
	if ( info.GetAttacker() && vecDamagePos != vec3_origin )
	{
		vecDamagePos = info.GetAttacker()->GetAbsOrigin();	// use the attacker's position when determining block, to stop marines shooting through gaps and hurting the sbug from any angle
	}

	if ( vecDamagePos == vec3_origin )		// don't block non-locational damage (i.e. fire burning)
		return false;

	// if we're in the middle of meleeing, don't block damage
	if ( IsMeleeAttacking() )
	{
#ifdef GAME_DLL
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "Not blocking as I'm meleeing\n" );
		}
#endif
		return false;
	}

	sparkNormal = vecDamagePos - GetAbsOrigin();	// should be head pos
	VectorNormalize( sparkNormal );
	Vector vecFacing;
	AngleVectors( GetAbsAngles(), &vecFacing );
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	sparkNormal.z = 0;
	VectorNormalize( sparkNormal );
	float dot = vecFacing.Dot( sparkNormal );

#ifdef GAME_DLL
	if ( asw_debug_shieldbug.GetBool() )
	{
		Msg("Defending dot %f\n", dot);
	}
	if (dot > 0)
	{
		if ( asw_debug_shieldbug.GetBool() )
		{
			Msg( "  blocked damage\n" );
		}
		return true;
	}
	if ( asw_debug_shieldbug.GetBool() )
	{
		Msg( "  Not blocked dmg\n" );
	}
#else
	if ( dot > 0 )
	{
		return true;
	}
#endif

#endif
	return false;
}

void CASW_Shieldbug::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	//Msg( "Shieldbug hit on hitbox %d hitgroup %d\n", ptr->hitbox, ptr->hitgroup );

	if ( info.GetDamageType() != DMG_BLAST && BlockedDamage( info, vecDir, ptr ) )	// m_bDefending && 
	{
#ifdef GAME_DLL
		m_fLastHurtTime = gpGlobals->curtime;
		CheckForShieldbugHint(info);
		ASWFailAdvice()->OnShiedbugBlocked();
#endif
		/*
		Vector position = info.GetDamagePosition() + m_LagCompensation.GetLagCompensationOffset();		

		// make sure the position is actually at our edge
		
		Vector vecPosDir = position - GetAbsOrigin();
		vecPosDir.NormalizeInPlace();
		position = GetAbsOrigin() + vecPosDir * 40.0f;
		*/
		Vector vecSparkPos = ptr->endpos;
		Vector vecSparkDir = -vecDir;
		CPVSFilter filter( vecSparkPos );	
		te->Sparks( filter, 0.0, &vecSparkPos, 1, 1, &vecSparkDir );

		return;
	}

	BaseClass::TraceAttack(info, vecDir, ptr);
}


void CASW_Shieldbug::Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr )
{
	if ( BlockedDamage( info, vecDir, ptr ) )
		return;

	BaseClass::Bleed( info, vecPos, vecDir, ptr );
}