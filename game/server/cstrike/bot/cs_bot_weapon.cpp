//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"
#include "basecsgrenade_projectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Fire our active weapon towards our current enemy
 * NOTE: Aiming our weapon is handled in RunBotUpkeep()
 */
void CCSBot::FireWeaponAtEnemy( void )
{
	if (cv_bot_dont_shoot.GetBool())
	{
		return;
	}

	CBasePlayer *enemy = GetBotEnemy();
	if (enemy == NULL)
	{
		return;
	}

	Vector myOrigin = GetCentroid( this );

	if (IsUsingSniperRifle())
	{
		// if we're using a sniper rifle, don't fire until we are standing still, are zoomed in, and not rapidly moving our view
		if (!IsNotMoving() || IsWaitingForZoom() || !HasViewBeenSteady( GetProfile()->GetReactionTime() ) )
		{
			return;
		}
	}

	if (gpGlobals->curtime > m_fireWeaponTimestamp &&
		GetTimeSinceAcquiredCurrentEnemy() >= GetProfile()->GetAttackDelay() &&
		!IsSurprised())
	{
		if (!(IsRecognizedEnemyProtectedByShield() && IsPlayerFacingMe( enemy )) &&	// don't shoot at enemies behind shields
			!IsReloading() && 
			!IsActiveWeaponClipEmpty() && 
			//gpGlobals->curtime > m_reacquireTimestamp &&
			IsEnemyVisible())
		{
			// we have a clear shot - pull trigger if we are aiming at enemy
			Vector toAimSpot = m_aimSpot - EyePosition();
			float rangeToEnemy = toAimSpot.NormalizeInPlace();

			if ( IsUsingSniperRifle() )
			{
				// check our accuracy versus our target distance
				float fProjectedSpread = rangeToEnemy * GetActiveCSWeapon()->GetInaccuracy();
				float fRequiredSpread = IsUsing( WEAPON_AWP ) ? 50.0f : 25.0f;	// AWP will kill with any hit
				if ( fProjectedSpread > fRequiredSpread )
					return;
			}

			// get actual view direction vector
			Vector aimDir = GetViewVector();

			float onTarget = DotProduct( toAimSpot, aimDir );

			// aim more precisely with a sniper rifle
			// because rifles' bullets spray, don't have to be very precise
			const float halfSize = (IsUsingSniperRifle()) ? HalfHumanWidth : 2.0f * HalfHumanWidth;

			// aiming tolerance depends on how close the target is - closer targets subtend larger angles
			float aimTolerance = (float)cos( atan( halfSize / rangeToEnemy ) );

			if (onTarget > aimTolerance)
			{
				bool doAttack;

				// if friendly fire is on, don't fire if a teammate is blocking our line of fire
				if (TheCSBots()->AllowFriendlyFireDamage())
				{
					if (IsFriendInLineOfFire())
						doAttack = false;
					else
						doAttack = true;
				}
				else
				{
					// fire freely
					doAttack = true;
				}

				if (doAttack)
				{
					// if we are using a knife, only swing it if we're close
					if (IsUsingKnife())
					{
						const float knifeRange = 75.0f;		// 50
						if (rangeToEnemy < knifeRange)
						{
							// since we've given ourselves away - run!
							ForceRun( 5.0f );

							// if our prey is facing away, backstab him!
							if (!IsPlayerFacingMe( enemy ))
							{
								SecondaryAttack();
							}
							else
							{
								// randomly choose primary and secondary attacks with knife
								const float knifeStabChance = 33.3f;
								if (RandomFloat( 0, 100 ) < knifeStabChance)
									SecondaryAttack();
								else
									PrimaryAttack();
							}
						}
					}
					else
					{
						PrimaryAttack();
					}
				}

				if (IsUsingPistol())
				{
					// high-skill bots fire their pistols quickly at close range
					const float closePistolRange = 360.0f;
					if (GetProfile()->GetSkill() > 0.75f && rangeToEnemy < closePistolRange)
					{
						// fire as fast as possible
						m_fireWeaponTimestamp = 0.0f; 
					}
					else
					{
						// fire somewhat quickly
						m_fireWeaponTimestamp = RandomFloat( 0.15f, 0.4f );
					}
				}
				else	// not using a pistol
				{
					const float sprayRange = 400.0f;
					if (GetProfile()->GetSkill() < 0.5f || rangeToEnemy < sprayRange || IsUsingMachinegun())
					{
						// spray 'n pray if enemy is close, or we're not that good, or we're using the big machinegun
						m_fireWeaponTimestamp = 0.0f;
					}
					else
					{
						const float distantTargetRange = 800.0f;
						if (!IsUsingSniperRifle() && rangeToEnemy > distantTargetRange)
						{
							// if very far away, fire slowly for better accuracy
							m_fireWeaponTimestamp = RandomFloat( 0.3f, 0.7f );
						}
						else
						{
							// fire short bursts for accuracy
							m_fireWeaponTimestamp = RandomFloat( 0.15f, 0.25f );		// 0.15, 0.5
						}
					}
				}

				// subtract system latency
				m_fireWeaponTimestamp -= g_BotUpdateInterval;

				m_fireWeaponTimestamp += gpGlobals->curtime;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Set the current aim offset using given accuracy (1.0 = perfect aim, 0.0f = terrible aim)
 */
void CCSBot::SetAimOffset( float accuracy )
{
	// if our accuracy is less than perfect, it will improve as we "focus in" while not rotating our view
	if (accuracy < 1.0f)
	{
		// if we moved our view, reset our "focus" mechanism
		if (IsViewMoving( 100.0f ))
			m_aimSpreadTimestamp = gpGlobals->curtime;

		// focusTime is the time it takes for a bot to "focus in" for very good aim, from 2 to 5 seconds
		const float focusTime = MAX( 5.0f * (1.0f - accuracy), 2.0f );
		float focusInterval = gpGlobals->curtime - m_aimSpreadTimestamp;

		float focusAccuracy = focusInterval / focusTime;

		// limit how much "focus" will help
		const float maxFocusAccuracy = 0.75f;
		if (focusAccuracy > maxFocusAccuracy)
			focusAccuracy = maxFocusAccuracy;

		accuracy = MAX( accuracy, focusAccuracy );
	}

	//PrintIfWatched( "Accuracy = %4.3f\n", accuracy );

	// aim error increases with distance, such that actual crosshair error stays about the same
	float range = (m_lastEnemyPosition - EyePosition()).Length();
	float maxOffset = (GetFOV()/GetDefaultFOV()) * 0.05f * range;		// 0.1
	float error = maxOffset * (1.0f - accuracy);

	m_aimOffsetGoal.x = RandomFloat( -error, error );
	m_aimOffsetGoal.y = RandomFloat( -error, error );
	m_aimOffsetGoal.z = RandomFloat( -error, error );

	// define time when aim offset will automatically be updated
	m_aimOffsetTimestamp = gpGlobals->curtime + RandomFloat( 0.25f, 1.0f ); // 0.25, 1.5f
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Wiggle aim error based on GetProfile()->GetSkill()
 */
void CCSBot::UpdateAimOffset( void )
{
	if (gpGlobals->curtime >= m_aimOffsetTimestamp)
	{
		SetAimOffset( GetProfile()->GetSkill() );
	}

	// move current offset towards goal offset
	Vector d = m_aimOffsetGoal - m_aimOffset;
	const float stiffness = 0.1f;
	m_aimOffset.x += stiffness * d.x;
	m_aimOffset.y += stiffness * d.y;
	m_aimOffset.z += stiffness * d.z;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Change our zoom level to be appropriate for the given range.
 * Return true if the zoom level changed.
 */
bool CCSBot::AdjustZoom( float range )
{
	bool adjustZoom = false;

	if (IsUsingSniperRifle())
	{
		const float sniperZoomRange = 150.0f;	// NOTE: This must be less than sniperMinRange in AttackState
		const float sniperFarZoomRange = 1500.0f;

		// if range is too close, don't zoom
		if (range <= sniperZoomRange)
		{
			// zoom out
			if (GetZoomLevel() != NO_ZOOM)
			{
				adjustZoom = true;
			}
		}
		else if (range < sniperFarZoomRange)
		{
			// maintain low zoom
			if (GetZoomLevel() != LOW_ZOOM)
			{
				adjustZoom = true;
			}
		}
		else
		{
			// maintain high zoom
			if (GetZoomLevel() != HIGH_ZOOM)
			{
				adjustZoom = true;
			}
		}
	}
	else
	{
		// zoom out
		if (GetZoomLevel() != NO_ZOOM)
		{
			adjustZoom = true;
		}
	}

	if (adjustZoom)
	{
		SecondaryAttack();

		// pause after zoom to allow "eyes" to refocus
// 		m_zoomTimer.Start( 0.25f + (1.0f - GetProfile()->GetSkill()) );
		m_zoomTimer.Start( 0.25f );
	}

	return adjustZoom;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if using the specific weapon
 */
bool CCSBot::IsUsing( CSWeaponID weaponID ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == NULL)
		return false;

	if (weapon->IsA( weaponID ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we are using a weapon with a removable silencer
 */
bool CCSBot::DoesActiveWeaponHaveSilencer( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == NULL)
		return false;

	if (weapon->IsA( WEAPON_M4A1 ) || weapon->IsA( WEAPON_USP ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a sniper rifle
 */
bool CCSBot::IsUsingSniperRifle( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && IsSniperRifle( weapon ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a sniper rifle in our inventory
 */
bool CCSBot::IsSniper( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

	if (weapon && IsSniperRifle( weapon ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are actively sniping (moving to sniper spot or settled in)
 */
bool CCSBot::IsSniping( void ) const
{
	if (GetTask() == MOVE_TO_SNIPER_SPOT || GetTask() == SNIPING)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are using a shotgun
 */
bool CCSBot::IsUsingShotgun( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon == NULL)
		return false;

	return weapon->IsKindOf(WEAPONTYPE_SHOTGUN);
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if using the big 'ol machinegun
 */
bool CCSBot::IsUsingMachinegun( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && weapon->IsA( WEAPON_M249 ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if primary weapon doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPrimaryWeaponEmpty( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

	if (weapon == NULL)
		return true;

	// check if gun has any ammo left
	if (weapon->HasAnyAmmo())
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if pistol doesn't exist or is totally out of ammo
 */
bool CCSBot::IsPistolEmpty( void ) const
{
	CWeaponCSBase *weapon = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );

	if (weapon == NULL)
		return true;

	// check if gun has any ammo left
	if (weapon->HasAnyAmmo())
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the given item
 */
bool CCSBot::DoEquip( CWeaponCSBase *weapon )
{
	if (weapon == NULL)
		return false;

	// check if weapon has any ammo left
	if (!weapon->HasAnyAmmo())
		return false;

	// equip it
	SelectItem( weapon->GetClassname() );
	m_equipTimer.Start();

	return true;
}


// throttle how often equipping is allowed
const float minEquipInterval = 5.0f;


//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the best weapon we are carrying that has ammo
 */
void CCSBot::EquipBestWeapon( bool mustEquip )
{
	// throttle how often equipping is allowed
	if (!mustEquip && m_equipTimer.GetElapsedTime() < minEquipInterval)
		return;

	CCSBotManager *ctrl = static_cast<CCSBotManager *>( TheBots );

	CWeaponCSBase *primary = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );
	if (primary)
	{
		CSWeaponType weaponClass = primary->GetCSWpnData().m_WeaponType;

		if ((ctrl->AllowShotguns() && weaponClass == WEAPONTYPE_SHOTGUN) ||
			(ctrl->AllowMachineGuns() && weaponClass == WEAPONTYPE_MACHINEGUN) || 
			(ctrl->AllowRifles() && weaponClass == WEAPONTYPE_RIFLE) || 
			(ctrl->AllowShotguns() && weaponClass == WEAPONTYPE_SHOTGUN) ||
			(ctrl->AllowSnipers() && weaponClass == WEAPONTYPE_SNIPER_RIFLE) || 
			(ctrl->AllowSubMachineGuns() && weaponClass == WEAPONTYPE_SUBMACHINEGUN))
		{
			if (DoEquip( primary ))
				return;
		}
	}

	if (ctrl->AllowPistols())
	{
		if (DoEquip( static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) ) ))
			return;
	}

	// always have a knife
	EquipKnife();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip our pistol
 */
void CCSBot::EquipPistol( void )
{
	// throttle how often equipping is allowed
	if (m_equipTimer.GetElapsedTime() < minEquipInterval)
		return;

	if (TheCSBots()->AllowPistols() && !IsUsingPistol())
	{
		CWeaponCSBase *pistol = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );
		DoEquip( pistol );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip the knife
 */
void CCSBot::EquipKnife( void )
{
	if (!IsUsingKnife())
	{
		SelectItem( "weapon_knife" );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we have a grenade in our inventory
 */
bool CCSBot::HasGrenade( void ) const
{
	CWeaponCSBase *grenade = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_GRENADES ) );
	return (grenade) ? true : false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Equip a grenade, return false if we cant
 */
bool CCSBot::EquipGrenade( bool noSmoke )
{
	// snipers don't use grenades
	if (IsSniper())
		return false;

	if (IsUsingGrenade())
		return true;

	if (HasGrenade())
	{
		CWeaponCSBase *grenade = static_cast<CWeaponCSBase *>( Weapon_GetSlot( WEAPON_SLOT_GRENADES ) );

		if (noSmoke && grenade->IsA( WEAPON_SMOKEGRENADE ))
			return false;

		SelectItem( grenade->GetClassname() );

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have knife equipped
 */
bool CCSBot::IsUsingKnife( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && weapon->IsA( WEAPON_KNIFE ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have pistol equipped
 */
bool CCSBot::IsUsingPistol( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (weapon && weapon->IsPistol())
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if we have a grenade equipped
 */
bool CCSBot::IsUsingGrenade( void ) const
{
	CWeaponCSBase *weapon = GetActiveCSWeapon();

	if (!weapon)
		return false;

	if (weapon->IsA( WEAPON_FLASHBANG ) ||
		weapon->IsA( WEAPON_SMOKEGRENADE ) ||
		weapon->IsA( WEAPON_HEGRENADE ))
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the process of throwing the grenade
 */
void CCSBot::ThrowGrenade( const Vector &target )
{
	if (IsUsingGrenade() && m_grenadeTossState == NOT_THROWING && !IsOnLadder())
	{
		m_grenadeTossState = START_THROW;
		m_tossGrenadeTimer.Start( 2.0f );

		const float angleTolerance = 3.0f;
		SetLookAt( "GrenadeThrow", target, PRIORITY_UNINTERRUPTABLE, 4.0f, false, angleTolerance ); 

		Wait( RandomFloat( 2.0f, 4.0f ) );

		if (cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
		{
			NDebugOverlay::Cross3D( target, 25.0f, 255, 125, 0, true, 3.0f );
		}

		PrintIfWatched( "%3.2f: Grenade: START_THROW\n", gpGlobals->curtime );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if our weapon can attack
 */
bool CCSBot::CanActiveWeaponFire( void ) const
{
	return ( GetActiveWeapon() && GetActiveWeapon()->m_flNextPrimaryAttack <= gpGlobals->curtime );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Find spot to throw grenade ahead of us and "around the corner" along our path
 */
bool CCSBot::FindGrenadeTossPathTarget( Vector *pos )
{
	if (!HasPath())
		return false;

	// find farthest point we can see on the path
	int i;
	for( i=m_pathIndex; i<m_pathLength; ++i )
	{
		if (!FVisible( m_path[i].pos + Vector( 0, 0, HalfHumanHeight ) ))
			break;
	}

	if (i == m_pathIndex)
		return false;

	// find exact spot where we lose sight
	Vector dir = m_path[i].pos - m_path[i-1].pos;
	float length = dir.NormalizeInPlace();

	const float inc = 25.0f;
	Vector p;
	Vector visibleSpot = m_path[i-1].pos;
	for( float t = 0.0f; t<length; t += inc )
	{
		p = m_path[i-1].pos + t * dir;
		p.z += HalfHumanHeight;
		if (!FVisible( p ))
			break;

		visibleSpot = p;
	}

	// massage the location a bit
	visibleSpot.z += 10.0f;

	const float bufferRange = 50.0f;

	trace_t result;
	Vector check;

	// check +X
	check = visibleSpot + Vector( 999.9f, 0, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = result.endpos.x - visibleSpot.x;
		if (range < bufferRange)
		{
			visibleSpot.x = result.endpos.x - bufferRange;
		}
	}

	// check -X
	check = visibleSpot + Vector( -999.9f, 0, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = visibleSpot.x - result.endpos.x;
		if (range < bufferRange)
		{
			visibleSpot.x = result.endpos.x + bufferRange;
		}
	}

	// check +Y
	check = visibleSpot + Vector( 0, 999.9f, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = result.endpos.y - visibleSpot.y;
		if (range < bufferRange)
		{
			visibleSpot.y = result.endpos.y - bufferRange;
		}
	}

	// check -Y
	check = visibleSpot + Vector( 0, -999.9f, 0 );
	UTIL_TraceLine( visibleSpot, check, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.fraction < 1.0f)
	{
		float range = visibleSpot.y - result.endpos.y;
		if (range < bufferRange)
		{
			visibleSpot.y = result.endpos.y + bufferRange;
		}
	}

	*pos = visibleSpot;
	return true;	
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Look for grenade throw targets and throw the grenade
 */
void CCSBot::LookForGrenadeTargets( void )
{
	if (!IsUsingGrenade() || IsThrowingGrenade())
	{
		return;
	}

	const CNavArea *tossArea = GetInitialEncounterArea();
	if (tossArea == NULL)
	{
		return;
	}

	int enemyTeam = OtherTeam( GetTeamNumber() );

	// check if we should put our grenade away
	if (tossArea->GetEarliestOccupyTime( enemyTeam ) > gpGlobals->curtime)
	{
		EquipBestWeapon( MUST_EQUIP );
		return;
	}

	// throw grenades at initial encounter area
	Vector tossTarget = Vector( 0, 0, 0 );
	if (!tossArea->IsVisible( EyePosition(), &tossTarget ))
	{
		return;
	}


	CWeaponCSBase *weapon = GetActiveCSWeapon();
	if (weapon && weapon->IsA( WEAPON_SMOKEGRENADE ))
	{
		// don't worry so much about smokes
		ThrowGrenade( tossTarget );
		PrintIfWatched( "Throwing smoke grenade!" );
		SetInitialEncounterArea( NULL );
		return;
	}
	else	// explosive and flashbang grenades
	{
		// initial encounter area is visible, wait to throw until timing is right

		const float leadTime = 1.5f;
		float enemyTime = tossArea->GetEarliestOccupyTime( enemyTeam );
		if (enemyTime - TheCSBots()->GetElapsedRoundTime() > leadTime)
		{
			// don't throw yet
			return;
		}


		Vector to = tossTarget - EyePosition();
		float range = to.Length();

		const float slope = 0.2f; // 0.25f;
		float tossHeight = slope * range;

		trace_t result;
		CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );

		const float heightInc = tossHeight / 10.0f;
		Vector target;
		float safeSpace = tossHeight / 2.0f;

		// Build a box to sweep along the ray when looking for obstacles
		const Vector& eyePosition = EyePosition();
		Vector mins = VEC_HULL_MIN;
		Vector maxs = VEC_HULL_MAX;
		mins.z = 0;
		maxs.z = heightInc;


		// find low and high bounds of toss window
		float low = 0.0f;
		float high = tossHeight + safeSpace;
		bool gotLow = false;
		float lastH = 0.0f;
		for( float h = 0.0f; h < 3.0f * tossHeight; h += heightInc )
		{
			target = tossTarget + Vector( 0, 0, h );

			// make sure toss line is clear

			QAngle angles( 0, 0, 0 );
			Ray_t ray;
			ray.Init( eyePosition, target, mins, maxs );
			enginetrace->TraceRay( ray, MASK_VISIBLE_AND_NPCS | CONTENTS_GRATE, &traceFilter, &result );
			if (result.fraction == 1.0f)
			{
				//NDebugOverlay::SweptBox( eyePosition, target, mins, maxs, angles, 0, 0, 255, 40, 10.0f );

				// line is clear
				if (!gotLow)
				{
					low = h;
					gotLow = true;
				}
			}
			else
			{
				//NDebugOverlay::SweptBox( eyePosition, target, mins, maxs, angles, 255, 0, 0, 5, 10.0f );

				// line is blocked
				if (gotLow)
				{
					high = lastH;
					break;
				}
			}

			lastH = h;
		}

		if (gotLow)
		{
			// throw grenade into toss window
			if (tossHeight < low)
			{
				if (low + safeSpace > high)
				{
					// narrow window
					tossHeight = (high + low)/2.0f;
				}
				else
				{
					tossHeight = low + safeSpace;
				}
			}
			else if (tossHeight > high - safeSpace)
			{
				if (high - safeSpace < low)
				{
					// narrow window
					tossHeight = (high + low)/2.0f;
				}
				else
				{
					tossHeight = high - safeSpace;
				}
			}

			ThrowGrenade( tossTarget + Vector( 0, 0, tossHeight ) );
			SetInitialEncounterArea( NULL );
			return;
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
class FOVClearOfFriends
{
public:
	FOVClearOfFriends( CCSBot *me )
	{
		m_me = me;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player == m_me || !player->IsAlive())
			return true;

		if (m_me->InSameTeam( player ))
		{
			Vector to = player->EyePosition() - m_me->EyePosition();
			to.NormalizeInPlace();

			Vector forward;
			m_me->EyeVectors( &forward );

			if (DotProduct( to, forward ) > 0.95f)
			{
				if (m_me->IsVisible( (CCSPlayer *)player ))
				{
					// we see a friend in our FOV
					return false;
				}
			}
		}

		return true;
	}

	CCSBot *m_me;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Process the grenade throw state machine
 */
void CCSBot::UpdateGrenadeThrow( void )
{
	switch( m_grenadeTossState )
	{
		case START_THROW:
		{
			if (m_tossGrenadeTimer.IsElapsed())
			{
				// something prevented the throw - give up
				EquipBestWeapon( MUST_EQUIP );
				ClearLookAt();
				m_grenadeTossState = NOT_THROWING;
				PrintIfWatched( "%3.2f: Grenade: THROW FAILED\n", gpGlobals->curtime );
				return;
			}

			if (m_lookAtSpotState == LOOK_AT_SPOT)
			{
				// don't throw if there are friends ahead of us
				FOVClearOfFriends fovClear( this );
				if (ForEachPlayer( fovClear ))
				{
					m_grenadeTossState = FINISH_THROW;
					m_tossGrenadeTimer.Start( 1.0f );
					PrintIfWatched( "%3.2f: Grenade: FINISH_THROW\n", gpGlobals->curtime );
				}
				else
				{
					PrintIfWatched( "%3.2f: Grenade: Friend is in the way...\n", gpGlobals->curtime );
				}
			}

			// hold in the trigger and be ready to throw
			PrimaryAttack();

			break;
		}

		case FINISH_THROW:
		{
			// throw the grenade and hold our aiming line for a moment
			if (m_tossGrenadeTimer.IsElapsed())
			{
				ClearLookAt();

				m_grenadeTossState = NOT_THROWING;
				PrintIfWatched( "%3.2f: Grenade: THROW COMPLETE\n", gpGlobals->curtime );
			}
			break;
		}

		default:
		{
			if (IsUsingGrenade())
			{
				// pull the pin
				PrimaryAttack();
			}
			break;
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
class GrenadeResponse
{
public:
	GrenadeResponse( CCSBot *me )
	{
		m_me = me;
	}

	bool operator() ( ActiveGrenade *ag ) const
	{
		const float retreatRange = 300.0f;
		const float hideTime = 1.0f;

		// do we see this grenade
		if (m_me->IsVisible( ag->GetPosition(), CHECK_FOV, (CBaseEntity *)ag->GetEntity() ))
		{
			// we see it
			if (ag->IsSmoke())
			{
				// ignore smokes
				return true;
			}

			Vector velDir = ag->GetEntity()->GetAbsVelocity();
			float grenadeSpeed = velDir.NormalizeInPlace();
			const float atRestSpeed = 50.0f;

			const float aboutToBlow = 0.5f;
			if (ag->IsFlashbang() && ag->GetEntity()->m_flDetonateTime - gpGlobals->curtime < aboutToBlow)
			{
				// turn away from flashbangs about to explode
				QAngle eyeAngles = m_me->EyeAngles();

				float yaw = RandomFloat( 100.0f, 135.0f );
				eyeAngles.y += (RandomFloat( -1.0f, 1.0f ) < 0.0f) ? (-yaw) : yaw;

				Vector forward;
				AngleVectors( eyeAngles, &forward );

				Vector away = m_me->EyePosition() - 1000.0f * forward;
				const float duration = 2.0f;

				m_me->ClearLookAt();
				m_me->SetLookAt( "Avoid Flashbang", away, PRIORITY_UNINTERRUPTABLE, duration );

				m_me->StopAiming();

				return false;
			}


			// flee from grenades if close by or thrown towards us
			const float throwDangerRange = 750.0f;
			const float nearDangerRange = 300.0f;
			Vector to = ag->GetPosition() - m_me->GetAbsOrigin();
			float range = to.NormalizeInPlace();
			if (range > throwDangerRange)
			{
				return true;
			}

			if (grenadeSpeed > atRestSpeed)
			{
				// grenade is moving
				if (DotProduct( to, velDir ) >= -0.5f)
				{
					// going away from us
					return true;
				}

				m_me->PrintIfWatched( "Retreating from a grenade thrown towards me!\n" );
			}
			else if (range < nearDangerRange)
			{
				// grenade has come to rest near us
				m_me->PrintIfWatched( "Retreating from a grenade that landed near me!\n" );
			}

			// retreat!
			m_me->TryToRetreat( retreatRange, hideTime );

			return false;
		}

		return true;
	}

	CCSBot *m_me;
};

/**
 * React to enemy grenades we see
 */
void CCSBot::AvoidEnemyGrenades( void )
{
	// low skill bots dont avoid grenades
	if (GetProfile()->GetSkill() < 0.5)
	{
		return;
	}

	if (IsAvoidingGrenade())
	{
		// already avoiding one
		return;
	}

	// low skill bots don't avoid grenades
	if (GetProfile()->GetSkill() < 0.6f)
	{
		return;
	}

	GrenadeResponse respond( this );	
	if (TheBots->ForEachGrenade( respond ) == false)
	{
		const float avoidTime = 4.0f;
		m_isAvoidingGrenade.Start( avoidTime );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Reload our weapon if we must
 */
void CCSBot::ReloadCheck( void )
{
	const float safeReloadWaitTime = 3.0f;
	const float reloadAmmoRatio = 0.6f;

	// don't bother to reload if there are no enemies left
	if (GetEnemiesRemaining() == 0)
		return;

	if (IsDefusingBomb() || IsReloading())
		return;

	if (IsActiveWeaponClipEmpty())
	{
		// high-skill players switch to pistol instead of reloading during combat
		if (GetProfile()->GetSkill() > 0.5f && IsAttacking())
		{
			if (!GetActiveCSWeapon()->IsPistol() && !IsPistolEmpty())
			{
				// switch to pistol instead of reloading
				EquipPistol();
				return;
			}
		}
	}
	else if (GetTimeSinceLastSawEnemy() > safeReloadWaitTime && GetActiveWeaponAmmoRatio() <= reloadAmmoRatio)
	{
		// high-skill players use all their ammo and switch to pistol instead of reloading during combat
		if (GetProfile()->GetSkill() > 0.5f && IsAttacking())
			return;
	}
	else
	{
		// do not need to reload
		return;
	}

	// don't reload the AWP until it is totally out of ammo
	if (IsUsing( WEAPON_AWP ) && !IsActiveWeaponClipEmpty())
		return;

	Reload();

	// move to cover to reload if there are enemies nearby
	if (GetNearbyEnemyCount())
	{
		// avoid enemies while reloading (above 0.75 skill always hide to reload)
		const float hideChance = 25.0f + 100.0f * GetProfile()->GetSkill();

		if (!IsHiding() && RandomFloat( 0, 100 ) < hideChance)
		{
			const float safeTime = 5.0f;
			if (GetTimeSinceLastSawEnemy() < safeTime)
			{
				PrintIfWatched( "Retreating to a safe spot to reload!\n" );
				const Vector *spot = FindNearbyRetreatSpot( this, 1000.0f );
				if (spot)
				{
					// ignore enemies for a second to give us time to hide
					// reaching our hiding spot clears our disposition
					IgnoreEnemies( 10.0f );

					Run();
					StandUp();
					Hide( *spot, 0.0f );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Silence/unsilence our weapon if we must
 */
void CCSBot::SilencerCheck( void )
{
	const float safeSilencerWaitTime = 3.5f; // longer than reload check because reloading should take precedence

	if (IsDefusingBomb() || IsReloading() || IsAttacking())
		return;

	// M4A1 and USP are the only weapons with removable silencers
	if (!DoesActiveWeaponHaveSilencer())
		return;

	if (GetTimeSinceLastSawEnemy() < safeSilencerWaitTime)
		return;

	// don't touch the silencer if there are enemies nearby
	if (GetNearbyEnemyCount() == 0)
	{
		CWeaponCSBase *weapon = GetActiveCSWeapon();
		if (weapon == NULL)
			return;

		bool isSilencerOn = weapon->IsSilenced();

		if ( weapon->m_flNextSecondaryAttack >= gpGlobals->curtime )
			return;

		// equip silencer if we want to and we don't have a shield.
		if ( isSilencerOn != (GetProfile()->PrefersSilencer() || GetProfile()->GetSkill() > 0.7f) && !HasShield() )
		{
			PrintIfWatched( "%s silencer!\n", (isSilencerOn) ? "Unequipping" : "Equipping" );
			weapon->SecondaryAttack();
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when in contact with a CBaseCombatWeapon
 */
bool CCSBot::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	CWeaponCSBase *droppedGun = dynamic_cast< CWeaponCSBase* >( pWeapon );

	// right now we only care about primary weapons on the ground
	if ( droppedGun && droppedGun->GetSlot() == WEAPON_SLOT_RIFLE )
	{
		CWeaponCSBase *myGun = dynamic_cast< CWeaponCSBase* >( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

		// if the gun on the ground is the same one we have, dont bother
		if ( myGun && droppedGun->GetWeaponID() != myGun->GetWeaponID() )
		{
			// if we don't have a weapon preference, give up
			if ( GetProfile()->HasPrimaryPreference() )
			{
				// don't change weapons if we've seen enemies recently
				const float safeTime = 2.5f;
				if ( GetTimeSinceLastSawEnemy() >= safeTime )
				{
					// we have a primary weapon - drop it if the one on the ground is better
					for( int i = 0; i < GetProfile()->GetWeaponPreferenceCount(); ++i )
					{
						CSWeaponID prefID = GetProfile()->GetWeaponPreference( i );

						if (!IsPrimaryWeapon( prefID ))
							continue;

						// if the gun we are using is more desirable, give up
						if ( prefID == myGun->GetWeaponID() )
							break;

						if ( prefID == droppedGun->GetWeaponID() )
						{
							// the gun on the ground is better than the one we have - drop our gun
							DropRifle();
							break;
						}
					}
				}
			}
		}
	}

	return BaseClass::BumpWeapon( droppedGun );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if a friend is in our weapon's way
 * @todo Check more rays for safety.
 */
bool CCSBot::IsFriendInLineOfFire( void )
{
	// compute the unit vector along our view
	Vector aimDir = GetViewVector();

	// trace the bullet's path
	trace_t result;
	UTIL_TraceLine( EyePosition(), EyePosition() + 10000.0f * aimDir, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	if (result.DidHitNonWorldEntity())
	{
		CBaseEntity *victim = result.m_pEnt;

		if (victim && victim->IsPlayer() && victim->IsAlive())
		{
			CBasePlayer *player = static_cast<CBasePlayer *>( victim );

			if (player->InSameTeam( this ))
				return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return line-of-sight distance to obstacle along weapon fire ray
 * @todo Re-use this computation with IsFriendInLineOfFire()
 */
float CCSBot::ComputeWeaponSightRange( void )
{
	// compute the unit vector along our view
	Vector aimDir = GetViewVector();

	// trace the bullet's path
	trace_t result;
	UTIL_TraceLine( EyePosition(), EyePosition() + 10000.0f * aimDir, MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &result );

	return (EyePosition() - result.endpos).Length();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given player just fired their weapon
 */
bool CCSBot::DidPlayerJustFireWeapon( const CCSPlayer *player ) const
{
	// if this player has just fired his weapon, we notice him
	CWeaponCSBase *weapon = player->GetActiveCSWeapon();
	return (weapon && !weapon->IsSilenced() && weapon->m_flNextPrimaryAttack > gpGlobals->curtime);
}

