//=============================================================================//
//
// Purpose: Unfortunate vortiguants that have succumbed to headcrabs
// 		either in Xen or in the temporal anomaly in the Arctic.
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "npc_zombigaunt.h"
#include "sceneentity.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_zombigaunt_health( "sk_zombigaunt_health", "150" );
ConVar sk_zombigaunt_dmg_rake( "sk_zombigaunt_dmg_rake", "15" );
ConVar sk_zombigaunt_dispel_time( "sk_zombigaunt_dispel_time", "5" );
ConVar sk_zombigaunt_health_drain_time( "sk_zombigaunt_health_drain_time", "10" );
ConVar sk_zombigaunt_dispel_radius( "sk_zombigaunt_dispel_radius", "512" );
// The range of a zombigaunt's attack is notably less than a vortigaunt's
ConVar sk_zombigaunt_zap_range( "sk_zombigaunt_zap_range", "30", FCVAR_NONE, "Range of zombie vortigaunt's ranged attack (feet)" );

// Think contexts
static const char *ZOMBIGAUNT_BLEED_THINK = "ZombigauntBleed";

extern int AE_VORTIGAUNT_CLAW_LEFT;
extern int AE_VORTIGAUNT_CLAW_RIGHT;
extern int AE_VORTIGAUNT_SWING_SOUND;
extern int AE_VORTIGAUNT_DISPEL;

#define ZOMBIE_BLOOD_LEFT_HAND		0
#define ZOMBIE_BLOOD_RIGHT_HAND		1

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Zombigaunt )

	DEFINE_FIELD( m_iszChargeResponse, FIELD_STRING ),
	DEFINE_FIELD( m_flChargeResponseEnd, FIELD_TIME ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_zombigaunt, CNPC_Zombigaunt );

//-----------------------------------------------------------------------------
// Default models by variant
//-----------------------------------------------------------------------------
const char *CNPC_Zombigaunt::pModelNames[EZ_VARIANT_COUNT] ={
	"models/zombie/zombigaunt.mdl",
	"models/zombie/xenbigaunt.mdl", // "Shackles. How long have these guys been down here?"
	"models/zombie/glowbigaunt.mdl", // "Now I've seen everything."
	"models/zombie/xenbigaunt.mdl" // No temporal zombigaunt model - right now temporal variants limited to poison headcrabs
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Spawn( void )
{
	// Default model by variant
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( pModelNames[ m_tEzVariant % EZ_VARIANT_COUNT ] ) );
	}

	// Disable back-away
	AddSpawnFlags( SF_NPC_NO_PLAYER_PUSHAWAY );

#ifdef EZ2
	// If the zap range keyvalue has not been set, default to the convar
	if (m_flZapRange == -1)
	{
		m_flZapRange = sk_zombigaunt_zap_range.GetFloat() * 12;;
	}
#endif

	BaseClass::Spawn();

	CapabilitiesAdd( bits_CAP_MOVE_JUMP );
	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK2 );

	m_iMaxHealth = sk_zombigaunt_health.GetFloat();
	m_iHealth = m_iMaxHealth;

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_ZOMBIE );
	}

	SetViewOffset( Vector ( 0, 0, 64 ) );// position of the eyes relative to monster's origin.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Precache()
{
	// Default model by variant
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( pModelNames[m_tEzVariant % EZ_VARIANT_COUNT] ) );
	}

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		PrecacheParticleSystem( "blood_drip_glowbigaunt_01" );
	}
	else
	{
		PrecacheParticleSystem( "blood_drip_zombigaunt_01" );
	}



	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::BuildScheduleTestBits( void )
{
	// Call to base
	BaseClass::BuildScheduleTestBits();

	if ( IsCurSchedule( SCHED_CHASE_ENEMY ) )
	{
		SetCustomInterruptCondition( COND_VORTIGAUNT_DISPEL_ANTLIONS );
		SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose : Translate SCHED_PC_MELEE_AND_MOVE_AWAY to SCHED_MELEE_ATTACK1
//-----------------------------------------------------------------------------
int CNPC_Zombigaunt::TranslateSchedule( int scheduleType )
{
	int schedule = BaseClass::TranslateSchedule( scheduleType );

    if ( schedule == SCHED_PC_MELEE_AND_MOVE_AWAY )
    {
		return SCHED_MELEE_ATTACK1;
    }

	return schedule;
}

//------------------------------------------------------------------------------
// Purpose : Translate some activites for the Vortigaunt
//------------------------------------------------------------------------------
Activity CNPC_Zombigaunt::NPC_TranslateActivity( Activity eNewActivity )
{
	// Zombigaunts use 'carry' activities while idle to look creepier
	if ( eNewActivity == ACT_IDLE )
		return ACT_IDLE_CARRY;
	if ( eNewActivity == ACT_WALK )
		return ACT_WALK_CARRY;

	// NOTE: This is a stand-in until the readiness system can handle non-weapon holding NPC's
	if ( eNewActivity == ACT_IDLE_CARRY )
	{
		// More than relaxed means we're stimulated
		if ( GetReadinessLevel() >= AIRL_STIMULATED )
			return ACT_IDLE_STIMULATED;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

#define HAND_BOTH	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->event == AE_VORTIGAUNT_SWING_SOUND)
	{
		// Make hands glow if not already glowing
		if ( m_fGlowAge == 0 )
		{
			StartHandGlow( VORTIGAUNT_BEAM_ZAP, HAND_BOTH );
		}
		m_fGlowAge = 1;
	}

	if (pEvent->event == AE_VORTIGAUNT_CLAW_RIGHT)
	{
		Vector right;
		AngleVectors( GetLocalAngles(), NULL, &right, NULL );
		right = right * -50;

		QAngle angle( -3, -5, -3 );

		// Find our right hand as the starting point
		Vector vecHandPos;
		QAngle vecHandAngle;
		GetAttachment( m_iRightHandAttachment, vecHandPos, vecHandAngle );
		DispelAntlions( vecHandPos, 200.0f, false ); // Dispel within a smaller radius

		ClawAttack( 64, sk_zombigaunt_dmg_rake.GetInt(), angle, right, ZOMBIE_BLOOD_RIGHT_HAND, DMG_SHOCK );
		EndHandGlow();
		return;
	}

	if (pEvent->event == AE_VORTIGAUNT_CLAW_LEFT)
	{
		Vector right;
		AngleVectors( GetLocalAngles(), NULL, &right, NULL );
		right = right * 50;
		QAngle angle( -3, 5, -3 );

		// Find our left hand as the starting point
		Vector vecHandPos;
		QAngle vecHandAngle;
		GetAttachment( m_iLeftHandAttachment, vecHandPos, vecHandAngle );
		DispelAntlions( vecHandPos, 200.0f, false ); // Dispel within a smaller radius

		ClawAttack( 64, sk_zombigaunt_dmg_rake.GetInt(), angle, right, ZOMBIE_BLOOD_LEFT_HAND, DMG_SHOCK );
		EndHandGlow();
		return;
	}

	// Kaboom!
	// Overridden for Zombigaunts because they have lower dispel range as they use dispel more frequently
	if (pEvent->event == AE_VORTIGAUNT_DISPEL)
	{
		DispelAntlions( GetAbsOrigin(), sk_zombigaunt_dispel_radius.GetFloat() );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );

}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Zombigaunt::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE		= 220.0f;
	const float MAX_JUMP_DISTANCE	= 512.0f;
	const float MAX_JUMP_DROP		= 384.0f;

	if (BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE ))
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_Zombigaunt::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	float delta = vecEnd.z - vecStart.z;

	float multiplier = 1;
	if (moveType == bits_CAP_MOVE_JUMP)
	{
		multiplier = (delta < 0) ? 0.5 : 1.5;
	}
	else if (moveType == bits_CAP_MOVE_CLIMB)
	{
		multiplier = (delta > 0) ? 0.5 : 4.0;
	}

	*pCost *= multiplier;

	return (multiplier != 1);
}

//-----------------------------------------------------------------------------
// Purpose: Overrides ai_playerally idle speech, which was heavily dependent on player presence
//-----------------------------------------------------------------------------
bool CNPC_Zombigaunt::SelectIdleSpeech( AISpeechSelection_t *pSelection )
{
	if ( !IsOkToSpeak( SPEECH_IDLE ) )
		return false;

	if ( /*ShouldSpeakRandom( TLK_IDLE, 2 ) &&*/ SelectSpeechResponse( TLK_IDLE, NULL, NULL, pSelection ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::OnStartSchedule( int scheduleType )
{
	BaseClass::OnStartSchedule( scheduleType );

	// Blixibon - Make weird motions while charging
	if (scheduleType == SCHED_CHASE_ENEMY)
	{
#ifdef NEW_RESPONSE_SYSTEM
		// We need to do this hacky stuff so the response doesn't interrupt sounds
		AI_Response response;
		bool result = SpeakFindResponse( response, TLK_VORT_CHARGE );
		if ( result )
		{
			if ( response.GetType() == ResponseRules::RESPONSE_SCENE )
			{
				char scene[256];
				response.GetResponse( scene, sizeof( scene ) );

				m_flChargeResponseEnd = PlayScene( scene, response.GetDelay(), &response );
				m_iszChargeResponse = AllocPooledString( scene );
			}
			else
			{
				SpeakDispatchResponse( TLK_VORT_CHARGE, &response );
			}
		}
#else
		// We need to do this hacky stuff so the response doesn't interrupt sounds
		AI_Response *result = GetExpresser()->SpeakFindResponse( TLK_VORT_CHARGE );
		if ( result )
		{
			if ( result->GetType() == RESPONSE_SCENE )
			{
				char response[256];
				result->GetResponse( response, sizeof( response ) );

				m_flChargeResponseEnd = PlayScene( response, result->GetDelay(), result );
				m_iszChargeResponse = AllocPooledString( response );
			}
			else
			{
				SpeakDispatchResponse( TLK_VORT_CHARGE, result );
			}
		}
#endif
	}
	else
	{
		if (gpGlobals->curtime < m_flChargeResponseEnd)
			RemoveActorFromScriptedScenes( this, true, false, STRING(m_iszChargeResponse) );
	}
}

//-----------------------------------------------------------------------------
//  Purpose: Next Vortigaunt dispel time
//-----------------------------------------------------------------------------
float CNPC_Zombigaunt::GetNextDispelTime( void )
{
	return sk_zombigaunt_dispel_time.GetFloat();
}

//-----------------------------------------------------------------------------
//		Next Zombigaunt health drain time
//-----------------------------------------------------------------------------
float CNPC_Zombigaunt::GetNextHealthDrainTime( void )
{
	return sk_zombigaunt_health_drain_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: How far away will this vortigaunt attempt a dispel attack
//-----------------------------------------------------------------------------
float CNPC_Zombigaunt::GetDispelAttackRange()
{
	// For zombigaunts, use 1/3 of the effective distance
	return sk_zombigaunt_dispel_radius.GetFloat() / 3.0f;
}

//-----------------------------------------------------------------------------
//  Purpose: Overridden to handle blood particle
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::StartEye( void )
{
	// Start blood drip particle
	if ( GetSleepState() == AISS_AWAKE )
	{
		if (m_tEzVariant == EZ_VARIANT_RAD)
		{
			DispatchParticleEffect( "blood_drip_glowbigaunt_01", PATTACH_POINT_FOLLOW, this, LookupAttachment( "mouth" ), true );
		}
		else
		{
			DispatchParticleEffect( "blood_drip_zombigaunt_01", PATTACH_POINT_FOLLOW, this, LookupAttachment( "mouth" ), true );
		}

		SetContextThink( &CNPC_Zombigaunt::BleedThink, gpGlobals->curtime + 0.1, ZOMBIGAUNT_BLEED_THINK );
	}
}

//-----------------------------------------------------------------------------
//  Purpose: Overridden to handle blood particle
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Wake( bool bFireOutput )
{
	BaseClass::Wake( bFireOutput );
	StartEye();
}

//-----------------------------------------------------------------------------
//  Purpose: Overridden to handle blood particle
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Wake( CBaseEntity * pActivator )
{
	BaseClass::Wake( pActivator );
	StartEye();
}

//-----------------------------------------------------------------------------
// Think function to reset particle
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::BleedThink()
{
	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		DispatchParticleEffect( "blood_drip_glowbigaunt_01", PATTACH_POINT_FOLLOW, this, LookupAttachment( "mouth" ), true );
	}
	else
	{
		DispatchParticleEffect( "blood_drip_zombigaunt_01", PATTACH_POINT_FOLLOW, this, LookupAttachment( "mouth" ), true );
	}

	SetNextThink( gpGlobals->curtime + random->RandomFloat( 1.0, 1.5 ), ZOMBIGAUNT_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//  Purpose: Overridden to kill all particles
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Event_Killed( const CTakeDamageInfo &info )
{
	// Stop all our thinks
	SetContextThink( NULL, 0, ZOMBIGAUNT_BLEED_THINK );

	StopParticleEffects( this );

	BaseClass::Event_Killed( info );
}