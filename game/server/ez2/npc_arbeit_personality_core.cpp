//=============================================================================//
//
// Purpose:		Portal 2 Personality Core NPC created from scratch in Source 2013
//				to fit into the world and mechanics of Half-Life 2. It has the same
//				response and choreography abilities as other ally NPCs and is based
//				largely on the Wilson NPC.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_arbeit_personality_core.h"
#include "filters.h"
#include "props.h"
#include "particle_parse.h"
#include "npcevent.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Activity ACT_CORE_GESTURE_FLOOR_IDLE;
Activity ACT_CORE_PLUG_ATTACH;
Activity ACT_CORE_PLUG_DETACH;
Activity ACT_CORE_PLUG_LOCK;

int AE_CORE_SPARK;

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_arbeit_personality_core, CNPC_Arbeit_PersonalityCore );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Arbeit_PersonalityCore )

	DEFINE_FIELD( m_nPanicLevel, FIELD_INTEGER ),

	DEFINE_FIELD( m_flCarryTime, FIELD_TIME ),
	DEFINE_FIELD( m_bUseCarryAngles, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flPlayerDropTime, FIELD_TIME ),
	DEFINE_FIELD( m_hPhysicsAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastPhysicsInfluenceTime, FIELD_TIME ),

	DEFINE_KEYFIELD( m_bMotionDisabled, FIELD_BOOLEAN, "motiondisabled" ),

	DEFINE_KEYFIELD( m_iszCustomIdleSequence, FIELD_STRING, "CustomIdleSequence" ),
	DEFINE_KEYFIELD( m_iszCustomAlertSequence, FIELD_STRING, "CustomAlertSequence" ),
	DEFINE_KEYFIELD( m_iszCustomStareSequence, FIELD_STRING, "CustomStareSequence" ),
	DEFINE_KEYFIELD( m_iszCustomPanicSequence, FIELD_STRING, "CustomPanicSequence" ),

	DEFINE_INPUT( m_bCanBeEnemy, FIELD_BOOLEAN, "SetCanBeEnemy" ),

	DEFINE_USEFUNC( Use ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Explode", InputExplode ),
	
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableMotion", InputEnableMotion ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableMotion", InputDisableMotion ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ForcePickup", InputForcePickup ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePickup", InputEnablePickup ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePickup", InputDisablePickup ),

	DEFINE_INPUTFUNC( FIELD_VOID, "PlayAttach", InputPlayAttach ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlayDetach", InputPlayDetach ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PlayLock", InputPlayLock ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomIdleSequence", InputSetCustomIdleSequence ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomAlertSequence", InputSetCustomAlertSequence ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomStareSequence", InputSetCustomStareSequence ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetCustomPanicSequence", InputSetCustomPanicSequence ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCustomIdleSequence", InputClearCustomIdleSequence ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCustomAlertSequence", InputClearCustomAlertSequence ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCustomStareSequence", InputClearCustomStareSequence ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCustomPanicSequence", InputClearCustomPanicSequence ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetUprightPose", InputSetUprightPose ),

	DEFINE_OUTPUT( m_OnPlayerUse, "OnPlayerUse" ),
	DEFINE_OUTPUT( m_OnPlayerPickup, "OnPlayerPickup" ),
	DEFINE_OUTPUT( m_OnPlayerDrop, "OnPlayerDrop" ),
	DEFINE_OUTPUT( m_OnDestroyed, "OnDestroyed" ),

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
//IMPLEMENT_SERVERCLASS_ST( CNPC_Arbeit_PersonalityCore, DT_NPC_Arbeit_PersonalityCore )

//END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_Arbeit_PersonalityCore::CNPC_Arbeit_PersonalityCore()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Spawn( void )
{
	Precache();

	SetModel( STRING( GetModelName() ) );

	SetHullType( HULL_TINY_CENTERED );

	BaseClass::Spawn();

	m_flFieldOfView = 0.5f;
	
	SetViewOffset( EyeOffset( ACT_IDLE ) );
	m_takedamage	= DAMAGE_EVENTS_ONLY;
	m_iHealth		= 100;
	m_iMaxHealth	= 100;

	AddEFlags( EFL_NO_DISSOLVE );

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD );

	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Activate( void )
{
	BaseClass::Activate();

	m_nPoseUpright = LookupPoseParameter( "upright_pose" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( MAKE_STRING( "models/npcs/personality_sphere/personality_sphere_arbeit.mdl" ) );
	}

	PropBreakablePrecacheAll( GetModelName() );
	PrecacheParticleSystem( "explosion_turret_break" );

	PrecacheScriptSound( "Sphere.Destruct" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------

bool CNPC_Arbeit_PersonalityCore::CreateVPhysics( void )
{
	//Spawn our physics hull
	IPhysicsObject *pPhys = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	if ( pPhys == NULL )
	{
		DevMsg( "%s unable to spawn physics object!\n", GetDebugName() );
		return false;
	}

	if ( m_bMotionDisabled )
	{
		pPhys->EnableMotion( false );
		PhysSetGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
unsigned int CNPC_Arbeit_PersonalityCore::PhysicsSolidMaskForEntity( void ) const
{
	// Cores should ignore NPC clips
	return BaseClass::PhysicsSolidMaskForEntity() & ~CONTENTS_MONSTERCLIP;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Touch( CBaseEntity *pOther )
{
	// Do this dodgy trick to avoid TestPlayerPushing in CAI_PlayerAlly.
	CAI_BaseActor::Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_OnPlayerUse.FireOutput( pActivator, this );

	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		if (!m_bMotionDisabled)
			pPlayer->PickupObject( this, true );

		SpeakIfAllowed( TLK_USE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
int CNPC_Arbeit_PersonalityCore::OnTakeDamage( const CTakeDamageInfo &info )
{
	CTakeDamageInfo	newInfo = info;

	if ( info.GetDamageType() & (DMG_SLASH|DMG_CLUB) )
	{
		// Take extra force from melee hits
		newInfo.ScaleDamageForce( 2.0f );
	}
	else if ( info.GetDamageType() & DMG_BLAST )
	{
		newInfo.ScaleDamageForce( 2.0f );
	}
	else if ( (info.GetDamageType() & DMG_BULLET) && !(info.GetDamageType() & DMG_BUCKSHOT) )
	{
		// Bullets, but not buckshot, do extra push
		newInfo.ScaleDamageForce( 2.5f );
	}

	// Manually apply vphysics because AI_BaseNPC takedamage doesn't call back to CBaseEntity OnTakeDamage
	VPhysicsTakeDamage( newInfo );

	// Respond to damage
	bool bTriggerHurt = info.GetInflictor() && info.GetInflictor()->ClassMatches("trigger_hurt");
	if ( !(info.GetDamageType() & DMG_RADIATION) || bTriggerHurt )
	{
		AI_CriteriaSet modifiers;
		ModifyOrAppendDamageCriteria(modifiers, info);
		SpeakIfAllowed( TLK_WOUND, modifiers );
	}

	if (bTriggerHurt || (m_hDamageFilter && static_cast<CBaseFilter*>(m_hDamageFilter.Get())->PassesDamageFilter(info)))
	{
		// Slowly take damage from trigger_hurts! (or anyone who passes our damage filter if we have one)
		m_iHealth -= info.GetDamage();

		// See this method's declaration comment for more info
		SetLastDamageTime(gpGlobals->curtime);

		if (m_iHealth <= 0)
		{
			Event_Killed( info );
			return 0;
		}
	}

	/*if (info.GetDamageType() & (DMG_SHOCK | DMG_ENERGYBEAM) && info.GetDamage() >= WILSON_TESLA_DAMAGE_THRESHOLD)
	{
		// Get a tesla effect on our hitboxes for a little while.
		SetContextThink( &CNPC_Wilson::TeslaThink, gpGlobals->curtime, g_DamageZapContext );
		m_flTeslaStopTime = gpGlobals->curtime + 2.0f;
	}*/

	return BaseClass::OnTakeDamage(info);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::Event_Killed( const CTakeDamageInfo &info )
{
	if (info.GetAttacker())
	{
		info.GetAttacker()->Event_KilledOther(this, info);
	}

	if ( CBasePlayer *pPlayer = UTIL_GetLocalPlayer() )
	{
		pPlayer->Event_NPCKilled(this, info);
	}

	SetState( NPC_STATE_DEAD );

	m_OnDestroyed.FireOutput( info.GetAttacker(), this );

	// Explode
	Vector vecUp;
	GetVectors( NULL, NULL, &vecUp );
	Vector vecOrigin = WorldSpaceCenter() + ( vecUp * 12.0f );

	// Our effect
	DispatchParticleEffect( "explosion_turret_break", vecOrigin, GetAbsAngles() );

	// Ka-boom!
	RadiusDamage( CTakeDamageInfo( this, info.GetAttacker(), 25.0f, DMG_BLAST ), vecOrigin, (10*12), CLASS_NONE, this );

	EmitSound( "Sphere.Destruct" );

	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), vec3_origin, RandomAngularImpulse( -800.0f, 800.0f ) );
	params.impactEnergyScale = 1.0f;
	params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;

	// no damage/damage force? set a burst of 100 for some movement
	params.defBurstScale = 100;
	PropBreakableCreateAll( GetModelIndex(), VPhysicsGetObject(), params, this, -1, true );

	// Throw out some small chunks too obscure the explosion even more
	CPVSFilter filter( vecOrigin );
	for ( int i = 0; i < 4; i++ )
	{
		Vector gibVelocity = RandomVector(-100,100);
		int iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( "MetalChunks" ) );	
		te->BreakModel( filter, 0.0, vecOrigin, GetAbsAngles(), Vector(40,40,40), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL );
	}

	// We're done!
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Arbeit_PersonalityCore::TranslateCustomizableActivity( const string_t &iszSequence, Activity actDefault )
{
	if (iszSequence != NULL_STRING)
	{
		m_iszSceneCustomMoveSeq = iszSequence;
		return ACT_SCRIPT_CUSTOM_MOVE;
	}
	else
		return actDefault;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Arbeit_PersonalityCore::NPC_TranslateActivity( Activity activity )
{
	switch (m_NPCState)
	{
		case NPC_STATE_IDLE:
			{
				activity = TranslateCustomizableActivity( m_iszCustomIdleSequence, activity );
			}
			break;

		case NPC_STATE_ALERT:
			{
				if (m_nPanicLevel >= 2)
				{
					activity = TranslateCustomizableActivity( m_iszCustomPanicSequence, ACT_IDLE_AGITATED );
				}

				activity = TranslateCustomizableActivity( m_iszCustomAlertSequence, ACT_IDLE_STIMULATED );
			}
			break;

		case NPC_STATE_COMBAT:
			{
				if (m_nPanicLevel >= 2)
				{
					activity = TranslateCustomizableActivity( m_iszCustomPanicSequence, ACT_IDLE_AGITATED );
				}
				else if (!IsHeldByPhyscannon())
				{
					// Stare at our enemy/don't move
					activity = TranslateCustomizableActivity( m_iszCustomStareSequence, ACT_IDLE_STEALTH );
				}
				else
				{
					activity = TranslateCustomizableActivity( m_iszCustomAlertSequence, ACT_IDLE_STIMULATED );
				}
			}
			break;
	}

	return BaseClass::NPC_TranslateActivity( activity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Arbeit_PersonalityCore::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	return BaseClass::GetFlinchActivity( bHeavyDamage, bGesture );
}

//-----------------------------------------------------------------------------
// Purpose: Handles custom combat speech stuff ported from Alyx.
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::DoCustomCombatAI( void )
{
	#define COMPANION_MIN_MOB_DIST_SQR Square(120)		// Any enemy closer than this adds to the 'mob'
	#define COMPANION_MIN_CONSIDER_DIST	Square(1200)	// Only enemies within this range are counted and considered to generate AI speech

	AIEnemiesIter_t iter;

	float visibleEnemiesScore = 0.0f;
	float closeEnemiesScore = 0.0f;

	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if ( IRelationType( pEMemory->hEnemy ) != D_NU && IRelationType( pEMemory->hEnemy ) != D_LI && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= COMPANION_MIN_CONSIDER_DIST )
		{
			if( pEMemory->hEnemy && pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 0.5f && pEMemory->hEnemy->Classify() != CLASS_BULLSEYE )
			{
				if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= COMPANION_MIN_MOB_DIST_SQR )
				{
					closeEnemiesScore += 1.0f;
				}
				else
				{
					visibleEnemiesScore += 1.0f;
				}
			}
		}
	}

	if( closeEnemiesScore > 2 )
	{
		SetCondition( COND_MOBBED_BY_ENEMIES );

		// mark anyone in the mob as having mobbed me
		for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
		{
			if ( pEMemory->bMobbedMe )
				continue;

			if ( IRelationType( pEMemory->hEnemy ) != D_NU && IRelationType( pEMemory->hEnemy ) != D_LI && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= COMPANION_MIN_CONSIDER_DIST )
			{
				if( pEMemory->hEnemy && pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 0.5f && pEMemory->hEnemy->Classify() != CLASS_BULLSEYE )
				{
					if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= COMPANION_MIN_MOB_DIST_SQR )
					{
						pEMemory->bMobbedMe = true;
					}
				}
			}
		}
	}
	else
	{
		ClearCondition( COND_MOBBED_BY_ENEMIES );
	}

	// Say a combat thing
	if( HasCondition( COND_MOBBED_BY_ENEMIES ) )
	{
		SpeakIfAllowed( TLK_MOBBED );
	}
	else if( visibleEnemiesScore > 4 )
	{
		SpeakIfAllowed( TLK_MANY_ENEMIES );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::OnSeeEntity( CBaseEntity *pEntity )
{
	BaseClass::OnSeeEntity(pEntity);

	if ( pEntity->IsPlayer() )
	{
		if ( pEntity->IsEFlagSet(EFL_IS_BEING_LIFTED_BY_BARNACLE) )
		{
			SetSpeechTarget( pEntity );
			SpeakIfAllowed( TLK_ALLY_IN_BARNACLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttackerEnt )
{
	BaseClass::OnFriendDamaged( pSquadmate, pAttackerEnt );

	CAI_BaseNPC *pAttacker = pAttackerEnt->MyNPCPointer();
	if ( pAttacker )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		if ( pPlayer && ( pPlayer->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D() ).LengthSqr() < Square( 25*12 ) && IsAllowedToSpeak( TLK_WATCHOUT ) )
		{
			if ( !pPlayer->FInViewCone( pAttacker ) )
			{
				Vector2D vPlayerDir = pPlayer->EyeDirection2D().AsVector2D();
				Vector2D vEnemyDir = pAttacker->EyePosition().AsVector2D() - pPlayer->EyePosition().AsVector2D();
				vEnemyDir.NormalizeInPlace();
				float dot = vPlayerDir.Dot( vEnemyDir );
				if ( dot < 0 )
					Speak( TLK_WATCHOUT, "dangerloc:behind" );
				else if ( ( pPlayer->GetAbsOrigin().AsVector2D() - pAttacker->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square( 40*12 ) )
					Speak( TLK_WATCHOUT, "dangerloc:far" );
			}
			else if ( pAttacker->GetAbsOrigin().z - pPlayer->GetAbsOrigin().z > 128 )
			{
				Speak( TLK_WATCHOUT, "dangerloc:above" );
			}
			else if ( pAttacker->GetHullType() <= HULL_TINY && ( pPlayer->GetAbsOrigin().AsVector2D() - pAttacker->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square( 100*12 ) )
			{
				Speak( TLK_WATCHOUT, "dangerloc:far" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Arbeit_PersonalityCore::IsValidEnemy( CBaseEntity *pEnemy )
{
	if (!BaseClass::IsValidEnemy( pEnemy ))
		return false;

	// HACKHACK: Just completely ignore bullseyes for now
	if (pEnemy->Classify() == CLASS_BULLSEYE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Arbeit_PersonalityCore::CanBeAnEnemyOf( CBaseEntity *pEnemy )
{ 
	// Don't be anyone's enemy unless it's needed for something
	if (!m_bCanBeEnemy)
	{
		return false;
	}

	return BaseClass::CanBeAnEnemyOf(pEnemy);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::UpdatePanicLevel()
{
	int nPanicLevel = 0;

	if (gpGlobals->curtime - GetLastDamageTime() <= 5.0f)
		nPanicLevel++;

	if (HasCondition( COND_HEAR_DANGER ))
		nPanicLevel += 2;

	if (GetEnemy())
	{
		nPanicLevel++;

		if (EnemyDistance( GetEnemy() ) <= 96.0f)
			nPanicLevel++;
	}

	if (nPanicLevel != m_nPanicLevel)
	{
		SetCondition( COND_PERSONALITYCORE_PANIC_LEVEL_CHANGE );
	}

	m_nPanicLevel = nPanicLevel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::GatherConditions()
{
	BaseClass::GatherConditions();

	if (GetState() == NPC_STATE_COMBAT)
	{
		UpdatePanicLevel();
	}
	else
	{
		m_nPanicLevel = 0;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if (!m_bMotionDisabled)
	{
		// Check the floor layer
		int iIndex = FindGestureLayer( ACT_CORE_GESTURE_FLOOR_IDLE );
		if (iIndex == -1)
		{
			iIndex = AddGesture( ACT_CORE_GESTURE_FLOOR_IDLE );
			SetLayerLooping( iIndex, true );

			SetLayerBlendIn( iIndex, 0.1f );
			SetLayerBlendOut( iIndex, 0.1f );
		}

		// HACKHACK: Refresh the cycle each think
		if (GetLayerCycle(iIndex) >= 0.5f)
			SetLayerCycle( iIndex, 0.5f );
	}
	else
	{
		// Remove the floor layer
		int iIndex = FindGestureLayer( ACT_CORE_GESTURE_FLOOR_IDLE );
		if (iIndex != -1)
		{
			if (GetLayerCycle( iIndex ) < 0.89f)
			{
				SetLayerCycle( iIndex, 0.89f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Arbeit_PersonalityCore::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_ALERT_STAND:
			return SCHED_PERSONALITYCORE_ALERT_STAND;
			break;
		case SCHED_COMBAT_STAND:
			return SCHED_PERSONALITYCORE_COMBAT_STAND;
			break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::StartTask( const Task_t *pTask )
{
	//switch ( pTask->iTask )
	//{
		// UNDONE: CAI_SensingDummy overrides these tasks, but we can run them if our motion is disabled
		/*case TASK_FACE_HINTNODE:
		case TASK_FACE_LASTPOSITION:
		case TASK_FACE_SAVEPOSITION:
		case TASK_FACE_AWAY_FROM_SAVEPOSITION:
		case TASK_FACE_TARGET:
		case TASK_FACE_IDEAL:
		case TASK_FACE_SCRIPT:
		case TASK_FACE_PATH:
		case TASK_FACE_REASONABLE:
			{
				if (m_bMotionDisabled)
				{
					CAI_PlayerAlly::StartTask( pTask );
					return;
				}
			}*/
	//}

	BaseClass::StartTask(pTask);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_CORE_SPARK )
	{
		Vector vecSparkPosition, vecSparkDir;

		if (pEvent->options && *pEvent->options)
		{
			// Use attachment position
			int nAttachment = LookupAttachment( pEvent->options );
			if (nAttachment != -1)
				GetAttachment( nAttachment, vecSparkPosition, &vecSparkDir );
		}
		else
		{
			vecSparkPosition = WorldSpaceCenter();
			GetVectors( NULL, NULL, &vecSparkDir );
		}

		g_pEffects->Sparks( vecSparkPosition, 4, RandomInt( 2, 4 ), &vecSparkDir );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Whether this should return carry angles
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Arbeit_PersonalityCore::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return m_bUseCarryAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const QAngle
//-----------------------------------------------------------------------------
QAngle CNPC_Arbeit_PersonalityCore::PreferredCarryAngles( void )
{
	// FIXME: Embed this into the class
	static QAngle g_prefAngles;

	Vector vecUserForward;
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	pPlayer->EyeVectors( &vecUserForward );

	g_prefAngles.Init();

	// Always look at the player as if you're saying "HELOOOOOO"
	g_prefAngles.y = 90.0f;
	
	return g_prefAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	if (gpGlobals->curtime - m_flLastSawPlayerTime > 1.0f)
	{
		// Pretend we're seeing the player
		OnSeeEntity( pPhysGunUser );
		m_flLastSawPlayerTime = gpGlobals->curtime;
	}

	if ( reason != PUNTED_BY_CANNON )
	{
		m_flCarryTime = gpGlobals->curtime;
		m_OnPlayerPickup.FireOutput( pPhysGunUser, this );

		// We want to use preferred carry angles if we're not nicely upright
		Vector vecToTurret = pPhysGunUser->GetAbsOrigin() - GetAbsOrigin();
		vecToTurret.z = 0;
		VectorNormalize( vecToTurret );

		// We want to use preferred carry angles if we're not nicely upright
		Vector	forward, up;
		GetVectors( &forward, NULL, &up );

		bool bUpright = DotProduct( up, Vector(0,0,1) ) > 0.9f;
		bool bInFront = DotProduct( vecToTurret, forward ) > 0.10f;

		// Correct our angles only if we're not upright or we're mostly in front of the turret
		m_bUseCarryAngles = ( bUpright == false || bInFront );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	m_flCarryTime = -1.0f;
	m_bUseCarryAngles = false;
	m_OnPlayerDrop.FireOutput( pPhysGunUser, this );

	m_flPlayerDropTime = gpGlobals->curtime + 2.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Arbeit_PersonalityCore::OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	return true;
}

//------------------------------------------------------------------------------
// Do we have a physics attacker?
//------------------------------------------------------------------------------
CBasePlayer *CNPC_Arbeit_PersonalityCore::HasPhysicsAttacker( float dt )
{
	// If the player is holding me now, or I've been recently thrown
	// then return a pointer to that player
	if ( IsHeldByPhyscannon( ) || (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime) )
	{
		return m_hPhysicsAttacker;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Make us explode
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputExplode( inputdata_t &inputdata )
{
	const CTakeDamageInfo info(this, inputdata.pActivator, NULL, DMG_GENERIC);
	Event_Killed(info);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputEnableMotion( inputdata_t &inputdata )
{
	m_bMotionDisabled = false;

	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys != NULL )
	{
		pPhys->EnableMotion( true );
		pPhys->Wake();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputDisableMotion( inputdata_t &inputdata )
{
	m_bMotionDisabled = true;

	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys != NULL )
	{
		pPhys->EnableMotion( false );

		if ( pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			pPlayer->ForceDropOfCarriedPhysObjects( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputForcePickup( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		pPlayer->ForceDropOfCarriedPhysObjects( NULL );
		pPlayer->PickupObject( this, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputEnablePickup( inputdata_t &inputdata )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys != NULL )
	{
		PhysClearGameFlags( pPhys, FVPHYSICS_NO_PLAYER_PICKUP );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputDisablePickup( inputdata_t &inputdata )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys != NULL )
	{
		PhysSetGameFlags( pPhys, FVPHYSICS_NO_PLAYER_PICKUP );

		if ( pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
		{
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			pPlayer->ForceDropOfCarriedPhysObjects( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputPlayAttach( inputdata_t &inputdata )
{
	if ( HaveSequenceForActivity( ACT_CORE_PLUG_ATTACH ) )
		SetSchedule( SCHED_PERSONALITYCORE_PLUG_ATTACH );
}

void CNPC_Arbeit_PersonalityCore::InputPlayDetach( inputdata_t &inputdata )
{
	if ( HaveSequenceForActivity( ACT_CORE_PLUG_DETACH ) )
		SetSchedule( SCHED_PERSONALITYCORE_PLUG_DETACH );
}

void CNPC_Arbeit_PersonalityCore::InputPlayLock( inputdata_t &inputdata )
{
	if ( HaveSequenceForActivity( ACT_CORE_PLUG_LOCK ) )
		SetSchedule( SCHED_PERSONALITYCORE_PLUG_LOCK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputSetCustomIdleSequence( inputdata_t &inputdata )
{
	m_iszCustomIdleSequence = inputdata.value.StringID();
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputSetCustomAlertSequence( inputdata_t &inputdata )
{
	m_iszCustomAlertSequence = inputdata.value.StringID();
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputSetCustomStareSequence( inputdata_t &inputdata )
{
	m_iszCustomStareSequence = inputdata.value.StringID();
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputSetCustomPanicSequence( inputdata_t &inputdata )
{
	m_iszCustomPanicSequence = inputdata.value.StringID();
	ResetActivity();
}

//-----------------------------------------------------------------------------

void CNPC_Arbeit_PersonalityCore::InputClearCustomIdleSequence( inputdata_t &inputdata )
{
	m_iszCustomIdleSequence = NULL_STRING;
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputClearCustomAlertSequence( inputdata_t &inputdata )
{
	m_iszCustomAlertSequence = NULL_STRING;
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputClearCustomStareSequence( inputdata_t &inputdata )
{
	m_iszCustomStareSequence = NULL_STRING;
	ResetActivity();
}

void CNPC_Arbeit_PersonalityCore::InputClearCustomPanicSequence( inputdata_t &inputdata )
{
	m_iszCustomPanicSequence = NULL_STRING;
	ResetActivity();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Arbeit_PersonalityCore::InputSetUprightPose( inputdata_t &inputdata )
{
	if (m_nPoseUpright != -1)
	{
		SetPoseParameter( m_nPoseUpright, inputdata.value.Float() );
	}
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_arbeit_personality_core, CNPC_Arbeit_PersonalityCore )

	DECLARE_CONDITION( COND_PERSONALITYCORE_PANIC_LEVEL_CHANGE )

	DECLARE_ACTIVITY( ACT_CORE_GESTURE_FLOOR_IDLE )
	DECLARE_ACTIVITY( ACT_CORE_PLUG_ATTACH )
	DECLARE_ACTIVITY( ACT_CORE_PLUG_DETACH )
	DECLARE_ACTIVITY( ACT_CORE_PLUG_LOCK )

	DECLARE_ANIMEVENT( AE_CORE_SPARK )

	DEFINE_SCHEDULE
	(
		SCHED_PERSONALITYCORE_ALERT_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_REASONABLE		0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT					10"
		"		TASK_SUGGEST_STATE			STATE:IDLE"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_IDLE_INTERRUPT"
	);

	DEFINE_SCHEDULE
	(
		SCHED_PERSONALITYCORE_COMBAT_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT_INDEFINITE		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SEE_ENEMY"
		"		COND_IDLE_INTERRUPT"
		"		COND_PERSONALITYCORE_PANIC_LEVEL_CHANGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_PERSONALITYCORE_PLUG_ATTACH,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CORE_PLUG_ATTACH"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_PERSONALITYCORE_PLUG_DETACH,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CORE_PLUG_DETACH"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_PERSONALITYCORE_PLUG_LOCK,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CORE_PLUG_LOCK"
		""
		"	Interrupts"
	);

AI_END_CUSTOM_NPC()
