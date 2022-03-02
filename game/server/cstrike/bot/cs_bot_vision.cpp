//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"
#include "datacache/imdlcache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Used to update view angles to stay on a ladder
 */
inline float StayOnLadderLine( CCSBot *me, const CNavLadder *ladder )
{
	// determine our facing
	NavDirType faceDir = AngleToDirection( me->EyeAngles().y );

	const float stiffness = 1.0f;

	// move toward ladder mount point
	switch( faceDir )
	{
		case NORTH:
			return stiffness * (ladder->m_top.x - me->GetAbsOrigin().x);

		case SOUTH:
			return -stiffness * (ladder->m_top.x - me->GetAbsOrigin().x);

		case WEST:
			return -stiffness * (ladder->m_top.y - me->GetAbsOrigin().y);

		case EAST:
			return stiffness * (ladder->m_top.y - me->GetAbsOrigin().y);
	}

	return 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
void CCSBot::ComputeLadderAngles( float *yaw, float *pitch )
{
	if ( !yaw || !pitch )
		return;

	Vector myOrigin = GetCentroid( this );

	// set yaw to aim at ladder
	Vector to = m_pathLadder->GetPosAtHeight(myOrigin.z) - myOrigin;
	float idealYaw = UTIL_VecToYaw( to );

	Vector faceDir = (m_pathLadderFaceIn) ? -m_pathLadder->GetNormal() : m_pathLadder->GetNormal();
	QAngle faceAngles;
	VectorAngles( faceDir, faceAngles );

	const float lookAlongLadderRange = 50.0f;
	const float ladderPitchUpApproach = -30.0f;
	const float ladderPitchUpTraverse = -60.0f;		// -80
	const float ladderPitchDownApproach = 0.0f;
	const float ladderPitchDownTraverse = 80.0f;

	// adjust pitch to look up/down ladder as we ascend/descend
	switch( m_pathLadderState )
	{
		case APPROACH_ASCENDING_LADDER:
		{
			Vector to = m_goalPosition - myOrigin;
			*yaw = idealYaw;

			if (to.IsLengthLessThan( lookAlongLadderRange ))
				*pitch = ladderPitchUpApproach;
			break;
		}

		case APPROACH_DESCENDING_LADDER:
		{
			Vector to = m_goalPosition - myOrigin;
			*yaw = idealYaw;

			if (to.IsLengthLessThan( lookAlongLadderRange ))
				*pitch = ladderPitchDownApproach;
			break;
		}

		case FACE_ASCENDING_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( idealYaw + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( idealYaw - 90.0f );
			}
			else
			{
				*yaw = idealYaw;
			}
			*pitch = ladderPitchUpApproach;
			break;

		case FACE_DESCENDING_LADDER:
			*yaw = idealYaw;
			*pitch = ladderPitchDownApproach;
			break;

		case MOUNT_ASCENDING_LADDER:
		case ASCEND_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( idealYaw + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( idealYaw - 90.0f );
			}
			else
			{
				*yaw = faceAngles[ YAW ] + StayOnLadderLine( this, m_pathLadder );
			}
			*pitch = ( m_pathLadderState == ASCEND_LADDER ) ? ladderPitchUpTraverse : ladderPitchUpApproach;
			break;

		case MOUNT_DESCENDING_LADDER:
		case DESCEND_LADDER:
			*yaw = faceAngles[ YAW ] + StayOnLadderLine( this, m_pathLadder );
			*pitch = ( m_pathLadderState == DESCEND_LADDER ) ? ladderPitchDownTraverse : ladderPitchDownApproach;
			break;

		case DISMOUNT_ASCENDING_LADDER:
			if ( m_pathLadderDismountDir == LEFT )
			{
				*yaw = AngleNormalizePositive( faceAngles[ YAW ] + 90.0f );
			}
			else if ( m_pathLadderDismountDir == RIGHT )
			{
				*yaw = AngleNormalizePositive( faceAngles[ YAW ] - 90.0f );
			}
			else
			{
				*yaw = faceAngles[ YAW ];
			}
			break;

		case DISMOUNT_DESCENDING_LADDER:
			*yaw = faceAngles[ YAW ];
			break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move actual view angles towards desired ones.
 * This is the only place v_angle is altered.
 * @todo Make stiffness and turn rate constants timestep invariant.
 */
void CCSBot::UpdateLookAngles( void )
{
	VPROF_BUDGET( "CCSBot::UpdateLookAngles", VPROF_BUDGETGROUP_NPCS );

	const float deltaT = g_BotUpkeepInterval;
	float maxAccel;
	float stiffness;
	float damping;

	// If mimicing the player, don't modify the view angles.
	if ( bot_mimic.GetInt() )
		return;

	// springs are stiffer when attacking, so we can track and move between targets better
	if (IsAttacking())
	{
		stiffness = 300.0f;
		damping = 30.0f;			// 20
		maxAccel = 3000.0f;	// 4000
	}
	else
	{
		stiffness = 200.0f;
		damping = 25.0f;
		maxAccel = 3000.0f;
	}

	// these may be overridden by ladder logic
	float useYaw = m_lookYaw;
	float usePitch = m_lookPitch;

	//
	// Ladders require precise movement, therefore we need to look at the 
	// ladder as we approach and ascend/descend it.
	// If we are on a ladder, we need to look up or down to traverse it - override pitch in this case.
	//
	// If we're trying to break something, though, we actually need to look at it before we can
	// look at the ladder
	//
	if ( IsUsingLadder() && !(IsLookingAtSpot( PRIORITY_HIGH ) && m_lookAtSpotAttack) )
	{
		ComputeLadderAngles( &useYaw, &usePitch );
	}

	// get current view angles
	QAngle viewAngles = EyeAngles();

	//
	// Yaw
	//
	float angleDiff = AngleNormalize( useYaw - viewAngles.y );

	/*
	 * m_forwardAngle is unreliable. Need to simulate mouse sliding & centering
	if (!IsAttacking())
	{
		// do not allow rotation through our reverse facing angle - go the "long" way instead
		float toCurrent = AngleNormalize( pev->v_angle.y - m_forwardAngle );
		float toDesired = AngleNormalize( useYaw - m_forwardAngle );

		// if angle differences are different signs, they cross the forward facing
		if (toCurrent * toDesired < 0.0f)
		{
			// if the sum of the angles is greater than 180, turn the "long" way around
			if (abs( toCurrent - toDesired ) >= 180.0f)
			{
				if (angleDiff > 0.0f)
					angleDiff -= 360.0f;
				else
					angleDiff += 360.0f;
			}
		}
	}
	*/

	// if almost at target angle, snap to it
	const float onTargetTolerance = 1.0f;		// 3
	if (angleDiff < onTargetTolerance && angleDiff > -onTargetTolerance)
	{
		m_lookYawVel = 0.0f;
		viewAngles.y = useYaw;
	}
	else
	{
		// simple angular spring/damper
		float accel = stiffness * angleDiff - damping * m_lookYawVel;

		// limit rate
		if (accel > maxAccel)
			accel = maxAccel;
		else if (accel < -maxAccel)
			accel = -maxAccel;

		m_lookYawVel += deltaT * accel;
		viewAngles.y += deltaT * m_lookYawVel;

		// keep track of how long our view remains steady
		const float steadyYaw = 1000.0f;
		if (fabs( accel ) > steadyYaw)
		{
			m_viewSteadyTimer.Start();
		}
	}

	//
	// Pitch
	// Actually, this is negative pitch.
	//
	angleDiff = usePitch - viewAngles.x;

	angleDiff = AngleNormalize( angleDiff );


	if (false && angleDiff < onTargetTolerance && angleDiff > -onTargetTolerance)
	{
		m_lookPitchVel = 0.0f;
		viewAngles.x = usePitch;
	}
	else
	{
		// simple angular spring/damper
		// double the stiffness since pitch is only +/- 90 and yaw is +/- 180
		float accel = 2.0f * stiffness * angleDiff - damping * m_lookPitchVel;

		// limit rate
		if (accel > maxAccel)
			accel = maxAccel;
		else if (accel < -maxAccel)
			accel = -maxAccel;

		m_lookPitchVel += deltaT * accel;
		viewAngles.x += deltaT * m_lookPitchVel;

		// keep track of how long our view remains steady
		const float steadyPitch = 1000.0f;
		if (fabs( accel ) > steadyPitch)
		{
			m_viewSteadyTimer.Start();
		}
	}

	//PrintIfWatched( "yawVel = %g, pitchVel = %g\n", m_lookYawVel, m_lookPitchVel );

	// limit range - avoid gimbal lock
	if (viewAngles.x < -89.0f)
		viewAngles.x = -89.0f;
	else if (viewAngles.x > 89.0f)
		viewAngles.x = 89.0f;

	// update view angles
	SnapEyeAngles( viewAngles );

	// if our weapon is zooming, our view is not steady
	if (IsWaitingForZoom())
	{
		m_viewSteadyTimer.Start();
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see the point
 */
bool CCSBot::IsVisible( const Vector &pos, bool testFOV, const CBaseEntity *ignore ) const
{
	VPROF_BUDGET( "CCSBot::IsVisible( pos )", VPROF_BUDGETGROUP_NPCS );

	// we can't see anything if we're blind
	if (IsBlind())
		return false;

	// is it in my general viewcone?
	if (testFOV && !(const_cast<CCSBot *>(this)->FInViewCone( pos )))
		return false;

	// check line of sight against smoke
	if (TheCSBots()->IsLineBlockedBySmoke( EyePositionConst(), pos ))
		return false;

	// check line of sight
	// Must include CONTENTS_MONSTER to pick up all non-brush objects like barrels
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( ignore, COLLISION_GROUP_NONE );
	UTIL_TraceLine( EyePositionConst(), pos, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );
	if (result.fraction != 1.0f)
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we can see any part of the player
 * Check parts in order of importance. Return the first part seen in "visPart" if it is non-NULL.
 */
bool CCSBot::IsVisible( CCSPlayer *player, bool testFOV, unsigned char *visParts ) const
{
	VPROF_BUDGET( "CCSBot::IsVisible( player )", VPROF_BUDGETGROUP_NPCS );

	// optimization - assume if center is not in FOV, nothing is
	// we're using WorldSpaceCenter instead of GUT so we can skip GetPartPosition below - that's
	// the most expensive part of this, and if we can skip it, so much the better.
	if (testFOV && !(const_cast<CCSBot *>(this)->FInViewCone( player->WorldSpaceCenter() )))
	{
		return false;
	}

	unsigned char testVisParts = NONE;

	// check gut
	Vector partPos = GetPartPosition( player, GUT );

	// finish gut check
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == NULL)
			return true;

		testVisParts |= GUT;
	}


	// check top of head
	partPos = GetPartPosition( player, HEAD );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == NULL)
			return true;

		testVisParts |= HEAD;
	}

	// check feet
	partPos = GetPartPosition( player, FEET );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == NULL)
			return true;

		testVisParts |= FEET;
	}

	// check "edges"
	partPos = GetPartPosition( player, LEFT_SIDE );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == NULL)
			return true;

		testVisParts |= LEFT_SIDE;
	}

	partPos = GetPartPosition( player, RIGHT_SIDE );
	if (IsVisible( partPos, testFOV ))
	{
		if (visParts == NULL)
			return true;

		testVisParts |= RIGHT_SIDE;
	}

	if (visParts)
		*visParts = testVisParts;

	if (testVisParts)
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Interesting part positions
 */
CCSBot::PartInfo CCSBot::m_partInfo[ MAX_PLAYERS ];

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute part positions from bone location.
 */
void CCSBot::ComputePartPositions( CCSPlayer *player )
{
	const int headBox = 12;
	const int gutBox = 9;
	const int leftElbowBox = 14;
	const int rightElbowBox = 17;
	//const int hipBox = 0;
	//const int leftFootBox = 4;
	//const int rightFootBox = 8;
	const int maxBoxIndex = rightElbowBox;

	VPROF_BUDGET( "CCSBot::ComputePartPositions", VPROF_BUDGETGROUP_NPCS );

	// which PartInfo corresponds to the given player
	PartInfo *info = &m_partInfo[ player->entindex() % MAX_PLAYERS ];

	// always compute feet, since it doesn't rely on bones
	info->m_feetPos = player->GetAbsOrigin();
	info->m_feetPos.z += 5.0f;

	// get bone positions for interesting points on the player
	MDLCACHE_CRITICAL_SECTION();
	CStudioHdr *studioHdr = player->GetModelPtr();
	if (studioHdr)
	{
		mstudiohitboxset_t *set = studioHdr->pHitboxSet( player->GetHitboxSet() );
		if (set && maxBoxIndex < set->numhitboxes)
		{
			QAngle angles;
			mstudiobbox_t *box;

			// gut
			box = set->pHitbox( gutBox );
			player->GetBonePosition( box->bone, info->m_gutPos, angles );	

			// head
			box = set->pHitbox( headBox );
			player->GetBonePosition( box->bone, info->m_headPos, angles );

			Vector forward, right;
			AngleVectors( angles, &forward, &right, NULL );

			// in local bone space
			const float headForwardOffset = 4.0f;
			const float headRightOffset = 2.0f;
			info->m_headPos += headForwardOffset * forward + headRightOffset * right;

			/// @todo Fix this hack - lower the head target because it's a bit too high for the current T model
			info->m_headPos.z -= 2.0f;


			// left side
			box = set->pHitbox( leftElbowBox );
			player->GetBonePosition( box->bone, info->m_leftSidePos, angles );	

			// right side
			box = set->pHitbox( rightElbowBox );
			player->GetBonePosition( box->bone, info->m_rightSidePos, angles );	

			return;
		}
	}


	// default values if bones are not available
	info->m_headPos = GetCentroid( player );
	info->m_gutPos = info->m_headPos;
	info->m_leftSidePos = info->m_headPos;
	info->m_rightSidePos = info->m_headPos;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return world space position of given part on player.
 * Uses hitboxes to get accurate positions.
 * @todo Optimize by computing once for each player and storing.
 */
const Vector &CCSBot::GetPartPosition( CCSPlayer *player, VisiblePartType part ) const
{
	VPROF_BUDGET( "CCSBot::GetPartPosition", VPROF_BUDGETGROUP_NPCS );

	// which PartInfo corresponds to the given player
	PartInfo *info = &m_partInfo[ player->entindex() % MAX_PLAYERS ];

	if (gpGlobals->framecount > info->m_validFrame)
	{
		// update part positions
		const_cast< CCSBot * >( this )->ComputePartPositions( player );
		info->m_validFrame = gpGlobals->framecount;
	}

	// return requested part position
	switch( part )
	{
		default:
		{
			AssertMsg( false, "GetPartPosition: Invalid part" );
			// fall thru to GUT
		}

		case GUT:
			return info->m_gutPos;

		case HEAD:
			return info->m_headPos;
			
		case FEET:
			return info->m_feetPos;

		case LEFT_SIDE:
			return info->m_leftSidePos;
			
		case RIGHT_SIDE:
			return info->m_rightSidePos;
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Update desired view angles to point towards m_lookAtSpot
 */
void CCSBot::UpdateLookAt( void )
{
	Vector to = m_lookAtSpot - EyePositionConst();

	QAngle idealAngle;
	VectorAngles( to, idealAngle );

	//Vector idealAngle = UTIL_VecToAngles( to );
	//idealAngle.x = 360.0f - idealAngle.x;

	SetLookAngles( idealAngle.y, idealAngle.x );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Look at the given point in space for the given duration (-1 means forever)
 */
void CCSBot::SetLookAt( const char *desc, const Vector &pos, PriorityType pri, float duration, bool clearIfClose, float angleTolerance, bool attack )
{
	if (IsBlind())
		return;

	// if currently looking at a point in space with higher priority, ignore this request
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && m_lookAtSpotPriority > pri)
		return;

	// if already looking at this spot, just extend the time
	const float tolerance = 10.0f;
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && VectorsAreEqual( pos, m_lookAtSpot, tolerance ))
	{
		m_lookAtSpotDuration = duration;

		if (m_lookAtSpotPriority < pri)
			m_lookAtSpotPriority = pri;
	}
	else
	{
		// look at new spot
		m_lookAtSpot = pos; 
		m_lookAtSpotState = LOOK_TOWARDS_SPOT;
		m_lookAtSpotDuration = duration;
		m_lookAtSpotPriority = pri;
	}

	m_lookAtSpotAngleTolerance = angleTolerance;
	m_lookAtSpotClearIfClose = clearIfClose;
	m_lookAtDesc = desc;
	m_lookAtSpotAttack = attack;

	PrintIfWatched( "%3.1f SetLookAt( %s ), duration = %f\n", gpGlobals->curtime, desc, duration );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Block all "look at" and "look around" behavior for given duration - just look ahead
 */
void CCSBot::InhibitLookAround( float duration )
{
	m_inhibitLookAroundTimestamp = gpGlobals->curtime + duration;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update enounter spot timestamps, etc
 */
void CCSBot::UpdatePeripheralVision()
{
	VPROF_BUDGET( "CCSBot::UpdatePeripheralVision", VPROF_BUDGETGROUP_NPCS );

	const float peripheralUpdateInterval = 0.29f;		// if we update at 10Hz, this ensures we test once every three
	if (gpGlobals->curtime - m_peripheralTimestamp < peripheralUpdateInterval)
		return;

	m_peripheralTimestamp = gpGlobals->curtime;

	if (m_spotEncounter)
	{
		// check LOS to all spots in case we see them with our "peripheral vision"
		const SpotOrder *spotOrder;
		Vector pos;

		FOR_EACH_VEC( m_spotEncounter->spots, it )
		{
			spotOrder = &m_spotEncounter->spots[ it ];

			const Vector &spotPos = spotOrder->spot->GetPosition();

			pos.x = spotPos.x;
			pos.y = spotPos.y;
			pos.z = spotPos.z + HalfHumanHeight;

			if (!IsVisible( pos, CHECK_FOV ))
				continue;

			// can see hiding spot, remember when we saw it last
			SetHidingSpotCheckTimestamp( spotOrder->spot );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update the "looking around" behavior.
 */
void CCSBot::UpdateLookAround( bool updateNow )
{
	VPROF_BUDGET( "CCSBot::UpdateLookAround", VPROF_BUDGETGROUP_NPCS );

	//
	// If we recently saw an enemy, look towards where we last saw them
	// Unless we can hear them moving, in which case look towards the noise
	//
	const float closeRange = 500.0f;
	if (!IsNoiseHeard() || GetNoiseRange() > closeRange)
	{
		const float recentThreatTime = 1.0f; // 0.25f;
		if (!IsLookingAtSpot( PRIORITY_MEDIUM ) && gpGlobals->curtime - m_lastSawEnemyTimestamp < recentThreatTime)
		{
			ClearLookAt();

			Vector spot = m_lastEnemyPosition;

			// find enemy position on the ground
			if (TheNavMesh->GetSimpleGroundHeight( m_lastEnemyPosition, &spot.z ))
			{
				spot.z += HalfHumanHeight;
				SetLookAt( "Last Enemy Position", spot, PRIORITY_MEDIUM, RandomFloat( 2.0f, 3.0f ), true );
				return;
			}
		}
	}

	//
	// Look at nearby enemy noises
	//
	if (UpdateLookAtNoise())
		return;


	// check if looking around has been inhibited
	// Moved inhibit to allow high priority enemy lookats to still occur
	if (gpGlobals->curtime < m_inhibitLookAroundTimestamp)
		return;

	//
	// If we are hiding (or otherwise standing still), watch all approach points leading into this region
	//
	const float minStillTime = 2.0f;
	if (IsAtHidingSpot() || IsNotMoving( minStillTime ))
	{
		// update approach points
		const float recomputeApproachPointTolerance = 50.0f;
		if ((m_approachPointViewPosition - GetAbsOrigin()).IsLengthGreaterThan( recomputeApproachPointTolerance ))
		{
			ComputeApproachPoints();
			m_approachPointViewPosition = GetAbsOrigin();
		}

		// if we're sniping, zoom in to watch our approach points
		if (IsUsingSniperRifle())
		{
			// low skill bots don't pre-zoom
			if (GetProfile()->GetSkill() > 0.4f)
			{
				if (!IsViewMoving())
				{
					float range = ComputeWeaponSightRange();
					AdjustZoom( range );
				}
				else
				{
					// zoom out
					if (GetZoomLevel() != NO_ZOOM)
						SecondaryAttack();
				}
			}
		}

		if (m_lastKnownArea == NULL)
			return;

		if (gpGlobals->curtime < m_lookAroundStateTimestamp)
			return;

		// if we're sniping, switch look-at spots less often
		if (IsUsingSniperRifle())
			m_lookAroundStateTimestamp = gpGlobals->curtime + RandomFloat( 5.0f, 10.0f );
		else
			m_lookAroundStateTimestamp = gpGlobals->curtime + RandomFloat( 1.0f, 2.0f );	// 0.5, 1.0


		#define MAX_APPROACHES 16
		Vector validSpot[ MAX_APPROACHES ];
		int validSpotCount = 0;

		Vector *earlySpot = NULL;
		float earliest = 999999.9f;

		for( int i=0; i<m_approachPointCount; ++i )
		{
			float spotTime = m_approachPoint[i].m_area->GetEarliestOccupyTime( OtherTeam( GetTeamNumber() ) );

			// ignore approach areas the enemy could not have possibly reached yet
			if (TheCSBots()->GetElapsedRoundTime() >= spotTime)
			{
				validSpot[ validSpotCount++ ] = m_approachPoint[i].m_pos;
			}
			else
			{
				// keep track of earliest spot we can see in case we get there very early
				if (spotTime < earliest)
				{
					earlySpot = &m_approachPoint[i].m_pos;
					earliest = spotTime;
				}
			}
		}

		Vector spot;

		if (validSpotCount)
		{
			int which = RandomInt( 0, validSpotCount-1 );
			spot = validSpot[ which ];
		}
		else if (earlySpot)
		{
			// all of the spots we can see can't be reached yet by the enemy - look at the earliest spot
			spot = *earlySpot;
		}
		else
		{
			return;
		}

		// don't look at the floor, look roughly at chest level
		/// @todo If this approach point is very near, this will cause us to aim up in the air if were crouching
		spot.z += HalfHumanHeight;

		SetLookAt( "Approach Point (Hiding)", spot, PRIORITY_LOW );

		return;
	}

	//
	// Glance at "encouter spots" as we move past them
	//
	if (m_spotEncounter)
	{
		//
		// Check encounter spots
		//
		if (!IsSafe() && !IsLookingAtSpot( PRIORITY_LOW ))
		{
			// allow a short time to look where we're going
			if (gpGlobals->curtime < m_spotCheckTimestamp)
				return;

			/// @todo Use skill parameter instead of accuracy

			// lower skills have exponentially longer delays
			float asleep = (1.0f - GetProfile()->GetSkill());
			asleep *= asleep;
			asleep *= asleep;

			m_spotCheckTimestamp = gpGlobals->curtime + asleep * RandomFloat( 10.0f, 30.0f );


			// figure out how far along the path segment we are
			Vector delta = m_spotEncounter->path.to - m_spotEncounter->path.from;
			float length = delta.Length();
			float adx = (float)fabs(delta.x);
			float ady = (float)fabs(delta.y);
			float t;
			Vector myOrigin = GetCentroid( this );

			if (adx > ady)
				t = (myOrigin.x - m_spotEncounter->path.from.x) / delta.x;
			else
				t = (myOrigin.y - m_spotEncounter->path.from.y) / delta.y;

			// advance parameter a bit so we "lead" our checks
			const float leadCheckRange = 50.0f;
			t += leadCheckRange / length;

			if (t < 0.0f)
				t = 0.0f;
			else if (t > 1.0f)
				t = 1.0f;

			// collect the unchecked spots so far
			#define MAX_DANGER_SPOTS 16
			HidingSpot *dangerSpot[MAX_DANGER_SPOTS];
			int dangerSpotCount = 0;
			int dangerIndex = 0;

			const float checkTime = 10.0f;
			const SpotOrder *spotOrder;
			FOR_EACH_VEC( m_spotEncounter->spots, it )
			{
				spotOrder = &(m_spotEncounter->spots[ it ]);

				// if we have seen this spot recently, we don't need to look at it
				if (gpGlobals->curtime - GetHidingSpotCheckTimestamp( spotOrder->spot ) <= checkTime)
					continue;

				if (spotOrder->t > t)
					break;

				// ignore spots the enemy could not have possibly reached yet
				if (spotOrder->spot->GetArea())
				{
					if (TheCSBots()->GetElapsedRoundTime() < spotOrder->spot->GetArea()->GetEarliestOccupyTime( OtherTeam( GetTeamNumber() ) ))
					{
						continue;
					}
				}

				dangerSpot[ dangerIndex++ ] = spotOrder->spot;
				if (dangerIndex >= MAX_DANGER_SPOTS)
					dangerIndex = 0;
				if (dangerSpotCount < MAX_DANGER_SPOTS)
					++dangerSpotCount;
			}

			if (dangerSpotCount)
			{
				// pick one of the spots at random
				int which = RandomInt( 0, dangerSpotCount-1 );

				// glance at the spot for minimum time 
				SetLookAt( "Encounter Spot", dangerSpot[which]->GetPosition() + Vector( 0, 0, HalfHumanHeight ), PRIORITY_LOW, 0.2f, true, 10.0f );

				// immediately mark it as "checked", so we don't check it again
				// if we get distracted before we check it - that's the way it goes
				SetHidingSpotCheckTimestamp( dangerSpot[which] );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * "Bend" our line of sight around corners until we can "see" the point. 
 */
bool CCSBot::BendLineOfSight( const Vector &eye, const Vector &target, Vector *bend, float angleLimit ) const
{
	VPROF_BUDGET( "CCSBot::BendLineOfSight", VPROF_BUDGETGROUP_NPCS );

	bool doDebug = false;
	const float debugDuration = 0.04f;
	if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
		NDebugOverlay::Line( eye, target, 255, 255, 255, true, debugDuration );

	// if we can directly see the point, use it
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceLine( eye, target, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );
	if (result.fraction == 1.0f && !result.startsolid)
	{
		// can directly see point, no bending needed
		*bend = target;
		return true;
	}

	// "bend" our line of sight until we can see the approach point
	Vector to = target - eye;
	float startAngle = UTIL_VecToYaw( to );
	float length = to.Length2D();
	to.NormalizeInPlace();

	struct Color3
	{
		int r, g, b;
	};
	const int colorCount = 6;
	Color3 colorSet[ colorCount ] =
	{
		{ 255,   0,   0 },
		{   0, 255,   0 },
		{   0,   0, 255 },
		{ 255, 255,   0 },
		{   0, 255, 255 },
		{ 255,   0, 255 },
	};

	int color = 0;

	// optiming assumption - previous rays cast "shadow" on subsequent rays since they already
	// enumerated visible space along their length.
	// We should do a dot product and compute the exact length, but since the angular changes
	// are incremental, using the direct length should be close enough.
	float priorVisibleLength[2] = { 0.0f, 0.0f };

	float angleInc = 5.0f; 
	for( float angle = angleInc; angle <= angleLimit; angle += angleInc )
	{
		// check both sides at this angle offset
		for( int side=0; side<2; ++side )
		{
			float actualAngle = (side) ? (startAngle + angle) : (startAngle - angle);

			float dx = cos( 3.141592f * actualAngle / 180.0f );
			float dy = sin( 3.141592f * actualAngle / 180.0f );

			// compute rotated point ray endpoint
			Vector rotPoint( eye.x + length * dx, eye.y + length * dy, target.z );

			// check LOS to find length to test along ray
			UTIL_TraceLine( eye, rotPoint, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );

			// if this ray started in an obstacle, skip it
			if (result.startsolid)
			{
				continue;
			}

			Vector ray = rotPoint - eye;
			float rayLength = ray.NormalizeInPlace();
			float visibleLength = rayLength * result.fraction;

			if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
			{
				NDebugOverlay::Line( eye, eye + visibleLength * ray, colorSet[color].r, colorSet[color].g, colorSet[color].b, true, debugDuration );
			}

			// step along ray, checking if point is visible from ray point
			const float bendStepSize = 50.0f; 

			// start from point that prior rays couldn't see
			float startLength = priorVisibleLength[ side ];

			for( float bendLength=startLength; bendLength <= visibleLength; bendLength += bendStepSize )
			{
				// compute point along ray
				Vector bendPoint = eye + bendLength * ray;

				// check if we can see approach point from this bend point
				UTIL_TraceLine( bendPoint, target, MASK_VISIBLE_AND_NPCS, &traceFilter, &result );

				if (doDebug && cv_bot_debug.GetBool() && IsLocalPlayerWatchingMe())
				{
					NDebugOverlay::Line( bendPoint, result.endpos, colorSet[color].r/2, colorSet[color].g/2, colorSet[color].b/2, true, debugDuration );
				}

				if (result.fraction == 1.0f && !result.startsolid)
				{
					// target is visible from this bend point on the ray - use this point on the ray as our point

					// keep "bent" point at correct height along line of sight
					bendPoint.z = eye.z + bendLength * to.z;

					*bend = bendPoint;

					return true;
				}	
			}

			priorVisibleLength[ side ] = visibleLength;

			++color;
			if (color >= colorCount)
			{
				color = 0;
			}
		} // side
	}

	// bending rays didn't help - still can't see the point
	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we "notice" given player
 * @todo Increase chance if player is rotating
 * @todo Decrease chance as nears edge of FOV
 */
bool CCSBot::IsNoticable( const CCSPlayer *player, unsigned char visParts ) const
{
	// if this player has just fired his weapon, we notice him
	if (DidPlayerJustFireWeapon( player ))
	{
		return true;
	}

	float deltaT = m_attentionInterval.GetElapsedTime();

	// all chances are specified in terms of a standard "quantum" of time
	// in which a normal person would notice something
	const float noticeQuantum = 0.25f;

	// determine percentage of player that is visible
	float coverRatio = 0.0f;

	if (visParts & GUT)
	{
		const float chance = 40.0f;
		coverRatio += chance;
	}

	if (visParts & HEAD)
	{
		const float chance = 10.0f;
		coverRatio += chance;
	}

	if (visParts & LEFT_SIDE)
	{
		const float chance = 20.0f;
		coverRatio += chance;
	}

	if (visParts & RIGHT_SIDE)
	{
		const float chance = 20.0f;
		coverRatio += chance;
	}

	if (visParts & FEET)
	{
		const float chance = 10.0f;
		coverRatio += chance;
	}


	// compute range modifier - farther away players are harder to notice, depeding on what they are doing
	float range = (player->GetAbsOrigin() - GetAbsOrigin()).Length();
	const float closeRange = 300.0f;
	const float farRange = 1000.0f;

	float rangeModifier;
	if (range < closeRange)
	{
		rangeModifier = 0.0f;
	}
	else if (range > farRange)
	{
		rangeModifier = 1.0f;
	}
	else
	{
		rangeModifier = (range - closeRange)/(farRange - closeRange);
	}


	// harder to notice when crouched
	bool isCrouching = (player->GetFlags() & FL_DUCKING);


	// moving players are easier to spot
	float playerSpeedSq = player->GetAbsVelocity().LengthSqr();
	const float runSpeed = 200.0f;
	const float walkSpeed = 30.0f;
	float farChance, closeChance;
	if (playerSpeedSq > runSpeed * runSpeed)
	{
		// running players are always easy to spot (must be standing to run)
		return true;
	}
	else if (playerSpeedSq > walkSpeed * walkSpeed)
	{
		// walking players are less noticable far away
		if (isCrouching)
		{
			closeChance = 90.0f;
			farChance = 60.0f;
		}
		else // standing
		{
			closeChance = 100.0f;
			farChance = 75.0f;
		}
	}
	else
	{
		// motionless players are hard to notice
		if (isCrouching)
		{
			// crouching and motionless - very tough to notice
			closeChance = 80.0f;
			farChance = 5.0f;		// takes about three seconds to notice (50% chance)
		}
		else // standing
		{
			closeChance = 100.0f;
			farChance = 10.0f;
		}
	}

	// combine posture, speed, and range chances
	float dispositionChance = closeChance + (farChance - closeChance) * rangeModifier;

	// determine actual chance of noticing player
	float noticeChance = dispositionChance * coverRatio/100.0f;

	// scale by skill level
	noticeChance *= (0.5f + 0.5f * GetProfile()->GetSkill());

	// if we are alert, our chance of noticing is much higher
	if (IsAlert())
	{
		const float alertBonus = 50.0f;
		noticeChance += alertBonus;
	}

	// scale by time quantum
	noticeChance *= deltaT / noticeQuantum;

	// there must always be a chance of detecting the enemy
	const float minChance = 0.1f;
	if (noticeChance < minChance)
	{
		noticeChance = minChance;
	}

	//PrintIfWatched( "Notice chance = %3.2f\n", noticeChance );

	return (RandomFloat( 0.0f, 100.0f ) < noticeChance);
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return most dangerous threat in my field of view (feeds into reaction time queue).
 * @todo Account for lighting levels, cover, and distance to see if we notice enemy
 */
CCSPlayer *CCSBot::FindMostDangerousThreat( void )
{
	VPROF_BUDGET( "CCSBot::FindMostDangerousThreat", VPROF_BUDGETGROUP_NPCS );

	if (IsBlind())
	{
		return NULL;
	}

	enum { MAX_THREATS = 16 };		// maximum number of simulataneously attendable threats	
	struct CloseInfo
	{
		CCSPlayer *enemy;
		float range;
	}
	threat[ MAX_THREATS ];
	int threatCount = 0;

	int prevIndex = m_enemyQueueIndex - 1;
	if ( prevIndex < 0 )
		prevIndex = MAX_ENEMY_QUEUE - 1;
	CCSPlayer *currentThreat = m_enemyQueue[ prevIndex ].player;

	m_bomber = NULL;
	m_isEnemySniperVisible = false;

	m_closestVisibleFriend = NULL;
	float closeFriendRange = 99999999999.9f;

	m_closestVisibleHumanFriend = NULL;
	float closeHumanFriendRange = 99999999999.9f;

	CCSPlayer *sniperThreat = NULL;
	float sniperThreatRange = 99999999999.9f;
	bool sniperThreatIsFacingMe = false;

	const float lookingAtMeTolerance = 0.7071f;

	int i;

	{
		VPROF_BUDGET( "CCSBot::Collect Threats", VPROF_BUDGETGROUP_NPCS );

		for( i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *entity = UTIL_PlayerByIndex( i );

			if (entity == NULL)
				continue;

			// is it a player?
			if (!entity->IsPlayer())
				continue;

			CCSPlayer *player = static_cast<CCSPlayer *>( entity );

			// ignore self
			if (player->entindex() == entindex())
				continue;

			// is it alive?
			if (!player->IsAlive())
				continue;

			// is it an enemy?
			if (player->InSameTeam( this ))
			{
				// keep track of nearby friends - use less exact visibility check
				if (IsVisible( entity->WorldSpaceCenter(), false, this ))
				{
					// update watch timestamp
					int idx = player->entindex();
					m_watchInfo[idx].timestamp = gpGlobals->curtime;
					m_watchInfo[idx].isEnemy = false;

					// keep track of our closest friend 
					Vector to = GetAbsOrigin() - player->GetAbsOrigin();
					float rangeSq = to.LengthSqr();
					if (rangeSq < closeFriendRange)
					{
						m_closestVisibleFriend = player;
						closeFriendRange = rangeSq;
					}

					// keep track of our closest human friend 
					if (!player->IsBot() && rangeSq < closeHumanFriendRange)
					{
						m_closestVisibleHumanFriend = player;
						closeHumanFriendRange = rangeSq;
					}
				}

				continue;
			}

			// check if this enemy is fully or partially visible
			unsigned char visParts;
			if (!IsVisible( player, CHECK_FOV, &visParts ))
				continue;

			// do we notice this enemy? (always notice current enemy)
			if (player != currentThreat)
			{
				if (!IsNoticable( player, visParts ))
				{
					continue;
				}
			}

			// update watch timestamp
			int idx = player->entindex();
			m_watchInfo[idx].timestamp = gpGlobals->curtime;
			m_watchInfo[idx].isEnemy = true;

			// note if we see the bomber
			if (player->HasC4())
			{
				m_bomber = player;
			}

			// keep track of all visible threats
			Vector d = GetAbsOrigin() - player->GetAbsOrigin();
			float distSq = d.LengthSqr();

			// track enemy sniper threats
			if (IsSniperRifle( player->GetActiveCSWeapon() ))
			{
				m_isEnemySniperVisible = true;

				// keep track of the most dangerous sniper we see
				if (sniperThreat)
				{
					if (IsPlayerLookingAtMe( player, lookingAtMeTolerance ))
					{
						if (sniperThreatIsFacingMe)
						{
							// several snipers are facing us - keep closest
							if (distSq < sniperThreatRange)
							{
								sniperThreat = player;
								sniperThreatRange = distSq;
								sniperThreatIsFacingMe = true;
							}
						}
						else
						{
							// even if this sniper is farther away, keep it because he's aiming at us
							sniperThreat = player;
							sniperThreatRange = distSq;
							sniperThreatIsFacingMe = true;
						}
					}
					else
					{
						// this sniper is not looking at us, only consider it if we dont have a sniper facing us
						if (!sniperThreatIsFacingMe && distSq < sniperThreatRange)
						{
							sniperThreat = player;
							sniperThreatRange = distSq;
						}
					}
				}
				else
				{
					// first sniper we see
					sniperThreat = player;
					sniperThreatRange = distSq;
					sniperThreatIsFacingMe = IsPlayerLookingAtMe( player, lookingAtMeTolerance );
				}
			}


			{
				VPROF_BUDGET( "CCSBot::Sort Threats", VPROF_BUDGETGROUP_NPCS );

				// maintain set of visible threats, sorted by increasing distance
				if (threatCount == 0)
				{
					threat[0].enemy = player;
					threat[0].range = distSq;
					threatCount = 1;
				}
				else
				{
					// find insertion point
					int j;
					for( j=0; j<threatCount; ++j )
					{
						if (distSq < threat[j].range)
							break;
					}

					// shift lower half down a notch
					for( int k=threatCount-1; k>=j; --k )
						threat[k+1] = threat[k];

					// insert threat into sorted list
					threat[j].enemy = player;
					threat[j].range = distSq;

					if (threatCount < MAX_THREATS)
						++threatCount;
				}
			}
		}
	}


	{
		VPROF_BUDGET( "CCSBot::Count nearby Friends & Enemies", VPROF_BUDGETGROUP_NPCS );

		// track the maximum enemy and friend counts we've seen recently
		int prevEnemies = m_nearbyEnemyCount;
		m_nearbyEnemyCount = 0;
		m_nearbyFriendCount = 0;
		for( i=0; i<MAX_PLAYERS; ++i )
		{
			if (m_watchInfo[i].timestamp <= 0.0f)
				continue;

			const float recentTime = 3.0f;
			if (gpGlobals->curtime - m_watchInfo[i].timestamp < recentTime)
			{
				if (m_watchInfo[i].isEnemy)
					++m_nearbyEnemyCount;
				else
					++m_nearbyFriendCount;
			}
		}

		// note when we saw this batch of enemies
		if (prevEnemies == 0 && m_nearbyEnemyCount > 0)
		{
			m_firstSawEnemyTimestamp = gpGlobals->curtime;
		}
	}


	{
		VPROF_BUDGET( "CCSBot::Track enemy Place", VPROF_BUDGETGROUP_NPCS );

		//
		// Track the place where we saw most of our enemies
		//
		struct PlaceRank
		{
			unsigned int place;
			int count;
		};
		static PlaceRank placeRank[ MAX_PLACES_PER_MAP ];
		int locCount = 0;

		PlaceRank common;
		common.place = 0;
		common.count = 0;

		for( i=0; i<threatCount; ++i )
		{
			// find the area the player/bot is standing on
			CNavArea *area;
			CCSBot *bot = dynamic_cast<CCSBot *>(threat[i].enemy);
			if (bot && bot->IsBot())
			{
				area = bot->GetLastKnownArea();
			}
			else
			{
				Vector enemyOrigin = GetCentroid( threat[i].enemy );
				area = TheNavMesh->GetNearestNavArea( enemyOrigin );
			}

			if (area == NULL)
				continue;

			unsigned int threatLoc = area->GetPlace();
			if (!threatLoc)
				continue;

			// if place is already in set, increment count
			int j;
			for( j=0; j<locCount; ++j )
				if (placeRank[j].place == threatLoc)
					break;

			if (j == locCount)
			{
				// new place
				if (locCount < MAX_PLACES_PER_MAP)
				{
					placeRank[ locCount ].place = threatLoc;
					placeRank[ locCount ].count = 1;

					if (common.count == 0)
						common = placeRank[locCount];

					++locCount;
				}
			}
			else
			{
				// others are in that place, increment
				++placeRank[j].count;

				// keep track of the most common place
				if (placeRank[j].count > common.count)
					common = placeRank[j];
			}
		}

		// remember most common place
		m_enemyPlace = common.place;
	}


	{
		VPROF_BUDGET( "CCSBot::Select Threat", VPROF_BUDGETGROUP_NPCS );

		if (threatCount == 0)
			return NULL;

		// if we can still see our current threat, keep it
		// unless a new one is much closer
		bool sawCloserThreat = false;
		bool sawCurrentThreat = false;
		int t;
		for( t=0; t<threatCount; ++t )
		{
			if ( threat[t].enemy == currentThreat )
			{
				sawCurrentThreat = true;
			}
			else if ( threat[t].enemy != currentThreat &&
				IsSignificantlyCloser( threat[t].enemy, currentThreat ) )
			{
				sawCloserThreat = true;
			}
		}

		if ( sawCurrentThreat && !sawCloserThreat )
		{
			return currentThreat;
		}

		// if we are a sniper and we see a sniper threat, attack it unless 
		// there are other close enemies facing me
		if (IsSniper() && sniperThreat)
		{
			const float closeCombatRange = 500.0f;

			for( t=0; t<threatCount; ++t )
			{
				if (threat[t].range < closeCombatRange && IsPlayerLookingAtMe( threat[t].enemy, lookingAtMeTolerance ))
				{
					return threat[t].enemy;
				}
			}

			return sniperThreat;
		}
		
		// otherwise, find the closest threat that is looking at me
		for( t=0; t<threatCount; ++t )
		{
			if (IsPlayerLookingAtMe( threat[t].enemy, lookingAtMeTolerance ))
			{
				return threat[t].enemy;
			}
		}
	}


	// return closest threat
	return threat[0].enemy;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Update our reaction time queue
 */
void CCSBot::UpdateReactionQueue( void )
{
	VPROF_BUDGET( "CCSBot::UpdateReactionQueue", VPROF_BUDGETGROUP_NPCS );

	// zombies dont see any threats
	if (cv_bot_zombie.GetBool())
		return;

	// find biggest threat at this instant
	CCSPlayer *threat = FindMostDangerousThreat();

	// reset timer
	m_attentionInterval.Start();


	int now = m_enemyQueueIndex;

	// store a snapshot of its state at the end of the reaction time queue
	if (threat)
	{
		m_enemyQueue[ now ].player = threat;
		m_enemyQueue[ now ].isReloading = threat->IsReloading();
		m_enemyQueue[ now ].isProtectedByShield = threat->IsProtectedByShield();
	}
	else
	{
		m_enemyQueue[ now ].player = NULL;
		m_enemyQueue[ now ].isReloading = false;
		m_enemyQueue[ now ].isProtectedByShield = false;
	}

	// queue is round-robin
	++m_enemyQueueIndex;
	if (m_enemyQueueIndex >= MAX_ENEMY_QUEUE)
		m_enemyQueueIndex = 0;
	
	if (m_enemyQueueCount < MAX_ENEMY_QUEUE)
		++m_enemyQueueCount;

	// clamp reaction time to enemy queue size
	float reactionTime = GetProfile()->GetReactionTime() - g_BotUpdateInterval;
	float maxReactionTime = (MAX_ENEMY_QUEUE * g_BotUpdateInterval) - 0.01f;
	if (reactionTime > maxReactionTime)
		reactionTime = maxReactionTime;

	// "rewind" time back to our reaction time
	int reactionTimeSteps = (int)((reactionTime / g_BotUpdateInterval) + 0.5f);

	int i = now - reactionTimeSteps;
	if (i < 0)
		i += MAX_ENEMY_QUEUE;

	m_enemyQueueAttendIndex = (unsigned char)i;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the most dangerous threat we are "conscious" of
 */
CCSPlayer *CCSBot::GetRecognizedEnemy( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount || IsBlind())
	{
		return NULL;
	}

	return m_enemyQueue[ m_enemyQueueAttendIndex ].player;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is reloading
 */
bool CCSBot::IsRecognizedEnemyReloading( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount)
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isReloading;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the enemy we are "conscious" of is hiding behind a shield
 */
bool CCSBot::IsRecognizedEnemyProtectedByShield( void )
{
	if (m_enemyQueueAttendIndex >= m_enemyQueueCount)
		return false;

	return m_enemyQueue[ m_enemyQueueAttendIndex ].isProtectedByShield;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return distance to closest enemy we are "conscious" of
 */
float CCSBot::GetRangeToNearestRecognizedEnemy( void )
{
	const CCSPlayer *enemy = GetRecognizedEnemy();

	if (enemy)
		return (GetAbsOrigin() - enemy->GetAbsOrigin()).Length();

	return 99999999.9f;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Blind the bot for the given duration
 */
void CCSBot::Blind( float holdTime, float fadeTime, float startingAlpha )
{
	PrintIfWatched( "Blinded: holdTime = %3.2f, fadeTime = %3.2f, alpha = %3.2f\n", holdTime, fadeTime, startingAlpha );

	// if we were only blinded a little bit, shake it off
	const float mildBlindTime = 3.0f;
	if (holdTime < mildBlindTime)
	{
		Wait( 0.75f * holdTime );
		BecomeAlert();
		BaseClass::Blind( holdTime, fadeTime, startingAlpha );
		return;
	}


	// if blinded while in combat - then spray and pray!
	m_blindFire = IsAttacking();

	// retreat
	// do this first, so spot selection happens before IsBlind() is set
	const float hideRange = 400.0f;
	TryToRetreat( hideRange );

	PrintIfWatched( "I'm blind!\n" );

	if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
	{
		GetChatter()->Say( "Blinded", 1.0f );
	}

	// no longer safe
	AdjustSafeTime();

	// decide which way to move while blind
	m_blindMoveDir = static_cast<NavRelativeDirType>( RandomInt( 1, NUM_RELATIVE_DIRECTIONS-1 ) );

	// if we're defusing, don't give up
	if (IsDefusingBomb())
	{
		return;
	}

	// can't see to aim at enemy
	StopAiming();

	// dont override "facing away" behavior unless we are going to spray and pray
	if (m_blindFire)
	{
		ClearLookAt();

		// just look straight ahead while blind
		Vector forward;
		EyeVectors( &forward );
		SetLookAt( "Blind", EyePosition() + 10000.0f * forward, PRIORITY_UNINTERRUPTABLE, holdTime + 0.5f * fadeTime );
	}
	
	StopWaiting();
	BecomeAlert();

	BaseClass::Blind( holdTime, fadeTime, startingAlpha );
}


//--------------------------------------------------------------------------------------------------------------
class CheckLookAt
{
public:
	CheckLookAt( const CCSBot *me, bool testFOV )
	{
		m_me = me;
		m_testFOV = testFOV;
	}

	bool operator() ( CBasePlayer *player )
	{
		if (!m_me->IsEnemy( player ))
			return true;

		if (m_testFOV && !(const_cast< CCSBot * >(m_me)->FInViewCone( player->WorldSpaceCenter() )))
			return true;

		if (!m_me->IsPlayerLookingAtMe( player ))
			return true;
			
		if (m_me->IsVisible( (CCSPlayer *)player ))
			return false;

		return true;
	}

	const CCSBot *m_me;
	bool m_testFOV;
};

/**
 * Return true if any enemy I have LOS to is looking directly at me
 * @todo Use reaction time pipeline
 */
bool CCSBot::IsAnyVisibleEnemyLookingAtMe( bool testFOV ) const
{
	CheckLookAt checkLookAt( this, testFOV );
	return (ForEachPlayer( checkLookAt ) == false) ? true : false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Do panic behavior
 */
void CCSBot::UpdatePanicLookAround( void )
{
	if (m_panicTimer.IsElapsed())
	{
		return;
	}

	if (IsEnemyVisible())
	{
		StopPanicking();
		return;
	}

	if (HasLookAtTarget())
	{
		// wait until we finish our current look at
		return;
	}

	// select a spot somewhere behind us to look at as we search for our attacker
	const QAngle &eyeAngles = EyeAngles();

	QAngle newAngles;
	newAngles.x = RandomFloat( -30.0f, 30.0f );

	// Look directly behind at a random offset in a 90 window.
	float yaw = RandomFloat( 135.0f, 225.0f );
	newAngles.y = eyeAngles.y + yaw;
	newAngles.z = 0.0f;

	Vector forward;
	AngleVectors( newAngles, &forward );

	Vector spot;
	spot = EyePosition() + 1000.0f * forward;

	SetLookAt( "Panic", spot, PRIORITY_HIGH, 0.0f );
	PrintIfWatched( "Panic yaw angle = %3.2f\n", newAngles.y );
}
