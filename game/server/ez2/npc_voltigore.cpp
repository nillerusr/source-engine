//=============================================================================//
//
// Purpose: Large Race X predator that fires unstable portals
//
// Author: 1upD
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "npc_voltigore.h"
#include "movevars_shared.h"
#include "grenade_hopwire.h"
#include "hl2_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_voltigore_health( "sk_voltigore_health", "400" );
ConVar sk_voltigore_dmg_spit( "sk_voltigore_dmg_spit", "15" );
ConVar sk_voltigore_dmg_slash( "sk_voltigore_dmg_slash", "30" );
ConVar sk_voltigore_dmg_double_slash( "sk_voltigore_dmg_slash", "60" );
ConVar sk_voltigore_spit_gravity( "sk_voltigore_spit_gravity", "600" );
ConVar sk_voltigore_spit_arc_size( "sk_voltigore_spit_arc_size", "3");
ConVar sk_voltigore_always_gib( "sk_voltigore_always_gib", "0" );

LINK_ENTITY_TO_CLASS( npc_voltigore, CNPC_Voltigore );

//=========================================================
// Gonome's spit projectile
//=========================================================
class CVoltigoreProjectile : public CBaseEntity
{
	DECLARE_CLASS( CVoltigoreProjectile, CBaseEntity );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );

	static void Shoot( CBaseEntity *pOwner, Vector vecStart, Vector vecVelocity );

	DECLARE_DATADESC();

	void SetSprite( CBaseEntity *pSprite )
	{
		m_hSprite = pSprite;
	}

	CBaseEntity *GetSprite( void )
	{
		return m_hSprite.Get();
	}

	void ThinkRemove();

	void DetachSprite();

	void StopMovement();

private:
	int m_nSpriteIndex;
	EHANDLE m_hSprite;
};

LINK_ENTITY_TO_CLASS( npc_voltigore_projectile, CVoltigoreProjectile );

BEGIN_DATADESC( CVoltigoreProjectile )
DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
DEFINE_FIELD( m_nSpriteIndex, FIELD_INTEGER ),
DEFINE_THINKFUNC( ThinkRemove ),
END_DATADESC()

void CVoltigoreProjectile::Spawn( void )
{
	Precache();

	SetMoveType ( MOVETYPE_FLY );
	SetClassname( "npc_voltigore_projectile" );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_nRenderMode = kRenderTransAlpha;
	SetRenderColorA( 255 );
	SetModel( "" );

	SetRenderColor( 150, 0, 0, 255 );

	// This is a dummy model that is never used!
	UTIL_SetSize( this, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ) );

	SetCollisionGroup( HL2COLLISION_GROUP_SPIT );
}

void CVoltigoreProjectile::Shoot( CBaseEntity *pOwner, Vector vecStart, Vector vecVelocity )
{
	CVoltigoreProjectile *pSpit = CREATE_ENTITY( CVoltigoreProjectile, "npc_voltigore_projectile" );
	pSpit->Spawn();

	UTIL_SetOrigin( pSpit, vecStart );
	pSpit->SetAbsVelocity( vecVelocity );
	pSpit->SetOwnerEntity( pOwner );
	pSpit->SetRenderColor( 231, 62, 255 );

	CSprite * pSprite = CSprite::SpriteCreate( "sprites/glow01.spr", pOwner->GetAbsOrigin(), true );
	pSpit->SetSprite( pSprite );

	if ( pSprite )
	{
		pSprite->SetAttachment( pSpit, 0 );
		pSprite->SetOwnerEntity( pSpit );

		pSprite->SetColor( 231, 62, 255 );
		pSprite->SetRenderMode( kRenderWorldGlow );

		pSprite->SetScale( 2 );
	}

	pSpit->SetThink( &CVoltigoreProjectile::ThinkRemove );
	pSpit->SetNextThink( gpGlobals->curtime + 6.0f );

	CPVSFilter filter( vecStart );

	VectorNormalize( vecVelocity );
	te->SpriteSpray( filter, 0.0, &vecStart, &vecVelocity, pSpit->m_nSpriteIndex, 210, 25, 15 );


	// Make a tesla to follow the projectile
	CBaseEntity * pTesla;
	pTesla = CreateNoSpawn( "point_tesla", vecStart, pSpit->GetAbsAngles(), pSpit );

	pTesla->KeyValue( "beamcount_max", "8" );
	pTesla->KeyValue( "beamcount_min", "6" );
	pTesla->KeyValue( "interval_max", "0.75" );
	pTesla->KeyValue( "interval_min", "0.1" );
	pTesla->KeyValue( "lifetime_max", "0.3" );
	pTesla->KeyValue( "lifetime_min", "0.3" );
	pTesla->KeyValue( "thick_max", "5" );
	pTesla->KeyValue( "thick_min", "4" );
	pTesla->KeyValue( "m_Color", "231 62 255" );
	pTesla->KeyValue( "texture", "sprites/hydragutbeam.vmt" );
	pTesla->KeyValue( "m_flRadius", "434" );
	pTesla->KeyValue( "m_SoundName", "DoSpark" );
	pTesla->KeyValue( "m_bOn", "1" );

	DispatchSpawn( pTesla );
	pTesla->Activate();
	pTesla->SetAbsOrigin( pSpit->GetAbsOrigin() );
	pTesla->SetParent( pSpit );

	pTesla->AcceptInput( "TurnOn", pSpit, pSpit, variant_t(), 0 );

	CBaseEntity *pGravityController = CGravityVortexController::Create( pSpit->GetAbsOrigin(), 128.0f, 128.0f, 4.5f, pSpit );
	pGravityController->AddContext( "owner_classname:npc_pitdrone" );
	pGravityController->SetParent( pSpit );
}

void CVoltigoreProjectile::ThinkRemove()
{
	StopMovement();

	DetachSprite();

	SUB_Remove();
}

void CVoltigoreProjectile::DetachSprite()
{
	if (GetSprite() != NULL)
	{
		((CSprite *)GetSprite())->FadeAndDie( 2.0f );
		GetSprite()->SetParent( NULL );
		GetSprite()->SetOwnerEntity( NULL );
		GetSprite()->SetAbsOrigin( GetAbsOrigin() );
		SetSprite( NULL );
	}
}

void CVoltigoreProjectile::StopMovement()
{
	SetAbsVelocity( vec3_origin );

	if (GetMoveType() != MOVETYPE_NONE)
	{
		SetMoveType ( MOVETYPE_NONE );
	}
}

void CVoltigoreProjectile::Touch ( CBaseEntity *pOther )
{
	// Don't collide with the projectile's owner
	if (pOther == GetOwnerEntity())
	{
		return;
	}

	if (pOther->GetSolidFlags() & FSOLID_TRIGGER)
		return;

	if (pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT)
	{
		return;
	}

	StopMovement();
}

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Voltigore )

	DEFINE_FIELD( m_nextSoundTime,	FIELD_TIME ),

END_DATADESC()

//=========================================================
// Spawn
//=========================================================
void CNPC_Voltigore::Spawn()
{
	Precache( );

	SetModel( STRING( GetModelName() ) );

	SetHullType( HULL_LARGE );
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
	
	m_iMaxHealth		= sk_voltigore_health.GetFloat();
	m_iHealth			= m_iMaxHealth;
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	// For the purposes of melee attack conditions triggering attacks, we are treating the flying predator as though it has two melee attacks like the bullsquid.
	// In reality, the melee attack schedules will be translated to SCHED_RANGE_ATTACK_1.
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 | bits_CAP_SQUAD | bits_CAP_NO_HIT_SQUADMATES );

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	NPCInit();

	m_flDistTooFar		= 784;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Voltigore::Precache()
{

	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/voltigore.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}
	else
	{
		PrecacheParticleSystem( "blood_impact_yellow_01" );
	}

	PrecacheScriptSound( "NPC_Voltigore.Idle" );
	PrecacheScriptSound( "NPC_Voltigore.Pain" );
	PrecacheScriptSound( "NPC_Voltigore.Alert" );
	PrecacheScriptSound( "NPC_Voltigore.Death" );
	PrecacheScriptSound( "NPC_Voltigore.Attack1" );
	PrecacheScriptSound( "NPC_Voltigore.FoundEnemy" );
	PrecacheScriptSound( "NPC_Voltigore.Growl" );
	PrecacheScriptSound( "NPC_Voltigore.TailWhip");
	PrecacheScriptSound( "NPC_Voltigore.Bite" );
	PrecacheScriptSound( "NPC_Voltigore.Eat" );
	PrecacheScriptSound( "NPC_Voltigore.Explode" );

	PrecacheModel( "sprites/glow01.spr" );// spit projectile.

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Voltigore::Classify( void )
{
	return CLASS_RACE_X; 
}

//=========================================================
// IdleSound 
//=========================================================
void CNPC_Voltigore::IdleSound( void )
{
	EmitSound( "NPC_Voltigore.Idle" );
}

//=========================================================
// PainSound 
//=========================================================
void CNPC_Voltigore::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Voltigore.Pain" );
}

//=========================================================
// AlertSound
//=========================================================
void CNPC_Voltigore::AlertSound( void )
{
	EmitSound( "NPC_Voltigore.Alert" );
}

//=========================================================
// DeathSound
//=========================================================
void CNPC_Voltigore::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Voltigore.Death" );
}

//=========================================================
// AttackSound
//=========================================================
void CNPC_Voltigore::AttackSound( void )
{
	EmitSound( "NPC_Voltigore.Attack1" );
}

//=========================================================
// FoundEnemySound
//=========================================================
void CNPC_Voltigore::FoundEnemySound( void )
{
	if (gpGlobals->curtime >= m_nextSoundTime)
	{
		EmitSound( "NPC_Voltigore.FoundEnemy" );
		m_nextSoundTime	= gpGlobals->curtime + random->RandomInt( 1.5, 3.0 );
	}
}

//=========================================================
// GrowlSound
//=========================================================
void CNPC_Voltigore::GrowlSound( void )
{
	if (gpGlobals->curtime >= m_nextSoundTime)
	{
		EmitSound( "NPC_Voltigore.Growl" );
		m_nextSoundTime	= gpGlobals->curtime + random->RandomInt(1.5,3.0);
	}
}

//=========================================================
// BiteSound
//=========================================================
void CNPC_Voltigore::BiteSound( void )
{
	EmitSound( "NPC_Voltigore.Bite" );
}

//=========================================================
// EatSound
//=========================================================
void CNPC_Voltigore::EatSound( void )
{
	EmitSound( "NPC_Voltigore.Eat" );
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Voltigore::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	// Allow these schedules to stop for melee attacks
	// just like player companions
	if (IsCurSchedule( SCHED_RANGE_ATTACK1 ) )
	{
		ClearCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
		ClearCustomInterruptCondition( COND_LIGHT_DAMAGE );
		ClearCustomInterruptCondition( COND_HEAVY_DAMAGE );
	}
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_Voltigore::MaxYawSpeed( void )
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

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CNPC_Voltigore::RangeAttack1Conditions( float flDot, float flDist )
{
	// Be sure not to use ranged attacks against potential food
	if ( IsPrey( GetEnemy() ))
	{
		// Don't spit at prey - monch it!
		return ( COND_NONE );
	}

	return(BaseClass::RangeAttack1Conditions( flDot, flDist ));
}

//=========================================================
// Translate missing activities to custom ones
//=========================================================
// Shared activities from base predator
extern int ACT_EAT;
extern int ACT_EXCITED;
extern int ACT_DETECT_SCENT;
extern int ACT_INSPECT_FLOOR;

Activity CNPC_Voltigore::NPC_TranslateActivity( Activity eNewActivity )
{
	if (eNewActivity == ACT_EAT)
	{
		return (Activity)ACT_VICTORY_DANCE;
	}
	else if (eNewActivity == ACT_EXCITED)
	{
		return (Activity) ACT_IDLE;
	}
	else if ( eNewActivity == ACT_HOP )
	{
		return (Activity) ACT_BIG_FLINCH;
	}
	else if (eNewActivity == ACT_DETECT_SCENT)
	{
		return (Activity) ACT_INSPECT_FLOOR;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Voltigore::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case PREDATOR_AE_SPIT:
		{
			if ( GetEnemy() )
			{
				Vector	vecSpitOffset;
				Vector	vecSpitDir;
				Vector  vRight, vUp, vForward;

				AngleVectors ( GetAbsAngles(), &vForward, &vRight, &vUp );

				// !!!HACKHACK - the spot at which the spit originates (in front of the mouth) was measured in 3ds and hardcoded here.
				// we should be able to read the position of bones at runtime for this info.
				vecSpitOffset = ( vRight * 8 + vForward * 60 + vUp * 50 );
				vecSpitOffset = ( GetAbsOrigin() + vecSpitOffset );
				vecSpitDir = ( (GetEnemy()->BodyTarget( GetAbsOrigin() ) ) - vecSpitOffset );

				VectorNormalize( vecSpitDir );

				vecSpitDir.x += random->RandomFloat( -0.05, 0.05 );
				vecSpitDir.y += random->RandomFloat( -0.05, 0.05 );
				vecSpitDir.z += random->RandomFloat( -0.05, 0 );

				AttackSound();
				CVoltigoreProjectile::Shoot( this, vecSpitOffset, vecSpitDir * 256.0f );
			}
		}
		break;
		case PREDATOR_AE_BITE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), GetBiteDamage(), DMG_SLASH | DMG_ALWAYSGIB, 1.0f, ShouldMeleeDamageAnyNPC() );
			if (pHurt)
			{
				BiteSound(); // Only play the bite sound if we have a target

				// If the player is holding this, make sure it's dropped
				Pickup_ForcePlayerToDropThisObject( pHurt );

				Vector forward, up;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );
				pHurt->ApplyAbsVelocityImpulse( 100 * (up-forward) * GetModelScale() );
				pHurt->SetGroundEntity( NULL );
			}
		}
		break;

		case PREDATOR_AE_WHIP_SND:
		{
			EmitSound( "NPC_Voltigore.TailWhip" );
			break;
		}

		case PREDATOR_AE_TAILWHIP:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), GetWhipDamage(), DMG_SLASH | DMG_ALWAYSGIB, 1.0f, ShouldMeleeDamageAnyNPC() );
			if (pHurt)
			{
				// If the player is holding this, make sure it's dropped
				Pickup_ForcePlayerToDropThisObject( pHurt );

				Vector right, up;
				AngleVectors( GetAbsAngles(), NULL, &right, &up );

				if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
					pHurt->ViewPunch( QAngle( 20, 0, -20 ) );

				pHurt->ApplyAbsVelocityImpulse( 100 * (up+2*right) * GetModelScale() );
			}
		}
		break;

		case PREDATOR_AE_BLINK:
		{
			// Close eye. 
			// Even skins are eyes open, odd are closed
			m_nSkin = ((m_nSkin / 2) * 2) + 1;
		}
		break;

		case PREDATOR_AE_HOP:
		{
			float flGravity = GetCurrentGravity();

			// throw the squid up into the air on this frame.
			if (GetFlags() & FL_ONGROUND)
			{
				SetGroundEntity( NULL );
			}

			// jump 40 inches into the air
			Vector vecVel = GetAbsVelocity();
			vecVel.z += sqrt( flGravity * 2.0 * 40 );
			SetAbsVelocity( vecVel );
		}
		break;

		default:
			BaseClass::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Damage for projectile attack
//=========================================================
float CNPC_Voltigore::GetProjectileDamge()
{
	return sk_voltigore_dmg_spit.GetFloat();
}

//=========================================================
// Damage for claw slash attack
//=========================================================
float CNPC_Voltigore::GetBiteDamage( void )
{
	return sk_voltigore_dmg_slash.GetFloat();
}

//=========================================================
// Damage for "whip" claw attack
//=========================================================
float CNPC_Voltigore::GetWhipDamage( void )
{
	return sk_voltigore_dmg_double_slash.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
//bool CNPC_Voltigore::ShouldGib( const CTakeDamageInfo &info )
//{
//	if (sk_voltigore_always_gib.GetBool())
//		return true;
//
//	// If the damage type is "always gib", we better gib!
//	if (info.GetDamageType() & DMG_ALWAYSGIB)
//		return true;
//
//	// Babysquids gib if crushed, exploded, or overkilled
//	if (m_bIsBaby && (info.GetDamage() > GetMaxHealth() || info.GetDamageType() & DMG_CRUSH || info.GetDamageType() & DMG_BLAST))
//	{
//		return true;
//	}
//
//	return IsBoss() || BaseClass::ShouldGib( info );
//}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( NPC_Voltigore, CNPC_Voltigore )

AI_END_CUSTOM_NPC()
