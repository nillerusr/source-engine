//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the bullsquid
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_squad.h"
#include "npcevent.h"
#include "npc_bullsquid.h"
#include "grenade_spit.h"
#include "movevars_shared.h"
#include "npc_BaseZombie.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_bullsquid_health( "sk_bullsquid_health", "100" );
ConVar sk_bullsquid_dmg_bite( "sk_bullsquid_dmg_bite", "15" );
ConVar sk_bullsquid_dmg_whip( "sk_bullsquid_dmg_whip", "25" );
ConVar sk_bullsquid_spit_arc_size( "sk_bullsquid_spit_arc_size", "3");
ConVar sk_bullsquid_spit_min_wait( "sk_bullsquid_spit_min_wait", "0.5");
ConVar sk_bullsquid_spit_max_wait( "sk_bullsquid_spit_max_wait", "5");
ConVar sk_bullsquid_spit_speed( "sk_bullsquid_spit_speed", "600" );
ConVar sk_bullsquid_gestation( "sk_bullsquid_gestation", "15.0" );
ConVar sk_bullsquid_spawn_time( "sk_bullsquid_spawn_time", "5.0" );
ConVar sk_bullsquid_monster_infighting( "sk_bullsquid_monster_infighting", "1" );
ConVar sk_bullsquid_antlion_style_spit( "sk_bullsquid_antlion_style_spit", "1" );
ConVar sk_bullsquid_lay_eggs( "sk_bullsquid_lay_eggs", "1" );
ConVar sk_bullsquid_eatincombat_percent( "sk_bullsquid_eatincombat_percent", "1.0", FCVAR_NONE, "Below what percentage of health should bullsquids eat during combat?" );
ConVar sk_max_squad_squids( "sk_max_squad_squids", "4" ); // How many squids in a pack before offspring start branching off into their own pack?

//=========================================================
// Interactions
//=========================================================
int	g_interactionBullsquidThrow		= 0;
int	g_interactionBullsquidMonch		= 0;

LINK_ENTITY_TO_CLASS( npc_bullsquid, CNPC_Bullsquid );

int ACT_SQUID_EXCITED;
int ACT_SQUID_EAT;
int ACT_SQUID_DETECT_SCENT;
int ACT_SQUID_INSPECT_FLOOR;

int AE_BULLSQUID_EAT_HEADCRAB_OFF_ZOMBIE;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Bullsquid )

	DEFINE_KEYFIELD( m_AdultModelName, FIELD_MODELNAME, "adultmodel" ),
	DEFINE_KEYFIELD( m_BabyModelName, FIELD_MODELNAME, "babymodel" ),
	DEFINE_KEYFIELD( m_EggModelName, FIELD_MODELNAME, "eggmodel" ),

END_DATADESC()

//=========================================================
// Spawn
//=========================================================
void CNPC_Bullsquid::Spawn()
{
	Precache( );

	// Baby squid do do do-do do-do
	if ( m_bIsBaby )
	{
		SetModel( STRING( m_BabyModelName ) );
		SetHullType( HULL_TINY );
	} 
	else
	{
		SetModel( STRING( m_AdultModelName ) );
		SetHullType( HULL_WIDE_SHORT );
	}

	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_GREEN );
	}
	
	SetRenderColor( 255, 255, 255, 255 );
	
	m_iMaxHealth		= sk_bullsquid_health.GetFloat();
	m_iHealth			= m_iMaxHealth;
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_MOVE_JUMP ); // THEY FLY NOW

	if (IsBoss())
	{
		// The king bullsquid is huge
		SetModelScale( 2.5 );
	}
	else if ( m_bIsBaby )
	{
		// Baby squids have 3/4 health
		m_iMaxHealth = 3 * m_iMaxHealth / 4;
		m_iHealth = m_iMaxHealth;

		// Baby squids can't spit yet!
		CapabilitiesRemove( bits_CAP_INNATE_RANGE_ATTACK1 );
	}

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	BaseClass::Spawn();

	NPCInit();

	m_flDistTooFar		= 784;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Bullsquid::Precache()
{
#ifdef EZ
	if (gm_iszGooPuddle == NULL_STRING)
		gm_iszGooPuddle = AllocPooledString( "zombie_goo_puddle" );
#endif

	if ( GetModelName() == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
			case EZ_VARIANT_XEN:
				SetModelName( AllocPooledString( "models/bullsquid_xen.mdl" ) );
				break;
			case EZ_VARIANT_RAD:
				SetModelName( AllocPooledString( "models/bullsquid_rad.mdl" ) );
				break;
			default:
				SetModelName( AllocPooledString( "models/bullsquid.mdl" ) );
				break;
		}
	}

	if ( m_AdultModelName == NULL_STRING )
	{
		m_AdultModelName = GetModelName();
	}
	else
		PrecacheModel( STRING( m_AdultModelName ) );

	// If there is no baby model name, use the same model as regular
	if ( m_BabyModelName == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
		case EZ_VARIANT_XEN:
			m_BabyModelName = AllocPooledString( "models/babysquid_xen.mdl" );
			break;
		case EZ_VARIANT_RAD:
			m_BabyModelName = AllocPooledString( "models/babysquid_rad.mdl" );
			break;
		default:
			m_BabyModelName = AllocPooledString( "models/babysquid.mdl" );
			break;
		}
	}

	// If there is no baby model name, use the same model as regular
	if ( m_EggModelName == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
		case EZ_VARIANT_XEN:
			m_EggModelName = AllocPooledString( "models/eggs/bullsquid_egg_xen.mdl" );
			break;
		case EZ_VARIANT_RAD:
			m_EggModelName = AllocPooledString( "models/eggs/bullsquid_egg_slime.mdl" );
			break;
		default:
			m_EggModelName = AllocPooledString( "models/eggs/bullsquid_egg.mdl" );
			break;
		}
	}

	PrecacheModel( STRING( GetModelName() ) );
	PrecacheModel( STRING( m_BabyModelName ) );
	PrecacheModel( STRING( m_EggModelName ) );

	m_nSquidSpitSprite = PrecacheModel("sprites/greenspit1.vmt");// client side spittle.

	UTIL_PrecacheOther( "grenade_spit" );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}
	else
	{
		PrecacheParticleSystem( "blood_impact_yellow_01" );
	}

	// Use this gib particle system to show baby squids 'molting'
	PrecacheParticleSystem( "bullsquid_explode" );

	PrecacheScriptSound( "NPC_Bullsquid.Idle" );
	PrecacheScriptSound( "NPC_Bullsquid.Pain" );
	PrecacheScriptSound( "NPC_Bullsquid.Alert" );
	PrecacheScriptSound( "NPC_Bullsquid.Death" );
	PrecacheScriptSound( "NPC_Bullsquid.Attack1" );
	PrecacheScriptSound( "NPC_Bullsquid.FoundEnemy" );
	PrecacheScriptSound( "NPC_Bullsquid.Growl" );
	PrecacheScriptSound( "NPC_Bullsquid.TailWhip");
	PrecacheScriptSound( "NPC_Bullsquid.Bite" );
	PrecacheScriptSound( "NPC_Bullsquid.Eat" );

	PrecacheScriptSound( "NPC_Babysquid.Idle" );
	PrecacheScriptSound( "NPC_Babysquid.Pain" );
	PrecacheScriptSound( "NPC_Babysquid.Alert" );
	PrecacheScriptSound( "NPC_Babysquid.Death" );
	PrecacheScriptSound( "NPC_Babysquid.Attack1" );
	PrecacheScriptSound( "NPC_Babysquid.FoundEnemy" );
	PrecacheScriptSound( "NPC_Babysquid.Growl" );
	PrecacheScriptSound( "NPC_Babysquid.TailWhip" );
	PrecacheScriptSound( "NPC_Babysquid.Bite" );
	PrecacheScriptSound( "NPC_Babysquid.Eat" );

	PrecacheScriptSound( "NPC_Antlion.PoisonShoot" );
	PrecacheScriptSound( "NPC_Antlion.PoisonBall" );
	PrecacheScriptSound( "NPC_Bullsquid.Explode" );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Bullsquid::Classify( void )
{
	return CLASS_BULLSQUID; 
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_Bullsquid::MaxYawSpeed( void )
{
	float flYS = 0;

	switch ( GetActivity() )
	{
	case	ACT_WALK:			flYS = 90;	break;
	case	ACT_RUN:			flYS = 90;	break;
	case	ACT_IDLE:			flYS = 90;	break;
	case	ACT_RANGE_ATTACK1:	flYS = 90;	break;
	default:
		flYS = 90;
		break;
	}

	return flYS;
}

extern ConVar sk_gib_carcass_smell;

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CNPC_Bullsquid::RangeAttack1Conditions( float flDot, float flDist )
{
	// If gibs can't be used as food, be sure not to use ranged attacks against headcrabs
	if ( !sk_gib_carcass_smell.GetBool() && IsPrey( GetEnemy() ))
	{
		// Don't spit at prey - monch it!
		return ( COND_NONE );
	}

	return(BaseClass::RangeAttack1Conditions( flDot, flDist ));
}

extern ConVar ai_force_serverside_ragdoll;

//=========================================================
// MeleeAttack2Conditions
//=========================================================
int CNPC_Bullsquid::MeleeAttack1Conditions( float flDot, float flDist )
{
	// Don't tail whip prey - monch it!
	if ( !ai_force_serverside_ragdoll.GetBool() && IsPrey( GetEnemy() ) )
	{
		return ( COND_NONE );
	}

	return( BaseClass::MeleeAttack1Conditions( flDot, flDist ) );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Bullsquid::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if (pEvent->event == AE_BULLSQUID_EAT_HEADCRAB_OFF_ZOMBIE)
		{
			// Remove the headcrab from the zombie interaction partner
			CNPC_BaseZombie *pInteractionPartner = dynamic_cast<CNPC_BaseZombie*>( GetInteractionPartner() );
			if ( pInteractionPartner )
			{	
				// Get head location
				Vector vecHeadcrabOrigin;
				QAngle angHeadcrabAngles;
				int iCrabAttachment = pInteractionPartner->LookupAttachment( "headcrab" );
				if (iCrabAttachment > 0)
					pInteractionPartner->GetAttachment( iCrabAttachment, vecHeadcrabOrigin, angHeadcrabAngles );
				else
					pInteractionPartner->HeadTarget( vecHeadcrabOrigin );

				// Show blood at head location
				if (UTIL_ShouldShowBlood( BLOOD_COLOR_YELLOW ))
				{
					UTIL_BloodImpact( vecHeadcrabOrigin, Vector( 0, 0, 1 ), BLOOD_COLOR_YELLOW, 1 );

					for (int i = 0; i < 3; i++)
					{
						Vector vecSpot = vecHeadcrabOrigin;

						vecSpot.x += random->RandomFloat( -8, 8 );
						vecSpot.y += random->RandomFloat( -8, 8 );
						vecSpot.z += random->RandomFloat( -8, 8 );

						UTIL_BloodDrips( vecSpot, vec3_origin, BLOOD_COLOR_YELLOW, 50 );
					}
				}

				// Remove the head
				pInteractionPartner->RemoveHead();

				// Now kill the zombie itself
				CTakeDamageInfo info = CTakeDamageInfo( this, this, pInteractionPartner->GetMaxHealth(), DMG_PREVENT_PHYSICS_FORCE );
				pInteractionPartner->TakeDamage( info );

				OnFed();
				//m_bFiredEatEvent = true;
			}
		}
		else
			BaseClass::HandleAnimEvent( pEvent );
		return;
	}

	switch( pEvent->event )
	{
		case PREDATOR_AE_SPIT:
		{
			// Reusing this activity / animation event for babysquid spawning
			if ( m_bReadyToSpawn && gpGlobals->curtime > m_flNextSpawnTime )
			{
				Vector vSpitPos;
				GetAttachment( "Mouth", vSpitPos );
				if ( SpawnNPC( vSpitPos, GetAbsAngles() ) )
				{
					ExplosionEffect();
				}
			}
			else if ( GetEnemy() )
			{
				if ( sk_bullsquid_antlion_style_spit.GetBool() ) {
					Vector vSpitPos;
					GetAttachment( "Mouth", vSpitPos );

					Vector	vTarget;

					// If our enemy is looking at us and far enough away, lead him
					if ( HasCondition( COND_ENEMY_FACING_ME ) && UTIL_DistApprox( GetAbsOrigin(), GetEnemy()->GetAbsOrigin() ) > ( 40*12 ) )
					{
						UTIL_PredictedPosition( GetEnemy(), 0.5f, &vTarget );
						vTarget.z = GetEnemy()->GetAbsOrigin().z;
					}
					else
					{
						// Otherwise he can't see us and he won't be able to dodge
						vTarget = GetEnemy()->BodyTarget( vSpitPos, true );
					}

					vTarget[2] += random->RandomFloat( 0.0f, 32.0f );

					// Try and spit at our target
					Vector	vecToss;
					if (GetSpitVector( vSpitPos, vTarget, &vecToss ) == false)
					{
						// Now try where they were
						if (GetSpitVector( vSpitPos, m_vSavePosition, &vecToss ) == false)
						{
							// Failing that, just shoot with the old velocity we calculated initially!
							vecToss = m_vecSaveSpitVelocity;
						}
					}

					// Find what our vertical theta is to estimate the time we'll impact the ground
					Vector vecToTarget = ( vTarget - vSpitPos );
					VectorNormalize( vecToTarget );
					float flVelocity = VectorNormalize( vecToss );
					float flCosTheta = DotProduct( vecToTarget, vecToss );
					float flTime = ( vSpitPos-vTarget ).Length2D() / ( flVelocity * flCosTheta );

					// Emit a sound where this is going to hit so that targets get a chance to act correctly
					CSoundEnt::InsertSound( SOUND_DANGER, vTarget, (15*12), flTime, this );

					// Don't fire again until this volley would have hit the ground (with some lag behind it)
					SetNextAttack( gpGlobals->curtime + flTime + random->RandomFloat( 0.5f, 2.0f ) );

					for (int i = 0; i < 6; i++)
					{
						CGrenadeSpit *pGrenade = ( CGrenadeSpit* )CreateEntityByName( "grenade_spit" );
						pGrenade->SetAbsOrigin( vSpitPos );
						pGrenade->SetAbsAngles( vec3_angle );
						DispatchSpawn( pGrenade );
						pGrenade->SetThrower( this );
						pGrenade->SetOwnerEntity( this );

						if (i == 0)
						{
							pGrenade->SetSpitSize( SPIT_LARGE );
							pGrenade->SetAbsVelocity( vecToss * flVelocity );
						}
						else
						{
							pGrenade->SetAbsVelocity( (vecToss + RandomVector( -0.035f, 0.035f )) * flVelocity );
							pGrenade->SetSpitSize( random->RandomInt( SPIT_SMALL, SPIT_MEDIUM ) );
						}

						// Tumble through the air
						pGrenade->SetLocalAngularVelocity(
							QAngle( random->RandomFloat( -250, -500 ),
								random->RandomFloat( -250, -500 ),
								random->RandomFloat( -250, -500 ) ) );
					}

					for ( int i = 0; i < 8; i++ )
					{
						DispatchParticleEffect( "blood_impact_yellow_01", vSpitPos + RandomVector( -12.0f, 12.0f ), RandomAngle( 0, 360 ) );
					}
					AttackSound();
				}
				else
				{
					Vector vSpitPos;

					GetAttachment( "Mouth", vSpitPos );

					Vector			vTarget = GetEnemy()->GetAbsOrigin();
					Vector			vToss;
					CBaseEntity*	pBlocker;
					float flGravity  = SPIT_GRAVITY;
					ThrowLimit( vSpitPos, vTarget, flGravity, sk_bullsquid_spit_arc_size.GetFloat(), Vector( 0, 0, 0 ), Vector( 0, 0, 0 ), GetEnemy(), &vToss, &pBlocker );

					CGrenadeSpit *pGrenade = (CGrenadeSpit*)CreateNoSpawn( "grenade_spit", vSpitPos, vec3_angle, this );
					//pGrenade->KeyValue( "velocity", vToss );
					DispatchSpawn( pGrenade );
					pGrenade->SetThrower( this );
					pGrenade->SetOwnerEntity( this );
					pGrenade->SetSpitSize( 2 );
					pGrenade->SetAbsVelocity( vToss );

					// Tumble through the air
					pGrenade->SetLocalAngularVelocity(
						QAngle(
							random->RandomFloat( -100, -500 ),
							random->RandomFloat( -100, -500 ),
							random->RandomFloat( -100, -500 )
						)
					);

					AttackSound();

					CPVSFilter filter( vSpitPos );

					// Don't show a sprite because grenade_spit now has the antlion worker model
					//te->SpriteSpray( filter, 0.0,
					//	&vSpitPos, &vToss, m_nSquidSpitSprite, 5, 10, 15 );
				}
			}
		}
		break;

		case PREDATOR_AE_BITE:
		{
			BiteAttack( 70, Vector( -16, -16, -16), Vector( 16, 16, 16 ));
		}
		break;

		case PREDATOR_AE_WHIP_SND:
		{
			EmitSound( "NPC_Bullsquid.TailWhip" );
			break;
		}

		case PREDATOR_AE_TAILWHIP:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), GetWhipDamage(), DMG_SLASH );
			if ( pHurt ) 
			{
				// TODO - This should ALWAYS create a server ragdoll regardless of the convar setting
				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
					 pHurt->ViewPunch( QAngle( 20, 0, -20 ) );
			
				if ( ShouldApplyHitVelocityToTarget( pHurt ) )
				{
					Vector right, up;
					AngleVectors( GetAbsAngles(), NULL, &right, &up );
					pHurt->ApplyAbsVelocityImpulse( 100 * (up+2*right) * GetModelScale() * (m_bIsBaby ? 0.5f : 1.0f) );
				}
			}
		}
		break;

		case PREDATOR_AE_BLINK:
		{
			// Close eye. 
			// Even skins are eyes open, odd are closed
			m_nSkin = ( (m_nSkin / 2) * 2) + 1;
		}
		break;

		case PREDATOR_AE_HOP:
		{
			float flGravity = GetCurrentGravity();

			// throw the squid up into the air on this frame.
			if ( GetFlags() & FL_ONGROUND )
			{
				SetGroundEntity( NULL );
			}

			// jump 40 inches into the air
			Vector vecVel = GetAbsVelocity();
			vecVel.z += sqrt( flGravity * 2.0 * 40 );
			SetAbsVelocity( vecVel );
		}
		break;

		case PREDATOR_AE_THROW:
			{
				// squid throws its prey IF the prey is a client. 
				CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), 0, 0 );


				if ( pHurt && !m_bIsBaby )
				{
					pHurt->ViewPunch( QAngle(20,0,-20) );
							
					// screeshake transforms the viewmodel as well as the viewangle. No problems with seeing the ends of the viewmodels.
					UTIL_ScreenShake( pHurt->GetAbsOrigin(), 25.0, 1.5, 0.7, 2, SHAKE_START );

					// If the player, throw him around
					if ( pHurt->IsPlayer())
					{
						Vector forward, up;
						AngleVectors( GetLocalAngles(), &forward, NULL, &up );
						pHurt->ApplyAbsVelocityImpulse( forward * 300 + up * 300 );
					}
					// If not the player see if has bullsquid throw interatcion
					else
					{
						CBaseCombatCharacter *pVictim = ToBaseCombatCharacter( pHurt );
						if (pVictim)
						{
							if ( pVictim->DispatchInteraction( g_interactionBullsquidThrow, NULL, this ) )
							{
								Vector forward, up;
								AngleVectors( GetLocalAngles(), &forward, NULL, &up );
								pVictim->ApplyAbsVelocityImpulse( forward * 300 + up * 250 );
							}
						}
					}
				}
			}
		break;

		default:
			BaseClass::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Bite attack
//=========================================================
CBaseEntity * CNPC_Bullsquid::BiteAttack( float flDist, const Vector & mins, const Vector & maxs )
{
	CBaseEntity *pHurt = CheckTraceHullAttack( flDist, mins, maxs, GetBiteDamage(), DMG_SLASH | DMG_ALWAYSGIB, 1.0f, ShouldMeleeDamageAnyNPC() );
	if ( pHurt )
	{
		BiteSound(); // Only play the bite sound if we have a target
		
		// If the player is holding this, make sure it's dropped
		Pickup_ForcePlayerToDropThisObject( pHurt );

		CBaseCombatCharacter *pVictim = ToBaseCombatCharacter( pHurt );
		// Try to eat the target
		if (pVictim && pVictim->DispatchInteraction( g_interactionBullsquidMonch, NULL, this ))
		{
			OnFed();
			m_flHungryTime = gpGlobals->curtime + 10.0f; // Headcrabs only satiate the squid for 10 seconds
		}
		// I don't want that!
		else if( ShouldApplyHitVelocityToTarget( pHurt ) || pHurt->GetMoveType() == MOVETYPE_FLY )
		{
			Vector forward, up;
			AngleVectors( GetAbsAngles(), &forward, NULL, &up );

			Vector vecImpulse = 100 * (up - forward);
			if (pHurt->IsNPC())
			{
				Vector vecInteractionDir;
				if ( GetNearestInteractionDir( pHurt->MyNPCPointer(), vecInteractionDir ) )
				{
					// Push the target in the direction of the interaction
					vecImpulse = vecInteractionDir + (100 * up);
				}
			}

			pHurt->ApplyAbsVelocityImpulse( vecImpulse * GetModelScale() * ( m_bIsBaby ? 0.5f : 1.0f ) );
			pHurt->SetGroundEntity( NULL );
		}
	}

	return pHurt;
}

//=========================================================
// Delays for next bullsquid spit time
//=========================================================
float CNPC_Bullsquid::GetMaxSpitWaitTime( void )
{
	return sk_bullsquid_spit_max_wait.GetFloat();
}

float CNPC_Bullsquid::GetMinSpitWaitTime( void )
{
	return sk_bullsquid_spit_max_wait.GetFloat();
}

//=========================================================
// Damage for bullsquid whip attack
//=========================================================
float CNPC_Bullsquid::GetWhipDamage( void )
{
	// Multiply the damage value by the scale of the model so that baby squids do less damage
	return sk_bullsquid_dmg_whip.GetFloat() * GetModelScale() * ( m_bIsBaby ? 0.5f : 1.0f );
}

//=========================================================
// Damage for bullsquid whip attack
//=========================================================
float CNPC_Bullsquid::GetBiteDamage( void )
{
	// Multiply the damage value by the scale of the model so that baby squids do less damage
	return sk_bullsquid_dmg_bite.GetFloat() * GetModelScale() * ( m_bIsBaby ? 0.5f : 1.0f );
}

//=========================================================
// At what percentage health should this NPC seek food?
//=========================================================
float CNPC_Bullsquid::GetEatInCombatPercentHealth( void )
{
	return sk_bullsquid_eatincombat_percent.GetFloat();
}

// Helper function from Antlion
extern Vector VecCheckThrowTolerance( CBaseEntity *pEdict, const Vector &vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance );

//-----------------------------------------------------------------------------
// Purpose: Get a toss direction that will properly lob spit to hit a target
//			Copied from antlion worker
// Input  : &vecStartPos - Where the spit will start from
//			&vecTarget - Where the spit is meant to land
//			*vecOut - The resulting vector to lob the spit
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bullsquid::GetSpitVector( const Vector &vecStartPos, const Vector &vecTarget, Vector *vecOut )
{
	// Try the most direct route
	Vector vecToss = VecCheckThrowTolerance( this, vecStartPos, vecTarget, sk_bullsquid_spit_speed.GetFloat(), (10.0f*12.0f) );

	// If this failed then try a little faster (flattens the arc)
	if ( vecToss == vec3_origin )
	{
		vecToss = VecCheckThrowTolerance( this, vecStartPos, vecTarget, sk_bullsquid_spit_speed.GetFloat() * 1.5f, (10.0f*12.0f) );
		if ( vecToss == vec3_origin )
			return false;
	}

	// Save out the result
	if ( vecOut )
	{
		*vecOut = vecToss;
	}

	return true;

}

//=========================================================
// Bullsquids have a complicated relationship check
// in order to determine when to seek out a potential mate.
//=========================================================
bool CNPC_Bullsquid::ShouldInfight( CBaseEntity * pTarget )
{
	if ( IsSameSpecies( pTarget ) )
	{
		// Bullsquids that are ready to spawn don't attack other squids, neither do baby squids
		if ( m_bReadyToSpawn || m_bIsBaby )
		{
			return false;
		}

		CNPC_Bullsquid * pOther = static_cast<CNPC_Bullsquid*>(pTarget->MyNPCPointer());
		if ( pOther )
		{
			if ( pOther->m_bReadyToSpawn || pOther->m_bIsBaby )
			{
				// Don't baby squids or squids that are ready to spawn
				return false;
			}

			// Should I try to mate?
			if ( ShouldFindMate() )
			{
				if (CanMateWithTarget( pOther, false ) )
				{
					// This squid is now marked as my mate
					m_hMate = pOther;
					m_iszMate = NULL_STRING;

					// If this mate is my squadmate, break off into a new squad for now
					if ( this->GetSquad() != NULL && pOther->GetSquad() == this->GetSquad() )
					{
						g_AI_SquadManager.CreateSquad( NULL_STRING )->AddToSquad( this );
					}

					// Attack that squid
					return true;
				}
			}

			// Don't attack squadmates
			if ( ( this->m_pSquad != NULL && this->m_pSquad == pOther->GetSquad() ) )
			{
				PredMsg( "Bullsquid '%s' wants to infight but can't fight squadmates. \n" );
				return false;
			}

			// bullsquids should attack other bullsquids who are eating! (or bullsquids they are already targeting)
			if ( sk_bullsquid_monster_infighting.GetBool() && ( HasCondition( COND_PREDATOR_SMELL_FOOD ) || pOther == GetEnemy() ) )
			{
				// Don't aggro your mate over food
				if (pOther == m_hMate)
				{
					PredMsg( "Bullsquid '%s' wants to infight but can't fight mate. \n" );
					return false;
				}

				if ( pOther->IsCurSchedule( SCHED_CHASE_ENEMY_FAILED, true ) )
				{
					PredMsg( "Bullsquid '%s' wants to infight but other is scared. \n" );
					return false;
				}

				// if this bullsquid is not my squadmate and they smell the food, I hate them
				// if ( pOther->HasCondition( COND_PREDATOR_SMELL_FOOD ) )
				{
					PredMsg( "Bullsquid '%s' fighting a rival over food. \n" );
					return true;
				}
			}
		}
	}

	return false;
}

//=========================================================
// TakeDamage - overridden for bullsquid so we can keep track
// of how much time has passed since it was last injured
//=========================================================
int CNPC_Bullsquid::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
#if 0 //Fix later.

	float flDist;
	Vector vecApex, vOffset;

	// if the squid is running, has an enemy, was hurt by the enemy, hasn't been hurt in the last 3 seconds, and isn't too close to the enemy,
	// it will swerve. (whew).
	if (GetEnemy() != NULL && IsMoving() && pevAttacker == GetEnemy() && gpGlobals->curtime - m_flLastHurtTime > 3)
	{
		flDist = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).Length2D();

		if (flDist > SQUID_SPRINT_DIST)
		{
			AI_Waypoint_t*	pRoute = GetNavigator()->GetPath()->Route();

			if (pRoute)
			{
				flDist = (GetAbsOrigin() - pRoute[pRoute->iNodeID].vecLocation).Length2D();// reusing flDist. 

				if (GetNavigator()->GetPath()->BuildTriangulationRoute( GetAbsOrigin(), pRoute[pRoute->iNodeID].vecLocation, flDist * 0.5, GetEnemy(), &vecApex, &vOffset, NAV_GROUND ))
				{
					GetNavigator()->PrependWaypoint( vecApex, bits_WP_TO_DETOUR | bits_WP_DONT_SIMPLIFY );
				}
			}
		}
	}
#endif

	if( IsSameSpecies( inputInfo.GetAttacker() ) && inputInfo.GetAttacker() != this )
	{
		// Infant squids shouldn't take damage from adults
		// There were too many accidents in testing
		if ( m_bIsBaby )
		{
			return 0;
		}

		// if attacked by another squid and capable of spawning, there is a chance to spawn offspring
		if ( m_bSpawningEnabled && ShouldFindMate() )
		{
			CNPC_Bullsquid * pMate = static_cast<CNPC_Bullsquid*>(inputInfo.GetAttacker());
			if ( CanMateWithTarget( pMate, true ) )
			{
				// Bullsquid can now spawn offspring
				m_bReadyToSpawn = true;

				// Set the next spawn time based on the gestation period of the squid
				m_flNextSpawnTime = gpGlobals->curtime + sk_bullsquid_gestation.GetFloat();

				// Set other bullsquid to be the mate
				m_hMate = pMate;
				m_iszMate = NULL_STRING;

				// The other bullsquid is now part of my squad
				if ( this->GetSquad() == NULL )
				{
					g_AI_SquadManager.CreateSquad( NULL_STRING )->AddToSquad( this );
				}
				this->GetSquad()->AddToSquad( pMate );

				// Don't take damage
				return 0;
			}
		}
	}

	return BaseClass::OnTakeDamage_Alive( inputInfo );
}

//========================================================
// RunAI - overridden for bullsquid because there are things
// that need to be checked every think.
//========================================================
void CNPC_Bullsquid::RunAI( void )
{
	// first, do base class stuff
	BaseClass::RunAI();

	if ( m_nSkin % 2 != 0 )
	{
		// Close eye if it was open.
		// Even skins are open eyes, odd are closed
		m_nSkin--; 
	}

	if ( random->RandomInt( 0,39 ) == 0 )
	{
		// Open eye
		// Even skins are open eyes, odd are closed
		m_nSkin++;
	}
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule. 
//
// Overridden for bullsquids to play specific activities
//=========================================================
void CNPC_Bullsquid::StartTask( const Task_t *pTask )
{
	CNPC_Bullsquid * newSquid = NULL;
	int maxSquidsPerSquad = sk_max_squad_squids.GetInt() + 1; // Add 1 to account for the duplicate baby and adult squids

	switch (pTask->iTask)
	{
	case TASK_PREDATOR_PLAY_EXCITE_ACT:
	{
		SetIdealActivity( (Activity)ACT_SQUID_EXCITED );
		break;
	}
	case TASK_PREDATOR_PLAY_EAT_ACT:
		EatSound();
		SetIdealActivity( (Activity)ACT_SQUID_EAT );
		break;
	case TASK_PREDATOR_PLAY_SNIFF_ACT:
		SetIdealActivity( (Activity)ACT_SQUID_DETECT_SCENT );
		break;
	case TASK_PREDATOR_PLAY_INSPECT_ACT:
		SetIdealActivity( (Activity)ACT_SQUID_INSPECT_FLOOR );
		break;
	case TASK_PREDATOR_GROW:
		// Temporarily become non-solid so the new NPC can spawn
		SetSolid( SOLID_NONE );

		// Spawn an adult squid
	    newSquid = SpawnLive( GetAbsOrigin() + Vector( 0, 0, 32 ), false );

		SetSolid( SOLID_BBOX );

		if ( newSquid == NULL )
		{
			TaskFail( FAIL_BAD_POSITION );
			break;
		}

		newSquid->ExplosionEffect();

		// Split off into a new squad if there are already max squids
		if ( this->GetSquad() != NULL && this->GetSquad()->NumMembers() > maxSquidsPerSquad)
		{
			g_AI_SquadManager.CreateSquad( NULL_STRING )->AddToSquad( newSquid );
		}

		// Delete baby squid
		SUB_Remove();

		// Task should end after scale is applied
		TaskComplete();
		break;
	case TASK_PREDATOR_SPAWN:
		SetIdealActivity( (Activity) ACT_RANGE_ATTACK1 );
		break;
	default:
	{
		BaseClass::StartTask( pTask );
		break;
	}
	}
}

//=========================================================
// GetSchedule 
//=========================================================
int CNPC_Bullsquid::SelectSchedule( void )
{
	if( HasCondition( COND_PREDATOR_CAN_GROW ) && !HasCondition( COND_PREDATOR_GROWTH_INVALID ) )
	{	
		if ( GetState() == NPC_STATE_COMBAT && GetEnemy() && !HasMemory( bits_MEMORY_INCOVER ) )
		{
			PredMsg( "Babysquid '%s' wants to grow, but is in combat. Take cover! \n" );
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}

		return SCHED_PREDATOR_GROW;
	}

	return BaseClass::SelectSchedule();
}

int CNPC_Bullsquid::TranslateSchedule( int scheduleType )
{
	// For some reason, bullsquids chasing one another get frozen.
	if ( scheduleType == SCHED_CHASE_ENEMY && IsSameSpecies( GetEnemy() ) )
	{
		return SCHED_ESTABLISH_LINE_OF_FIRE;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

// Shared activities from base predator
extern int ACT_EAT;
extern int ACT_EXCITED;
extern int ACT_DETECT_SCENT;
extern int ACT_INSPECT_FLOOR;

//=========================================================
// Translate missing activities to custom ones
//=========================================================
Activity CNPC_Bullsquid::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_EAT )
	{
		return (Activity) ACT_SQUID_EAT;
	}
	else if ( eNewActivity == ACT_EXCITED )
	{
		return (Activity) ACT_SQUID_EXCITED;
	}
	else if ( eNewActivity == ACT_DETECT_SCENT )
	{
		return (Activity) ACT_SQUID_DETECT_SCENT;
	}
	else if ( eNewActivity == ACT_INSPECT_FLOOR )
	{
		return (Activity) ACT_SQUID_INSPECT_FLOOR;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bullsquid::ShouldGib( const CTakeDamageInfo &info )
{
	// If the damage type is "always gib", we better gib!
	if ( info.GetDamageType() & DMG_ALWAYSGIB )
		return true;

	// Babysquids gib if crushed, exploded, or overkilled
	if ( m_bIsBaby && ( info.GetDamage() > GetMaxHealth() || info.GetDamageType() & DMG_CRUSH || info.GetDamageType() & DMG_BLAST ) )
	{
		return true;
	}

	return IsBoss() || BaseClass::ShouldGib( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bullsquid::CorpseGib( const CTakeDamageInfo &info )
{
	ExplosionEffect();

	// TODO bullsquids might need unique gibs - especially for babies and the bullsquid king, since they are off scale
	return BaseClass::CorpseGib( info );
}

void CNPC_Bullsquid::ExplosionEffect( void )
{
	DispatchParticleEffect( "bullsquid_explode", WorldSpaceCenter(), GetAbsAngles() );
	EmitSound( "NPC_Bullsquid.Explode" );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new baby bullsquid
// Output : True if the new bullsquid is created
//-----------------------------------------------------------------------------
bool CNPC_Bullsquid::SpawnNPC( const Vector position, const QAngle angle )
{
	CAI_BaseNPC * spawned;

	if (sk_bullsquid_lay_eggs.GetBool())
	{
		spawned = SpawnEgg( position, angle );
	}
	else
	{
		spawned = SpawnLive( position, true );
	}

	if ( spawned != NULL )
	{
		EndSpawnSound();
		m_flNextSpawnTime = gpGlobals->curtime + sk_bullsquid_spawn_time.GetFloat();
		return true;
    }

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new bullsquid egg
// Output : True if the new egg is created
//-----------------------------------------------------------------------------
CNPC_Egg * CNPC_Bullsquid::SpawnEgg( const Vector position, const QAngle angle )
{
	// Try to create entity
	CNPC_Egg *pEgg = static_cast< CNPC_Egg * >(CreateEntityByName( "npc_egg" ));
	if ( pEgg )
	{
		pEgg->m_tEzVariant = this->m_tEzVariant;
		pEgg->KeyValue( "AlertSound", "npc_bullsquid.egg_alert" );
		pEgg->KeyValue( "ChildClassname", "npc_bullsquid" );
		pEgg->KeyValue( "HatchParticle", "bullsquid_egg_hatch" );
		pEgg->KeyValue( "IncubationTime", "30" );
		pEgg->KeyValue( "health", "200" );
		pEgg->KeyValue( "HatchSound", "npc_bullsquid.egg_hatch" );
		pEgg->KeyValue( "babymodel", STRING( m_BabyModelName ) );
		pEgg->SetModel( STRING( m_EggModelName ) );
		pEgg->Precache();

		DispatchSpawn( pEgg );

		// Now attempt to drop into the world
		pEgg->Teleport( &position, NULL, NULL );
		pEgg->SetAbsAngles( angle );

		Vector forward, up;
		AngleVectors( GetLocalAngles(), &forward, NULL, &up );
		pEgg->ApplyAbsVelocityImpulse( forward * 300 + up * 300 );

		// TODO Validate that this location is not inside a solid object or through a wall!

		pEgg->SetSquad( this->GetSquad() );
		pEgg->Activate();

		// Decrement feeding counter
		m_iTimesFed--;
		if (m_iTimesFed <= 0)
		{
			m_bReadyToSpawn = false;
		}

		// Fire output
		variant_t value;
		value.SetEntity( pEgg );
		m_OnSpawnNPC.CBaseEntityOutput::FireOutput( value, this, this );
	}

	// Returns NULL if the egg could not be instantiated
	return pEgg;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new baby bullsquid
// Output : True if the new bullsquid is created
//-----------------------------------------------------------------------------
CNPC_Bullsquid * CNPC_Bullsquid::SpawnLive( const Vector position, bool isBaby )
{

	// Try to create entity
	CNPC_Bullsquid *pChild = dynamic_cast< CNPC_Bullsquid * >(CreateEntityByName( "npc_bullsquid" ));
	if ( pChild )
	{
		pChild->m_bIsBaby = isBaby;
		pChild->m_tEzVariant = this->m_tEzVariant;
		pChild->m_tWanderState = this->m_tWanderState;
		pChild->m_bSpawningEnabled = true;
		pChild->m_nSkin = this->m_nSkin;
		pChild->KeyValue( "eggmodel", STRING( m_EggModelName ) );
		pChild->KeyValue( "babymodel", STRING( m_BabyModelName ) );
		pChild->KeyValue( "adultmodel", STRING( m_AdultModelName ) );
		pChild->Precache();

		DispatchSpawn( pChild );

		// Now attempt to drop into the world
		pChild->Teleport( &position, NULL, NULL );
		UTIL_DropToFloor( pChild, MASK_NPCSOLID );

		// Now check that this is a valid location for the new npc to be
		Vector	vUpBit = pChild->GetAbsOrigin();
		vUpBit.z += 1;

		trace_t tr;
		AI_TraceHull( pChild->GetAbsOrigin(), vUpBit, pChild->GetHullMins(), pChild->GetHullMaxs(),
			MASK_NPCSOLID, pChild, COLLISION_GROUP_NONE, &tr );
		if (tr.startsolid || (tr.fraction < 1.0))
		{
			pChild->SUB_Remove();
			DevMsg( "Can't create squid. Bad Position!\n" );
			return NULL;
		}

		pChild->SetSquad( this->GetSquad() );
		pChild->Activate();

		// Decrement feeding counter
		m_iTimesFed--;
		if ( m_iTimesFed <= 0 )
		{
			m_bReadyToSpawn = false;
		}

		// Fire output
		variant_t value;
		value.SetEntity( pChild );
		m_OnSpawnNPC.CBaseEntityOutput::FireOutput( value, this, this );
	}

	// Returns NULL if the child could not be instantiated
	return pChild;
}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_bullsquid, CNPC_Bullsquid )

	DECLARE_ACTIVITY( ACT_SQUID_EAT )
	DECLARE_ACTIVITY( ACT_SQUID_EXCITED )
	DECLARE_ACTIVITY( ACT_SQUID_DETECT_SCENT )
	DECLARE_ACTIVITY( ACT_SQUID_INSPECT_FLOOR )

	DECLARE_ANIMEVENT( AE_BULLSQUID_EAT_HEADCRAB_OFF_ZOMBIE )

	DECLARE_INTERACTION( g_interactionBullsquidThrow )
	DECLARE_INTERACTION( g_interactionBullsquidMonch )

AI_END_CUSTOM_NPC()
