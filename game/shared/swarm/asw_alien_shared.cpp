#include "cbase.h"
#ifdef CLIENT_DLL
#include "c_asw_alien.h"
#include "c_asw_player.h"
#include "igameevents.h"
#include "asw_util_shared.h"
#else
#include "asw_alien.h"
#include "asw_player.h"
#endif
#include "asw_fx_shared.h"
#include "takedamageinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
extern ConVar showhitlocation;
#endif

ConVar asw_drone_weak_from_behind( "asw_drone_weak_from_behind", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Drones take double damage from behind" );
ConVar asw_alien_mining_laser_damage_scale( "asw_alien_mining_laser_damage_scale", "0.25f", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_alien_debug_death_style( "asw_alien_debug_death_style", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "For debugging alien deaths" );

void CASW_Alien::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
#ifdef GAME_DLL
	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;
#endif	

	CTakeDamageInfo subInfo = info;

#ifdef GAME_DLL
	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces
#endif

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );	

	// mining laser does reduced damage
	if ( info.GetDamageType() & DMG_ENERGYBEAM )
	{
		subInfo.ScaleDamage( asw_alien_mining_laser_damage_scale.GetFloat() );
	}

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		&& !( subInfo.GetDamageType() & DMG_BURN ) )
	{
#ifdef GAME_DLL
		Bleed( subInfo, ptr->endpos + m_LagCompensation.GetLagCompensationOffset(), vecDir, ptr );
		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
#else
		Bleed( subInfo, ptr->endpos, vecDir, ptr );
		//OnHurt();
#endif		
	}

	if( !info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}


	AddMultiDamage( subInfo, this );
#ifdef GAME_DLL
#else
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( subInfo.GetAttacker() );
	CASW_Player *pPlayerAttacker = NULL;

	if ( pMarine )
	{
		pPlayerAttacker = pMarine->GetCommander();
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "alien_hurt" );
	if ( event )
	{
		event->SetInt( "attacker", ( pPlayerAttacker ? pPlayerAttacker->GetUserID() : 0 ) );
		event->SetInt( "entindex", entindex() );
		event->SetInt( "amount", subInfo.GetDamage() );
		gameeventmanager->FireEventClientSide( event );
	}

	UTIL_ASW_ClientFloatingDamageNumber( subInfo );
#endif
}

void CASW_Alien::Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr )
{
	UTIL_ASW_DroneBleed( vecPos, -vecDir, 4 );

	DoBloodDecal( info.GetDamage(), vecPos, vecDir, ptr, info.GetDamageType() );
}

void CASW_Alien::DoBloodDecal( float flDamage, const Vector &vecPos, const Vector &vecDir, trace_t *ptr, int bitsDamageType )
{
	if ( ( BloodColor() == DONT_BLEED) || ( BloodColor() == BLOOD_COLOR_MECH ) )
	{
		return;
	}

	if (flDamage == 0)
		return;

	if ( !( bitsDamageType & ( DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_AIRBOAT ) ) )
		return;

	// make blood decal on the wall!
	trace_t Bloodtr;
	Vector vecTraceDir;
	float flNoise;
	int cCount;
	int i;

#ifdef GAME_DLL
	if ( !IsAlive() )
	{
		// dealing with a dead npc.
		if ( GetMaxHealth() <= 0 )
		{
			// no blood decal for a npc that has already decalled its limit.
			return;
		}
		else
		{
			m_iMaxHealth -= 1;
		}
	}
#endif

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	float flTraceDist = (bitsDamageType & DMG_AIRBOAT) ? 384 : 172;
	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.y += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.z += random->RandomFloat( -flNoise, flNoise );

		// Don't bleed on grates.
		UTIL_TraceLine( vecPos, vecPos + vecTraceDir * -flTraceDist, MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}
