#include "cbase.h"
#include "asw_shareddefs.h"
#include "asw_jump_trigger.h"
#include "asw_drone_advanced.h"
#include "asw_util_shared.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( trigger_asw_jump, CASW_Jump_Trigger );


BEGIN_DATADESC( CASW_Jump_Trigger )
	DEFINE_FUNCTION( VolumeTouch ),
	DEFINE_KEYFIELD( m_JumpDestName, FIELD_STRING, "JumpDestName" ),
	DEFINE_KEYFIELD( m_fMinMarineDistance, FIELD_STRING, "MinMarineDist" ),
	DEFINE_KEYFIELD( m_bClearOrders, FIELD_BOOLEAN, "ClearOrders" ),
	DEFINE_KEYFIELD( m_bCheckEnemyDirection, FIELD_BOOLEAN, "CheckEnemyDirection" ),
	DEFINE_KEYFIELD( m_bOneJumpPerAlien, FIELD_BOOLEAN, "CheckTriggerJumped" ),		
	DEFINE_KEYFIELD( m_bRetryFailedJumps, FIELD_BOOLEAN, "RetryFailedJumps" ),
	DEFINE_KEYFIELD( m_bForceJump, FIELD_BOOLEAN, "ForceJump" ),
	DEFINE_KEYFIELD( m_ForceAngle, FIELD_VECTOR, "ForceAngle" ),
	DEFINE_KEYFIELD( m_fForceSpeed, FIELD_FLOAT, "ForceSpeed" ),
	DEFINE_ARRAY( m_vecJumpDestination, FIELD_FLOAT, ASW_MAX_JUMP_DESTS ), // NOTE: MUST BE IN LOCAL SPACE, NOT POSITION_VECTOR!!! (see CBaseEntity::Restore)
END_DATADESC()

CASW_Jump_Trigger::CASW_Jump_Trigger()
{
	
}

//-----------------------------------------------------------------------------
// Purpose: Touch function. Activates the trigger.
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CASW_Jump_Trigger::VolumeTouch(CBaseEntity *pOther)
{
	CASW_Alien_Jumper *pJumper = dynamic_cast<CASW_Alien_Jumper*>(pOther);
	if (pJumper && ( pJumper->CapabilitiesGet() & bits_CAP_MOVE_JUMP )
			&& !pJumper->IsJumping() && ( pJumper->GetFlags() & FL_ONGROUND ) )
				
	{
		bool bJumped = false;

		if ( m_bForceJump && m_fForceSpeed != 0 )
		{
			Vector vecVelocity;
			AngleVectors( m_ForceAngle, &vecVelocity );
			vecVelocity *= m_fForceSpeed;
			bJumped = pJumper->DoForcedJump( vecVelocity );
		}
		else
		{
			// pick a jump dest
			int iChosen = random->RandomInt(0, m_iNumJumpDests-1);
			if (!ReasonableJump(pJumper, iChosen))
			{
				// the dest was blocked, so we're going to just go and try all our dests
				iChosen = -1;
				for (int i=0;i<m_iNumJumpDests;i++)
				{
					if (ReasonableJump(pJumper, i))
					{
						iChosen = i;
						break;
					}
				}
				if (iChosen == -1)	// all dests blocked
					return;
			}
			// don't jump if a marine is too near us
			float marine_distance = 0;
			CASW_Marine *pMarine = UTIL_ASW_NearestMarine(pJumper->GetAbsOrigin(), marine_distance);
			if (pMarine && marine_distance < m_fMinMarineDistance)
			{
				return;
			}

			bJumped = pJumper->DoJumpTo(m_vecJumpDestination[iChosen]);

			if ( !bJumped )
			{
				bool bHasOrders = ( pJumper->m_AlienOrders != AOT_None );
				if ( m_bRetryFailedJumps && !bHasOrders )  // don't stop and wait for retry if we're still moving somewhere
					pJumper->WaitAndRetryJump( m_vecJumpDestination[iChosen] );
			}
		}

		// start to jump
		pJumper->m_bTriggerJumped = true;
		
		// clear the jumpers previous orders
		if ( bJumped && m_bClearOrders )
		{
			pJumper->SetAlienOrders(AOT_None, vec3_origin, NULL);
		}
	}
}

bool CASW_Jump_Trigger::ReasonableJump(CASW_Alien_Jumper *pJumper, int iJumpNum)
{
	if (!pJumper)
		return false;

	if (m_bOneJumpPerAlien && pJumper->m_bTriggerJumped)
		return false;

	if (!pJumper->GetEnemy())	// let him jump if he has no enemy
		return true;

	if (!m_bCheckEnemyDirection)		// let him jump if he doesn't care about the direction of his enemy
		return true;

	// if he does have an enemy, check it's in the direction of the jump
	float fEnemyYaw = UTIL_VecToYaw(pJumper->GetEnemy()->GetAbsOrigin() - pJumper->GetAbsOrigin());
	float fJumpYaw = UTIL_VecToYaw(m_vecJumpDestination[iJumpNum] - pJumper->GetAbsOrigin());

	return fabs(UTIL_AngleDiff(fEnemyYaw, fJumpYaw)) < 90;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASW_Jump_Trigger::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	if ( m_flWait == 0 )
	{
		m_flWait = 0.2;
	}

	ASSERTSZ( m_iHealth == 0, "trigger_asw_jump with health" );
		
	// find the target this use area is attached to
	m_iNumJumpDests = 0;

	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_JumpDestName, NULL );
	while ( pEnt )
	{
		m_vecJumpDestination[m_iNumJumpDests] = pEnt->GetAbsOrigin();
		m_iNumJumpDests++;

		pEnt = gEntList.FindEntityByName(pEnt, m_JumpDestName, NULL);
	}

	if ( m_iNumJumpDests > 0 || m_bForceJump )
	{
		SetTouch( &CASW_Jump_Trigger::VolumeTouch );
	}
	else
	{
		Msg("Error: trigger_asw_jump has no valid destinations.\n");
	}
	//Assert(!m_hUseTarget);
}