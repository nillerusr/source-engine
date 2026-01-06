//=============================================================================//
//
// Purpose:		Clone Cop, a former human bent on killing anyone who stands in his way.
//				He was trapped under Arbeit 1 for a long time (from his perspective),
//				but now he's back and he's bigger, badder, and possibly even more deranged than ever before.
//				I mean, you could just see the brains of this guy.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_clonecop.h"
#include "gameweaponmanager.h"
#include "ammodef.h"
#include "grenade_hopwire.h"
#include "npc_manhack.h"
#include "particle_parse.h"
#include "prop_combine_ball.h"
#include "ez2_player.h"
#include "ai_interactions.h"
#include "items.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_clonecop_health( "sk_clonecop_health","1000" );
ConVar	sk_clonecop_kick( "sk_clonecop_kick", "40" );

ConVar	npc_clonecop_suppress_min_occlude_dist( "npc_clonecop_suppress_min_occlude_dist", "128" );

extern ConVar sk_plr_dmg_buckshot;
extern ConVar sk_plr_num_shotgun_pellets;

// Think contexts
static const char *CC_BLEED_THINK = "CCBleed";

int CNPC_CloneCop::gm_nBloodAttachment = -1;
float CNPC_CloneCop::gm_flBodyRadius = 10.0f;

#define COMBINE_AE_GREN_DROP		( 9 )
#define COMBINE_AE_KICK				( 3 )

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_CloneCop )

	DEFINE_INPUT( m_ArmorValue, FIELD_INTEGER, "SetArmor" ),
	DEFINE_FIELD( m_bIsBleeding, FIELD_BOOLEAN ),

	DEFINE_INPUT( m_bThrowXenGrenades, FIELD_BOOLEAN, "SetThrowXenGrenades" ),

	// Function Pointers
	DEFINE_THINKFUNC( BleedThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_clonecop, CNPC_CloneCop );

CNPC_CloneCop::CNPC_CloneCop()
{
	// KV will override this if the NPC was spawned by Hammer
	AddGrenades( 99 );
	SetAlternateCapable( true );
	AddSpawnFlags( SF_COMBINE_REGENERATE );
	RemoveSpawnFlags( SF_COMBINE_COMMANDABLE );
	m_tEzVariant = EZ_VARIANT_RAD;
	m_ArmorValue = 200;
	m_bThrowXenGrenades = true;
	m_bLookForItems = true;
	SetDisplacementImpossible( true );
	m_SquadName = MAKE_STRING( "cc_squad" );

	// TODO - See comment in npc_combine.cpp
	// Clone Cop probably shouldn't order surrender by default
	m_iCanOrderSurrender = TRS_FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	AimGun();

	// Stronger, tougher.
	SetHealth( sk_clonecop_health.GetFloat() );
	SetMaxHealth( sk_clonecop_health.GetFloat() );
	SetKickDamage( sk_clonecop_kick.GetFloat() );

	AddEFlags( EFL_NO_DISSOLVE );

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	BaseClass::Spawn();

	if (m_tEzVariant == EZ_VARIANT_RAD || m_tEzVariant == EZ_VARIANT_TEMPORAL)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Precache()
{
	// For now, CC is always an elite
	m_fIsElite = true;

	if( !GetModelName() )
	{
		if (IsBadCop())
		{
			SetModelName( MAKE_STRING( "models/bad_cop.mdl" ) );
		}
		else
		{
			SetModelName( MAKE_STRING( "models/clone_cop.mdl" ) );
		}
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_ammo_ar2_altfire" );

	PrecacheParticleSystem( "blood_spurt_synth_01" );
	PrecacheParticleSystem( "blood_drip_synth_01" );

	if (m_tEzVariant == EZ_VARIANT_RAD || m_tEzVariant == EZ_VARIANT_TEMPORAL)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}

	if (m_bThrowXenGrenades)
	{
		VerifyXenRecipeManager( GetClassname() );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Activate()
{
	gm_nBloodAttachment = LookupAttachment( "body_blood_loc" );

	if ( IsBleeding() )
	{
		StartBleeding();
	}

	BaseClass::Activate();
}


void CNPC_CloneCop::DeathSound( const CTakeDamageInfo &info )
{
	AI_CriteriaSet set;
	ModifyOrAppendDamageCriteria(set, info);
	SpeakIfAllowed( TLK_CMB_DIE, set, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CloneCop::ClearAttackConditions()
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check the innate weapon LOS for an owner at an arbitrary position
//			If bSetConditions is true, LOS related conditions will also be set
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	bool bBase = BaseClass::WeaponLOSCondition( ownerPos, targetPos, bSetConditions );

	if (HasCondition( COND_WEAPON_SIGHT_OCCLUDED ) && bSetConditions)
	{
		// Re-do the trace, but this time from our eye position
		trace_t tr;
		AI_TraceLine( EyePosition(), targetPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		// If the end pos is within a certain number of units (or the start pos is solid), set the custom condition
		if (tr.startsolid || (EyePosition() - tr.endpos).LengthSqr() <= Square( npc_clonecop_suppress_min_occlude_dist.GetFloat() ))
		{
			SetCondition( COND_COMBINE_WEAPON_SIGHT_OCCLUDED );
		}
	}

	return bBase;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::GatherConditions()
{
	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
	
	// Health opportunities can interrupt alert schedules
	if ( GetState() == NPC_STATE_IDLE || IsCurSchedule( SCHED_ALERT_STAND ) || IsCurSchedule( SCHED_COMBINE_PATROL ) || IsCurSchedule( SCHED_INVESTIGATE_SOUND ) )
	{
		SetCustomInterruptCondition( COND_HEALTH_ITEM_AVAILABLE );
	}

	if ( IsCurSchedule( SCHED_RUN_FROM_ENEMY, true ) && IsCurSchedule( SCHED_PC_MELEE_AND_MOVE_AWAY, false ) )
	{
		// HACKHACK: CNPC_PlayerCompanion's melee code thinks the schedule is still SCHED_RUN_FROM_ENEMY, so don't let it interrupt and get stuck in a loop
		ClearCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if (!IsInAScript())
	{
		// TODO: Consider caching this
		int iNumWeapons = 0;
		int iMyWeapon = WEAPONSWITCH_COUNT;
		CHandle<CBaseCombatWeapon> pWeapons[WEAPONSWITCH_COUNT];
		for (int i=0;i<MAX_WEAPONS;i++)
		{
			if ( m_hMyWeapons[i].Get() )
			{
				iNumWeapons++;

				if (EntIsClass( m_hMyWeapons[i], gm_isz_class_Shotgun ))
				{
					pWeapons[WEAPONSWITCH_SHOTGUN] = m_hMyWeapons[i];
					if (GetActiveWeapon() == m_hMyWeapons[i].Get())
						iMyWeapon = WEAPONSWITCH_SHOTGUN;
				}
				else if (EntIsClass( m_hMyWeapons[i], gm_isz_class_AR2 ) || FClassnameIs( m_hMyWeapons[i], "weapon_ar2_proto" ))
				{
					pWeapons[WEAPONSWITCH_AR2] = m_hMyWeapons[i];
					if (GetActiveWeapon() == m_hMyWeapons[i].Get())
						iMyWeapon = WEAPONSWITCH_AR2;
				}
				else if (FClassnameIs( m_hMyWeapons[i], "weapon_crossbow" ))
				{
					pWeapons[WEAPONSWITCH_CROSSBOW] = m_hMyWeapons[i];
					if (GetActiveWeapon() == m_hMyWeapons[i].Get())
						iMyWeapon = WEAPONSWITCH_CROSSBOW;
				}
			}
		}

		// Behavior for when CC has >1 weapons
		if (iNumWeapons > 1 && iMyWeapon < WEAPONSWITCH_COUNT)
		{
			int iSwitchTo = iMyWeapon;

			// Check if enemy is too far
			if (HasCondition( COND_TOO_FAR_TO_ATTACK ))
				iSwitchTo = clamp( iSwitchTo + 1, WEAPONSWITCH_SHOTGUN, WEAPONSWITCH_CROSSBOW );
			else if (HasCondition( COND_TOO_CLOSE_TO_ATTACK ))
				iSwitchTo = clamp( iSwitchTo - 1, WEAPONSWITCH_SHOTGUN, WEAPONSWITCH_CROSSBOW );

			// Check if we have no ammo
			if ( iSwitchTo == iMyWeapon && HasCondition( COND_NO_PRIMARY_AMMO ))
				iSwitchTo = RandomInt(0, WEAPONSWITCH_COUNT-1);

			if (iSwitchTo != iMyWeapon && pWeapons[iSwitchTo].Get() != NULL)
			{
				inputdata_t inputdata;
				inputdata.value.SetString( pWeapons[iSwitchTo]->m_iClassname );
				InputChangeWeapon( inputdata );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CloneCop::SelectSchedule( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CloneCop::TranslateSchedule( int scheduleType )
{
	scheduleType = BaseClass::TranslateSchedule( scheduleType );

	switch (scheduleType)
	{
		case SCHED_COMBINE_ASSAULT:
		case SCHED_COMBINE_PRESS_ATTACK:
		case SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE:
		{
			if (!HasCondition(COND_COMBINE_CAN_ORDER_SURRENDER))
			{
				// Just suppress if we've been damaged recently and we see our enemy's last seen position
				if (gpGlobals->curtime - GetLastDamageTime() < 15.0f && GetEnemy() && CBaseCombatCharacter::FVisible(GetEnemies()->LastSeenPosition(GetEnemy())))
					return SCHED_COMBINE_MERCILESS_SUPPRESS;

				// Clone Cop attempts to flank unless he hasn't seen his enemy in a bit
				float flTimeLastSeen = GetEnemies()->LastTimeSeen(GetEnemy());
				if ( flTimeLastSeen != AI_INVALID_TIME && gpGlobals->curtime - flTimeLastSeen < 5.0f )
				{
					return SCHED_COMBINE_FLANK_LINE_OF_FIRE;
				}
			}
		} break;

		case SCHED_RANGE_ATTACK1:
		case SCHED_COMBINE_RANGE_ATTACK1:
		{
			// Don't stop firing
			return SCHED_COMBINE_MERCILESS_RANGE_ATTACK1;
		} break;

		case SCHED_COMBINE_SUPPRESS:
		case SCHED_COMBINE_SIGNAL_SUPPRESS:
		{
			// Merciless
			return SCHED_COMBINE_MERCILESS_SUPPRESS;
		} break;

		case SCHED_RUN_FROM_ENEMY:
		{
			if (HasCondition( COND_CAN_MELEE_ATTACK1 ))
			{
				return SCHED_PC_MELEE_AND_MOVE_AWAY;
			}
		} break;
	}

	return scheduleType;
}

extern ConVar sk_healthkit;
extern ConVar sk_healthvial;

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CNPC_CloneCop::PickupItem( CBaseEntity *pItem )
{
	// Overrides base class for suit sounds

	// Must cache number of elements in case any fire only once (therefore being removed after being fired)
	int iNumElements = m_OnItemPickup.NumberOfElements();
	if (iNumElements > 0)
		m_OnItemPickup.Set( pItem, pItem, this );

	Assert( pItem != NULL );
	if( FClassnameIs( pItem, "item_healthkit" ) )
	{
		if ( TakeHealth( sk_healthkit.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
			EmitSound( "HealthKit.Touch" ); // TODO: E:Z variant distinction
		}
	}
	else if( FClassnameIs( pItem, "item_healthvial" ) )
	{
		if ( TakeHealth( sk_healthvial.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
			EmitSound( "HealthKit.Touch" ); // TODO: E:Z variant distinction
		}
	}
	else if (FClassnameIs( pItem, "item_battery" ))
	{
		// Add batteries to armor value
		m_ArmorValue += 25;
		UTIL_Remove( pItem );
		EmitSound( "ItemBattery.Touch" );
	}
	else if ( FClassnameIs(pItem, "item_ammo_ar2_altfire") || FClassnameIs(pItem, "item_ammo_smg1_grenade") ||
		FClassnameIs(pItem, "weapon_frag") )
	{
		AddGrenades( 1 );
		UTIL_Remove( pItem );
	}

	// Only warn if we didn't have any elements of OnItemPickup
	else if (iNumElements <= 0)
	{
		DevMsg("%s doesn't know how to pick up %s!\n", GetClassname(), pItem->GetClassname() );
	}
}

extern ConVar sk_npc_dmg_combineball;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CloneCop::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// Take less damage from NPC balls
	if ( info.GetInflictor() && FClassnameIs(info.GetInflictor(), "prop_combine_ball") &&
			static_cast<CPropCombineBall*>(info.GetInflictor())->WasFiredByNPC() )
	{
		if (info.GetDamage() == GetMaxHealth())
			info.SetDamage( sk_npc_dmg_combineball.GetFloat() );
	}

	// Clone Cop has his own armor, similar to the player's. 
	if (info.GetDamage() && m_ArmorValue && !(info.GetDamageType() & (DMG_FALL | DMG_DROWN | DMG_POISON | DMG_RADIATION)) )// armor doesn't protect against fall or drown damage!
	{
		float flRatio = 0.2f;

		float flNew = info.GetDamage() * flRatio;

		float flArmor;

		flArmor = (info.GetDamage() - flNew);

		if( flArmor < 1.0 )
		{
			flArmor = 1.0;
		}

		// Does this use more armor than we have?
		if (flArmor > m_ArmorValue)
		{
			flArmor = m_ArmorValue;
			flNew = info.GetDamage() - flArmor;
			m_ArmorValue = 0;
		}
		else
		{
			m_ArmorValue -= flArmor;
		}
		
		info.SetDamage( flNew );
	}

	if (m_lifeState == LIFE_ALIVE)
	{
		// Spark at 30% health.
		if ( !IsBleeding() && ( GetHealth() <= GetMaxHealth() * 0.3 ) )
		{
			StartBleeding();
		}
	}

	return BaseClass::OnTakeDamage( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_CloneCop::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
	case HITGROUP_HEAD:
		{
			// Soldiers take double headshot damage
			return 2.0f;
		}
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CNPC_CloneCop::GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy )
{
	CBaseEntity *pEnemy = GetEnemy();

	if ( pEnemy )
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		Vector vecEnemyOffset;

		if (GetEnemy()->IsNPC())
		{
			float flDist = EnemyDistance( GetEnemy() );
			if (flDist < 768.0f && flDist > 32.0f)
			{
				// Aim for the head, like players do
				vecEnemyOffset = pEnemy->HeadTarget( shootOrigin ) - pEnemy->GetAbsOrigin();
			}
		}
		else
		{
			vecEnemyOffset = pEnemy->BodyTarget( shootOrigin, bNoisy ) - pEnemy->GetAbsOrigin();
		}

		Vector retval = vecEnemyOffset + vecEnemyLKP - shootOrigin;
		VectorNormalize( retval );
		return retval;
	}
	else
	{
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		return forward;
	}
}

extern ConVar ai_lead_time;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Vector CNPC_CloneCop::GetActualShootPosition( const Vector &shootOrigin )
{
	if ( GetEnemy() && GetEnemy()->IsNPC() )
	{
		float flDist = EnemyDistance( GetEnemy() );
		if (flDist < 768.0f && flDist > 32.0f)
		{
			// Aim for the head, like players do
			Vector vecEnemyLKP = GetEnemyLKP();
			Vector vecEnemyOffset = GetEnemy()->HeadTarget( shootOrigin ) - GetEnemy()->GetAbsOrigin();

			// Scale down towards the torso the closer the target is
			if (flDist < 192.0f)
			{
				vecEnemyOffset *= ( ((flDist / 192.0f) * 0.5f) + 0.5f );
			}

			Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

			// lead for some fraction of a second.
			return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
		}
	}

	return BaseClass::GetActualShootPosition( shootOrigin );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::HandleAnimEvent( animevent_t *pEvent )
{
	bool handledEvent = false;

	CBaseEntity *pHurt;
	CBaseCombatCharacter* pBCC;;
	trace_t tr;

	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		BaseClass::HandleAnimEvent( pEvent );
	}
	else
	{
		switch( pEvent->event )
		{
		case COMBINE_AE_GREN_TOSS:
			{
				Vector vecSpin;
				vecSpin.x = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.y = random->RandomFloat( -1000.0, 1000.0 );
				vecSpin.z = random->RandomFloat( -1000.0, 1000.0 );

				Vector vecStart;
				GetAttachment( "lefthand", vecStart );

				if( m_NPCState == NPC_STATE_SCRIPT )
				{
					// Use a fixed velocity for grenades thrown in scripted state.
					// Grenades thrown from a script do not count against grenades remaining for the AI to use.
					Vector forward, up, vecThrow;

					GetVectors( &forward, NULL, &up );
					vecThrow = forward * 750 + up * 175;

					CBaseEntity *pGrenade = NULL;
					if ( ShouldThrowXenGrenades() )
					{
						pGrenade = HopWire_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER );
					}
					else
					{
						pGrenade = Fraggrenade_Create( vecStart, vec3_angle, vecThrow, vecSpin, this, COMBINE_GRENADE_TIMER, true );
					}

					m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
				}
				else
				{
					// Use the Velocity that AI gave us.
					CBaseEntity *pGrenade = NULL;
					if ( ShouldThrowXenGrenades() )
					{
						pGrenade = HopWire_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER );
					}
					else
					{
						pGrenade = Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vecSpin, this, COMBINE_GRENADE_TIMER, true );
					}

					m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
					AddGrenades(-1, pGrenade);
				}

				// wait six seconds before even looking again to see if a grenade can be thrown.
				m_flNextGrenadeCheck = gpGlobals->curtime + 6;
			}
			handledEvent = true;
			break;

		case COMBINE_AE_GREN_DROP:
			{
				Vector vecStart;
				QAngle angStart;
				m_vecTossVelocity.x = 15;
				m_vecTossVelocity.y = 0;
				m_vecTossVelocity.z = 0;

				GetAttachment( "lefthand", vecStart, angStart );

				CBaseEntity *pGrenade = NULL;
				if (m_NPCState == NPC_STATE_SCRIPT)
				{
					// While scripting, have the grenade face upwards like it was originally and also don't decrement grenade count.
					pGrenade = Fraggrenade_Create( vecStart, vec3_angle, m_vecTossVelocity, vec3_origin, this, COMBINE_GRENADE_TIMER, true );
				}
				else
				{
					pGrenade = Fraggrenade_Create( vecStart, angStart, m_vecTossVelocity, vec3_origin, this, COMBINE_GRENADE_TIMER, true );
					AddGrenades(-1);
				}

				// Well, technically we're not throwing, but...still.
				m_OnThrowGrenade.Set(pGrenade, pGrenade, this);
			}
			handledEvent = true;
			break;
		case COMBINE_AE_KICK:
			// Try to dispatch a kick interaction
			pHurt = CheckTraceHullAttack( 70, -Vector( 16, 16, 18 ), Vector( 16, 16, 18 ), 0, DMG_CLUB );
			pBCC = ToBaseCombatCharacter( pHurt );
			
			if (pBCC)
			{
				CTakeDamageInfo dmgInfo( this, this, sk_clonecop_kick.GetFloat(), DMG_CLUB );

				UTIL_TraceLine( WorldSpaceCenter(), pHurt->WorldSpaceCenter(), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
				KickInfo_t kickInfo( &tr, &dmgInfo );
				if (pBCC && pBCC->DispatchInteraction( g_interactionBadCopKick, &kickInfo, this ))
				{
					// If the interaction returns true, exit
					handledEvent = true;
					break;
				}
			}
			// Fall through if the kick was unhandled
		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
		}
	}

	if( handledEvent )
	{
		m_iLastAnimEventHandled = pEvent->event;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CloneCop::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	CBaseEntity *pEnemy = GetEnemy();
	if (pEnemy)
	{
		set.AppendCriteria("enemy_visible", (FInViewCone(pEnemy) && FVisible(pEnemy)) ? "1" : "0");
	}
}

//-----------------------------------------------------------------------------
// Our health is low. Show damage effects.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::BleedThink()
{
	if (GetHealth() > GetMaxHealth() * 0.3)
	{
		// We don't need to bleed anymore
		StopBleeding();
		SetNextThink( TICK_NEVER_THINK, CC_BLEED_THINK );
		return;
	}

	// Spurt blood from random points on the hunter's head.
	Vector vecOrigin;
	QAngle angDir;
	GetAttachment( gm_nBloodAttachment, vecOrigin, angDir );
	
	Vector vecDir = RandomVector( -1, 1 );
	VectorNormalize( vecDir );
	VectorAngles( vecDir, Vector( 0, 0, 1 ), angDir );

	vecDir *= gm_flBodyRadius;
	DispatchParticleEffect( "blood_spurt_synth_01", vecOrigin + vecDir, angDir );

	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.7, 1.9 ), CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::StartBleeding()
{
	// Do this even if we're already bleeding (see OnRestore).
	m_bIsBleeding = true;

	// Start gushing blood from our... anus or something.
	DispatchParticleEffect( "blood_drip_synth_01", PATTACH_POINT_FOLLOW, this, gm_nBloodAttachment );

	// Emit spurts of our blood
	SetContextThink( &CNPC_CloneCop::BleedThink, gpGlobals->curtime + 0.1, CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::StopBleeding()
{
	m_bIsBleeding = false;

	// Not really any other way to stop the bleeding
	StopParticleEffects( this );

	SetContextThink( &CNPC_CloneCop::BleedThink, TICK_NEVER_THINK, CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::ShouldThrowXenGrenades()
{
	if (!m_bThrowXenGrenades)
		return false;

	if (GetEnemy() && !m_hForcedGrenadeTarget)
	{
		// Don't throw Xen grenades at Xen enemies (prevents Clone Cop from getting stuck in a loop)
		if (GetEnemy()->GetEZVariant() == EZ_VARIANT_XEN)
			return false;

		// If we have just one enemy (and they're not a player), only throw a Xen grenade if we've been engaged for a bit
		if (GetEnemies()->NumEnemies() == 1 && !GetEnemy()->IsPlayer())
		{
			float flTimeAtFirstHand = GetEnemies()->TimeAtFirstHand(GetEnemy());
			if ( flTimeAtFirstHand != AI_INVALID_TIME && gpGlobals->curtime - flTimeAtFirstHand < 15.0f )
			{
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::HandleManhackSpawn( CAI_BaseNPC *pNPC )
{
	CNPC_Manhack *pManhack = static_cast<CNPC_Manhack*>(pNPC);

	pManhack->TurnIntoNemesis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Event_Killed( const CTakeDamageInfo &info )
{
	if( IsElite() )
	{
		if ( HasSpawnFlags( SF_COMBINE_NO_AR2DROP ) == false )
		{
			CBaseEntity *pItem;
			if (GetActiveWeapon() && FClassnameIs(GetActiveWeapon(), "weapon_smg1"))
				pItem = DropItem( "item_ammo_smg1_grenade", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			else
				pItem = DropItem( "item_ammo_ar2_altfire", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel		= RandomVector( -64.0f, 64.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}

				if( info.GetDamageType() & DMG_DISSOLVE )
				{
					CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

					if( pAnimating )
					{
						pAnimating->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
					}
				}
				else
				{
					WeaponManager_AddManaged( pItem );
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther(pVictim, info);

	// Actually shoot at the Combine's aim target instead of just aiming
	// Helps mask some of the other stuff ported from Alyx.
	if ( GetEnemies()->NumEnemies() == 1 && GetAimTarget() )
	{
		// Don't do this against dissolve or blast damage
		if( !HasShotgun() && !(info.GetDamageType() & (DMG_DISSOLVE | DMG_BLAST)) )
		{
			CAI_BaseNPC *pTarget = GetAimTarget()->MyNPCPointer();
			if (pTarget)
			{
				AddEntityRelationship( pTarget, IRelationType(pVictim), IRelationPriority(pVictim) );

				GetEnemies()->UpdateMemory( GetNavigator()->GetNetwork(), pTarget, pTarget->GetAbsOrigin(), 0.0f, true );
				AI_EnemyInfo_t *pMemory = GetEnemies()->Find( pTarget );

				if( pMemory )
				{
					// Pretend we've known about this target longer than we really have.
					pMemory->timeFirstSeen = gpGlobals->curtime - 10.0f;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clone Cop only uses gesture flinches.
//-----------------------------------------------------------------------------
Activity CNPC_CloneCop::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	if (!bGesture)
		return ACT_INVALID;

	return BaseClass::GetFlinchActivity( bHeavyDamage, bGesture );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	return BaseClass::IsHeavyDamage( info );
}

Activity CNPC_CloneCop::NPC_TranslateActivity( Activity eNewActivity )
{
	// If there's any reason to treat this as an actual melee attack 2, do that instead. For now I see no reason not to just translate the activity
	if ( eNewActivity == ACT_MELEE_ATTACK1 )
	{
		return ACT_MELEE_ATTACK2;
	}
	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Translate base class activities
//-----------------------------------------------------------------------------
Activity CNPC_CloneCop::Weapon_TranslateActivity( Activity eNewActivity )
{
	return BaseClass::Weapon_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_CloneCop::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	// TODO: Different proficiency?
	//if( pWeapon->ClassMatches( gm_isz_class_AR2 ) )
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}
	//else if( pWeapon->ClassMatches( gm_isz_class_Shotgun ) )
	//{
	//	return WEAPON_PROFICIENCY_PERFECT;
	//}
	//else if( pWeapon->ClassMatches( gm_isz_class_SMG1 ) )
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}
	//else if (FClassnameIs( pWeapon, "weapon_smg2" ))
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Allows NPC to holster from more than just the animation event
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::DoHolster( void )
{
	if (BaseClass::DoHolster())
	{
		EmitSound( "NPC_Combine.Zipline_MidClothing" );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows NPC to unholster from more than just the animation event
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::DoUnholster( void )
{
	if (BaseClass::DoUnholster())
	{
		PlayDeploySound( GetActiveWeapon() );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ )
{
	if (BaseClass::Weapon_Switch(pWeapon, viewmodelindex))
	{
		EmitSound( "NPC_Combine.Zipline_MidClothing" );
		PlayDeploySound( pWeapon );

		if (EntIsClass( pWeapon, gm_isz_class_Shotgun ))
		{
			pWeapon->m_fMaxRange1 = MIN( 512, pWeapon->m_fMaxRange1 );
		}
		else if (EntIsClass( pWeapon, gm_isz_class_AR2 ) || FClassnameIs( pWeapon, "weapon_ar2_proto" ))
		{
			pWeapon->m_fMinRange1 = MAX( 256, pWeapon->m_fMinRange1 );
			pWeapon->m_fMaxRange1 = MIN( 1024, pWeapon->m_fMaxRange1 );
		}
		else if (FClassnameIs( pWeapon, "weapon_crossbow" ))
		{
			pWeapon->m_fMinRange1 = MAX( 768, pWeapon->m_fMinRange1 );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::PlayDeploySound( CBaseCombatWeapon *pWeapon )
{
	// Don't play sound if not in combat
	if (GetState() != NPC_STATE_COMBAT)
		return;

	if (EntIsClass( pWeapon, gm_isz_class_Shotgun ))
	{
		pWeapon->WeaponSound( SPECIAL1 );
	}
	else if (EntIsClass( pWeapon, gm_isz_class_SMG1 ))
	{
		pWeapon->WeaponSound( SPECIAL2 );
	}
	else
	{
		pWeapon->WeaponSound( RELOAD_NPC );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE = 96.0f; // How high CC can jump; Default 64
	const float MAX_JUMP_DISTANCE = 384.0f; // How far CC can jump; Default 384
	const float MAX_JUMP_DROP = 384.0f; // How far CC can fall; Default 160

	return BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.channel = 3;
	params.x = -1;
	params.y = 0.6;
	params.effect = 0;

	params.r1 = 253;
	params.g1 = 162;
	params.b1 = 2;

	return true;
}

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
LINK_ENTITY_TO_CLASS( npc_badcop, CNPC_BadCop );

CNPC_BadCop::CNPC_BadCop()
{
	m_SquadName = MAKE_STRING( "bc_squad" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_BadCop::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_BadCop::Precache()
{
	BaseClass::Precache();

	// Pretend we're the player
	AddContext( "classname", "player" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_BadCop::Activate()
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_clonecop, CNPC_CloneCop )

DECLARE_CONDITION( COND_COMBINE_WEAPON_SIGHT_OCCLUDED )

 DEFINE_SCHEDULE 
 (
	 SCHED_COMBINE_FLANK_LINE_OF_FIRE,

	 "	Tasks "
	 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_ESTABLISH_LINE_OF_FIRE"
	 "		TASK_SET_TOLERANCE_DISTANCE		48"
	 "		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	0"
	 "		TASK_COMBINE_SET_STANDING		1"
	 "		TASK_SPEAK_SENTENCE				1"
	 "		TASK_RUN_PATH					0"
	 "		TASK_WAIT_FOR_MOVEMENT			0"
	 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
	 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	 "	"
	 "	Interrupts "
	 "		COND_NEW_ENEMY"
	 "		COND_ENEMY_DEAD"
	 //"		COND_CAN_RANGE_ATTACK1"
	 //"		COND_CAN_RANGE_ATTACK2"
	 "		COND_CAN_MELEE_ATTACK1"
	 "		COND_CAN_MELEE_ATTACK2"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_HEAVY_DAMAGE"
 )

 //===============================================
 //	> RangeAttack1
 //===============================================
 DEFINE_SCHEDULE
 (
 	SCHED_COMBINE_MERCILESS_RANGE_ATTACK1,
 
 	"	Tasks"
 	"		TASK_STOP_MOVING		0"
 	"		TASK_FACE_ENEMY			0"
 	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
 	"		TASK_RANGE_ATTACK1		0"
 	""
 	"	Interrupts"
	"		COND_NEW_ENEMY"
 	"		COND_ENEMY_WENT_NULL"
 	"		COND_HEAVY_DAMAGE"
 	"		COND_ENEMY_OCCLUDED"
 	"		COND_NO_PRIMARY_AMMO"
 	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	"		COND_COMBINE_NO_FIRE"
 	"		COND_WEAPON_BLOCKED_BY_FRIEND"
 	"		COND_WEAPON_SIGHT_OCCLUDED"
 )

 DEFINE_SCHEDULE
 (
	SCHED_COMBINE_MERCILESS_SUPPRESS,

	 "	Tasks"
	 "		TASK_STOP_MOVING			0"
	 "		TASK_FACE_ENEMY				0"
	 "		TASK_COMBINE_SET_STANDING	0"
	 "		TASK_RANGE_ATTACK1			0"
	 ""
	 "	Interrupts"
	 "		COND_ENEMY_WENT_NULL"
	 "		COND_HEAVY_DAMAGE"
	 "		COND_NO_PRIMARY_AMMO"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_COMBINE_NO_FIRE"
	 "		COND_WEAPON_BLOCKED_BY_FRIEND"
	 "		COND_COMBINE_WEAPON_SIGHT_OCCLUDED"
 )

 AI_END_CUSTOM_NPC()
