//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "decals.h"
#include "cs_gamerules.h"
#include "weapon_c4.h"
#include "in_buttons.h"
#include "datacache/imdlcache.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "soundent.h"
	#include "bot/cs_bot.h"
	#include "KeyValues.h"
	#include "triggers.h"
	#include "cs_gamestats.h"
#endif

#include "cs_playeranimstate.h"
#include "basecombatweapon_shared.h"
#include "util_shared.h"
#include "takedamageinfo.h"
#include "effect_dispatch_data.h"
#include "engine/ivdebugoverlay.h"
#include "obstacle_pushaway.h"
#include "props_shared.h"

ConVar sv_showimpacts("sv_showimpacts", "0", FCVAR_REPLICATED, "Shows client (red) and server (blue) bullet impact point (1=both, 2=client-only, 3=server-only)" );
ConVar sv_showplayerhitboxes( "sv_showplayerhitboxes", "0", FCVAR_REPLICATED, "Show lag compensated hitboxes for the specified player index whenever a player fires." );

#define	CS_MASK_SHOOT (MASK_SOLID|CONTENTS_DEBRIS)

void DispatchEffect( const char *pName, const CEffectData &data );


#ifdef _DEBUG

	// This is some extra code to collect weapon accuracy stats:

	struct bulletdata_s
	{
		float	timedelta;	// time delta since first shot of this round
		float	derivation;	// derivation for first shoot view angle
		int		count;
	};

	#define STATS_MAX_BULLETS	50

	static bulletdata_s s_bullet_stats[STATS_MAX_BULLETS];

	Vector	s_firstImpact = Vector(0,0,0);
	float	s_firstTime = 0;
	float	s_LastTime = 0;
	int		s_bulletCount = 0;

	void ResetBulletStats()
	{
		s_firstTime = 0;
		s_LastTime = 0;
		s_bulletCount = 0;
		s_firstImpact = Vector(0,0,0);
		Q_memset( s_bullet_stats, 0, sizeof(s_bullet_stats) );
	}

	void PrintBulletStats()
	{
		for (int i=0; i<STATS_MAX_BULLETS; i++ )
		{
			if (s_bullet_stats[i].count == 0)
				break;

			Msg("%3i;%3i;%.4f;%.4f\n", i, s_bullet_stats[i].count,
				s_bullet_stats[i].timedelta, s_bullet_stats[i].derivation );
		}
	}

	void AddBulletStat( float time, float dist, Vector &impact )
	{
		if ( time > s_LastTime + 2.0f )
		{
			// time delta since last shoot is bigger than 2 seconds, start new row
			s_LastTime = s_firstTime = time;
			s_bulletCount = 0;
			s_firstImpact = impact;

		}
		else
		{
			s_LastTime = time;
			s_bulletCount++;
		}

		if ( s_bulletCount >= STATS_MAX_BULLETS )
			s_bulletCount = STATS_MAX_BULLETS -1;

		if ( dist < 1 )
			dist = 1;

		int i = s_bulletCount;

		float offset = VectorLength( s_firstImpact - impact );

		float timedelta = time - s_firstTime;
		float derivation = offset / dist;

		float weight = (float)s_bullet_stats[i].count/(float)(s_bullet_stats[i].count+1);

		s_bullet_stats[i].timedelta *= weight;
		s_bullet_stats[i].timedelta += (1.0f-weight) * timedelta;

		s_bullet_stats[i].derivation *= weight;
		s_bullet_stats[i].derivation += (1.0f-weight) * derivation;

		s_bullet_stats[i].count++;
	}

	CON_COMMAND( stats_bullets_reset, "Reset bullet stats")
	{
		ResetBulletStats();
	}

	CON_COMMAND( stats_bullets_print, "Print bullet stats")
	{
		PrintBulletStats();
	}

#endif

float CCSPlayer::GetPlayerMaxSpeed()
{
	if ( GetMoveType() == MOVETYPE_NONE )
	{
		return CS_PLAYER_SPEED_STOPPED;
	}

	if ( IsObserver() )
	{
		// Player gets speed bonus in observer mode
		return CS_PLAYER_SPEED_OBSERVER;
	}

	bool bValidMoveState = ( State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE );
	if ( !bValidMoveState || m_bIsDefusing || CSGameRules()->IsFreezePeriod() )
	{
		// Player should not move during the freeze period
		return CS_PLAYER_SPEED_STOPPED;
	}

	float speed = BaseClass::GetPlayerMaxSpeed();

	if ( IsVIP() == true )  // VIP is slow due to the armour he's wearing
	{
		speed = MIN(speed, CS_PLAYER_SPEED_VIP);
	}
	else
	{

		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>( GetActiveWeapon() );

		if ( pWeapon )
		{
			if ( HasShield() && IsShieldDrawn() )
			{
				speed = MIN(speed, CS_PLAYER_SPEED_SHIELD);
			}
			else
			{
				speed = MIN(speed, pWeapon->GetMaxSpeed());
			}
		}
	}

	return speed;
}


void CCSPlayer::GetBulletTypeParameters(
	int iBulletType,
	float &fPenetrationPower,
	float &flPenetrationDistance )
{
	//MIKETODO: make ammo types come from a script file.
	if ( IsAmmoType( iBulletType, BULLET_PLAYER_50AE ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 1000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_762MM ) )
	{
		fPenetrationPower = 39;
		flPenetrationDistance = 5000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_556MM ) ||
			  IsAmmoType( iBulletType, BULLET_PLAYER_556MM_BOX ) )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 4000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_338MAG ) )
	{
		fPenetrationPower = 45;
		flPenetrationDistance = 8000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_9MM ) )
	{
		fPenetrationPower = 21;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_BUCKSHOT ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_45ACP ) )
	{
		fPenetrationPower = 15;
		flPenetrationDistance = 500.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_357SIG ) )
	{
		fPenetrationPower = 25;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_57MM ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 2000.0;
	}
	else
	{
		// What kind of ammo is this?
		Assert( false );
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
}

static void GetMaterialParameters( int iMaterial, float &flPenetrationModifier, float &flDamageModifier )
{
	switch ( iMaterial )
	{
		case CHAR_TEX_METAL :
			flPenetrationModifier = 0.5;  // If we hit metal, reduce the thickness of the brush we can't penetrate
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_DIRT :
			flPenetrationModifier = 0.5;
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_CONCRETE :
			flPenetrationModifier = 0.4;
			flDamageModifier = 0.25;
			break;
		case CHAR_TEX_GRATE	:
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.99;
			break;
		case CHAR_TEX_VENT :
			flPenetrationModifier = 0.5;
			flDamageModifier = 0.45;
			break;
		case CHAR_TEX_TILE :
			flPenetrationModifier = 0.65;
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_COMPUTER :
			flPenetrationModifier = 0.4;
			flDamageModifier = 0.45;
			break;
		case CHAR_TEX_WOOD :
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.6;
			break;
		default :
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.5;
			break;
	}

	Assert( flPenetrationModifier > 0 );
	Assert( flDamageModifier < 1.0f ); // Less than 1.0f for avoiding infinite loops
}


static bool TraceToExit(Vector &start, Vector &dir, Vector &end, float flStepSize, float flMaxDistance )
{
	float flDistance = 0;
	Vector last = start;

	while ( flDistance <= flMaxDistance )
	{
		flDistance += flStepSize;

		end = start + flDistance *dir;

		if ( (UTIL_PointContents ( end ) & MASK_SOLID) == 0 )
		{
			// found first free point
			return true;
		}
	}

	return false;
}

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask,
					 const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

void CCSPlayer::FireBullet(
	Vector vecSrc,	// shooting postion
	const QAngle &shootAngles,  //shooting angle
	float flDistance, // max distance
	int iPenetration, // how many obstacles can be penetrated
	int iBulletType, // ammo type
	int iDamage, // base damage
	float flRangeModifier, // damage range modifier
	CBaseEntity *pevAttacker, // shooter
	bool bDoEffects,
	float xSpread, float ySpread
	)
{
	float fCurrentDamage = iDamage;   // damage of the bullet at it's current trajectory
	float flCurrentDistance = 0.0;  //distance that the bullet has traveled so far

	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( shootAngles, &vecDirShooting, &vecRight, &vecUp );

	// MIKETODO: put all the ammo parameters into a script file and allow for CS-specific params.
	float flPenetrationPower = 0;		// thickness of a wall that this bullet can penetrate
	float flPenetrationDistance = 0;	// distance at which the bullet is capable of penetrating a wall
	float flDamageModifier = 0.5;		// default modification of bullets power after they go through a wall.
	float flPenetrationModifier = 1.f;

	GetBulletTypeParameters( iBulletType, flPenetrationPower, flPenetrationDistance );


	if ( !pevAttacker )
		pevAttacker = this;  // the default attacker is ourselves

	// add the spray
	Vector vecDir = vecDirShooting + xSpread * vecRight + ySpread * vecUp;

	VectorNormalize( vecDir );

	//Adrian: visualize server/client player positions
	//This is used to show where the lag compesator thinks the player should be at.
#if 0
	for ( int k = 1; k <= gpGlobals->maxClients; k++ )
	{
		CBasePlayer *clientClass = (CBasePlayer *)CBaseEntity::Instance( k );

		if ( clientClass == NULL )
			 continue;

		if ( k == entindex() )
			 continue;

#ifdef CLIENT_DLL
		debugoverlay->AddBoxOverlay( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), QAngle( 0, 0, 0), 255,0,0,127, 4 );
#else
		NDebugOverlay::Box( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), 0,0,255,127, 4 );
#endif

	}

#endif


//=============================================================================
// HPE_BEGIN:
//=============================================================================

#ifndef CLIENT_DLL
	// [pfreese] Track number player entities killed with this bullet
	int iPenetrationKills = 0;

	// [menglish] Increment the shots fired for this player
	CCS_GameStats.Event_ShotFired( this, GetActiveWeapon() );
#endif

//=============================================================================
// HPE_END
//=============================================================================

	bool bFirstHit = true;

	CBasePlayer *lastPlayerHit = NULL;

	if( sv_showplayerhitboxes.GetInt() > 0 )
	{
		CBasePlayer *lagPlayer = UTIL_PlayerByIndex( sv_showplayerhitboxes.GetInt() );
		if( lagPlayer )
		{
#ifdef CLIENT_DLL
			lagPlayer->DrawClientHitboxes(4, true);
#else
			lagPlayer->DrawServerHitboxes(4, true);
#endif
		}
	}

	MDLCACHE_CRITICAL_SECTION();
	while ( fCurrentDamage > 0 )
	{
		Vector vecEnd = vecSrc + vecDir * flDistance;

		trace_t tr; // main enter bullet trace

		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, this, lastPlayerHit, COLLISION_GROUP_NONE, &tr );
		{
			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );

			// Check for player hitboxes extending outside their collision bounds
			const float rayExtension = 40.0f;
			UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vecDir * rayExtension, CS_MASK_SHOOT|CONTENTS_HITBOX, &filter, &tr );
		}

		lastPlayerHit = ToBasePlayer(tr.m_pEnt);

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

#ifdef _DEBUG
		if ( bFirstHit )
			AddBulletStat( gpGlobals->realtime, VectorLength( vecSrc-tr.endpos), tr.endpos );
#endif

		bFirstHit = false;

#ifndef CLIENT_DLL
		//
		// Propogate a bullet impact event
		// @todo Add this for shotgun pellets (which dont go thru here)
		//
		IGameEvent * event = gameeventmanager->CreateEvent( "bullet_impact" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetFloat( "x", tr.endpos.x );
			event->SetFloat( "y", tr.endpos.y );
			event->SetFloat( "z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}
#endif

		/************* MATERIAL DETECTION ***********/
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		int iEnterMaterial = pSurfaceData->game.material;

		GetMaterialParameters( iEnterMaterial, flPenetrationModifier, flDamageModifier );

		bool hitGrate = tr.contents & CONTENTS_GRATE;

		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}

#ifdef CLIENT_DLL
		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 2 )
		{
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), QAngle( 0, 0, 0), 255,0,0,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawClientHitboxes( 4, true );
			}
		}
#else
		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 3 )
		{
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawServerHitboxes( 4, true );
			}
		}
#endif

		//calculate the damage based on the distance the bullet travelled.
		flCurrentDistance += tr.fraction * flDistance;
		fCurrentDamage *= pow (flRangeModifier, (flCurrentDistance / 500));

		// check if we reach penetration distance, no more penetrations after that
		if (flCurrentDistance > flPenetrationDistance && iPenetration > 0)
			iPenetration = 0;

#ifndef CLIENT_DLL
		// This just keeps track of sounds for AIs (it doesn't play anything).
		CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 400, 0.2f, this );
#endif

		int iDamageType = DMG_BULLET | DMG_NEVERGIB;

		if( bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
			{
				trace_t waterTrace;
				UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, COLLISION_GROUP_NONE, &waterTrace );

				if( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
 					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
				{
					CBaseEntity *pEntity = tr.m_pEnt;
					if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
					{
						UTIL_ImpactTrace( &tr, iDamageType );
					}
				}
			}
		} // bDoEffects

		// add damage to entity that we hit

#ifndef CLIENT_DLL
		ClearMultiDamage();

		//=============================================================================
		// HPE_BEGIN:
		// [pfreese] Check if enemy players were killed by this bullet, and if so,
		// add them to the iPenetrationKills count
		//=============================================================================
		
		CBaseEntity *pEntity = tr.m_pEnt;

		CTakeDamageInfo info( pevAttacker, pevAttacker, fCurrentDamage, iDamageType );
		CalculateBulletDamageForce( &info, iBulletType, vecDir, tr.endpos );
		pEntity->DispatchTraceAttack( info, vecDir, &tr );

		bool bWasAlive = pEntity->IsAlive();

		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );

		ApplyMultiDamage();

		if (bWasAlive && !pEntity->IsAlive() && pEntity->IsPlayer() && pEntity->GetTeamNumber() != GetTeamNumber())
		{
			++iPenetrationKills;
		}
		
		//=============================================================================
		// HPE_END
		//=============================================================================

#endif

		// check if bullet can penetrate another entity
		if ( iPenetration == 0 && !hitGrate )
			break; // no, stop

		// If we hit a grate with iPenetration == 0, stop on the next thing we hit
		if ( iPenetration < 0 )
			break;

		Vector penetrationEnd;

		// try to penetrate object, maximum penetration is 128 inch
		if ( !TraceToExit( tr.endpos, vecDir, penetrationEnd, 24, 128 ) )
			break;

		// find exact penetration exit
		trace_t exitTr;
		UTIL_TraceLine( penetrationEnd, tr.endpos, CS_MASK_SHOOT|CONTENTS_HITBOX, NULL, &exitTr );

		if( exitTr.m_pEnt != tr.m_pEnt && exitTr.m_pEnt != NULL )
		{
			// something was blocking, trace again
			UTIL_TraceLine( penetrationEnd, tr.endpos, CS_MASK_SHOOT|CONTENTS_HITBOX, exitTr.m_pEnt, COLLISION_GROUP_NONE, &exitTr );
		}

		// get material at exit point
		pSurfaceData = physprops->GetSurfaceData( exitTr.surface.surfaceProps );
		int iExitMaterial = pSurfaceData->game.material;

		hitGrate = hitGrate && ( exitTr.contents & CONTENTS_GRATE );

		// if enter & exit point is wood or metal we assume this is
		// a hollow crate or barrel and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if( iExitMaterial == CHAR_TEX_WOOD ||
				iExitMaterial == CHAR_TEX_METAL )
			{
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		// check if bullet has enough power to penetrate this distance for this material
		if ( flTraceDistance > ( flPenetrationPower * flPenetrationModifier ) )
			break; // bullet hasn't enough power to penetrate this distance

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

		//setup new start end parameters for successive trace

		flPenetrationPower -= flTraceDistance / flPenetrationModifier;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = (flDistance - flCurrentDistance) * 0.5;

		// reduce damage power each time we hit something other than a grate
		fCurrentDamage *= flDamageModifier;

		// reduce penetration counter
		iPenetration--;
	}

#ifndef CLIENT_DLL
	//=============================================================================
	// HPE_BEGIN:
	// [pfreese] If we killed at least two enemies with a single bullet, award the
	// TWO_WITH_ONE_SHOT achievement
	//=============================================================================
	
	if (iPenetrationKills >= 2)
	{
		AwardAchievement(CSKillTwoWithOneShot);
	}
	
	//=============================================================================
	// HPE_END
	//=============================================================================
#endif
}


void CCSPlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  )
{
	float speedSqr = vecVelocity.AsVector2D().LengthSqr();

	// the fastest walk is 135 ( scout ), see CCSGameMovement::CheckParameters()
	if ( speedSqr < 150.0 * 150.0 ) 
		return; // player is not running, no footsteps

	BaseClass::UpdateStepSound( psurface, vecOrigin, vecVelocity  );
}


// GOOSEMAN : Kick the view..
void CCSPlayer::KickBack( float up_base, float lateral_base, float up_modifier, float lateral_modifier, float up_max, float lateral_max, int direction_change )
{
	float flKickUp;
	float flKickLateral;

	if (m_iShotsFired == 1) // This is the first round fired
	{
		flKickUp = up_base;
		flKickLateral = lateral_base;
	}
	else
	{
		flKickUp = up_base + m_iShotsFired*up_modifier;
		flKickLateral = lateral_base + m_iShotsFired*lateral_modifier;
	}


	QAngle angle = GetPunchAngle();

	angle.x -= flKickUp;
	if ( angle.x < -1 * up_max )
		angle.x = -1 * up_max;

	if ( m_iDirection == 1 )
	{
		angle.y += flKickLateral;
		if (angle.y > lateral_max)
			angle.y = lateral_max;
	}
	else
	{
		angle.y -= flKickLateral;
		if ( angle.y < -1 * lateral_max )
			angle.y = -1 * lateral_max;
	}

	if ( !SharedRandomInt( "KickBack", 0, direction_change ) )
		m_iDirection = 1 - m_iDirection;

	SetPunchAngle( angle );
}


bool CCSPlayer::CanMove() const
{
	// When we're in intro camera mode, it's important to return false here
	// so our physics object doesn't fall out of the world.
	if ( GetMoveType() == MOVETYPE_NONE )
		return false;

	if ( IsObserver() )
		return true; // observers can move all the time

	bool bValidMoveState = (State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE);

	if ( m_bIsDefusing || !bValidMoveState || CSGameRules()->IsFreezePeriod() )
	{
		return false;
	}
	else
	{
		// Can't move while planting C4.
		CC4 *pC4 = dynamic_cast< CC4* >( GetActiveWeapon() );
		if ( pC4 && pC4->m_bStartedArming )
			return false;

		return true;
	}
}


void CCSPlayer::OnJump( float fImpulse )
{
	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if ( pActiveWeapon != NULL )
		pActiveWeapon->OnJump(fImpulse);
}


void CCSPlayer::OnLand( float fVelocity )
{
	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if ( pActiveWeapon != NULL )
		pActiveWeapon->OnLand(fVelocity);
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* Track the last time we were on a ladder, along with the ladder's normal and where we
* were grabbing it, so we don't reach behind us and grab it again as we are trying to
* dismount.
*/
void CCSPlayer::SurpressLadderChecks( const Vector& pos, const Vector& normal )
{
	m_ladderSurpressionTimer.Start( 1.0f );
	m_lastLadderPos = pos;
	m_lastLadderNormal = normal;
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* Prevent us from re-grabbing the same ladder we were just on:
*  - if the timer is elapsed, let us grab again
*  - if the normal is different, let us grab
*  - if the 2D pos is very different, let us grab, since it's probably a different ladder
*/
bool CCSPlayer::CanGrabLadder( const Vector& pos, const Vector& normal )
{
	if ( m_ladderSurpressionTimer.GetRemainingTime() <= 0.0f )
	{
		return true;
	}

	const float MaxDist = 64.0f;
	if ( pos.AsVector2D().DistToSqr( m_lastLadderPos.AsVector2D() ) < MaxDist * MaxDist )
	{
		return false;
	}

	if ( normal != m_lastLadderNormal )
	{
		return true;
	}

	return false;
}


void CCSPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// In CS, its CPlayerAnimState object manages ALL the animation state.
	return;
}


CWeaponCSBase* CCSPlayer::CSAnim_GetActiveWeapon()
{
	return GetActiveCSWeapon();
}


bool CCSPlayer::CSAnim_CanMove()
{
	return CanMove();
}

//--------------------------------------------------------------------------------------------------------------

#define MATERIAL_NAME_LENGTH 16

#ifdef GAME_DLL

class CFootstepControl : public CBaseTrigger
{
public:
	DECLARE_CLASS( CFootstepControl, CBaseTrigger );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual int UpdateTransmitState( void );
	virtual void Spawn( void );

	CNetworkVar( string_t, m_source );
	CNetworkVar( string_t, m_destination );
};

LINK_ENTITY_TO_CLASS( func_footstep_control, CFootstepControl );


BEGIN_DATADESC( CFootstepControl )
	DEFINE_KEYFIELD( m_source, FIELD_STRING, "Source" ),
	DEFINE_KEYFIELD( m_destination, FIELD_STRING, "Destination" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CFootstepControl, DT_FootstepControl )
	SendPropStringT( SENDINFO(m_source) ),
	SendPropStringT( SENDINFO(m_destination) ),
END_SEND_TABLE()

int CFootstepControl::UpdateTransmitState( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CFootstepControl::Spawn( void )
{
	InitTrigger();
}

#else

//--------------------------------------------------------------------------------------------------------------

class C_FootstepControl : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FootstepControl, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FootstepControl( void );
	~C_FootstepControl();

	char m_source[MATERIAL_NAME_LENGTH];
	char m_destination[MATERIAL_NAME_LENGTH];
};

IMPLEMENT_CLIENTCLASS_DT(C_FootstepControl, DT_FootstepControl, CFootstepControl)
	RecvPropString( RECVINFO(m_source) ),
	RecvPropString( RECVINFO(m_destination) ),
END_RECV_TABLE()

CUtlVector< C_FootstepControl * > s_footstepControllers;

C_FootstepControl::C_FootstepControl( void )
{
	s_footstepControllers.AddToTail( this );
}

C_FootstepControl::~C_FootstepControl()
{
	s_footstepControllers.FindAndRemove( this );
}

surfacedata_t * CCSPlayer::GetFootstepSurface( const Vector &origin, const char *surfaceName )
{
	for ( int i=0; i<s_footstepControllers.Count(); ++i )
	{
		C_FootstepControl *control = s_footstepControllers[i];

		if ( FStrEq( control->m_source, surfaceName ) )
		{
			if ( control->CollisionProp()->IsPointInBounds( origin ) )
			{
				return physprops->GetSurfaceData( physprops->GetSurfaceIndex( control->m_destination ) );
			}
		}
	}

	return physprops->GetSurfaceData( physprops->GetSurfaceIndex( surfaceName ) );
}

#endif


