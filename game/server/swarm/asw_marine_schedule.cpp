#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_behavior.h"
#include "ai_baseactor.h"
#include "ai_behavior_lead.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_assault.h"
#include "ai_playerally.h"
#include "ai_pathfinder.h"
#include "ai_link.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "activitylist.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "sceneentity.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_weapon.h"
#include "asw_marine_speech.h"
//#include "asw_drone.h"
#include "asw_pickup.h"
#include "asw_pickup_weapon.h"
#include "asw_gamerules.h"
#include "asw_mission_manager.h"
#include "weapon_flaregun.h"
#include "ammodef.h"
#include "asw_shareddefs.h"
#include "asw_sentry_base.h"
#include "asw_button_area.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_fx_shared.h"
#include "asw_parasite.h"
#include "shareddefs.h"
#include "datacache/imdlcache.h"
#include "asw_bloodhound.h"
#include "beam_shared.h"
#include "rope.h"
#include "shot_manipulator.h"
#include "asw_use_area.h"
#include "asw_computer_area.h"
#include "ai_network.h"
#include "ai_networkmanager.h"
#include "asw_util_shared.h"
#include "asw_ammo.h"
#include "asw_ammo_drop.h"
#include "asw_door_area.h"
#include "asw_door.h"
#include "smartptr.h"
#include "ai_waypoint.h"
#include "particle_parse.h"
#include "asw_alien_goo_shared.h"
#include "asw_prop_physics.h"
#include "asw_weapon_heal_gun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// anim events
int AE_MARINE_KICK;
int AE_MARINE_UNFREEZE;

// activities
int ACT_MARINE_GETTING_UP;
int ACT_MARINE_LAYING_ON_FLOOR;

ConVar asw_marine_aim_error_max("asw_marine_aim_error_max", "20.0f", FCVAR_CHEAT, "Maximum firing error angle for AI marines with base accuracy skill\n");
ConVar asw_marine_aim_error_min("asw_marine_aim_error_min", "5.0f", FCVAR_CHEAT, "Minimum firing error angle for AI marines with base accuracy skill\n");
// todo: have this value vary based on marine skill/level/distance
ConVar asw_marine_aim_error_decay_multiplier("asw_marine_aim_error_decay_multiplier", "0.9f", FCVAR_CHEAT, "Value multiplied per turn to reduce aim error over time\n");
ConVar asw_blind_follow( "asw_blind_follow", "0", FCVAR_NONE, "Set to 1 to give marines short sight range while following (old school alien swarm style)" );
ConVar asw_debug_marine_aim( "asw_debug_marine_aim", "0", FCVAR_CHEAT, "Shows debug info on marine aiming" );
ConVar asw_debug_throw( "asw_debug_throw", "0", FCVAR_CHEAT, "Show node debug info on throw visibility checks" );
ConVar asw_debug_order_weld( "asw_debug_order_weld", "0", FCVAR_DEVELOPMENTONLY, "Debug lines for ordering marines to offhand weld a door" );

extern ConVar ai_lead_time;

#define ASW_MARINE_GOO_SCAN_TIME 0.5f

// offsets and yaws for following formation

const static Vector g_MarineFollowOffset[]=
{
	Vector( -60, -70, 0 ),
	Vector( -120, 0, 0 ),
	Vector( -60, 70, 0 ),
	Vector( -40, -80, 0 ),
	Vector( -40, -80, 0 ),
	Vector( -40, -80, 0 ),
};

const static float g_MarineFollowDirection[]=
{
	-70,
	180,
	70,
	90,
	90,
	90
};

bool CASW_Marine::CreateBehaviors()
{
	//AddBehavior( &m_ActBusyBehavior );
	//AddBehavior( &m_AssaultBehavior );
	//AddBehavior( &m_StandoffBehavior );
	
	return true;
}

// make the view cone 
bool CASW_Marine::FInViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );
	float flDist = los.NormalizeInPlace();
	if ( flDist <= GetCloseCombatSightRange() )		// see 360 up close
		return true;

	if ( GetASWOrders() == ASW_ORDER_HOLD_POSITION )
	{
		// otherwise check if this enemy is within our holding cone
		Vector holdingDir = vec3_origin;
		QAngle holdingAng(0, m_fHoldingYaw, 0);		
		AngleVectors(holdingAng, &holdingDir);

		float flDot = DotProduct( los, holdingDir );

		return ( flDot > m_flFieldOfView );
	}
	else if ( GetASWOrders() == ASW_ORDER_FOLLOW )
	{
		Vector vecHead = HeadDirection2D();
		float flDot = DotProduct( los, vecHead );
		return ( flDot > m_flFieldOfView );
	}

	// see 360 otherwise
	return true;	
}

float CASW_Marine::GetFollowSightRange()
{
	return GetSenses()->GetDistLook();
}

float CASW_Marine::GetCloseCombatSightRange()
{
	if (ASWGameRules())
	{
		switch (ASWGameRules()->GetSkillLevel())
		{
		case 1: return ASW_CLOSE_COMBAT_SIGHT_RANGE_EASY; break;
		case 2: return ASW_CLOSE_COMBAT_SIGHT_RANGE_NORMAL; break;
		case 3: return ASW_CLOSE_COMBAT_SIGHT_RANGE_HARD; break;
		case 4: 
		case 5:
		default: return ASW_CLOSE_COMBAT_SIGHT_RANGE_INSANE; break;
		}
	}
	return ASW_CLOSE_COMBAT_SIGHT_RANGE_INSANE;
}

bool CASW_Marine::WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	if (!BaseClass::WeaponLOSCondition(ownerPos, targetPos, bSetConditions))
		return false;

	if (!GetSenses())
	{
		if (bSetConditions)
			SetCondition(COND_NOT_FACING_ATTACK);
		return false;
	}

	float flDist = targetPos.DistTo(ownerPos);

	// too far away?
	if (flDist > GetSenses()->GetDistLook())
	{
		if (bSetConditions)
			SetCondition(COND_TOO_FAR_TO_ATTACK);
		return false;
	}

	if ( flDist < GetCloseCombatSightRange() )
	{
		return true;
	}

	Vector vBodyDir = BodyDirection2D( );
	float  flDot	= DotProduct( targetPos - ownerPos, vBodyDir );

	// in hold mode, we can shoot if it's in our cone, or if it's very close
	if (GetASWOrders() == ASW_ORDER_HOLD_POSITION)
	{		
		if ( flDot > ASW_HOLD_POSITION_FOV_DOT )
		{
			//if (bSetConditions)
				//SetCondition(COND_CAN_RANGE_ATTACK1);
			return true;
		}
		
		if (bSetConditions)
			SetCondition(COND_NOT_FACING_ATTACK);
		return false;		
	}
	if ( GetASWOrders() == ASW_ORDER_FOLLOW )
	{
		if ( flDot < ASW_FOLLOW_MODE_FOV_DOT )
			return false;
	}

	if ( flDist < GetFollowSightRange() ) //&& flDot > 0.5f )
	{
		//if (bSetConditions)
			//SetCondition(COND_CAN_RANGE_ATTACK1);
		return true;
	}
	
	if (bSetConditions)
		SetCondition(COND_TOO_FAR_TO_ATTACK);
	return false;
}
/*
int CASW_Marine::RangeAttack1Conditions ( float flDot, float flDist )
{	
	if (!GetSenses())
		return COND_NOT_FACING_ATTACK;

	// too far away?
	if (flDist > GetSenses()->GetDistLook())
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	
	// in hold mode, we can shoot if it's in our cone, or if it's very close	
	if (GetASWOrders() == ASW_ORDER_HOLD_POSITION)
	{
		if (flDist < GetCloseCombatSightRange())
			return COND_CAN_RANGE_ATTACK1;

		if (flDot > 0.5f)
			return COND_CAN_RANGE_ATTACK1;
		else
			return COND_NOT_FACING_ATTACK;		
	}

	if (flDist < GetFollowSightRange())
	{
		Msg("%f in follow mode, alien is within follow range (r=%f fw=%f\n", gpGlobals->curtime, flDist, GetFollowSightRange());
		return COND_CAN_RANGE_ATTACK1;
	}
	Msg("%f in follow mode, alien IS NOT in follow range (r=%f fw=%f\n", gpGlobals->curtime, flDist, GetFollowSightRange());
	
	// must mean we're in follow mode and it's too far away
	return COND_TOO_FAR_TO_ATTACK;
}
*/

// ========== ASW Schedule Stuff =========
int CASW_Marine::SelectSchedule()
{	
	if ( (HasCondition(COND_ENEMY_DEAD) || HasCondition(COND_ENEMY_OCCLUDED) || HasCondition(COND_WEAPON_SIGHT_OCCLUDED))
		&& m_fOverkillShootTime > gpGlobals->curtime)
	{
		return SCHED_ASW_OVERKILL_SHOOT;
	}

	if (m_bWaitingToRappel && !m_bOnGround)
	{
		return SCHED_ASW_RAPPEL;
	}
	
	if (m_bPreventMovement)
		return SCHED_ASW_HOLD_POSITION;

	// if we acquire a new enemy, create our aim error offset
	if (HasCondition(COND_NEW_ENEMY))
		SetNewAimError(GetEnemy());

	ClearCondition( COND_ASW_NEW_ORDERS );

	if ( HasCondition( COND_PATH_BLOCKED_BY_PHYSICS_PROP ) || HasCondition( COND_COMPLETELY_OUT_OF_AMMO ) )
	{
		int iMeleeSchedule = SelectMeleeSchedule();
		if( iMeleeSchedule != -1 )
			return iMeleeSchedule;
	}

	int iOffhandSchedule = SelectOffhandItemSchedule();
	if ( iOffhandSchedule != -1 )
		return iOffhandSchedule;

	int iHackingSchedule = SelectHackingSchedule();
	if ( iHackingSchedule != -1 )
		return iHackingSchedule;	

	if ( m_hUsingEntity.Get() )
		return SCHED_ASW_USING_OVER_TIME;

	int iHealSchedule = SelectHealSchedule();
	if( iHealSchedule != -1)
		return iHealSchedule;

	if ( HasCondition( COND_SQUADMATE_NEEDS_AMMO ) )
	{
		int iResupplySchedule = SelectGiveAmmoSchedule();
		if ( iResupplySchedule != -1 )
			return iResupplySchedule;
	}

	int iTakeAmmoSchedule = SelectTakeAmmoSchedule();
	if ( iTakeAmmoSchedule != -1 )
		return iTakeAmmoSchedule;

	if ( HasCondition ( COND_SQUADMATE_WANTS_AMMO ) )
	{
		int iResupplySchedule = SelectGiveAmmoSchedule();
		if ( iResupplySchedule != -1 )
			return iResupplySchedule;
	}

	// === following ===
	if ( GetASWOrders() == ASW_ORDER_FOLLOW )
	{
		int iFollowSchedule = SelectFollowSchedule();
		if ( iFollowSchedule != -1 )
			return iFollowSchedule;
	}

	if ( GetASWOrders() == ASW_ORDER_MOVE_TO )
	{/*
		if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
		{
			//Msg("Marine's select schedule returning SCHED_RANGE_ATTACK1\n");
			CBaseEntity* pEnemy = GetEnemy();
			if (pEnemy)		// don't shoot unless we actually have an enemy
				return SCHED_RANGE_ATTACK1;
		}	*/

		// if we're already there, hold position
		float dist = GetAbsOrigin().DistTo(m_vecMoveToOrderPos);
		if ( dist > 30 )
		{
			return SCHED_ASW_MOVE_TO_ORDER_POS;			
		}
		SetASWOrders(ASW_ORDER_HOLD_POSITION, m_fHoldingYaw);
	}

	// === holding position ===	
	if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		//Msg("Marine's select schedule returning SCHED_RANGE_ATTACK1\n");
		CBaseEntity* pEnemy = GetEnemy();
		if (pEnemy)		// don't shoot unless we actually have an enemy
			return SCHED_RANGE_ATTACK1;
	}		
	
	//Msg("Marine's select schedule returning SCHED_ASW_HOLD_POSITION\n");
	return SCHED_ASW_HOLD_POSITION;
}

// translation of schedules
int CASW_Marine::TranslateSchedule( int scheduleType ) 
{
	Assert(scheduleType != SCHED_STANDOFF);
	Assert(scheduleType != SCHED_CHASE_ENEMY);

	int result = CAI_PlayerAlly::TranslateSchedule(scheduleType);
	if (result == SCHED_RANGE_ATTACK1)
		result = SCHED_ASW_RANGE_ATTACK1;

	return result;
}

//-----------------------------------------------------------------------------
void CASW_Marine::TaskFail( AI_TaskFailureCode_t code )
{
	if ( IsCurSchedule( SCHED_ASW_MOVE_TO_ORDER_POS, false ) )
	{
		// if we failed to reach some ammo to pickup, then don't try to pick it up again for a while
		if ( m_hTakeAmmo.Get() )
		{
			m_hIgnoreAmmo.AddToTail( m_hTakeAmmo );
			m_hTakeAmmo = NULL;
			m_flResetAmmoIgnoreListTime = gpGlobals->curtime + 15.0f;
		}

		if ( m_hTakeAmmoDrop.Get() )
		{
			m_hIgnoreAmmo.AddToTail( m_hTakeAmmoDrop );
			m_hTakeAmmoDrop = NULL;
			m_flResetAmmoIgnoreListTime = gpGlobals->curtime + 15.0f;
		}

		// change orders back to hold/follow
		m_hAreaToUse = NULL;
		if ( m_bWasFollowing )
		{
			SetASWOrders( ASW_ORDER_FOLLOW );
		}
		else
		{
			SetASWOrders( ASW_ORDER_HOLD_POSITION, GetHoldingYaw() );
		}
	}
	else if ( IsCurSchedule( SCHED_MELEE_ATTACK_PROP1, false ) )
	{
		m_hPhysicsPropTarget = NULL;
	}

	BaseClass::TaskFail( code );
}

ConVar asw_marine_auto_hack( "asw_marine_auto_hack", "0", FCVAR_CHEAT, "If set to 1, marine will automatically hack nearby computers and button panels" );

#define AUTO_HACK_DIST 768.0f

int CASW_Marine::SelectHackingSchedule()
{
	if ( !GetMarineProfile() || GetMarineProfile()->GetMarineClass() != MARINE_CLASS_TECH )
		return -1;

	if ( asw_marine_auto_hack.GetBool() )
	{
		CASW_Use_Area *pClosestArea = NULL;
		float flClosestDist = FLT_MAX;

		// check for a computer hack nearby
		for ( int i = 0; i < IASW_Use_Area_List::AutoList().Count(); i++ )
		{
			CASW_Use_Area *pArea = static_cast< CASW_Use_Area* >( IASW_Use_Area_List::AutoList()[ i ] );
			if ( pArea->Classify() == CLASS_ASW_BUTTON_PANEL )
			{
				CASW_Button_Area *pButton = assert_cast< CASW_Button_Area* >( pArea );
				if ( !pButton->IsLocked() || !pButton->HasPower() )
					continue;

				float flDist = GetAbsOrigin().DistTo( pArea->WorldSpaceCenter() );
				if ( flDist < flClosestDist && flDist < AUTO_HACK_DIST )
				{
					flClosestDist = flDist;
					pClosestArea = pArea;
				}
			}
			else if ( pArea->Classify() == CLASS_ASW_COMPUTER_AREA )
			{
				CASW_Computer_Area *pComputer = assert_cast< CASW_Computer_Area* >( pArea );
				if ( pComputer->IsLocked() || ( pComputer->HasDownloadObjective() && pComputer->GetHackProgress() < 1.0f ) )
				{
					float flDist = GetAbsOrigin().DistTo( pArea->WorldSpaceCenter() );
					if ( flDist < flClosestDist && flDist < AUTO_HACK_DIST )
					{
						flClosestDist = flDist;
						pClosestArea = pArea;
					}
				}
			}
		}

		if ( pClosestArea )
		{
			m_hAreaToUse = pClosestArea;
		}
	}

	if ( m_hAreaToUse.Get() )
	{
		if ( m_hAreaToUse->IsUsable( this ) )
		{
			// notify to kill the effect in a couple of seconds
			CASW_Player *pPlayer = GetCommander();
			if ( pPlayer && pPlayer->GetMarine() )
			{
				CSingleUserRecipientFilter user( pPlayer );
				UserMessageBegin( user, "ASWOrderStopItemFX" );
				WRITE_SHORT( entindex() );
				WRITE_BOOL( false );
				WRITE_BOOL( false );
				MessageEnd();
			}

			if ( m_hUsingEntity.Get() == m_hAreaToUse.Get() )
			{
				return SCHED_ASW_USING_OVER_TIME;
			}
			else
			{
				return SCHED_ASW_USE_AREA;
			}
		}
		else
		{
			m_vecMoveToOrderPos = m_hAreaToUse->WorldSpaceCenter();		// TODO: hacking spot marked up in the level?
			return SCHED_ASW_MOVE_TO_ORDER_POS;
		}
	}
	return -1;
}

void CASW_Marine::OrderHackArea( CASW_Use_Area *pArea )
{
	if ( !pArea )
		return;

	bool bHackOrdered = false;

	if ( pArea->Classify() == CLASS_ASW_BUTTON_PANEL )
	{
		CASW_Button_Area *pButton = assert_cast< CASW_Button_Area* >( pArea );
		if ( !pButton->IsLocked() || !pButton->HasPower() )
			return;

		float flDist = GetAbsOrigin().DistTo( pArea->WorldSpaceCenter() );
		if ( flDist < AUTO_HACK_DIST )
		{
			m_hAreaToUse = pArea;
			bHackOrdered = true;
		}
	}
	else if ( pArea->Classify() == CLASS_ASW_COMPUTER_AREA )
	{
		CASW_Computer_Area *pComputer = assert_cast< CASW_Computer_Area* >( pArea );
		if ( pComputer->IsLocked() || ( pComputer->HasDownloadObjective() && pComputer->GetHackProgress() < 1.0f ) )
		{
			float flDist = GetAbsOrigin().DistTo( pArea->WorldSpaceCenter() );
			if ( flDist < AUTO_HACK_DIST )
			{
				m_hAreaToUse = pArea;
				bHackOrdered = true;
			}
		}
	}

	if( bHackOrdered )
	{
		CASW_Player *pPlayer = GetCommander();
		if ( pPlayer && pPlayer->GetMarine() )
		{
			Vector vecOrigin = pArea->WorldSpaceCenter();
			trace_t tr;

				// Find where that leg hits the ground
				UTIL_TraceLine(vecOrigin, vecOrigin + Vector(0, 0, -60), 
				MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

			CSingleUserRecipientFilter user( pPlayer );
			UserMessageBegin( user, "ASWOrderUseItemFX" );
			WRITE_SHORT( entindex() );
			WRITE_SHORT( ASW_USE_ORDER_HACK );
			WRITE_SHORT( -1 );
			WRITE_FLOAT( tr.endpos.x );
			WRITE_FLOAT( tr.endpos.y );
			WRITE_FLOAT( tr.endpos.z );
			MessageEnd();
		}
	}
}

int CASW_Marine::SelectTakeAmmoSchedule()
{
	if ( !m_hTakeAmmo.Get() && !m_hTakeAmmoDrop.Get() )
	{
		if ( gpGlobals->curtime < m_flNextAmmoScanTime )
			return -1;
		m_flNextAmmoScanTime = gpGlobals->curtime + 1.0f;

		if ( m_flResetAmmoIgnoreListTime != 0.0f && gpGlobals->curtime > m_flResetAmmoIgnoreListTime )
		{
			m_hIgnoreAmmo.Purge();
			m_flResetAmmoIgnoreListTime = 0.0f;
		}

		const float flAmmoScanRadiusSqr = 384.0f * 384.0f;
		int nAmmoPickupCount = IAmmoPickupAutoList::AutoList().Count();
		for ( int i = 0; i < nAmmoPickupCount; i++ )
		{
			CASW_Ammo *pAmmo = static_cast< CASW_Ammo* >( IAmmoPickupAutoList::AutoList()[ i ] );
			EHANDLE hAmmo = pAmmo;
			if ( m_hIgnoreAmmo.Find( hAmmo ) != m_hIgnoreAmmo.InvalidIndex() )
				continue;

			// check ammo is nearby
			if ( GetAbsOrigin().DistToSqr( pAmmo->GetAbsOrigin() ) > flAmmoScanRadiusSqr )
				continue;

			// check we need this ammo
			if ( !pAmmo->AllowedToPickup( this ) )
				continue;

			int nCurrentCount = GetAmmoCount( pAmmo->GetAmmoType() );

			// check we don't have more ammo than a human player
			bool bHumanNeedsAmmo = false;
			for ( int m = 0; m < ASWGameResource()->GetMaxMarineResources(); m++ )
			{
				CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( m );
				if ( !pMR || pMR == GetMarineResource() || !pMR->IsInhabited() || !pMR->GetMarineEntity() )
					continue;

				if ( !pAmmo->AllowedToPickup( pMR->GetMarineEntity() ) )
					continue;

				if ( pMR->GetMarineEntity()->GetAmmoCount( pAmmo->GetAmmoType() ) < nCurrentCount )
				{
					bHumanNeedsAmmo = true;
					break;
				}
			}

			if ( bHumanNeedsAmmo )
				continue;

			m_hTakeAmmo = pAmmo;
			m_hTakeAmmoDrop = NULL;
			break;
		}

		// search for ammo satchel drops
		if ( !m_hTakeAmmo )
		{
			int nAmmoDropCount = IAmmoDropAutoList::AutoList().Count();
			for( int i = 0; i < nAmmoDropCount; i++ )
			{
				CASW_Ammo_Drop *pAmmoDrop = static_cast< CASW_Ammo_Drop* >( IAmmoDropAutoList::AutoList()[ i ] );
				EHANDLE hAmmoDrop = pAmmoDrop;
				EHANDLE hAmmo = pAmmoDrop;
				if ( m_hIgnoreAmmo.Find( hAmmoDrop ) != m_hIgnoreAmmo.InvalidIndex() )
					continue;

				if( GetAbsOrigin().DistToSqr( pAmmoDrop->GetAbsOrigin() ) > flAmmoScanRadiusSqr )
					continue;

				if( !pAmmoDrop->AllowedToPickup( this ) )
					continue;
	
				bool bHumanNeedsAmmo = false;
				for( int j = 0; j < ASWGameResource()->GetMaxMarineResources(); j++)
				{
					CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( j );
					if( !pMR || pMR == GetMarineResource() || !pMR->IsInhabited() || !pMR->GetMarineEntity() )
						continue;

					if( !pAmmoDrop->AllowedToPickup( pMR->GetMarineEntity() ) )
						continue;

					if ( !pAmmoDrop->NeedsAmmoMoreThan( this, pMR->GetMarineEntity() ) )
					{
						bHumanNeedsAmmo = true;
						break;
					}
				}

				if( bHumanNeedsAmmo )
					continue;

				m_hTakeAmmoDrop = pAmmoDrop;
				break;
			}
		}
	}

	EHANDLE hAmmo = ( m_hTakeAmmo.Get() ? m_hTakeAmmo : ( m_hTakeAmmoDrop.Get() ? m_hTakeAmmoDrop : NULL ) );

	if ( hAmmo != NULL )
	{
		SetPoseParameter( "move_x", 1.0f );
		SetPoseParameter( "move_y", 0.0f );

		// if we're in range of the deploy spot, then use the item
		if ( hAmmo->GetAbsOrigin().DistTo( GetAbsOrigin() ) < ASW_MARINE_USE_RADIUS )
		{
			if( m_hTakeAmmo.Get() )
			{
				m_hTakeAmmo->ActivateUseIcon( this, ASW_USE_RELEASE_QUICK );
			}
			else
			{
				m_hTakeAmmoDrop->ActivateUseIcon( this, ASW_USE_RELEASE_QUICK );
			}

			m_hTakeAmmo = NULL;
			m_hTakeAmmoDrop = NULL;

			DoAnimationEvent( PLAYERANIMEVENT_PICKUP );
			m_flNextAmmoScanTime = gpGlobals->curtime + 1.0f;
			return SCHED_ASW_PICKUP_WAIT;
		}
		else
		{
			// move to the ammo
			m_vecMoveToOrderPos = hAmmo->GetAbsOrigin();
			return SCHED_ASW_MOVE_TO_ORDER_POS;
		}
	}

	return -1;
}

bool CASW_Marine::OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
{
	SetPhysicsPropTarget( NULL );
	if ( pMoveGoal->directTrace.pObstruction )
	{
		CASW_Prop_Physics *pPropPhysics = dynamic_cast< CASW_Prop_Physics *>( pMoveGoal->directTrace.pObstruction );
		if ( pPropPhysics && pPropPhysics->m_takedamage == DAMAGE_YES && pPropPhysics->m_iHealth > 0 )
		{
			SetPhysicsPropTarget( pPropPhysics );
		}
	}

	return false;
}

void CASW_Marine::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_PATH_BLOCKED_BY_PHYSICS_PROP );
	ClearCondition( COND_PROP_DESTROYED );
	ClearCondition( COND_COMPLETELY_OUT_OF_AMMO );

	if( !GetCurSchedule() )
		return;

	bool bClearTargets = false;

	if( GetCurSchedule()->GetId() == GetGlobalScheduleId( SCHED_MELEE_ATTACK_PROP1 ) )
	{
		if( !GetPhysicsPropTarget() || GetPhysicsPropTarget()->GetHealth() <= 0 )
		{
			SetCondition( COND_PROP_DESTROYED );
			bClearTargets = true;
		}
	}
	else
	{
		if( GetPhysicsPropTarget() )
		{
			if( GetPhysicsPropTarget()->GetHealth() > 0 )
			{
				SetCondition( COND_PATH_BLOCKED_BY_PHYSICS_PROP );
			}
			else
			{
				bClearTargets = true;
			}
		}
	}

	if( bClearTargets )
	{
		if( GetEnemy() == GetPhysicsPropTarget() )
		{
			SetEnemy( NULL );
		}

		SetPhysicsPropTarget( NULL );
	}

	int iCurSchedule = GetCurSchedule()->GetId();
	bool bRangedAttackSchedule = false;
	switch( iCurSchedule )
	{
	case SCHED_RANGE_ATTACK1:
	case SCHED_RANGE_ATTACK2:
	case SCHED_SPECIAL_ATTACK1:
	case SCHED_SPECIAL_ATTACK2:
		{
			bRangedAttackSchedule = true;
		}
		break;
	};

	if( IsOutOfAmmo() && bRangedAttackSchedule )
	{
		SetCondition( COND_COMPLETELY_OUT_OF_AMMO );
	}
}

void CASW_Marine::BuildScheduleTestBits()
{
	// ammo bag functionality needs to interrupt base class behaviors
	SetCustomInterruptCondition(COND_SQUADMATE_WANTS_AMMO);
	SetCustomInterruptCondition(COND_SQUADMATE_NEEDS_AMMO);
	SetCustomInterruptCondition(COND_PATH_BLOCKED_BY_PHYSICS_PROP);
	SetCustomInterruptCondition(COND_COMPLETELY_OUT_OF_AMMO);
}

int CASW_Marine::SelectGiveAmmoSchedule()
{
	// iterate over all teammates, looking for most needy target for ammo
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if ( !pMarineResource )
			continue;

		CASW_Marine* pMarine = pMarineResource->GetMarineEntity();
		if ( !pMarine || ( pMarine == this ) )
			continue;

		// see if the current marine can use ammo I have
		if ( CanGiveAmmoTo( pMarine ) )
		{
			// $TODO: find most appropriate give ammo target a la AmmoNeed_t
			m_hGiveAmmoTarget = pMarine;
			return SCHED_ASW_GIVE_AMMO;
		}
	}

	m_hGiveAmmoTarget = NULL;
	return -1;
}

bool CASW_Marine::CanHeal() const
{
	for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
	{
		CASW_Weapon *pWeapon = GetASWWeapon( iWeapon );
		if ( pWeapon && pWeapon->Classify() == CLASS_ASW_HEAL_GUN && pWeapon->HasAmmo() )
		{
			return true;
		}
	}

	return false;
}

#define MARINE_START_HEAL_THRESHOLD 0.65f
#define MARINE_STOP_HEAL_THRESHOLD 0.85f

int CASW_Marine::SelectHealSchedule()
{
	// iterate over all teammates, looking for most needy target for health
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for ( int i=0; i<pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if ( !pMarineResource )
			continue;

		CASW_Marine* pMarine = pMarineResource->GetMarineEntity();
		if ( !pMarine )
			continue;

		// see if the current marine can use ammo I have
		if ( CanHeal() && pMarine->GetHealth() < pMarine->GetMaxHealth() * MARINE_START_HEAL_THRESHOLD )
		{
			m_hHealTarget = pMarine;
			return SCHED_ASW_HEAL_MARINE;
		}
	}

	m_hHealTarget = NULL;
	return -1;
}

void CASW_Marine::SetASWOrders(ASW_Orders NewOrders, float fHoldingYaw, const Vector *pOrderPos)
{
	Vector vecPos = vec3_origin;
	if (pOrderPos)
		vecPos = *pOrderPos;
	if (m_ASWOrders != NewOrders || m_vecMoveToOrderPos != vecPos
		|| (fHoldingYaw != -1 && fHoldingYaw != m_fHoldingYaw))
	{		
		SetCondition( COND_ASW_NEW_ORDERS );
		if (NewOrders == ASW_ORDER_HOLD_POSITION)
		{
			SetDistLook( ASW_HOLD_POSITION_SIGHT_RANGE );		// make marines see further when holding position
			m_flFieldOfView = ASW_HOLD_POSITION_FOV_DOT;
		}
		else
		{
			m_flFieldOfView = ASW_FOLLOW_MODE_FOV_DOT;
			if ( !asw_blind_follow.GetBool() )
			{
				SetDistLook( ASW_FOLLOW_MODE_SIGHT_RANGE );
				SetHeightLook( ASW_FOLLOW_MODE_SIGHT_HEIGHT );
			}
			else
			{
				SetDistLook( ASW_DUMB_FOLLOW_MODE_SIGHT_RANGE );
				SetHeightLook( ASW_DUMB_FOLLOW_MODE_SIGHT_HEIGHT );
			}
		}
	}
	if ( NewOrders != ASW_ORDER_FOLLOW )
	{
		GetSquadFormation()->Remove(this, true);
		m_hMarineFollowTarget = NULL;
	}
	m_bWasFollowing = ( NewOrders == ASW_ORDER_FOLLOW || ( NewOrders != ASW_ORDER_HOLD_POSITION && m_ASWOrders == ASW_ORDER_FOLLOW ) );		// keeps track of follow vs hold, so we can return to the right orders after completing the new ones
	m_ASWOrders = NewOrders;
	m_vecMoveToOrderPos = vecPos;
	if (fHoldingYaw != -1)
		m_fHoldingYaw = fHoldingYaw;
	//Msg("Marine receives asw orders: %d\n", NewOrders);
}

void CASW_Marine::OrdersFromPlayer(CASW_Player *pPlayer, ASW_Orders NewOrders, CBaseEntity *pMarine, bool bChatter, float fHoldingYaw, Vector *pVecOrderPos)
{
	MDLCACHE_CRITICAL_SECTION();
	// asw temp - AI ignore follow orders in SP
	//if (gpGlobals->maxClients <= 1 && NewOrders == ASW_ORDER_FOLLOW)
		//return;

	if (fHoldingYaw == -1)
		fHoldingYaw = GetAbsAngles()[YAW];

	m_bDoneOrderChatter = false;

	SetASWOrders(NewOrders, fHoldingYaw, pVecOrderPos);
	if (NewOrders == ASW_ORDER_FOLLOW)
	{
		if ( pMarine && pMarine->Classify() == CLASS_ASW_MARINE )
		{
			// Assert( assert_cast<CASW_Marine*>( pMarine )->GetSquadFormation()->Leader() == pMarine );
			CASW_Marine *pReallyMarine = assert_cast<CASW_Marine*>( pMarine );
			if ( pReallyMarine != GetSquadLeader() )
				GetSquadFormation()->ChangeLeader( pReallyMarine );
			if ( !GetSquadFormation()->IsValid( GetSquadFormation()->Find(this) ) )
				pReallyMarine->GetSquadFormation()->Add( this );
		}
		else
		{
			GetSquadFormation()->Remove( this, true );
		}
		//DoEmote(2);	// make them smile on receiving orders
		// make sure his move_x/y pose parameters are at full moving forwards, so the AI follow movement will detect some sequence motion when calculating goal speed
		SetPoseParameter( "move_x", 1.0f );
		SetPoseParameter( "move_y", 0.0f );

		if (bChatter && !IsInfested())
			GetMarineSpeech()->QueueChatter(CHATTER_USE, gpGlobals->curtime + 0.1f, gpGlobals->curtime + 5.0f);
	}
	else if (NewOrders == ASW_ORDER_HOLD_POSITION)
	{		
		//m_fHoldingYaw = GetAbsAngles().y;
		if (bChatter && !IsInfested() && ( GetFlags() & FL_ONGROUND ) && !m_hUsingEntity.Get())
			GetMarineSpeech()->QueueChatter(CHATTER_HOLDING_POSITION, gpGlobals->curtime + 0.1f, gpGlobals->curtime + 5.0f);
	}
	else if (NewOrders == ASW_ORDER_MOVE_TO && pVecOrderPos != NULL)
	{
		//DoEmote(2);	// make them smile on receiving orders
		// make sure his move_x/y pose parameters are at full moving fowards, so the AI follow movement will detect some sequence motion when calculating goal speed
		SetPoseParameter( "move_x", 1.0f );
		SetPoseParameter( "move_y", 0.0f );
		
		//Msg("Recieived orders to move to %f %f %f\n",
			//pVecOrderPos[0], pVecOrderPos[1], pVecOrderPos[2]);		
		//m_vecMoveToOrderPos = *pVecOrderPos;
		//Msg("m_vecMoveToOrderPos= %f %f %f\n",
//			m_vecMoveToOrderPos[0], m_vecMoveToOrderPos[1], m_vecMoveToOrderPos[2]);
	}

	m_fCachedIdealSpeed = MaxSpeed();
}

static void DebugWaypoint( AI_Waypoint_t * pWay, int r, int g, int b, float flDuration  = 3 )
{
	if ( pWay && pWay->GetNext() )
	{
		NDebugOverlay::Line( pWay->GetPos(), pWay->GetNext()->GetPos(), r, g, b, true, 3 );
		return DebugWaypoint( pWay->GetNext(), r, g, b, flDuration );
	}
}

/// because AI_Waypoint_t::flPathDistGoal is broken
static float GetWaypointDistToEnd( const Vector &vStartPos, AI_Waypoint_t *way ) 
{
	if ( !way ) return FLT_MAX;
	float ret = way->GetPos().DistTo(vStartPos);
	while ( way->GetNext() )
	{
		ret += way->GetNext()->GetPos().DistTo( way->GetPos() );
		way = way->GetNext();
	}
	return ret;
}

/// When a marine uses an offhand item, sometimes you need to pick a different
/// location for its use than exactly where the cursor was. And sometimes
/// you need to kibosh the use altogether. Returns true if the offhand use
/// should continue, false means it should be aborted
static bool AdjustOffhandItemDestination( CASW_Marine *pMarine, CASW_Weapon *pWeapon, const Vector &vecDestIn, Vector * RESTRICT vecDestOut )
{
	// NDebugOverlay::Cross3D( vecDestIn, 16, 255, 16, 16, true, 3 );
	if ( pWeapon->Classify() == CLASS_ASW_WELDER )
	{
		// find a door near the given position
		CASW_Door_Area *pClosestArea = NULL;
		CASW_Door * RESTRICT pDoor = NULL;
		float flClosestDist = FLT_MAX;

		for ( int i = 0; i < IASW_Use_Area_List::AutoList().Count(); i++ )
		{
			CASW_Use_Area *pArea = static_cast< CASW_Use_Area* >( IASW_Use_Area_List::AutoList()[ i ] );
			if ( pArea->Classify() == CLASS_ASW_DOOR_AREA )
			{
				CASW_Door_Area *pDoorArea = assert_cast<CASW_Door_Area*>( pArea );

				float flDist = (vecDestIn - pArea->WorldSpaceCenter()).Length2D( );
				if ( flDist < flClosestDist && flDist < 240 )
				{
					flClosestDist = flDist;
					pClosestArea = pDoorArea;
				}
			}
		}

		if ( !pClosestArea )
		{
			return false;
		}

		pDoor = pClosestArea->GetASWDoor();
		if ( !pDoor )
		{
			AssertMsg( false, "Door area is missing its door!\n" );
			return false;
		}

		if ( asw_debug_order_weld.GetBool() ) NDebugOverlay::EntityBounds( pDoor, 0, 0, 255, 64, 3 );

		// pick a point to either side of the door and build routes to them
		Vector vStandPoints[2];
		float fkickout = pMarine->BoundingRadius() * 1.1f;
		float flTolerance = pMarine->BoundingRadius() * 1.1f;

		// get the door's transform, and punch its saved "closed position" into the translate row
		if ( true )
		{
			matrix3x4_t mat = pDoor->EntityToWorldTransform();
			mat.SetOrigin( pDoor->GetClosedPosition() );
			VectorTransform( Vector(fkickout,0,0),  mat, vStandPoints[0] );
			VectorTransform( Vector(-fkickout,0,0), mat, vStandPoints[1] );
		}
		else
		{
			pDoor->EntityToWorldSpace( Vector(fkickout,0,0),  vStandPoints + 0 ); // forward
			pDoor->EntityToWorldSpace( Vector(-fkickout,0,0), vStandPoints + 1 );
		}

		CAI_Pathfinder *pathfinder = pMarine->GetPathfinder();
		AI_Waypoint_t *routeFwd =
			pathfinder->BuildRoute( pMarine->GetAbsOrigin(), vStandPoints[0],
			NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE );
		AI_Waypoint_t *routeBack =
			pathfinder->BuildRoute( pMarine->GetAbsOrigin(), vStandPoints[1],
			NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE );

		if ( asw_debug_order_weld.GetBool() ) 
		{
			if ( routeFwd )
			{
				NDebugOverlay::Circle( vStandPoints[0], QAngle(90, 0, 0), 24, 0, 128, 255, 255, true, 3 );
			}
			else
			{
				NDebugOverlay::Circle( vStandPoints[0], QAngle(90, 0, 0), 24, 255, 0, 0, 255, true, 3 );
			}

			if ( routeBack )
			{
				NDebugOverlay::Circle( vStandPoints[1], QAngle(90, 0, 0), 24, 0, 255, 128, 255, true, 3 );
			}
			else
			{
				NDebugOverlay::Circle( vStandPoints[1], QAngle(90, 0, 0), 24, 255, 0, 0, 255, true, 3 );
			}
		}

		if ( !routeBack && !routeFwd )
		{
			delete routeFwd;
			delete routeBack;
			return false;
		}

		CASW_Marine *pLeader = static_cast< CASW_Marine* >( pMarine->GetSquadLeader() );
		float flDists[2];
		flDists[0] = routeFwd  ? GetWaypointDistToEnd( pMarine->GetAbsOrigin(), routeFwd ) : FLT_MAX;
		flDists[1] = routeBack ? GetWaypointDistToEnd( pMarine->GetAbsOrigin(), routeBack ) : FLT_MAX;		
		bool bUseForwardRoute = ( routeFwd && ( !routeBack || flDists[0] < flDists[1] ) );

		if ( pLeader )
		{
			// move to whichever one is closer to our follow leader
			CAI_Pathfinder *pathfinder = pLeader->GetPathfinder();
			CPlainAutoPtr<AI_Waypoint_t> leaderRouteFwd(
				pathfinder->BuildRoute( pLeader->GetAbsOrigin(), vStandPoints[0],
				NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE   )	);
			CPlainAutoPtr<AI_Waypoint_t> leaderRouteBack(
				pathfinder->BuildRoute( pLeader->GetAbsOrigin(), vStandPoints[1],
				NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE  ) );

			float flLeaderDists[2];
			flLeaderDists[0] = leaderRouteFwd.IsValid() ? GetWaypointDistToEnd( pLeader->GetAbsOrigin(), leaderRouteFwd.Get() ) : FLT_MAX;
			flLeaderDists[1] = leaderRouteBack.IsValid() ? GetWaypointDistToEnd( pLeader->GetAbsOrigin(), leaderRouteBack.Get()) : FLT_MAX;

			if ( leaderRouteFwd.IsValid() || leaderRouteBack.IsValid() )
			{
				bool bLeaderUseForwardRoute = ( leaderRouteFwd.IsValid() && ( !leaderRouteBack.IsValid() || flLeaderDists[0] < flLeaderDists[1] ) );
				if ( bLeaderUseForwardRoute != bUseForwardRoute )
				{
					// leader is on an opposite side to the marine, push the point forward a bit more so marine clears the doorway he's running through
					matrix3x4_t mat = pDoor->EntityToWorldTransform();
					mat.SetOrigin( pDoor->GetClosedPosition() );
					float fkickout = pMarine->BoundingRadius() * 2.2f;
					VectorTransform( Vector(fkickout,0,0),  mat, vStandPoints[0] );
					VectorTransform( Vector(-fkickout,0,0), mat, vStandPoints[1] );

					delete routeFwd;
					delete routeBack;

					routeFwd =
						pathfinder->BuildRoute( pMarine->GetAbsOrigin(), vStandPoints[0],
						NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE );
					routeBack =
						pathfinder->BuildRoute( pMarine->GetAbsOrigin(), vStandPoints[1],
						NULL, flTolerance, NAV_GROUND, bits_BUILD_GET_CLOSE );

					if ( !routeBack && !routeFwd )
					{
						delete routeFwd;
						delete routeBack;
						return false;
					}

					flDists[0] = routeFwd  ? GetWaypointDistToEnd( pMarine->GetAbsOrigin(), routeFwd ) : FLT_MAX;
					flDists[1] = routeBack ? GetWaypointDistToEnd( pMarine->GetAbsOrigin(), routeBack ) : FLT_MAX;	

					if ( ( bLeaderUseForwardRoute && routeFwd ) || ( !bLeaderUseForwardRoute && routeBack ) )
					{
						bUseForwardRoute = bLeaderUseForwardRoute;
					}
				}
			}
		}

		// pick the shorter (or only existing) route
		if ( bUseForwardRoute )
		{
			*vecDestOut =  routeFwd->GetLast()->GetPos(); // vStandPoints[0];
			if ( asw_debug_order_weld.GetBool() ) 
			{
				DebugWaypoint( routeFwd, 255, 255, 0 );
				if ( routeBack ) 
				{
					Msg( "%.3f < %.3f\n", flDists[0], flDists[1] );
					DebugWaypoint( routeBack, 0, 255, 0 );
				}
			}
		}
		else
		{
			*vecDestOut =  routeBack->GetLast()->GetPos(); // vStandPoints[1];
			if ( asw_debug_order_weld.GetBool() ) 
			{
				DebugWaypoint( routeBack, 255, 255, 0 );
				if ( routeFwd ) 
				{
					Msg( "%.3f < %.3f\n", flDists[1], flDists[0] );
					DebugWaypoint( routeFwd, 0, 255, 0 );
				}
			}
		}
		delete routeFwd;
		delete routeBack;
		return true;
	}
	else
	{
		// do nothing, just copy through
		*vecDestOut = vecDestIn;
		return true;
	}
}

void CASW_Marine::OrderUseOffhandItem( int iInventorySlot, const Vector &vecDest )
{
	// check we have an item in that slot
	CASW_Weapon* pWeapon = GetASWWeapon( iInventorySlot );
	if ( !pWeapon || !pWeapon->GetWeaponInfo() || !pWeapon->GetWeaponInfo()->m_bOffhandActivate )
		return;

	// m_vecOffhandItemSpot = vecDest;
	if ( !AdjustOffhandItemDestination( this, pWeapon, vecDest, &m_vecOffhandItemSpot) )
		return;
	else if ( asw_debug_order_weld.GetBool() ) 
	{
		NDebugOverlay::Cross( m_vecOffhandItemSpot, 12.0f, 255, 0, 0, true, 3 );
	}

	if ( pWeapon->GetWeaponInfo()->m_nOffhandOrderType == ASW_OFFHAND_USE_IMMEDIATELY )
	{
		pWeapon->OffhandActivate();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	if ( pPlayer )
	{
		//DispatchParticleEffect( "marine_hit_blood_ff", vecDest, QAngle( 0, 0, 0 ) );
		// Tell the player's client that he's been hurt.
		CSingleUserRecipientFilter user( pPlayer );
		UserMessageBegin( user, "ASWOrderUseItemFX" );
		WRITE_SHORT( entindex() );
		WRITE_SHORT( ASW_USE_ORDER_WITH_ITEM );
		WRITE_SHORT( iInventorySlot );
		WRITE_FLOAT( m_vecOffhandItemSpot.x );
		WRITE_FLOAT( m_vecOffhandItemSpot.y );
		WRITE_FLOAT( m_vecOffhandItemSpot.z );
		MessageEnd();
	}

	m_ASWOrders = ASW_ORDER_USE_OFFHAND_ITEM;
	m_hOffhandItemToUse = pWeapon;

	SetCondition( COND_ASW_NEW_ORDERS );
}

#define ASW_DEPLOY_RANGE 75.0f

int CASW_Marine::SelectOffhandItemSchedule()
{
	if ( m_bWaitingForWeld )
	{
		if( gpGlobals->curtime > m_flBeginWeldTime + 5.0f )
		{
			FinishedUsingOffhandItem( false );
			return SCHED_ASW_FOLLOW_WAIT;
		}

		return SCHED_ASW_FOLLOW_WAIT;
	}

	if ( GetASWOrders() != ASW_ORDER_USE_OFFHAND_ITEM )
		return -1;

	if ( !m_hOffhandItemToUse.Get() )
		return -1;

	const CASW_WeaponInfo *pInfo = m_hOffhandItemToUse->GetWeaponInfo();
	if ( !pInfo )
		return -1;

	SetPoseParameter( "move_x", 1.0f );
	SetPoseParameter( "move_y", 0.0f );

	if ( pInfo->m_nOffhandOrderType == ASW_OFFHAND_DEPLOY )
	{
		// if we're in range of the deploy spot, then use the item
		if ( ( m_vecOffhandItemSpot - GetAbsOrigin() ).Length2D() < ASW_DEPLOY_RANGE )
		{
			m_hOffhandItemToUse->OffhandActivate();
			FinishedUsingOffhandItem( false );
			if ( m_bWaitingForWeld )
			{
				return SCHED_ASW_FOLLOW_WAIT;
			}
		}
		else
		{
			// otherwise, move to just in front of the deploy spot
			Vector dir = GetAbsOrigin() - m_vecOffhandItemSpot;
			dir.z = 0;
			dir.NormalizeInPlace();
			m_vecMoveToOrderPos = m_vecOffhandItemSpot + dir * 50.0f;
			return SCHED_ASW_MOVE_TO_ORDER_POS;
		}
	}
	else
	{
		// if we have line of sight to the target spot
		if ( CanThrowOffhand( m_hOffhandItemToUse, GetOffhandThrowSource(), m_vecOffhandItemSpot, asw_debug_throw.GetInt() == 3 ) )
		{
			m_hOffhandItemToUse->OffhandActivate();
			FinishedUsingOffhandItem( true );
		}
		else
		{
			const float MIN_THROW_RANGE = 50.0f;
			const float MAX_THROW_RANGE = 700.0f;
			Vector pos;
			int iNode = FindThrowNode( m_vecOffhandItemSpot, MIN_THROW_RANGE, MAX_THROW_RANGE, 1.0f );
			if ( iNode != NO_NODE )
			{
				// move to the spot with line of sight
				m_vecMoveToOrderPos = g_pBigAINet->GetNode(iNode)->GetPosition(GetHullType());
				return SCHED_ASW_MOVE_TO_ORDER_POS;
			}
			else
			{
				// abort
				Msg( "Failed to find throw node\n" );
				FinishedUsingOffhandItem( true, true );
			}
		}
	}
	return -1;
}

void CASW_Marine::FinishedUsingOffhandItem( bool bItemThrown, bool bFailed )
{
	// go back to following our commander's marine
	CASW_Player *pPlayer = GetCommander();
	if ( pPlayer && pPlayer->GetMarine() )
	{
		OrdersFromPlayer( pPlayer, ASW_ORDER_FOLLOW, pPlayer->GetMarine(), true );

		CSingleUserRecipientFilter user( pPlayer );
		UserMessageBegin( user, "ASWOrderStopItemFX" );
		WRITE_SHORT( entindex() );
		WRITE_BOOL( bItemThrown );
		WRITE_BOOL( bFailed );
		MessageEnd();
	}
	else
	{
		OrdersFromPlayer( NULL, ASW_ORDER_HOLD_POSITION, this, true, GetLocalAngles().y);
	}
}

// checks if our thrown offhand item will reach the destination
bool CASW_Marine::CanThrowOffhand( CASW_Weapon *pWeapon, const Vector &vecSrc, const Vector &vecDest, bool bDrawArc )
{
	if ( !pWeapon )
		return false;

	Vector vecVelocity = UTIL_LaunchVector( vecSrc, vecDest, pWeapon->GetThrowGravity() ) * 28.0f;

	Vector vecResult = UTIL_Check_Throw( vecSrc, vecVelocity, pWeapon->GetThrowGravity(),
			-Vector( 12,12,12 ), Vector( 12,12,12 ), MASK_NPCSOLID, COLLISION_GROUP_PROJECTILE, this, bDrawArc );

	float flDist = vecResult.DistTo( vecDest );
	return ( flDist < 50.0f );
}

int CASW_Marine::FindThrowNode( const Vector &vThreatPos, float flMinThreatDist, float flMaxThreatDist, float flBlockTime )
{
	if ( !CAI_NetworkManager::NetworksLoaded() )
		return NO_NODE;

	AI_PROFILE_SCOPE( CASW_Marine::FindThrowNode );

	Remember( bits_MEMORY_TASK_EXPENSIVE );

	int iMyNode	= GetPathfinder()->NearestNodeToNPC();
	if ( iMyNode == NO_NODE )
	{
		Vector pos = GetAbsOrigin();
		DevWarning( 2, "FindThrowNode() - %s has no nearest node! (Check near %f %f %f)\n", GetClassname(), pos.x, pos.y, pos.z);
		return NO_NODE;
	}

	// ------------------------------------------------------------------------------------
	// We're going to search for a shoot node by expanding to our current node's neighbors
	// and then their neighbors, until a shooting position is found, or all nodes are beyond MaxDist
	// ------------------------------------------------------------------------------------
	AI_NearNode_t *pBuffer = (AI_NearNode_t *)stackalloc( sizeof(AI_NearNode_t) * g_pBigAINet->NumNodes() );
	CNodeList list( pBuffer, g_pBigAINet->NumNodes() );
	CVarBitVec wasVisited(g_pBigAINet->NumNodes());	// Nodes visited

	// mark start as visited
	wasVisited.Set( iMyNode );
	list.Insert( AI_NearNode_t(iMyNode, 0) );

	static int nSearchRandomizer = 0;		// tries to ensure the links are searched in a different order each time;

	while ( list.Count() )
	{
		int nodeIndex = list.ElementAtHead().nodeIndex;
		// remove this item from the list
		list.RemoveAtHead();

		const Vector &nodeOrigin = g_pBigAINet->GetNode(nodeIndex)->GetPosition(GetHullType());

		// HACKHACK: Can't we rework this loop and get rid of this?
		// skip the starting node, or we probably wouldn't have called this function.
		if ( nodeIndex != iMyNode )
		{
			bool skip = false;

			// Don't accept climb nodes, and assume my nearest node isn't valid because
			// we decided to make this check in the first place.  Keep moving
			if ( !skip && !g_pBigAINet->GetNode(nodeIndex)->IsLocked() &&
				g_pBigAINet->GetNode(nodeIndex)->GetType() != NODE_CLIMB )
			{
				// Now check its distance and only accept if in range
				float flThreatDist = ( nodeOrigin - vThreatPos ).Length();

				if ( flThreatDist < flMaxThreatDist &&
					flThreatDist > flMinThreatDist )
				{
					//CAI_Node *pNode = g_pBigAINet->GetNode(nodeIndex);
					if ( CanThrowOffhand( m_hOffhandItemToUse.Get(), GetOffhandThrowSource( &nodeOrigin ), vThreatPos, asw_debug_throw.GetInt() == 2 ) )
					{
						// Note when this node was used, so we don't try 
						// to use it again right away.
						g_pBigAINet->GetNode(nodeIndex)->Lock( flBlockTime );

						if ( asw_debug_throw.GetBool() )
						{
							NDebugOverlay::Text( nodeOrigin, CFmtStr( "%d:los", nodeIndex), false, 1 );

							// draw the arc
							CanThrowOffhand( m_hOffhandItemToUse.Get(), GetOffhandThrowSource( &nodeOrigin ), vThreatPos, true );
						}

						// The next NPC who searches should use a slight different pattern
						nSearchRandomizer = nodeIndex;
						return nodeIndex;
					}
					else
					{
						if ( asw_debug_throw.GetBool() )
						{
							NDebugOverlay::Text( nodeOrigin, CFmtStr( "%d:!throw", nodeIndex), false, 1 );
						}
					}
				}
				else
				{
					if ( asw_debug_throw.GetBool() )
					{
						CFmtStr msg( "%d:%s", nodeIndex, ( flThreatDist < flMaxThreatDist ) ? "too close" : "too far" );
						NDebugOverlay::Text( nodeOrigin, msg, false, 1 );
					}
				}
			}
		}

		// Go through each link and add connected nodes to the list
		for (int link=0; link < g_pBigAINet->GetNode(nodeIndex)->NumLinks();link++) 
		{
			int index = (link + nSearchRandomizer) % g_pBigAINet->GetNode(nodeIndex)->NumLinks();
			CAI_Link *nodeLink = g_pBigAINet->GetNode(nodeIndex)->GetLinkByIndex(index);

			if ( !GetPathfinder()->IsLinkUsable( nodeLink, iMyNode ) )
				continue;

			int newID = nodeLink->DestNodeID(nodeIndex);

			// If not already visited, add to the list
			if (!wasVisited.IsBitSet(newID))
			{
				float dist = (GetLocalOrigin() - g_pBigAINet->GetNode(newID)->GetPosition(GetHullType())).LengthSqr();
				list.Insert( AI_NearNode_t(newID, dist) );
				wasVisited.Set( newID );
			}
		}
	}
	// We failed.  No range attack node node was found
	return NO_NODE;
}


// custom follow stuff
#define ASW_FOLLOW_DISTANCE 230
#define ASW_FOLLOW_DISTANCE_NO_LOS 100
#define ASW_FORMATION_FOLLOW_DISTANCE 40


bool CASW_Marine::NeedToUpdateSquad()
{
	// basically, if I am a leader and I've moved, an update is needed.
	return ( GetSquadLeader() == this && GetSquadFormation()->ShouldUpdateFollowPositions() );
}

bool CASW_Marine::NeedToFollowMove()
{
	CASW_Marine * RESTRICT pLeader = GetSquadLeader();
	if ( !pLeader || pLeader == this )
		return false;

	if( IsOutOfAmmo() && GetEnemy() )
		return false;

	// only move if we're not near our saved follow point
	float dist = ( GetAbsOrigin() - GetFollowPos() ).Length2DSqr();
	return dist > ( ASW_FORMATION_FOLLOW_DISTANCE * ASW_FORMATION_FOLLOW_DISTANCE );
}

static bool ValidMarineMeleeTarget( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return false;

	// don't punch big scary aliens
	Class_T entClass = pEnt->Classify();
	if ( entClass == CLASS_ASW_BOOMER || entClass == CLASS_ASW_SHIELDBUG ||
			entClass == CLASS_ASW_HARVESTER || entClass == CLASS_ASW_MORTAR_BUG )
		return false;
	
	return true;
}

int CASW_Marine::SelectMeleeSchedule()
{
	if( GetPhysicsPropTarget() )
	{
		SetEnemy( GetPhysicsPropTarget() );

		return SCHED_MELEE_ATTACK_PROP1;
	}
	else if( GetEnemy() )
	{
		bool bLastManStanding = true;

		CASW_Game_Resource *pGameResource = ASWGameResource();
		for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if( pMarine && pMarine != this && pMarine->GetHealth() > 0 )
			{
				bLastManStanding = false;
				break;
			}
		}

		if( GetHealth() > GetMaxHealth() * 0.5f || bLastManStanding || GetAbsOrigin().DistToSqr( GetEnemyLKP() ) < Square( 100.0f ) )
		{
			if ( ValidMarineMeleeTarget( GetEnemy() ) )
			{
				return SCHED_ENGAGE_AND_MELEE_ATTACK1;
			}
		}
	} 

	return -1;
}

ConVar asw_follow_slow_distance( "asw_follow_slow_distance", "1200.0f", FCVAR_CHEAT, "Marines will follow their leader slowly during combat if he's within this distance" );

int CASW_Marine::SelectFollowSchedule()
{
	// if we don't have anyone to follow, revert to holding position orders
	if ( !GetSquadLeader() && ASWGameRules() && gpGlobals->curtime > ASWGameRules()->m_fMissionStartedTime + 4.0f )
	{
		SetASWOrders(ASW_ORDER_HOLD_POSITION, GetAbsAngles()[YAW], &GetAbsOrigin());
		return SCHED_ASW_HOLD_POSITION;
	}

	if (NeedToFollowMove())
	{
		return SCHED_ASW_FOLLOW_MOVE;
	}

	// shoot if we need to
	if ( HasCondition(COND_CAN_RANGE_ATTACK1) )
	{
		//Msg("Marine's select schedule returning SCHED_RANGE_ATTACK1\n");
		CBaseEntity* pEnemy = GetEnemy();
		if (pEnemy)		// don't shoot unless we actually have an enemy
			return SCHED_RANGE_ATTACK1;
	}	

	if( IsOutOfAmmo() && GetEnemy() )
	{
		SetCondition( COND_COMPLETELY_OUT_OF_AMMO );

		int iMeleeSchedule = SelectMeleeSchedule();
		if( iMeleeSchedule != -1 )
			return iMeleeSchedule;
	}

	// check if we're too near another marine
	//CASW_Marine *pCloseMarine = TooCloseToAnotherMarine();
	//if (pCloseMarine)
	//{
		//Msg("marine is too close to %d\n", pCloseMarine->entindex());
		//return SCHED_ASW_FOLLOW_BACK_OFF;
	//}

	return SCHED_ASW_FOLLOW_WAIT;
}

#define ASW_MARINE_TOO_CLOSE 40
CASW_Marine* CASW_Marine::TooCloseToAnotherMarine()
{
	if ( !ASWGameResource() )
		return NULL;

	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;

		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if (!pMarine || pMarine == this)
			continue;

		float dist = pMarine->GetAbsOrigin().DistTo(GetAbsOrigin());		
		if ( dist < ASW_MARINE_TOO_CLOSE)
		{
			Msg("marine %d is %f away\n", i, dist);
			return pMarine;
		}
	}

	return NULL;
}

void CASW_Marine::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ASW_ORDER_TO_DEPLOY_SPOT:
		{
			CASW_Bloodhound *pDropship = dynamic_cast<CASW_Bloodhound*>(gEntList.FindEntityByClassname( NULL, "asw_bloodhound" ));
			if (pDropship)
				pDropship->MarineLanded(this);
			TaskComplete();
			break;
		}
	case TASK_ASW_FACE_HOLDING_YAW:
	case TASK_ASW_FACE_USING_ITEM:
		{
			SetWait( pTask->flTaskData );
			SetIdealActivity( ACT_IDLE );			
			break;
		}
	case TASK_ASW_START_USING_AREA:
		{
			if ( m_hAreaToUse.Get() && m_hAreaToUse->IsUsable( this ) )
			{
				m_hAreaToUse->ActivateUseIcon( this, ASW_USE_RELEASE_QUICK );
			}
			TaskComplete();
			break;
		}
	case TASK_ASW_CHATTER_CONFIRM:
		{
			if (!m_bDoneOrderChatter)
			{
				m_bDoneOrderChatter = true;
				if (!IsInfested() && random->RandomFloat() <= pTask->flTaskData)
					GetMarineSpeech()->QueueChatter(CHATTER_USE, gpGlobals->curtime + 0.1f, gpGlobals->curtime + 5.0f, GetCommander());
			}
			TaskComplete();
			break;
		}
	case TASK_ASW_FACE_FOLLOW_WAIT:
		{
			SetWait( pTask->flTaskData );
			break;
		}
	case TASK_ASW_FACE_ENEMY_WITH_ERROR:
		{
			break;
		}
	case TASK_ASW_OVERKILL_SHOOT:
		{
			m_fStartedFiringTime = gpGlobals->curtime;
			break;
		}
	case TASK_ASW_GET_PATH_TO_ORDER_POS:
		{			
			//NDebugOverlay::Line( GetAbsOrigin(), m_vecMoveToOrderPos, 255, 0, 0, true, 4.0f );
			float flTolerance = AIN_HULL_TOLERANCE;
			if ( m_vecMoveToOrderPos == m_vecOffhandItemSpot )
			{
				flTolerance = 100.0f;
			}
			AI_NavGoal_t goal(m_vecMoveToOrderPos, ACT_RUN, flTolerance);			
			GetNavigator()->SetGoal( goal );
			TaskComplete();
			break;
		}
	case TASK_ASW_GET_PATH_TO_FOLLOW_TARGET:
		{
			if ( !GetSquadLeader() )
				TaskFail("No follow target");
			else
			{				
				AI_NavGoal_t goal( GetFollowPos(), ACT_RUN, 60 ); // AIN_HULL_TOLERANCE
				GetNavigator()->SetGoal( goal );
				TaskComplete();
			}
		}
		break;
	case TASK_ASW_GET_PATH_TO_PROP:
		{
			if ( !GetPhysicsPropTarget() )
				TaskFail("No prop");
			else
			{				
				AI_NavGoal_t goal( GetPhysicsPropTarget()->WorldSpaceCenter(), ACT_RUN, 60 ); // AIN_HULL_TOLERANCE
				GetNavigator()->SetGoal( goal );
				TaskComplete();
			}
		}
		break;
	case TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT:
	case TASK_ASW_WAIT_FOR_MOVEMENT:
		{
			if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				TaskComplete();
				GetNavigator()->ClearGoal();		// Clear residual state
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
			}
			break;
		}
	case TASK_ASW_GET_BACK_OFF_PATH:
		{
			CASW_Marine *pCloseMarine = TooCloseToAnotherMarine();
			if (!pCloseMarine)
				TaskFail("No marine too close");
			else
			{
				Vector diff = GetAbsOrigin() - pCloseMarine->GetAbsOrigin();
				Vector diffnorm = diff;
				VectorNormalize(diffnorm);
				Vector vecGoalPos = GetAbsOrigin() + diffnorm * 60;	// move to x units away from this marine

				AI_NavGoal_t goal(vecGoalPos, ACT_RUN, AIN_HULL_TOLERANCE);
				GetNavigator()->SetGoal( goal );
				TaskComplete();
			}
		}
		break;
	case TASK_MOVE_AWAY_PATH:
		GetMotor()->SetIdealYaw( UTIL_AngleMod( GetLocalAngles().y - 180.0f ) );
		BaseClass::StartTask( pTask );
		break;

	case TASK_ASW_RAPPEL:
		{
			CreateZipline();
			SetDescentSpeed();
		}
		break;

	case TASK_ASW_HIT_GROUND:
		m_bOnGround = true;
		TaskComplete();
		break;

	case TASK_ASW_GET_PATH_TO_GIVE_AMMO:
		if (!m_hGiveAmmoTarget)
		{
			TaskComplete();
		}
		break;

	case TASK_ASW_MOVE_TO_GIVE_AMMO:
		if (!m_hGiveAmmoTarget)
		{
			TaskComplete();
		}
		break;

	case TASK_ASW_SWAP_TO_AMMO_BAG:
		{
			CASW_Weapon *pWeapon = GetActiveASWWeapon();
			if ( !pWeapon || pWeapon->Classify() != CLASS_ASW_AMMO_BAG )
			{
				for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
				{
					CASW_Weapon *pWeapon = GetASWWeapon( iWeapon );
					if ( pWeapon && pWeapon->Classify() == CLASS_ASW_AMMO_BAG )
					{
						Weapon_Switch( pWeapon );
						break;
					}
				}
			}
			TaskComplete();
		}
		break;

	case TASK_ASW_GIVE_AMMO_TO_MARINE:
		if (m_hGiveAmmoTarget)
		{
			CASW_Weapon *pWeapon = GetActiveASWWeapon();
			if ( pWeapon && pWeapon->Classify() == CLASS_ASW_AMMO_BAG )
			{
				CASW_Weapon_Ammo_Bag *pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pWeapon);
				pAmmoBag->ThrowAmmo();
			}
		}
		TaskComplete();
		break;

	case TASK_MELEE_ATTACK1:
		{
			if( !GetEnemy() )
			{
				TaskFail("No enemy!");
			}
			else if ( GetAbsOrigin().DistToSqr( GetEnemy()->GetAbsOrigin() ) > Square( 100.0f ) )
			{
				if ( GetEnemy() == GetPhysicsPropTarget() )
				{
					SetPhysicsPropTarget( NULL );
				}
				TaskFail("Melee target too far!");
			}
			else
			{
				DoAnimationEvent( (PlayerAnimEvent_t) ( PLAYERANIMEVENT_MELEE + ( int ) pTask->flTaskData ) ); // send anim event to clients
				BaseClass::StartTask(pTask);
			}
		}
		break;

	case TASK_ASW_GET_PATH_TO_HEAL:
		{
			if( !m_hHealTarget )
			{
				TaskFail( FAIL_NO_GOAL );
			}
		}
		break;

	case TASK_ASW_MOVE_TO_HEAL:
		{
			if( !m_hHealTarget )
			{
				TaskFail( FAIL_NO_GOAL );
			}
		}
		break;

	case TASK_ASW_SWAP_TO_HEAL_GUN:
		{
			CASW_Weapon *pWeapon = GetActiveASWWeapon();
			if( !pWeapon || pWeapon->Classify() != CLASS_ASW_HEAL_GUN )
			{
				for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
				{
					CASW_Weapon *pWeapon = GetASWWeapon( iWeapon );
					if ( pWeapon && pWeapon->Classify() == CLASS_ASW_HEAL_GUN && pWeapon->HasAmmo() )
					{
						Weapon_Switch( pWeapon );
						break;
					}
				}
			}

			TaskComplete();
		}
		break;

	case TASK_ASW_HEAL_MARINE:
		{
			CASW_Weapon *pWeapon = GetActiveASWWeapon();
			if( !pWeapon || pWeapon->Classify() != CLASS_ASW_HEAL_GUN || !pWeapon->HasAmmo() )
			{
				TaskComplete();
			}
			else
			{
				CASW_Weapon_Heal_Gun *pHealgun = static_cast<CASW_Weapon_Heal_Gun*>( pWeapon );

				// if the gun to longer has a target...
				if ( !pHealgun || !m_hHealTarget || 
					m_hHealTarget->GetHealth() <= 0 || 
					m_hHealTarget->GetHealth() >= m_hHealTarget->GetMaxHealth() * MARINE_STOP_HEAL_THRESHOLD ||
					m_hHealTarget->GetAbsOrigin().DistToSqr( GetAbsOrigin() ) >= Square( CASW_Weapon_Heal_Gun::GetWeaponRange()*0.5 ) )
				{
					TaskComplete();
				}
				else
				{
					bool bHealFailed = false;
					// facing another direction
					Vector vecAttach = m_hHealTarget->GetAbsOrigin() - GetAbsOrigin();
					Vector vecForward;

					AngleVectors( ASWEyeAngles(), &vecForward );
					if ( DotProduct( vecForward, vecAttach ) < 0.0f )
					{
						TaskComplete();
						bHealFailed = true;
					}

					if ( !bHealFailed )
						pHealgun->HealAttach( m_hHealTarget );
				}
			}
		}
		break;

	default:
		{
			return BaseClass::StartTask(pTask);
			break;
		}
	}
}

CBaseEntity *CASW_Marine::BestAlienGooTarget()
{
	CASW_Alien_Goo *pAlienGooTarget = NULL;
	float flClosestDistSqr = GetFollowSightRange() * GetFollowSightRange();

	for ( int iAlienGoo = 0; iAlienGoo < g_AlienGoo.Count(); iAlienGoo++ )
	{
		// Get the instance of alien goo - if it exists.
		CASW_Alien_Goo *pAlienGoo = g_AlienGoo[iAlienGoo];
		if ( !pAlienGoo )
			continue;
	
		float flDistSqr = GetAbsOrigin().DistToSqr( pAlienGoo->GetAbsOrigin() );
		if( flDistSqr < flClosestDistSqr && FInViewCone( pAlienGoo->GetAbsOrigin() ) && !pAlienGoo->m_bHasGrubs )	// grubs = decorative, not a target
		{
			flClosestDistSqr = flDistSqr;
			pAlienGooTarget = pAlienGoo;
		}
	}

	return pAlienGooTarget;
}

bool CASW_Marine::EngageNewAlienGooTarget()
{
	CASW_Weapon *pActiveWeapon = GetActiveASWWeapon();
	CASW_Weapon *pFlamer = NULL;

	if( pActiveWeapon && pActiveWeapon->Classify() == CLASS_ASW_FLAMER && pActiveWeapon->HasAmmo() )
	{
		pFlamer = pActiveWeapon;
	}
	else
	{
		for ( int iWeapon = 0; iWeapon < ASW_NUM_INVENTORY_SLOTS; iWeapon++ )
		{
			CASW_Weapon *pWeapon = GetASWWeapon( iWeapon );
			if ( pWeapon && pWeapon->Classify() == CLASS_ASW_FLAMER && pWeapon->HasAmmo() )
			{
				pFlamer = pWeapon;
				break;
			}
		}
	}

	if( pFlamer )
	{
		EHANDLE hAlienGooTarget = BestAlienGooTarget();
		if( hAlienGooTarget.Get() )
		{
			SetAlienGooTarget( hAlienGooTarget );

			// only switch weapons if we have a valid target and the weapon is usable
			if( pFlamer != pActiveWeapon )
			{
				Weapon_Switch( pFlamer );
			}

			return true;
		}
	}

	return false;
}

const Vector &CASW_Marine::GetEnemyLKP() const
{
	// Only override default behavior if we're tracking alien goo and have no other target
	if( GetAlienGooTarget() && ( !GetEnemy() || GetAlienGooTarget() == GetEnemy() ) )
	{
		return GetAlienGooTarget()->GetAbsOrigin();
	}
	else
	{
		CASW_Prop_Physics *pPropPhysics = dynamic_cast< CASW_Prop_Physics *>( GetEnemy() );
		if ( pPropPhysics && pPropPhysics->m_takedamage == DAMAGE_YES && pPropPhysics->m_iHealth > 0 )
		{
			return pPropPhysics->GetAbsOrigin();
		}

		return BaseClass::GetEnemyLKP();
	}
}

void CASW_Marine::RunTask( const Task_t *pTask )
{
	bool bOld = m_bWantsToFire;
	m_bWantsToFire = false;

	CheckForAIWeaponSwitch();

	// check for firing on the move, if our enemy is in range, with LOS and we're facing him
	if (GetASWOrders() == ASW_ORDER_MOVE_TO || GetASWOrders() == ASW_ORDER_FOLLOW)
	{
		bool bMelee = GetCurSchedule()->GetId() == GetGlobalScheduleId( SCHED_MELEE_ATTACK_PROP1 );

		if( !GetEnemy() )
		{
			if ( m_flLastGooScanTime + ASW_MARINE_GOO_SCAN_TIME < gpGlobals->curtime )
			{
				EngageNewAlienGooTarget();
				m_flLastGooScanTime = gpGlobals->curtime;
			}

			// base AI doesn't see biomass - Need to override every time if biomass targeted
			if( GetAlienGooTarget() )
			{
				SetEnemy( GetAlienGooTarget() );
			}
			else if( bMelee && GetPhysicsPropTarget() )
			{
				SetEnemy( GetPhysicsPropTarget() );
			}
		}
		else if( !bMelee && GetEnemy() == GetPhysicsPropTarget() )
		{
			SetEnemy( NULL );
			SetPhysicsPropTarget( NULL );
		}

		CASW_Weapon *pWeapon = GetActiveASWWeapon();
		bool bOffensiveWeapon = (pWeapon && pWeapon->IsOffensiveWeapon());

		if ( GetEnemy() )
		{
			Vector vecEnemyLKP = GetEnemyLKP();

			if ( bOffensiveWeapon && !bMelee && FInAimCone( vecEnemyLKP ) && 
				 GetAbsOrigin().DistTo( GetEnemy()->GetAbsOrigin() ) <= GetFollowSightRange() )
			{
				// check it's a valid offensive weapon
				bool bWeaponHasLOS = WeaponLOSCondition( GetAbsOrigin(), GetEnemy()->EyePosition(), true );
				if ( !bWeaponHasLOS )
				{
					bWeaponHasLOS = WeaponLOSCondition( GetAbsOrigin(), GetEnemy()->BodyTarget( GetAbsOrigin() ), true );
				}

				if ( bWeaponHasLOS && !HasCondition(COND_ENEMY_TOO_FAR) )
				{
					m_bWantsToFire = true;
					m_fMarineAimError *= asw_marine_aim_error_decay_multiplier.GetFloat();
					if (pTask->iTask != TASK_ASW_OVERKILL_SHOOT)
						m_vecOverkillPos = vecEnemyLKP;

					m_fRandomFacing = UTIL_VecToYaw(vecEnemyLKP - GetAbsOrigin());
					float fTwitch = float(GetHealth()) / float(GetMaxHealth());
					if (fTwitch < 0.2)
						fTwitch = 0.2f;
					m_fNewRandomFacingTime = gpGlobals->curtime + random->RandomFloat(5 * fTwitch, 9.0f * fTwitch);
				}
				else if ( asw_debug_marine_aim.GetBool() )
				{
					Msg( "Following AI not shooting enemy as we don't have weapon LOS\n" );
				}
			}
			else if ( asw_debug_marine_aim.GetBool() )
			{
				Msg("Following AI not shooting enemy: InAimcone=%d FacingIdeal=%d\n",
					FInAimCone( vecEnemyLKP ), FacingIdeal());

				Vector los = ( vecEnemyLKP - GetAbsOrigin() );

				// do this in 2D
				los.z = 0;
				VectorNormalize( los );

				Vector facingDir = BodyDirection2D( );

				float flDot = DotProduct( los, facingDir );
				Msg( "  los = %f %f %f (%f)", VectorExpand( los ), UTIL_VecToYaw( los ) );
				Msg( "  fac = %f %f %f (%f)", VectorExpand( facingDir ), UTIL_VecToYaw( facingDir ) );
				Msg( "  flDot = %f eye = %f\n", flDot, UTIL_VecToYaw( EyeDirection2D() ) );

				NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + facingDir * 200, 0, 0, 255, true, 0.1f );
				NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + los * 200, 255, 0, 0, true, 0.1f );
			}
		}
	}

	switch(pTask->iTask)
	{
	case TASK_ASW_FACE_HOLDING_YAW:
		{
			GetMotor()->SetIdealYawAndUpdate( m_fHoldingYaw );
			Scan();

			if ( IsWaitFinished())
			{
				TaskComplete();
			}
			break;
		}
	case TASK_ASW_FACE_USING_ITEM:
		{
			if (!m_hUsingEntity.Get())
			{
				GetMotor()->SetIdealYawAndUpdate( m_fHoldingYaw );
				TaskComplete();
			}
			else
			{
				CASW_Use_Area *pArea = dynamic_cast< CASW_Use_Area* >( m_hUsingEntity->GetBaseEntity() );
				Vector vecUseTarget = m_hUsingEntity.Get()->GetAbsOrigin();
				if ( pArea && pArea->GetProp() )
					vecUseTarget = pArea->GetProp()->GetAbsOrigin();
					
				Vector diff = vecUseTarget - GetAbsOrigin();
				float fUsingYaw = UTIL_VecToYaw( diff );
				GetMotor()->SetIdealYawAndUpdate( fUsingYaw );
			}
			
			break;
		}
	case TASK_ASW_FACE_FOLLOW_WAIT:
		{
			UpdateFacing();
			if ( IsWaitFinished())
			{
				TaskComplete();
			}
			break;
		}
	case TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT:
		{
			UpdateFacing();
			bool fTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );
			if (fTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				TaskComplete();
				GetNavigator()->StopMoving();		// Stop moving
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();

				const Vector &vecFollowPos = GetFollowPos();
				if ( ( GetNavigator()->GetGoalPos() - vecFollowPos ).LengthSqr() > Square( 60 ) )
				{
					if ( GetNavigator()->GetNavType() != NAV_JUMP )
					{
						if ( !GetNavigator()->UpdateGoalPos( vecFollowPos ) )
						{
							TaskFail(FAIL_NO_ROUTE);
						}
					}
				}
				else if (!NeedToFollowMove())
				{					
					TaskComplete();
				}
				else
				{
					// try to keep facing towards the last known position of the enemy
					//if (GetEnemy())
					//{
						//Vector vecEnemyLKP = GetEnemyLKP();
						//AddFacingTarget( GetEnemy(), vecEnemyLKP, 1.0, 0.8 );
						//Msg("follow task adding facing target\n");
					//}
				}
			}
			break;
		}
	case TASK_ASW_WAIT_FOR_MOVEMENT:
		{
			UpdateFacing();

			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			bool fTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );

			if (fTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				TaskComplete();
				GetNavigator()->StopMoving();		// Stop moving
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
			}
			break;
		}
	case TASK_ASW_FACE_ENEMY_WITH_ERROR:
		{
			// If the yaw is locked, this function will not act correctly
			Assert( GetMotor()->IsYawLocked() == false );

			UpdateFacing();
			
			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_ASW_OVERKILL_SHOOT:
		{
			RunTaskRangeAttack1(pTask);
			break;
		}
	case TASK_ASW_RAPPEL:
		{
			// If we don't do this, the beam won't show up sometimes. Ideally, all beams would update their
			// bboxes correctly, but we're close to shipping and we can't change that now.
			if ( m_hLine )
			{
				m_hLine->RelinkBeam();
			}

			//if( GetEnemy() )
			//{
				// Face the enemy if there's one.
				//Vector vecEnemyLKP = GetEnemyLKP();
				//GetMotor()->SetIdealYawToTargetAndUpdate( vecEnemyLKP );
			//}

			SetDescentSpeed();
			if( GetFlags() & FL_ONGROUND )
			{
				//m_OnRappelTouchdown.FireOutput( GetOuter(), GetOuter(), 0 );
				RemoveFlag( FL_FLY );
				
				CutZipline();

				TaskComplete();
			}
		}
		break;
	case TASK_ASW_GET_PATH_TO_GIVE_AMMO:
		{
			if(m_hGiveAmmoTarget)
			{
				AI_NavGoal_t goal( m_hGiveAmmoTarget->GetAbsOrigin(), ACT_RUN, CASW_Weapon_Ammo_Bag::GetAIMaxAmmoGiveDistance() );
				GetNavigator()->SetGoal( goal );
			}
			TaskComplete();
		}
		break;
	case TASK_ASW_MOVE_TO_GIVE_AMMO:
		// if target is still valid, move to target
		if( m_hGiveAmmoTarget)
		{
			UpdateFacing();
			bool bTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );
			bool bArrived = ( ( GetNavigator()->GetGoalPos() - GetAbsOrigin() ).LengthSqr() < Square( CASW_Weapon_Ammo_Bag::GetAIMaxAmmoGiveDistance() ) );
			if ( bTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE || bArrived )
			{
				TaskComplete();
				GetNavigator()->StopMoving();
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();

				// see if target marine has moved far enough that we need to adjust path
				const Vector &vecCurrentPos = m_hGiveAmmoTarget->GetAbsOrigin();
				if ( ( GetNavigator()->GetGoalPos() - vecCurrentPos ).LengthSqr() > Square( CASW_Weapon_Ammo_Bag::GetAIMaxAmmoGiveDistance() ) )
				{
					if ( GetNavigator()->GetNavType() != NAV_JUMP )
					{
						if ( !GetNavigator()->UpdateGoalPos( vecCurrentPos ) )
						{
							TaskFail( FAIL_NO_ROUTE );
						}
					}
				}
			}
		}
		else
		// otherwise, the move task is complete (either we've arrived or the target is invalid)
		{
			m_hGiveAmmoTarget = NULL;
			TaskComplete();
		}
		break;

	case TASK_ASW_GET_PATH_TO_HEAL:
		{
			if( m_hHealTarget )
			{
				AI_NavGoal_t goal( m_hHealTarget->GetAbsOrigin(), ACT_RUN, CASW_Weapon_Heal_Gun::GetWeaponRange() * 0.5f );
				GetNavigator()->SetGoal( goal );
			}
			TaskComplete();
		}
		break;

	case TASK_ASW_MOVE_TO_HEAL:
		{
			// if target is still valid, move to target
			if(  m_hHealTarget )
			{
				UpdateFacing();
				bool bTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );
				bool bArrived = ( ( GetNavigator()->GetGoalPos() - GetAbsOrigin() ).LengthSqr() < Square( CASW_Weapon_Heal_Gun::GetWeaponRange()*0.5f ) );
				if ( bTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE || bArrived )
				{
					TaskComplete();
					GetNavigator()->StopMoving();
				}
				else if (!GetNavigator()->IsGoalActive())
				{
					SetIdealActivity( GetStoppedActivity() );
				}
				else
				{
					// Check validity of goal type
					ValidateNavGoal();

					// see if target marine has moved far enough that we need to adjust path
					const Vector &vecCurrentPos = m_hHealTarget->GetAbsOrigin();
					if ( ( GetNavigator()->GetGoalPos() - vecCurrentPos ).LengthSqr() > Square( CASW_Weapon_Heal_Gun::GetWeaponRange()*0.5f ) )
					{
						if ( GetNavigator()->GetNavType() != NAV_JUMP )
						{
							if ( !GetNavigator()->UpdateGoalPos( vecCurrentPos ) )
							{
								TaskFail( FAIL_NO_ROUTE );
							}
						}
					}
				}
			}
			else
			// otherwise, the move task is complete (either we've arrived or the target is invalid)
			{
				m_hHealTarget = NULL;
				TaskComplete();
			}
		}
		break;

	case TASK_ASW_SWAP_TO_HEAL_GUN:
		{
			// this is done in StartTask
		}
		break;

	case TASK_ASW_HEAL_MARINE:
		{
			CASW_Weapon *pWeapon = GetActiveASWWeapon();
			if( !m_hHealTarget || !pWeapon || pWeapon->Classify() != CLASS_ASW_HEAL_GUN || !pWeapon->HasAmmo() )
			{
				TaskComplete();
			}
			else
			{
				CASW_Weapon_Heal_Gun *pHealgun = static_cast<CASW_Weapon_Heal_Gun*>( pWeapon );

				bool bHealSucceeded = true;

				// if the gun to longer has a target...
				if ( m_hHealTarget->GetHealth() <= 0 || 
					m_hHealTarget->GetHealth() >= m_hHealTarget->GetMaxHealth() * MARINE_STOP_HEAL_THRESHOLD )
				{
					TaskComplete();
					bHealSucceeded = false;
				}
				else
				{
					if ( m_hHealTarget->GetAbsOrigin().DistToSqr( GetAbsOrigin() ) >= Square( CASW_Weapon_Heal_Gun::GetWeaponRange()*0.75 ) )
					{
						TaskComplete();
					}
					else
					{
						// facing another direction
						Vector vecAttach = m_hHealTarget->GetAbsOrigin() - GetAbsOrigin();
						Vector vecForward;

						AngleVectors( ASWEyeAngles(), &vecForward );
						if ( DotProduct( vecForward, vecAttach ) < 0.0f )
						{
							TaskComplete();
						}
					}
				}

				if ( !pHealgun || !pHealgun->HasHealAttachTarget() )
				{
					bHealSucceeded = false;
					TaskComplete();
				}

				if ( bHealSucceeded )
					pHealgun->PrimaryAttack();
			}		
		}
		break;

	default:
		{
		BaseClass::RunTask(pTask);
		}
	}

	if (bOld && !m_bWantsToFire)
	{
		//Msg("Stopped firing\n");
	}
}

void CASW_Marine::CheckForAIWeaponSwitch()
{
	if ( !GetEnemy() )
		return;

	CASW_Weapon *pWeapon = GetActiveASWWeapon();
	if ( pWeapon && pWeapon->IsOffensiveWeapon() && pWeapon->HasPrimaryAmmo() )
		return;

	// see if any of our other inventory items are valid weapons
	for ( int i = 0; i < ASW_NUM_INVENTORY_SLOTS; i++ )
	{
		CASW_Weapon *pOtherWeapon = GetASWWeapon( i );
		if ( pOtherWeapon != pWeapon && pOtherWeapon && pOtherWeapon->IsOffensiveWeapon() && pOtherWeapon->HasPrimaryAmmo() )
		{
			Weapon_Switch( pOtherWeapon );
			m_iHurtWithoutOffensiveWeapon = 0;
			return;
		}
	}

	// if we have no guns with primary ammo, search for one with secondary ammo
	for ( int i = 0; i < ASW_NUM_INVENTORY_SLOTS; i++ )
	{
		CASW_Weapon *pOtherWeapon = GetASWWeapon( i );
		if ( pOtherWeapon != pWeapon && pOtherWeapon && pOtherWeapon->IsOffensiveWeapon() && pOtherWeapon->HasAmmo() )
		{
			Weapon_Switch( pOtherWeapon );
			m_iHurtWithoutOffensiveWeapon = 0;
			return;
		}
	}
}

#define ASW_OVERKILL_TIME random->RandomFloat(0.5f,1.0f)
void CASW_Marine::RunTaskRangeAttack1( const Task_t *pTask )
{
	m_bWantsToFire = true;

	AutoMovement( );

	Vector vecEnemyLKP = GetEnemyLKP();

	float fAimYaw;
	
	if (pTask->iTask != TASK_ASW_OVERKILL_SHOOT)
		fAimYaw = CalcIdealYaw(vecEnemyLKP);
	else
		fAimYaw = CalcIdealYaw(m_vecOverkillPos);

	//add in our aim error
	fAimYaw += m_fMarineAimError;
	// reduce the aim error
	m_fMarineAimError *= asw_marine_aim_error_decay_multiplier.GetFloat();

	// If our enemy was killed, but I'm not done animating, the last known position comes
	// back as the origin and makes the me face the world origin if my attack schedule
	// doesn't break when my enemy dies. (sjb)
	if( vecEnemyLKP != vec3_origin )
	{
		if ( ( pTask->iTask == TASK_RANGE_ATTACK1 || pTask->iTask == TASK_RELOAD || pTask->iTask == TASK_ASW_OVERKILL_SHOOT) && 
			 ( CapabilitiesGet() & bits_CAP_AIM_GUN ) && 
			 FInAimCone( vecEnemyLKP ) )
		{
			GetMotor()->SetIdealYawAndUpdate( fAimYaw, AI_KEEP_YAW_SPEED );
			if (pTask->iTask != TASK_ASW_OVERKILL_SHOOT)
				m_fOverkillShootTime = gpGlobals->curtime + ASW_OVERKILL_TIME;
		}
		else
		{
			GetMotor()->SetIdealYawAndUpdate( fAimYaw, AI_KEEP_YAW_SPEED );
		}
		if (pTask->iTask != TASK_ASW_OVERKILL_SHOOT)
		{
			m_vecOverkillPos = vecEnemyLKP;
		}
	}

	if (pTask->iTask != TASK_ASW_OVERKILL_SHOOT)
	{
		if (gpGlobals->curtime > m_fStartedFiringTime + 0.3f)
		{
			TaskComplete();
			return;
		}

		if (HasCondition(COND_ENEMY_DEAD)
			|| HasCondition(COND_ENEMY_OCCLUDED)
			|| HasCondition(COND_WEAPON_BLOCKED_BY_FRIEND)
			|| HasCondition(COND_WEAPON_SIGHT_OCCLUDED)
			|| HasCondition(COND_ASW_NEW_ORDERS))
		{
			m_bWantsToFire = false;
			TaskComplete();	
		}		
	}
	else
	{
		if (gpGlobals->curtime > m_fOverkillShootTime)
		{
			TaskComplete();
			return;
		}
	}
}

void CASW_Marine::StartTaskRangeAttack1( const Task_t *pTask )
{
	if (GetEnemy() == NULL)
	{
		//Msg("Started ranged attack task with no enemy!");
	}
	m_fStartedFiringTime = gpGlobals->curtime;
	BaseClass::StartTaskRangeAttack1(pTask);
}

bool CASW_Marine::SetNewAimError(CBaseEntity *pTarget)
{
	if (!pTarget)
		return false;

	// find our current yaw to the enemy
	float currentYaw = UTIL_AngleMod( GetLocalAngles().y );
	Vector	enemyDir = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();
	VectorNormalize( enemyDir );
	float angleDiff = VecToYaw( enemyDir );
	angleDiff = UTIL_AngleDiff( angleDiff, currentYaw );
	
	//  create some random error amount, within our angle
	m_fMarineAimError = random->RandomFloat( asw_marine_aim_error_min.GetFloat(), asw_marine_aim_error_max.GetFloat() );
	if ( m_fMarineAimError > fabs( angleDiff ) )
	{
		m_fMarineAimError = fabs( angleDiff );
	}
	
	if ( angleDiff > 0 )
	{
		m_fMarineAimError = -m_fMarineAimError;
	}

	if ( asw_debug_marine_aim.GetBool() )
	{
		Msg( "Set marine's aim error to %f\n", m_fMarineAimError );
	}

	return true;
}

// adjusts a target point by our aim error angle
void CASW_Marine::AddAimErrorToTarget(Vector &vecTarget)
{
	Vector diff = vecTarget;
	Vector diff_rotated;

	matrix3x4_t fRotateMatrix;
	AngleMatrix(QAngle(0, m_fMarineAimError, 0), fRotateMatrix);
	VectorRotate( diff, fRotateMatrix, diff_rotated);
	VectorRotate(diff, QAngle(0, m_fMarineAimError, 0), diff_rotated);
	vecTarget = diff_rotated;

	return;
}

//-----------------------------------------------------------------------------
// Purpose: Return the actual position the NPC wants to fire at when it's trying
//			to hit it's current enemy.
//-----------------------------------------------------------------------------
Vector CASW_Marine::GetActualShootPosition( const Vector &shootOrigin )
{
	// Project the target's location into the future.
	Vector vecEnemyLKP = GetEnemyLKP();
	Vector vecEnemyOffset = GetEnemy()->BodyTarget( shootOrigin ) - GetEnemy()->GetAbsOrigin();
	Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

	// lead for some fraction of a second.
	return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
}

// potentially miss
//  asw - this is mostly copied from ai_basenpc, but with our added aim error on top
Vector CASW_Marine::GetActualShootTrajectory( const Vector &shootOrigin )
{
	const Task_t *pCurTask = GetTask();
	if ( pCurTask && pCurTask->iTask == TASK_ASW_OVERKILL_SHOOT )
	{
		// if we're overkill shooting, fire at the overkill pos
		Vector shotDir = m_vecOverkillPos+Vector(0,0,45) - shootOrigin;
		VectorNormalize( shotDir );

		CollectShotStats( shootOrigin, shotDir );
		CShotManipulator manipulator( shotDir );
		manipulator.ApplySpread( GetAttackSpread( GetActiveWeapon(), GetEnemy() ), GetSpreadBias( GetActiveWeapon(), GetEnemy() ) );
		shotDir = manipulator.GetResult();
		return shotDir;
	}
	
	Vector vecShotDir;

	if( !GetEnemy() )
	{
		vecShotDir = GetShootEnemyDir( shootOrigin );
	}
	else
	{

		Vector vecProjectedPosition = GetActualShootPosition( shootOrigin );

		vecShotDir = vecProjectedPosition - shootOrigin;
		VectorNormalize( vecShotDir );

		// NOW we have a shoot direction. Where a 100% accurate bullet should go.
		// Modify it by weapon proficiency.
		// construct a manipulator 
		CShotManipulator manipulator( vecShotDir );

		manipulator.ApplySpread( GetAttackSpread( GetActiveWeapon(), GetEnemy() ), GetSpreadBias( GetActiveWeapon(), GetEnemy() ) );
		vecShotDir = manipulator.GetResult();
	}

	AddAimErrorToTarget( vecShotDir );	// asw

	return vecShotDir;
}

void CASW_Marine::DrawDebugGeometryOverlays()
{
	if (GetASWOrders() == ASW_ORDER_MOVE_TO)
	{
		float flViewRange	= acos(0.8);
		Vector vEyeDir = EyeDirection2D( );
		Vector vLeftDir, vRightDir;
		float fSin, fCos;
		SinCos( flViewRange, &fSin, &fCos );

		vLeftDir.x			= vEyeDir.x * fCos - vEyeDir.y * fSin;
		vLeftDir.y			= vEyeDir.x * fSin + vEyeDir.y * fCos;
		vLeftDir.z			=  vEyeDir.z;
		fSin				= sin(-flViewRange);
		fCos				= cos(-flViewRange);
		vRightDir.x			= vEyeDir.x * fCos - vEyeDir.y * fSin;
		vRightDir.y			= vEyeDir.x * fSin + vEyeDir.y * fCos;
		vRightDir.z			=  vEyeDir.z;

		//NDebugOverlay::BoxDirection(m_vecMoveToOrderPos, Vector(0,0,-40), Vector(200,0,40), vLeftDir, 255, 0, 0, 50, 0 );
		//NDebugOverlay::BoxDirection(m_vecMoveToOrderPos, Vector(0,0,-40), Vector(200,0,40), vRightDir, 255, 0, 0, 50, 0 );
		//NDebugOverlay::BoxDirection(m_vecMoveToOrderPos, Vector(0,0,-40), Vector(200,0,40), vEyeDir, 0, 255, 0, 50, 0 );
		NDebugOverlay::Box(m_vecMoveToOrderPos, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, 128, 0 );
	}

	BaseClass::DrawDebugGeometryOverlays();

	if ( GetSquadFormation() )
	{
		GetSquadFormation()->DrawDebugGeometryOverlays();
	}
}

ConVar asw_marine_face_last_enemy_time( "asw_marine_face_last_enemy_time", "5.0", FCVAR_CHEAT, "Amount of time that AI marines will face their last enemy after losing it while in follow mode" );

void CASW_Marine::UpdateFacing()
{
	if ( gpGlobals->curtime > m_flNextYawOffsetTime )
	{
		RecalculateAIYawOffset();
	}

	if ( m_vecFacingPointFromServer.Get() != vec3_origin )
	{
		float flAimYaw = UTIL_VecToYaw( m_vecFacingPointFromServer.Get() - GetAbsOrigin() );
		GetMotor()->SetIdealYawAndUpdate( flAimYaw );
	}
	else if ( GetEnemy() )
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		float flAimYaw = CalcIdealYaw( vecEnemyLKP ) + m_fMarineAimError;
		GetMotor()->SetIdealYawAndUpdate( flAimYaw );

		m_flLastEnemyYaw = flAimYaw;
		m_flLastEnemyYawTime = gpGlobals->curtime;
		m_flAIYawOffset = 0;

		if ( asw_debug_marine_aim.GetBool() )
		{
			Vector vecAim;
			QAngle angAim = QAngle( 0, flAimYaw, 0 );
			AngleVectors( angAim, &vecAim );

			NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vecAim * 50, 255, 255, 0, false, 0.1f );
			NDebugOverlay::Line( GetAbsOrigin(), vecEnemyLKP, 255, 255, 255, false, 0.1f );
			Msg( "aim error = %f fAimYaw = %f\n", m_fMarineAimError, flAimYaw );
		}
	}
	else if ( IsCurSchedule( SCHED_ASW_MOVE_TO_ORDER_POS ) )		// face our order destination
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		float flAimYaw = CalcIdealYaw( m_vecMoveToOrderPos );
		GetMotor()->SetIdealYawAndUpdate( flAimYaw );

		if ( asw_debug_marine_aim.GetBool() )
		{
			Vector vecAim;
			QAngle angAim = QAngle( 0, flAimYaw, 0 );
			AngleVectors( angAim, &vecAim );

			NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vecAim * 50, 255, 255, 0, false, 0.1f );
			NDebugOverlay::Line( GetAbsOrigin(), vecEnemyLKP, 255, 255, 255, false, 0.1f );
			Msg( "aim error = %f fAimYaw = %f\n", m_fMarineAimError, flAimYaw );
		}
	}
	else if ( IsCurSchedule( SCHED_ASW_HEAL_MARINE ) )		// face the marine that we want to heal
	{
		if ( m_hHealTarget.Get() )
		{
			float flAimYaw = CalcIdealYaw( m_hHealTarget.Get()->GetAbsOrigin() );
			GetMotor()->SetIdealYawAndUpdate( flAimYaw );

			if ( asw_debug_marine_aim.GetBool() )
			{
				Vector vecAim;
				QAngle angAim = QAngle( 0, flAimYaw, 0 );
				AngleVectors( angAim, &vecAim );

				NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vecAim * 50, 0, 255, 0, false, 0.2f );
			}
		}
	}
	else if ( GetASWOrders() == ASW_ORDER_FOLLOW )
	{
		float flAimYaw = GetSquadFormation()->GetYaw( GetSquadFormation()->Find(this) );
		if ( gpGlobals->curtime < m_flLastEnemyYawTime + asw_marine_face_last_enemy_time.GetFloat() )
		{
			flAimYaw = m_flLastEnemyYaw;
		}
		flAimYaw += m_flAIYawOffset;
		GetMotor()->SetIdealYawAndUpdate( flAimYaw );
		
		if ( asw_debug_marine_aim.GetBool() )
		{
			Vector vecAim;
			QAngle angAim = QAngle( 0, flAimYaw, 0 );
			AngleVectors( angAim, &vecAim );

			NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vecAim * 50, 255, 255, 0, false, 0.1f );
			Msg( "aim error = %f fAimYaw = %f m_flAIYawOffset = %f\n", m_fMarineAimError, flAimYaw, m_flAIYawOffset );
		}
	}
// 	else
// 	{
// 		// face our destination
// 		Vector vecDest = GetNavigator()->GetGoalPos();
// 
// 		float fAimYaw = CalcIdealYaw( vecDest );// + m_fMarineAimError;	// TODO: put aim error back in
// 		GetMotor()->SetIdealYawAndUpdate( fAimYaw );
// 	}
}

bool CASW_Marine::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	// make him looking towards his holding direction as he moves
	/*
	if (GetASWOrders() == ASW_ORDER_MOVE_TO)
	{
		QAngle holding(0, m_fHoldingYaw,0);
		Vector vecForward;
		AngleVectors(holding, &vecForward);

		Vector vDest = GetNavigator()->GetGoalPos();
		Msg("Adding facing target..\n");
		AddFacingTarget( vDest + vecForward * 100.0f, 10.0f, 1.0f );
	}*/

	// if we're moving to a specific spot, make sure we face our enemy so we can shoot while on the move
	if (GetEnemy() && (GetASWOrders()==ASW_ORDER_MOVE_TO || GetASWOrders() == ASW_ORDER_FOLLOW))
	{
		// ASWTODO - is this move_yaw set needed? (or needs to be move_x/y?)
		//float flMoveYaw = UTIL_VecToYaw( move.dir );

		//float idealYaw = UTIL_AngleMod( flMoveYaw );
		//float flEYaw = UTIL_VecToYaw( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() );

		// UpdateFacing();

		// find movement direction to compensate for not being turned far enough
		// ASWTODO - is this move_yaw set needed? (or needs to be move_x/y?)
		/*
		float fSequenceMoveYaw = GetSequenceMoveYaw( GetSequence() );
		float flDiff = UTIL_AngleDiff( flMoveYaw, GetLocalAngles().y + fSequenceMoveYaw );
		SetPoseParameter( "move_yaw", GetPoseParameter( "move_yaw" ) + flDiff );
		*/
		return true;
	}

	return true;

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

// check for AI changing weapon if he's getting hurt and has a non-offensive weapon equipped
bool CASW_Marine::CheckAutoWeaponSwitch()
{
	CASW_Weapon *pWeapon = GetActiveASWWeapon();
	if (pWeapon && pWeapon->IsOffensiveWeapon())
		return true;

	// marine doesn't auto switch weapons the first two times he's hurt
	m_iHurtWithoutOffensiveWeapon++;
	if (m_iHurtWithoutOffensiveWeapon <3)
		return false;

	// find best offensive weapon
	CASW_Weapon *pBestWeapon = NULL;
	for (int i=0;i<ASW_MAX_MARINE_WEAPONS;i++)
	{
		pWeapon = GetASWWeapon(i);
		if (pWeapon && pWeapon->IsOffensiveWeapon())
		{
			pBestWeapon = pWeapon;
			break;
		}
	}
	if (pBestWeapon)
	{
		Weapon_Switch( pWeapon );
		m_iHurtWithoutOffensiveWeapon = 0;
		return true;
	}
	return false;
}


inline const Vector & CASW_Marine::GetFollowPos()
{
	m_hMarineFollowTarget = GetSquadLeader();

	// if I'm in a squad and it has a leader, then
	// use the computed position. Otherwise fall
	// back to current pos.
	unsigned slot = GetSquadFormation()->Find(this);
	return ( GetSquadFormation()->IsValid(slot) ? 
		GetSquadFormation()->GetIdealPosition(slot) :
	GetAbsOrigin()	);
}

// ===== Rappeling ===================================

void CASW_Marine::BeginRappel()
{
	m_bWaitingToRappel = true;
	m_bOnGround = false;
	RemoveFlag(FL_ONGROUND);

	// Send the message to begin rappeling!
	SetCondition( COND_ASW_BEGIN_RAPPEL );

	m_vecRopeAnchor = GetAbsOrigin();
}

void CASW_Marine::CutZipline()
{
	if( m_hLine.Get() )
	{
		UTIL_Remove( m_hLine );
	}

	// create one just hanging down in its place
	CBeam *pBeam;
	pBeam = CBeam::BeamCreate( "cable/cable.vmt", 1 );
	pBeam->SetColor( 150, 150, 150 );
	pBeam->SetWidth( 1.0 );	// was 0.3
	pBeam->SetEndWidth( 1.0 );

	//pBeam->PointEntInit( GetAbsOrigin() + Vector( 0, 0, 80 ), this );

	//pBeam->SetEndAttachment( attachment );
	//pBeam->SetAbsEndPos(GetAbsOrigin());
	pBeam->PointsInit(m_vecRopeAnchor, GetAbsOrigin());

	if( m_hLine.Get() )
	{
		UTIL_Remove( m_hLine );
	}
	m_hLine.Set( pBeam );

	//CBaseEntity *pAnchor = CreateEntityByName( "asw_rope_anchor" );
	//pAnchor->SetOwnerEntity( this );
	//pAnchor->SetAbsOrigin( m_vecRopeAnchor );
	//pAnchor->Spawn();
}


void CASW_Marine::CreateZipline()
{
	if( !m_hLine )
	{
		int attachment = LookupAttachment( "zipline" );

		if( attachment != -1 )
		{
			CBeam *pBeam;
			pBeam = CBeam::BeamCreate( "cable/cable.vmt", 1 );
			pBeam->SetColor( 150, 150, 150 );
			pBeam->SetWidth( 1.0 );	// was 0.3
			pBeam->SetEndWidth( 1.0 );

			pBeam->PointEntInit( GetAbsOrigin() + Vector( 0, 0, 80 ), this );

			pBeam->SetEndAttachment( attachment );

			m_hLine.Set( pBeam );
		}
	}
}

#define RAPPEL_MAX_SPEED	600	// Go this fast if you're really high.
#define RAPPEL_MIN_SPEED	60 // Go no slower than this.
#define RAPPEL_DECEL_DIST	(20.0f * 12.0f)	// Start slowing down when you're this close to the ground.
void CASW_Marine::SetDescentSpeed()
{
	// Trace to the floor and see how close we're getting. Slow down if we're close.
	// STOP if there's an NPC under us.
	trace_t tr;
	AI_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 8192 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	float flDist = fabs( GetAbsOrigin().z - tr.endpos.z );

	float speed = RAPPEL_MAX_SPEED;

	if( flDist <= RAPPEL_DECEL_DIST )
	{
		float factor;
		factor = flDist / RAPPEL_DECEL_DIST;

		speed = MAX( RAPPEL_MIN_SPEED, speed * factor );
	}

	Vector vecNewVelocity = vec3_origin;
	vecNewVelocity.z = -speed;
	SetAbsVelocity( vecNewVelocity );
}

void CASW_Marine::CleanupOnDeath( CBaseEntity *pCulprit, bool bFireDeathOutput )
{
	BaseClass::CleanupOnDeath( pCulprit, bFireDeathOutput );

	//This will remove the beam and create a rope if the NPC dies while rappeling down.
	if ( m_hLine )
	{
		 CutZipline();
	}
}


// use head direction to determine if spot is shootable
bool CASW_Marine::FInAimCone( const Vector &vecSpot )
{
	return BaseClass::FInAimCone( vecSpot );

	Vector los = ( vecSpot - Weapon_ShootPosition() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = HeadDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if (CapabilitiesGet() & bits_CAP_AIM_GUN)
	{
		// FIXME: query current animation for ranges
		return ( flDot > DOT_30DEGREE );
	}

	if ( flDot > 0.994 )//!!!BUGBUG - magic number same as FacingIdeal(), what is this?
		return true;

	return false;
}

void CASW_Marine::OnWeldStarted()
{
	m_bWaitingForWeld = true;
	m_flBeginWeldTime = gpGlobals->curtime;
	SetCondition( COND_ASW_NEW_ORDERS );
}

void CASW_Marine::OnWeldFinished()
{
	m_bWaitingForWeld = false;
	SetCondition( COND_ASW_NEW_ORDERS );
}

ConVar asw_debug_combat_status( "asw_debug_combat_status", "0", FCVAR_CHEAT, "Show marine's combat status on the screen" );

void CASW_Marine::UpdateCombatStatus()
{
	const float flCombatEnemyDist = 512.0f;
	const float flNearbyMarineDist = 768.0f;
	CASW_Game_Resource *pGameResource = ASWGameResource();

	if ( !IsInhabited() && GetEnemy() && GetEnemy()->GetAbsOrigin().DistTo( GetAbsOrigin() ) < flCombatEnemyDist )
	{
		m_flLastSquadEnemyTime = gpGlobals->curtime;
	}
	else
	{
		// check nearby squad mates
		for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if ( pOtherMarine && pOtherMarine != this && !pOtherMarine->IsInhabited() && pOtherMarine->GetEnemy()
					&& pOtherMarine->GetAbsOrigin().DistTo( GetAbsOrigin() ) < flNearbyMarineDist
					&& pOtherMarine->GetEnemy()->GetAbsOrigin().DistTo( pOtherMarine->GetAbsOrigin() ) < flCombatEnemyDist )
			{
				m_flLastSquadEnemyTime = gpGlobals->curtime;
				break;
			}
		}
	}

	m_flLastSquadShotAlienTime = 0.0f;

	for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
		if ( pOtherMarine != this && pOtherMarine && pOtherMarine->GetAbsOrigin().DistTo( GetAbsOrigin() ) < flNearbyMarineDist )
		{
			m_flLastSquadShotAlienTime = MAX( m_flLastSquadShotAlienTime, pOtherMarine->m_flLastHurtAlienTime );
		}
	}

	if ( asw_debug_combat_status.GetBool() )
	{
		engine->Con_NPrintf( GetMarineResource()->m_MarineProfileIndex, "%s: %s", GetMarineProfile()->GetShortName(), IsInCombat() ? "COMBAT" : "not in combat" );
	}
}

ConVar asw_marine_force_combat_status( "asw_marine_force_combat_status", "0", FCVAR_CHEAT );

bool CASW_Marine::IsInCombat()
{
	if ( asw_marine_force_combat_status.GetBool() )
		return true;

	const float flCombatTime = 10.0f;
	return ( ( m_flLastSquadEnemyTime != 0.0f && m_flLastSquadEnemyTime > gpGlobals->curtime - flCombatTime ) ||
				( m_flLastSquadShotAlienTime != 0.0f && m_flLastSquadShotAlienTime > gpGlobals->curtime - flCombatTime ) );
}

bool CASW_Marine::FValidateHintType( CAI_Hint *pHint )
{
	if ( pHint->HintType() == HINT_FOLLOW_WAIT_POINT )
		return true;

	return BaseClass::FValidateHintType( pHint );
}

ConVar asw_marine_random_yaw( "asw_marine_random_yaw", "40", FCVAR_NONE, "Min/max angle the marine will change his yaw when idling in follow mode" );
ConVar asw_marine_yaw_interval_min( "asw_marine_yaw_interval_min", "3", FCVAR_NONE, "Min time between AI marine shifting his yaw" );
ConVar asw_marine_yaw_interval_max( "asw_marine_yaw_interval_max", "8", FCVAR_NONE, "Max time between AI marine shifting his yaw" );
ConVar asw_marine_toggle_crouch_chance( "asw_marine_toggle_crouch_chance", "0.4", FCVAR_NONE, "Chance of AI changing between crouched and non-crouched while idling in follow mode" );

void CASW_Marine::RecalculateAIYawOffset()
{
	m_flAIYawOffset = RandomFloat( -asw_marine_random_yaw.GetFloat(), asw_marine_random_yaw.GetFloat() );
	if ( asw_blind_follow.GetBool() )
	{
		m_bAICrouch = ( GetASWOrders() == ASW_ORDER_HOLD_POSITION );
	}
	else if ( RandomFloat() < asw_marine_toggle_crouch_chance.GetFloat() )
	{
		m_bAICrouch = !m_bAICrouch;
	}
	m_flNextYawOffsetTime = gpGlobals->curtime + RandomFloat( asw_marine_yaw_interval_min.GetFloat(), asw_marine_yaw_interval_max.GetFloat() );
}

// disables collision between the marine and certain props that the marine cannot break reliably
void CASW_Marine::CheckForDisablingAICollision( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	// don't disable collision between the marine and breakable crates
	const char *szModelName = STRING( pEntity->GetModelName() );
	if ( !szModelName )
		return;

	if ( !CanMarineGetStuckOnProp( szModelName ) )
		return;

	PhysDisableEntityCollisions( this, pEntity );
	SetPhysicsPropTarget( NULL );
}

//-----------------------------------------------------------------------------
// Schedules
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( asw_marine, CASW_Marine )

	DECLARE_CONDITION( COND_ASW_NEW_ORDERS )
	DECLARE_CONDITION( COND_ASW_BEGIN_RAPPEL )
	DECLARE_CONDITION( COND_SQUADMATE_WANTS_AMMO )
	DECLARE_CONDITION( COND_SQUADMATE_NEEDS_AMMO )
	DECLARE_CONDITION( COND_PATH_BLOCKED_BY_PHYSICS_PROP )
	DECLARE_CONDITION( COND_PROP_DESTROYED )
	DECLARE_CONDITION( COND_COMPLETELY_OUT_OF_AMMO )

	DECLARE_TASK( TASK_ASW_FACE_HOLDING_YAW )
	DECLARE_TASK( TASK_ASW_FACE_USING_ITEM )
	DECLARE_TASK( TASK_ASW_START_USING_AREA )
	DECLARE_TASK( TASK_ASW_FACE_ENEMY_WITH_ERROR )
	DECLARE_TASK( TASK_ASW_OVERKILL_SHOOT )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_ORDER_POS )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_FOLLOW_TARGET )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_PROP )
	DECLARE_TASK( TASK_ASW_FACE_FOLLOW_WAIT )
	DECLARE_TASK( TASK_ASW_GET_BACK_OFF_PATH )
	DECLARE_TASK( TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT )
	DECLARE_TASK( TASK_ASW_WAIT_FOR_MOVEMENT )
	DECLARE_TASK( TASK_ASW_CHATTER_CONFIRM )
	DECLARE_TASK( TASK_ASW_RAPPEL )
	DECLARE_TASK( TASK_ASW_HIT_GROUND )	
	DECLARE_TASK( TASK_ASW_ORDER_TO_DEPLOY_SPOT )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_GIVE_AMMO )
	DECLARE_TASK( TASK_ASW_MOVE_TO_GIVE_AMMO )
	DECLARE_TASK( TASK_ASW_SWAP_TO_AMMO_BAG )
	DECLARE_TASK( TASK_ASW_GIVE_AMMO_TO_MARINE )
	DECLARE_TASK( TASK_ASW_GET_PATH_TO_HEAL )
	DECLARE_TASK( TASK_ASW_MOVE_TO_HEAL )
	DECLARE_TASK( TASK_ASW_SWAP_TO_HEAL_GUN )
	DECLARE_TASK( TASK_ASW_HEAL_MARINE )

	DECLARE_ANIMEVENT( AE_MARINE_KICK )
	DECLARE_ANIMEVENT( AE_MARINE_UNFREEZE )
	// Activities
	DECLARE_ACTIVITY( ACT_MARINE_GETTING_UP )
	DECLARE_ACTIVITY( ACT_MARINE_LAYING_ON_FLOOR )
	
	DEFINE_SCHEDULE	
	(
		SCHED_ASW_HOLD_POSITION,		
		  
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		//"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_ASW_FACE_HOLDING_YAW		2"	
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_IDLE_INTERRUPT"
		"       COND_ASW_NEW_ORDERS"
		"		COND_ASW_BEGIN_RAPPEL"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_ASW_USE_AREA,		
		  
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_ASW_START_USING_AREA		0"
		"		TASK_ASW_FACE_USING_ITEM		0"	
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_IDLE_INTERRUPT"
		"       COND_ASW_NEW_ORDERS"
		"		COND_ASW_BEGIN_RAPPEL"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_ASW_USING_OVER_TIME,		
		  
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_ASW_FACE_USING_ITEM		0"	
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_IDLE_INTERRUPT"
		"       COND_ASW_NEW_ORDERS"
		"		COND_ASW_BEGIN_RAPPEL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_OVERKILL_SHOOT,

		"	Tasks"
		"		TASK_ASW_OVERKILL_SHOOT		0"
		""
		"	Interrupts"
		"		COND_NO_PRIMARY_AMMO"
		"       COND_ASW_NEW_ORDERS"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_ASW_FACE_ENEMY_WITH_ERROR			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_HEAR_DANGER"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_WEAPON_SIGHT_OCCLUDED"
		"		COND_LOST_ENEMY"
		"       COND_ASW_NEW_ORDERS"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_MOVE_TO_ORDER_POS,

		"	Tasks"		
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_FOLLOW_WAIT"
		"		TASK_ASW_GET_PATH_TO_ORDER_POS		0"
		"		TASK_ASW_CHATTER_CONFIRM		0.4"
		"		TASK_RUN_PATH					0"	
		"		TASK_ASW_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				1"		
		""
		"	Interrupts"
		"       COND_ASW_NEW_ORDERS"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_FOLLOW_MOVE,

		"	Tasks"		
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_FOLLOW_WAIT"
		"		TASK_ASW_GET_PATH_TO_FOLLOW_TARGET		0"
		"		TASK_RUN_PATH			 0"	
		"		TASK_ASW_WAIT_FOR_FOLLOW_MOVEMENT			0"
		"		TASK_STOP_MOVING				1"		
		""
		"	Interrupts"
		"       COND_ASW_NEW_ORDERS"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_ASW_FOLLOW_WAIT,		
		  
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_ASW_FACE_FOLLOW_WAIT	0.3"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_IDLE_INTERRUPT"
		"       COND_ASW_NEW_ORDERS"
		"		COND_GIVE_WAY"
	)

	DEFINE_SCHEDULE	
	(
		SCHED_ASW_PICKUP_WAIT,		
		  
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_WAIT				1.5"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ASW_FOLLOW_BACK_OFF,

		"	Tasks"		
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_FOLLOW_WAIT"
		"		TASK_ASW_GET_BACK_OFF_PATH		0"
		"		TASK_RUN_PATH					0"	
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				1"		
		""
		"	Interrupts"
		"       COND_ASW_NEW_ORDERS"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_ASW_RAPPEL_WAIT,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_RAPPEL_LOOP"
		"		TASK_WAIT_INDEFINITE			0"
		""
		"	Interrupts"
		"		COND_ASW_BEGIN_RAPPEL"
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_ASW_RAPPEL,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_RAPPEL_LOOP"
		"		TASK_WAIT					0.2"
		"		TASK_ASW_RAPPEL				0"
		"		TASK_ASW_HIT_GROUND			0"
		"		TASK_ASW_ORDER_TO_DEPLOY_SPOT 0"
		//"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ASW_CLEAR_RAPPEL_POINT"
		""
		"	Interrupts"
		""
		"		COND_NEW_ENEMY"	// Only so the enemy selection code will pick an enemy!
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_ASW_CLEAR_RAPPEL_POINT,

		"	Tasks"
		"		TASK_ASW_HIT_GROUND			0"
		"		TASK_MOVE_AWAY_PATH		128"	// Clear this spot for other rappellers
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		""
		"	Interrupts"
		""
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_ASW_GIVE_AMMO,

		"	Tasks"
		"		TASK_ASW_GET_PATH_TO_GIVE_AMMO		0"
		"		TASK_ASW_MOVE_TO_GIVE_AMMO			0"
		"		TASK_ASW_SWAP_TO_AMMO_BAG			0"
		"		TASK_ASW_GIVE_AMMO_TO_MARINE		0"
		""
		"	Interrupts"
		""
	);

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
		(
		SCHED_ASW_HEAL_MARINE,

		"	Tasks"
		"		TASK_ASW_GET_PATH_TO_HEAL			0"
		"		TASK_ASW_MOVE_TO_HEAL				0"
		"		TASK_ASW_SWAP_TO_HEAL_GUN			0"
		"		TASK_ASW_HEAL_MARINE				0"
		""
		"	Interrupts"
		""
		);

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
		(
		SCHED_MELEE_ATTACK_PROP1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_ASW_GET_PATH_TO_PROP		40"	// 40 = attack distance
		"		TASK_RUN_PATH			 0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		1"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		2"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		3"
		""
		"	Interrupts"
		//"		COND_NEW_ENEMY"
		"		COND_PROP_DESTROYED"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		);

	//=========================================================
	//=========================================================
	DEFINE_SCHEDULE
		(
		SCHED_ENGAGE_AND_MELEE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_GET_CHASE_PATH_TO_ENEMY		40"	// 40 = attack distance
		"		TASK_RUN_PATH			 0"	
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		1"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		2"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		3"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		);

AI_END_CUSTOM_NPC()

//	DECLARE_TASK( TASK_ASW_TRACK_AND_FIRE )
	
