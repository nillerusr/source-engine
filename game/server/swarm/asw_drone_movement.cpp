#include "cbase.h"
#include "asw_drone_movement.h"
#include "igamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CASW_Drone_Movement::CASW_Drone_Movement()
{
	mv = NULL;
	m_pNPC = NULL;
	m_flInterval = 0;
	m_LastHitWallNormal.Init(0,0,0);
	m_surfaceFriction = 0;	
}

const Vector&	CASW_Drone_Movement::GetOuterMins() const { return m_pNPC->GetHullMins(); }
const Vector&	CASW_Drone_Movement::GetOuterMaxs() const { return m_pNPC->GetHullMins(); }

extern bool g_bMovementOptimizations;
extern ConVar sv_bounce;
extern ConVar sv_gravity;
extern const char *DescribeAxis( int axis );

#define	STOP_EPSILON		0.1
#define	MAX_CLIP_PLANES		5
#define STEP_SIZE		24

int CASW_Drone_Movement::ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	angle = normal[ 2 ];

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 
	

	// Determine how far along plane to slide based on incoming direction.
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change; 
		// If out velocity is too small, zero it out.
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
	
#if 0
	// slight adjustment - hopefully to adjust for displacement surfaces
	float adjust = DotProduct( out, normal );
	if( adjust < 0.0f )
	{
		out += ( normal * -adjust );
//		Msg( "Adjustment = %lf\n", adjust );
	}
#endif

	// Return blocking flags.
	return blocked;
}

int CASW_Drone_Movement::TryMove( Vector *pFirstDest, trace_t *pFirstTrace )
{
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	int			i, j;
	trace_t	pm;
	Vector		end;
	float		time_left, allFraction;
	int			blocked;		
	
	numbumps  = 4;           // Bump up to four times
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy (mv->m_vecVelocity, original_velocity);  // Store original velocity
	VectorCopy (mv->m_vecVelocity, primal_velocity);
	
	allFraction = 0;
	time_left = m_flInterval;   // Total time for this movement operation.

	new_velocity.Init();

	// set the last wall normal z high, so any planes found are 'more upright'
	m_LastHitWallNormal.z = 2.0f;

	for (bumpcount=0 ; bumpcount < numbumps; bumpcount++)
	{
		if ( mv->m_vecVelocity.Length() == 0.0 )
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA( mv->GetAbsOrigin(), time_left, mv->m_vecVelocity, end );

		// See if we can make it from origin to end point.
		if ( g_bMovementOptimizations )
		{
			// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
			if ( pFirstDest && end == *pFirstDest )
				pm = *pFirstTrace;
			else
				UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), end, MASK_NPCSOLID, &pm );
				//TraceBBox( mv->GetAbsOrigin(), end, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), pm );
		}
		else
		{
			UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), end, MASK_NPCSOLID, &pm );
			//TraceBBox( mv->GetAbsOrigin(), end, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), pm );
		}

		allFraction += pm.fraction;
		
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			return 4;
		}

		if (pm.fraction < 1.0f)	// we've hit something, store the most wall-ish normal
		{
			// store the most upright plane
			if (m_LastHitWallNormal.z > pm.plane.normal.z)
				VectorCopy(pm.plane.normal, m_LastHitWallNormal);
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if( pm.fraction > 0 )
		{	
			// actually covered some distance
			mv->SetAbsOrigin(pm.endpos);
			VectorCopy (mv->m_vecVelocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm.fraction == 1)
		{
			 break;		// moved the entire distance
		}

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		//MoveHelper( )->AddToTouched( pm, mv->m_vecVelocity );

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm.plane.normal[2])
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (pm.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// relfect npc velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if ( numplanes == 1 &&
			m_pNPC->GetMoveType() == MOVETYPE_WALK &&
			m_pNPC->GetGroundEntity() == NULL )	
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{
					// floor or slope
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + sv_bounce.GetFloat() * (1 - m_surfaceFriction) );
				}
			}

			VectorCopy( new_velocity, mv->m_vecVelocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i < numplanes ; i++)
			{
				ClipVelocity (
					original_velocity,
					planes[i],
					mv->m_vecVelocity,
					1);

				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy (vec3_origin, mv->m_vecVelocity);
					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				d = dir.Dot(mv->m_vecVelocity);
				VectorScale (dir, d, mv->m_vecVelocity );
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (vec3_origin, mv->m_vecVelocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (vec3_origin, mv->m_vecVelocity);
	}

	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: Does the basic move attempting to climb up step heights.  It uses
//          the mv->m_vecAbsOrigin and mv->m_vecVelocity.  It returns a new
//          new mv->m_vecAbsOrigin, mv->m_vecVelocity, and mv->m_outStepHeight.
//-----------------------------------------------------------------------------
void CASW_Drone_Movement::StepMove( Vector &vecDestination, trace_t &trace )
{
//	CheckStuck();
	Vector vecEndPos;
	VectorCopy( vecDestination, vecEndPos );

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	Vector vecPos, vecVel;
	VectorCopy( mv->GetAbsOrigin(), vecPos );
	VectorCopy( mv->m_vecVelocity, vecVel );

	// Slide move down.
	Vector downWallNormal;
	TryMove( &vecEndPos, &trace );
	downWallNormal = m_LastHitWallNormal;
	
	// Down results.
	Vector vecDownPos, vecDownVel;
	VectorCopy( mv->GetAbsOrigin(), vecDownPos );
	VectorCopy( mv->m_vecVelocity, vecDownVel );
	
	// Reset original values.
	mv->SetAbsOrigin( vecPos );
	VectorCopy( vecVel, mv->m_vecVelocity );
	
	// Move up a stair height.
	VectorCopy( mv->GetAbsOrigin(), vecEndPos );
	vecEndPos.z += STEP_SIZE;	
	
	UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), vecEndPos, MASK_NPCSOLID, &trace );
	//TraceBBox( mv->GetAbsOrigin(), vecEndPos, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), trace );
	if ( !trace.startsolid && !trace.allsolid )
	{
		mv->SetAbsOrigin( trace.endpos );
	}
	
	// Slide move up.
	Vector upWallNormal;
	TryMove(NULL, NULL);
	upWallNormal = m_LastHitWallNormal;
	
	// Move down a stair (attempt to).
	VectorCopy( mv->GetAbsOrigin(), vecEndPos );
	vecEndPos.z -= STEP_SIZE;
		
	UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), vecEndPos, MASK_NPCSOLID, &trace );
	//TraceBBox( mv->GetAbsOrigin(), vecEndPos, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), trace );
	
	// If we are not on the ground any more then use the original movement attempt.
	if ( trace.plane.normal[2] < 0.7 )
	{
		mv->SetAbsOrigin( vecDownPos );
		VectorCopy( vecDownVel, mv->m_vecVelocity );
		VectorCopy( downWallNormal, m_LastHitWallNormal );
		float flStepDist = mv->GetAbsOrigin().z - vecPos.z;
		if ( flStepDist > 0.0f )
		{
			mv->m_outStepHeight += flStepDist;
		}

		return;
	}
	
	// If the trace ended up in empty space, copy the end over to the origin.
	if ( !trace.startsolid && !trace.allsolid )
	{
		mv->SetAbsOrigin( trace.endpos );
	}
	
	// Copy this origin to up.
	Vector vecUpPos;
	VectorCopy( mv->GetAbsOrigin(), vecUpPos );
	VectorCopy( upWallNormal, m_LastHitWallNormal);
	
	// decide which one went farther
	float flDownDist = ( vecDownPos.x - vecPos.x ) * ( vecDownPos.x - vecPos.x ) + ( vecDownPos.y - vecPos.y ) * ( vecDownPos.y - vecPos.y );
	float flUpDist = ( vecUpPos.x - vecPos.x ) * ( vecUpPos.x - vecPos.x ) + ( vecUpPos.y - vecPos.y ) * ( vecUpPos.y - vecPos.y );
	if ( flDownDist > flUpDist )
	{
		mv->SetAbsOrigin( vecDownPos );
		VectorCopy( vecDownVel, mv->m_vecVelocity );
		VectorCopy( downWallNormal, m_LastHitWallNormal);
	}
	else 
	{
		// copy z value from slide move
		mv->m_vecVelocity.z = vecDownVel.z;
	}
	
	float flStepDist = mv->GetAbsOrigin().z - vecPos.z;
	if ( flStepDist > 0 )
	{
		mv->m_outStepHeight += flStepDist;
	}
}

void CASW_Drone_Movement::WalkMove()
{
	// Add in any base velocity to the current velocity.
	VectorAdd (mv->m_vecVelocity, m_pNPC->GetBaseVelocity(), mv->m_vecVelocity );

	// if we're barely moving, then zero the velocity and stop
	float spd = VectorLength( mv->m_vecVelocity );
	if ( spd < 1.0f || m_flInterval <= 0)
	{
		mv->m_vecVelocity.Init();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, m_pNPC->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// first try just moving to the destination	
	Vector dest;
	dest[0] = mv->GetAbsOrigin()[0] + mv->m_vecVelocity[0]*m_flInterval;
	dest[1] = mv->GetAbsOrigin()[1] + mv->m_vecVelocity[1]*m_flInterval;
	dest[2] = mv->GetAbsOrigin()[2];
	trace_t pm;
	UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), dest, MASK_NPCSOLID, &pm );
	//TraceBBox( mv->GetAbsOrigin(), dest, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), pm );

	// if we made it all the way there, then set that as our new origin and return
	if ( pm.fraction == 1 )
	{
		mv->SetAbsOrigin( pm.endpos );
		m_pNPC->PhysicsTouchTriggers();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, m_pNPC->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// if NPC started the move on the ground, then try to move up/down steps
	if ( m_pNPC->GetGroundEntity() != NULL )
	{
		StepMove( dest, pm );		
	}

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, m_pNPC->GetBaseVelocity(), mv->m_vecVelocity );
}

void CASW_Drone_Movement::CategorizePosition()
{
	Vector point, bumpOrigin;
	trace_t pm;

	point = bumpOrigin = mv->GetAbsOrigin();
	point.z -= 2;
	bumpOrigin.z += 2;

	// Shooting up really fast.  Definitely not on ground.
	if ( mv->m_vecVelocity[2] > 140 || 
		( mv->m_vecVelocity[2] > 0.0f && m_pNPC->GetMoveType() == MOVETYPE_LADDER ) )   
	{
		SetGroundEntity( (CBaseEntity *)NULL );
	}
	else
	{
		// Try and move down.
		UTIL_TraceEntity( m_pNPC, bumpOrigin, point, MASK_NPCSOLID, &pm );
		//TraceBBox( bumpOrigin, point, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), pm );
		
		// Moving up two units got us stuck in something, start tracing down exactly at our
		//  current origin (since CheckStuck allowed us to get here, that pos is valid)
		if ( pm.startsolid )
		{
			bumpOrigin = mv->GetAbsOrigin();
			UTIL_TraceEntity( m_pNPC, bumpOrigin, point, MASK_NPCSOLID, &pm );
			//TraceBBox( bumpOrigin, point, MASK_NPCSOLID, m_pNPC->GetCollisionGroup(), pm );
		}

		// If we hit a steep plane, we are not on ground
		if ( pm.plane.normal[2] < 0.7)
		{
			SetGroundEntity( (CBaseEntity *)NULL );	// too steep
			// probably want to add a check for a +z velocity too!
			if ( ( mv->m_vecVelocity.z > 0.0f ) && ( m_pNPC->GetMoveType() != MOVETYPE_NOCLIP ) )
			{
				m_surfaceFriction = 0.25f;
			}
		}
		else
		{
			SetGroundEntity( pm.m_pEnt );  // Otherwise, point to index of ent under us.
		}

		// If we are on something...
		if (m_pNPC->GetGroundEntity() != NULL)
		{
			
			// If we could make the move, drop us down that 1 pixel
			if (!pm.startsolid && !pm.allsolid )
			{
				if( pm.fraction )
				{
					mv->SetAbsOrigin( pm.endpos );
				}
			}
		}		
	}
}

void CASW_Drone_Movement::SetGroundEntity( CBaseEntity *newGround )
{
	CBaseEntity *oldGround = m_pNPC->GetGroundEntity();
	Vector vecBaseVelocity = m_pNPC->GetBaseVelocity();

	if ( !oldGround && newGround )
	{
		// Subtract ground velocity at instant we hit ground jumping
		vecBaseVelocity -= newGround->GetAbsVelocity(); 
		vecBaseVelocity.z = newGround->GetAbsVelocity().z;
	}
	else if ( oldGround && !newGround )
	{
		// Add in ground velocity at instant we started jumping
 		vecBaseVelocity += oldGround->GetAbsVelocity();
		vecBaseVelocity.z = oldGround->GetAbsVelocity().z;
	}
	// TODO
	//else if ( oldGround && newGround && oldGround != newGround )
	//{
	//   subtract old and add new ground velocity?  When might his occur, who knows?  ywb 9/24/03
	//}

	m_pNPC->SetBaseVelocity( vecBaseVelocity );
	m_pNPC->SetGroundEntity( newGround );
}

void CASW_Drone_Movement::StartGravity( void )
{
	float ent_gravity;
	
	if (m_pNPC->GetGravity())
		ent_gravity = m_pNPC->GetGravity();
	else
		ent_gravity = 1.0; // asw, was 1.0
	
	if (!m_pNPC->GetGroundEntity())
		ent_gravity = 30.0f;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not. 
	float gravity_effect = (ent_gravity * sv_gravity.GetFloat() * 0.5 * m_flInterval );
	mv->m_vecVelocity[2] -= gravity_effect;
	mv->m_vecVelocity[2] += m_pNPC->GetBaseVelocity()[2] * m_flInterval;

	Vector temp = m_pNPC->GetBaseVelocity();
	temp[ 2 ] = 0;
	m_pNPC->SetBaseVelocity( temp );

	CheckVelocity();
}

void CASW_Drone_Movement::FinishGravity( void )
{
	/*
	float ent_gravity;

	if ( m_pNPC->GetGravity() )
		ent_gravity = m_pNPC->GetGravity();
	else
		ent_gravity = 1.0;

	ent_gravity = 3.0;

	// Get the correct velocity for the end of the dt 
  	mv->m_vecVelocity[2] -= (ent_gravity * sv_gravity.GetFloat() * m_flInterval * 0.5);
	*/
	// push the drone down by gravity
	Vector dest = mv->GetAbsOrigin();
	dest[2] = mv->GetAbsOrigin()[2] + mv->m_vecVelocity.z * m_flInterval;
	trace_t pm;
	UTIL_TraceEntity( m_pNPC, mv->GetAbsOrigin(), dest, MASK_NPCSOLID, &pm );
	if (!pm.startsolid && !pm.allsolid)
	{
		dest[2] = mv->GetAbsOrigin()[2] + mv->m_vecVelocity.z * m_flInterval * pm.fraction;
		mv->SetAbsOrigin( dest );
	}
}

void CASW_Drone_Movement::CheckVelocity( void )
{
	Vector origin = mv->GetAbsOrigin();
	bool bFixedOrigin = false;
	for (int i=0; i < 3; i++)
	{
		// See if it's bogus.
		if (IS_NAN(mv->m_vecVelocity[i]))
		{
			DevMsg( 1, "PM  Got a NaN velocity %s\n", DescribeAxis( i ) );
			mv->m_vecVelocity[i] = 0;
		}
		if (IS_NAN(origin[i]))
		{
			DevMsg( 1, "PM  Got a NaN origin on %s\n", DescribeAxis( i ) );
			origin[i] = 0;
			bFixedOrigin = true;
		}
	}
	if (bFixedOrigin)
	{
		mv->SetAbsOrigin( origin );
	}
}

void CASW_Drone_Movement::ProcessMovement( CAI_BaseNPC *pNPC, CMoveData *pMove, float flInterval)
{
	Assert( pMove && pNPC );

	m_pNPC = pNPC;
	mv = pMove;
	m_flInterval = flInterval;

	mv->m_outWishVel.Init();
	mv->m_outJumpVel.Init();

	Vector start = pMove->GetAbsOrigin();

	CategorizePosition();
	StartGravity();	
	WalkMove();
	FinishGravity();	// pushes him down by gravity
	CategorizePosition();		
}
