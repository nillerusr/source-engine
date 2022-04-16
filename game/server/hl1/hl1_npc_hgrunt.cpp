//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bullseyes act as targets for other NPC's to attack and to trigger
//			events 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include	"cbase.h"
#include	"beam_shared.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"hl1_npc_hgrunt.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include	"basecombatweapon.h"
#include	"hl1_basegrenade.h"
#include	"ai_interactions.h"
#include	"scripted.h"
#include	"hl1_basegrenade.h"
#include	"hl1_grenade_mp5.h"

ConVar	sk_hgrunt_health( "sk_hgrunt_health","0");
ConVar  sk_hgrunt_kick ( "sk_hgrunt_kick", "0" );
ConVar  sk_hgrunt_pellets ( "sk_hgrunt_pellets", "0" );
ConVar  sk_hgrunt_gspeed ( "sk_hgrunt_gspeed", "0" );

extern ConVar sk_plr_dmg_grenade;
extern ConVar sk_plr_dmg_mp5_grenade;

#define SF_GRUNT_LEADER	( 1 << 5  )

int g_fGruntQuestion;				// true if an idle grunt asked a question. Cleared when someone answers.
int g_iSquadIndex = 0;

#define HGRUNT_GUN_SPREAD 0.08716f

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	GRUNT_CLIP_SIZE					36 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define GRUNT_VOL						0.35		// volume of grunt sounds
#define GRUNT_SNDLVL					SNDLVL_NORM	// soundlevel of grunt sentences
#define HGRUNT_LIMP_HEALTH				20
#define HGRUNT_DMG_HEADSHOT				( DMG_BULLET | DMG_CLUB )	// damage types that can kill a grunt with a single headshot.
#define HGRUNT_NUM_HEADS				2 // how many grunt heads are there? 
#define HGRUNT_MINIMUM_HEADSHOT_DAMAGE	15 // must do at least this much damage in one shot to head to score a headshot kill
#define	HGRUNT_SENTENCE_VOLUME			(float)0.35 // volume of grunt sentences

#define HGRUNT_9MMAR				( 1 << 0)
#define HGRUNT_HANDGRENADE			( 1 << 1)
#define HGRUNT_GRENADELAUNCHER		( 1 << 2)
#define HGRUNT_SHOTGUN				( 1 << 3)

#define HEAD_GROUP					1
#define HEAD_GRUNT					0
#define HEAD_COMMANDER				1
#define HEAD_SHOTGUN				2
#define HEAD_M203					3
#define GUN_GROUP					2
#define GUN_MP5						0
#define GUN_SHOTGUN					1
#define GUN_NONE					2

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HGRUNT_AE_RELOAD		( 2 )
#define		HGRUNT_AE_KICK			( 3 )
#define		HGRUNT_AE_BURST1		( 4 )
#define		HGRUNT_AE_BURST2		( 5 ) 
#define		HGRUNT_AE_BURST3		( 6 ) 
#define		HGRUNT_AE_GREN_TOSS		( 7 )
#define		HGRUNT_AE_GREN_LAUNCH	( 8 )
#define		HGRUNT_AE_GREN_DROP		( 9 )
#define		HGRUNT_AE_CAUGHT_ENEMY	( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.
#define		HGRUNT_AE_DROP_GUN		( 11) // grunt (probably dead) is dropping his mp5.


const char *CNPC_HGrunt::pGruntSentences[] = 
{
	"HG_GREN", // grenade scared grunt
	"HG_ALERT", // sees player
	"HG_MONSTER", // sees monster
	"HG_COVER", // running to cover
	"HG_THROW", // about to throw grenade
	"HG_CHARGE",  // running out to get the enemy
	"HG_TAUNT", // say rude things
};

enum HGRUNT_SENTENCE_TYPES
{
	HGRUNT_SENT_NONE = -1,
	HGRUNT_SENT_GREN = 0,
	HGRUNT_SENT_ALERT,
	HGRUNT_SENT_MONSTER,
	HGRUNT_SENT_COVER,
	HGRUNT_SENT_THROW,
	HGRUNT_SENT_CHARGE,
	HGRUNT_SENT_TAUNT,
} ;

LINK_ENTITY_TO_CLASS( monster_human_grunt, CNPC_HGrunt );

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_GRUNT_FAIL = LAST_SHARED_SCHEDULE,
	SCHED_GRUNT_COMBAT_FAIL,
	SCHED_GRUNT_VICTORY_DANCE,
	SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE,
	SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE_RETRY,
	SCHED_GRUNT_FOUND_ENEMY,
	SCHED_GRUNT_COMBAT_FACE,
	SCHED_GRUNT_SIGNAL_SUPPRESS,
	SCHED_GRUNT_SUPPRESS,
	SCHED_GRUNT_WAIT_IN_COVER,
	SCHED_GRUNT_TAKE_COVER,
	SCHED_GRUNT_GRENADE_COVER,
	SCHED_GRUNT_TOSS_GRENADE_COVER,
	SCHED_GRUNT_HIDE_RELOAD,
	SCHED_GRUNT_SWEEP,
	SCHED_GRUNT_RANGE_ATTACK1A,
	SCHED_GRUNT_RANGE_ATTACK1B,
	SCHED_GRUNT_RANGE_ATTACK2,
	SCHED_GRUNT_REPEL,
	SCHED_GRUNT_REPEL_ATTACK,
	SCHED_GRUNT_REPEL_LAND,
	SCHED_GRUNT_TAKE_COVER_FAILED,
	SCHED_GRUNT_RELOAD,
	SCHED_GRUNT_TAKE_COVER_FROM_ENEMY,
	SCHED_GRUNT_BARNACLE_HIT,
	SCHED_GRUNT_BARNACLE_PULL,
	SCHED_GRUNT_BARNACLE_CHOMP,
	SCHED_GRUNT_BARNACLE_CHEW,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_GRUNT_FACE_TOSS_DIR = LAST_SHARED_TASK + 1,
	TASK_GRUNT_SPEAK_SENTENCE,
	TASK_GRUNT_CHECK_FIRE,
};


//=========================================================
// monster-specific conditions
//=========================================================
enum
{
	COND_GRUNT_NOFIRE = LAST_SHARED_CONDITION + 1,
};

// -----------------------------------------------
//	> Squad slots
// -----------------------------------------------
enum SquadSlot_T
{	
	SQUAD_SLOT_GRENADE1 = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_GRENADE2,
	SQUAD_SLOT_ENGAGE1,
	SQUAD_SLOT_ENGAGE2,
};


int ACT_GRUNT_LAUNCH_GRENADE;
int ACT_GRUNT_TOSS_GRENADE;
int ACT_GRUNT_MP5_STANDING;
int ACT_GRUNT_MP5_CROUCHING;
int ACT_GRUNT_SHOTGUN_STANDING;
int ACT_GRUNT_SHOTGUN_CROUCHING;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HGrunt )
	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( m_flCheckAttackTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_iLastGrenadeCondition, FIELD_INTEGER ),
	DEFINE_FIELD( m_fStanding, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( m_voicePitch, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSentence, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iWeapons, FIELD_INTEGER, "weapons" ),
	DEFINE_KEYFIELD( m_SquadName, FIELD_STRING, "netname" ),

	DEFINE_FIELD( m_bInBarnacleMouth, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLastEnemySightTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTalkWaitTime, FIELD_TIME ),
	//DEFINE_FIELD( m_iAmmoType, FIELD_INTEGER ),

END_DATADESC()


//=========================================================
// Spawn
//=========================================================
void CNPC_HGrunt::Spawn()
{
	Precache( );

	SetModel( "models/hgrunt.mdl" );

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_RED;
	ClearEffects();
	m_iHealth			= sk_hgrunt_health.GetFloat();
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->curtime + 1;
	m_flNextPainTime	= gpGlobals->curtime;
	m_iSentence			= HGRUNT_SENT_NONE;

	CapabilitiesClear();
	CapabilitiesAdd ( bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_MOVE_GROUND );

	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1 );

	// Innate range attack for grenade
	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2 );
	// Innate range attack for kicking
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1 );
			
	m_fFirstEncounter	= true;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.

	m_HackedGunPos = Vector ( 0, 0, 55 );

	if ( m_iWeapons == 0)
	{
		// initialize to original values
		m_iWeapons = HGRUNT_9MMAR | HGRUNT_HANDGRENADE;
		// pev->weapons = HGRUNT_SHOTGUN;
		// pev->weapons = HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER;
	}

	if (FBitSet( m_iWeapons, HGRUNT_SHOTGUN ))
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_iClipSize		= 8;
	}
	else
	{
		m_iClipSize		= GRUNT_CLIP_SIZE;
	}
	m_cAmmoLoaded		= m_iClipSize;

	if ( random->RandomInt( 0, 99 ) < 80)
		m_nSkin = 0;	// light skin
	else
		m_nSkin = 1;	// dark skin

	if (FBitSet( m_iWeapons, HGRUNT_SHOTGUN ))
	{
		SetBodygroup( HEAD_GROUP, HEAD_SHOTGUN);
	}
	else if (FBitSet( m_iWeapons, HGRUNT_GRENADELAUNCHER ))
	{
		SetBodygroup( HEAD_GROUP, HEAD_M203 );
		m_nSkin = 1; // alway dark skin
	}

	m_flTalkWaitTime = 0;


	//HACK
	g_iSquadIndex = 0;

	BaseClass::Spawn();

	NPCInit();
}

int CNPC_HGrunt::IRelationPriority( CBaseEntity *pTarget )
{
	//I hate alien grunts more than anything.
	if ( pTarget->Classify() == CLASS_ALIEN_MILITARY )
	{
		if ( FClassnameIs( pTarget, "monster_alien_grunt" ) )
		{
			 return ( BaseClass::IRelationPriority ( pTarget ) + 1 );
		}
	}

	return BaseClass::IRelationPriority( pTarget );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_HGrunt::Precache()
{
	m_iAmmoType = GetAmmoDef()->Index("9mmRound");

	PrecacheModel("models/hgrunt.mdl");

	// get voice pitch
	if ( random->RandomInt(0,1))
		m_voicePitch = 109 + random->RandomInt(0,7);
	else
		m_voicePitch = 100;

	PrecacheScriptSound( "HGrunt.Reload" );
	PrecacheScriptSound( "HGrunt.GrenadeLaunch" );
	PrecacheScriptSound( "HGrunt.9MM" );
	PrecacheScriptSound( "HGrunt.Shotgun" );
	PrecacheScriptSound( "HGrunt.Pain" );
	PrecacheScriptSound( "HGrunt.Die" );

	BaseClass::Precache();

	UTIL_PrecacheOther( "grenade_hand" );
	UTIL_PrecacheOther( "grenade_mp5" );
}	

//=========================================================
// someone else is talking - don't speak
//=========================================================
bool CNPC_HGrunt::FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if ( gpGlobals->curtime <= m_flTalkWaitTime )
		 return FALSE;

	if ( m_spawnflags & SF_NPC_GAG )
	{
		if ( m_NPCState != NPC_STATE_COMBAT )
		{
			// no talking outside of combat if gagged.
			return FALSE;
		}
	}

	return TRUE;
}


//=========================================================
// Speak Sentence - say your cued up sentence.
//
// Some grunt sentences (take cover and charge) rely on actually
// being able to execute the intended action. It's really lame
// when a grunt says 'COVER ME' and then doesn't move. The problem
// is that the sentences were played when the decision to TRY
// to move to cover was made. Now the sentence is played after 
// we know for sure that there is a valid path. The schedule
// may still fail but in most cases, well after the grunt has 
// started moving.
//=========================================================
void CNPC_HGrunt::SpeakSentence( void )
{
	if ( m_iSentence == HGRUNT_SENT_NONE )
	{
		// no sentence cued up.
		return; 
	}

	if ( FOkToSpeak() )
	{
		SENTENCEG_PlayRndSz( edict(), pGruntSentences[ m_iSentence ], HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
		JustSpoke();
	}
}

//=========================================================
//=========================================================
void CNPC_HGrunt::JustSpoke( void )
{
	m_flTalkWaitTime = gpGlobals->curtime + random->RandomFloat( 1.5f, 2.0f );
	m_iSentence = HGRUNT_SENT_NONE;
}

//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CNPC_HGrunt::PrescheduleThink ( void )
{
	BaseClass::PrescheduleThink();
	
	if ( m_pSquad && GetEnemy() != NULL )
	{
		if ( m_pSquad->GetLeader() == NULL )
			 return;

		CNPC_HGrunt *pSquadLeader = (CNPC_HGrunt*)m_pSquad->GetLeader()->MyNPCPointer();

		if ( pSquadLeader == NULL )
			 return; //Paranoid, so making sure it's ok.		
		
		if ( HasCondition ( COND_SEE_ENEMY ) )
		{
			// update the squad's last enemy sighting time.
			pSquadLeader->m_flLastEnemySightTime = gpGlobals->curtime;
		}
		else
		{
			if ( gpGlobals->curtime - pSquadLeader->m_flLastEnemySightTime > 5 )
			{
				// been a while since we've seen the enemy
				pSquadLeader->GetEnemies()->MarkAsEluded( GetEnemy() );
			}
		}
	}
}

Class_T	CNPC_HGrunt::Classify ( void )
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
//
// SquadRecruit(), get some monsters of my classification and
// link them as a group.  returns the group size
//
//=========================================================
int CNPC_HGrunt::SquadRecruit( int searchRadius, int maxMembers )
{
	int squadCount;
	int iMyClass = Classify();// cache this monster's class

	if ( maxMembers < 2 )
		 return 0;

	// I am my own leader
	squadCount = 1;
	
	CBaseEntity *pEntity = NULL;

	if ( m_SquadName != NULL_STRING )
	{
		// I have a netname, so unconditionally recruit everyone else with that name.
		pEntity = gEntList.FindEntityByClassname( pEntity, "monster_human_grunt" );

		while ( pEntity )
		{
			CNPC_HGrunt *pRecruit = (CNPC_HGrunt*)pEntity->MyNPCPointer();

			if ( pRecruit )
			{
				if ( !pRecruit->m_pSquad && pRecruit->Classify() == iMyClass && pRecruit != this )
				{
					// minimum protection here against user error.in worldcraft. 
					if ( pRecruit->m_SquadName != NULL_STRING && FStrEq( STRING( m_SquadName ), STRING( pRecruit->m_SquadName ) ) )
					{
						pRecruit->InitSquad();
						squadCount++;
					}
				}
			}
	
			pEntity = gEntList.FindEntityByClassname( pEntity, "monster_human_grunt" );
		}

		return squadCount;
	}
	else
	{
		char szSquadName[64];
		Q_snprintf( szSquadName, sizeof( szSquadName ), "squad%d\n", g_iSquadIndex );

		m_SquadName = MAKE_STRING( szSquadName );

		while ( ( pEntity = gEntList.FindEntityInSphere( pEntity, GetAbsOrigin(), searchRadius ) ) != NULL )
		{
			if ( !FClassnameIs ( pEntity, "monster_human_grunt" ) )
				  continue;

			CNPC_HGrunt *pRecruit = (CNPC_HGrunt*)pEntity->MyNPCPointer();

			if ( pRecruit && pRecruit != this && pRecruit->IsAlive() && !pRecruit->m_hCine )
			{
				// Can we recruit this guy?
				if ( !pRecruit->m_pSquad && pRecruit->Classify() == iMyClass &&
				   ( (iMyClass != CLASS_ALIEN_MONSTER) || FClassnameIs( this, pRecruit->GetClassname() ) ) &&
					!pRecruit->m_SquadName )
				{
					trace_t tr;
					UTIL_TraceLine( GetAbsOrigin() + GetViewOffset(), pRecruit->GetAbsOrigin() + GetViewOffset(), MASK_NPCSOLID_BRUSHONLY, pRecruit, COLLISION_GROUP_NONE, &tr );// try to hit recruit with a traceline.

					if ( tr.fraction == 1.0 )
					{
						//We're ready to recruit people, so start a squad if I don't have one.
						if ( !m_pSquad )
						{
							InitSquad();
						}

						pRecruit->m_SquadName = m_SquadName;

						pRecruit->CapabilitiesAdd ( bits_CAP_SQUAD );
						pRecruit->InitSquad();

						squadCount++;
					}
				}
			}
		}

		if ( squadCount > 1 )
		{
			 g_iSquadIndex++;
		}
	}

	return squadCount;
}

void CNPC_HGrunt::StartNPC ( void )
{
	if ( !m_pSquad )
	{
		if ( m_SquadName != NULL_STRING )
		{
			// if I have a groupname, I can only recruit if I'm flagged as leader
			if ( GetSpawnFlags() & SF_GRUNT_LEADER )
			{
				InitSquad();

				// try to form squads now.
				int iSquadSize = SquadRecruit( 1024, 4 );

				if ( iSquadSize )
				{
				  Msg ( "Squad of %d %s formed\n", iSquadSize, GetClassname() );
				}
			}
			else
			{

				//Hacky.
				//Revisit me later.
				const char *pSquadName = STRING( m_SquadName );

				m_SquadName = NULL_STRING;

				BaseClass::StartNPC();

				m_SquadName = MAKE_STRING( pSquadName );

				return;
			}
		}
		else
		{
			int iSquadSize = SquadRecruit( 1024, 4 );

			if ( iSquadSize )
			{
			  Msg ( "Squad of %d %s formed\n", iSquadSize, GetClassname() );
			}
		}
	}

	BaseClass::StartNPC();

	if ( m_pSquad && m_pSquad->IsLeader( this ) )
	{
		SetBodygroup( 1, 1 ); // UNDONE: truly ugly hack
		m_nSkin = 0;
	}
}

//=========================================================
// CheckMeleeAttack1
//=========================================================
int CNPC_HGrunt::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if (flDist > 64)
		return COND_TOO_FAR_TO_ATTACK;
	else if (flDot < 0.7)
		return COND_NOT_FACING_ATTACK;
	
	return COND_CAN_MELEE_ATTACK1;
}

//=========================================================
// CheckRangeAttack1 - overridden for HGrunt, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the HGrunt can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
int CNPC_HGrunt::RangeAttack1Conditions ( float flDot, float flDist )
{
	if ( !HasCondition( COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
	{
		trace_t	tr;

		if ( !GetEnemy()->IsPlayer() && flDist <= 64 )
		{
			// kick nonclients, but don't shoot at them.
			return COND_NONE;
		}

		Vector vecSrc;
		QAngle angAngles;

		GetAttachment( "0", vecSrc, angAngles );

		//NDebugOverlay::Line( GetAbsOrigin() + GetViewOffset(), GetEnemy()->BodyTarget(GetAbsOrigin() + GetViewOffset()), 255, 0, 0, false, 0.1 );
		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( GetAbsOrigin() + GetViewOffset(), GetEnemy()->BodyTarget(GetAbsOrigin() + GetViewOffset()), MASK_SHOT, this/*pentIgnore*/, COLLISION_GROUP_NONE, &tr);

		if ( tr.fraction == 1.0 || tr.m_pEnt == GetEnemy() )
		{
			//NDebugOverlay::Line( tr.startpos, tr.endpos, 0, 255, 0, false, 1.0 );
			return COND_CAN_RANGE_ATTACK1;
		}

		//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 1.0 );
	}


	if ( !NoFriendlyFire() )
		 return COND_WEAPON_BLOCKED_BY_FRIEND; //err =|

	return COND_NONE;
}

int CNPC_HGrunt::RangeAttack2Conditions( float flDot, float flDist  )
{
	m_iLastGrenadeCondition = GetGrenadeConditions( flDot, flDist );
	return m_iLastGrenadeCondition;
}

int CNPC_HGrunt::GetGrenadeConditions( float flDot, float flDist  )
{
	if ( !FBitSet( m_iWeapons, ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) ) )
		  return COND_NONE;

	// assume things haven't changed too much since last time
	if (gpGlobals->curtime < m_flNextGrenadeCheck )
		return m_iLastGrenadeCondition;
	
	if ( m_flGroundSpeed != 0 )
		return COND_NONE;

	CBaseEntity *pEnemy = GetEnemy();
	
	if (!pEnemy)
		return COND_NONE;
	
	Vector flEnemyLKP = GetEnemyLKP();
	if ( !(pEnemy->GetFlags() & FL_ONGROUND) && pEnemy->GetWaterLevel() == 0 && flEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z)  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}
	
	Vector vecTarget;

	if (FBitSet( m_iWeapons, HGRUNT_HANDGRENADE))
	{
		// find feet
		if ( random->RandomInt( 0,1 ) )
		{
			// magically know where they are
			pEnemy->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecTarget );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = flEnemyLKP;
		}
	}
	else
	{
		// find target
		// vecTarget = GetEnemy()->BodyTarget( GetAbsOrigin() );
		vecTarget = GetEnemy()->GetAbsOrigin() + (GetEnemy()->BodyTarget( GetAbsOrigin() ) - GetEnemy()->GetAbsOrigin());
		// estimate position
		if ( HasCondition( COND_SEE_ENEMY))
		{
			vecTarget = vecTarget + ((vecTarget - GetAbsOrigin()).Length() / sk_hgrunt_gspeed.GetFloat()) * GetEnemy()->GetAbsVelocity();
		}
	}

	// are any of my squad members near the intended grenade impact area?
	if ( m_pSquad )
	{
		if ( m_pSquad->SquadMemberInRange( vecTarget, 256 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
			return COND_NONE;
		}
	}
	
	if ( ( vecTarget - GetAbsOrigin() ).Length2D() <= 256 )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_NONE;
	}

		
	if (FBitSet( m_iWeapons, HGRUNT_HANDGRENADE))
	{
		Vector vGunPos;
		QAngle angGunAngles;
		GetAttachment( "0", vGunPos, angGunAngles );


		Vector vecToss = VecCheckToss( this, vGunPos, vecTarget, -1, 0.5, false );

		if ( vecToss != vec3_origin )
		{
			m_vecTossVelocity = vecToss;

			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 0.3; // 1/3 second.

			return COND_CAN_RANGE_ATTACK2;
		}
		else
		{
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.

			return COND_NONE;
		}
	}
	else
	{
		Vector vGunPos;
		QAngle angGunAngles;
		GetAttachment( "0", vGunPos, angGunAngles );
		
		Vector vecToss = VecCheckThrow( this, vGunPos, vecTarget, sk_hgrunt_gspeed.GetFloat(), 0.5 );

		if ( vecToss != vec3_origin )
		{
			m_vecTossVelocity = vecToss;

			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 0.3; // 1/3 second.

			return COND_CAN_RANGE_ATTACK2;
		}
		else
		{
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.

			return COND_NONE;
		}
	}
}


//=========================================================
// FCanCheckAttacks - this is overridden for human grunts
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
bool CNPC_HGrunt::FCanCheckAttacks( void )
{
	// This condition set when too close to a grenade to blow it up
	if ( !HasCondition( COND_TOO_CLOSE_TO_ATTACK ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

int CNPC_HGrunt::GetSoundInterests( void )
{
	return	SOUND_WORLD			|
			SOUND_COMBAT		|
			SOUND_PLAYER		|
			SOUND_BULLET_IMPACT	|
			SOUND_DANGER;
}


//=========================================================
// TraceAttack - make sure we're not taking it in the helmet
//=========================================================
void CNPC_HGrunt::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	CTakeDamageInfo info = inputInfo;

	// check for helmet shot
	if (ptr->hitgroup == 11)
	{
		// make sure we're wearing one
		if ( GetBodygroup( 1 ) == HEAD_GRUNT && (info.GetDamageType() & (DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB)))
		{
			// absorb damage
			info.SetDamage( info.GetDamage() - 20 );
			if ( info.GetDamage() <= 0 )
				info.SetDamage( 0.01 );
		}
		// it's head shot anyways
		ptr->hitgroup = HITGROUP_HEAD;
	}
	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}


//=========================================================
// TakeDamage - overridden for the grunt because the grunt
// needs to forget that he is in cover if he's hurt. (Obviously
// not in a safe place anymore).
//=========================================================
int CNPC_HGrunt::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	Forget( bits_MEMORY_INCOVER );

	return BaseClass::OnTakeDamage_Alive ( inputInfo );
}


//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_HGrunt::MaxYawSpeed( void )
{
	float flYS;

	switch ( GetActivity() )
	{
	case ACT_IDLE:	
		flYS = 150;		
		break;
	case ACT_RUN:	
		flYS = 150;	
		break;
	case ACT_WALK:	
		flYS = 180;		
		break;
	case ACT_RANGE_ATTACK1:	
		flYS = 120;	
		break;
	case ACT_RANGE_ATTACK2:	
		flYS = 120;	
		break;
	case ACT_MELEE_ATTACK1:	
		flYS = 120;	
		break;
	case ACT_MELEE_ATTACK2:	
		flYS = 120;	
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:	
		flYS = 180;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		flYS = 30;
		break;
	default:
		flYS = 90;
		break;
	}

	// Yaw speed is handled differently now!
	return flYS * 0.5f;
}

void CNPC_HGrunt::IdleSound( void )
{
	if (FOkToSpeak() && ( g_fGruntQuestion || random->RandomInt( 0,1 ) ) )
	{
		if (!g_fGruntQuestion)
		{
			// ask question or make statement
			switch ( random->RandomInt( 0,2 ) )
			{
			case 0: // check in
				SENTENCEG_PlayRndSz( edict(), "HG_CHECK", HGRUNT_SENTENCE_VOLUME, SNDLVL_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 1;
				break;
			case 1: // question
				SENTENCEG_PlayRndSz( edict(), "HG_QUEST", HGRUNT_SENTENCE_VOLUME, SNDLVL_NORM, 0, m_voicePitch);
				g_fGruntQuestion = 2;
				break;
			case 2: // statement
				SENTENCEG_PlayRndSz( edict(), "HG_IDLE", HGRUNT_SENTENCE_VOLUME, SNDLVL_NORM, 0, m_voicePitch);
				break;
			}
		}
		else
		{
			switch (g_fGruntQuestion)
			{
			case 1: // check in
				SENTENCEG_PlayRndSz( edict(), "HG_CLEAR", HGRUNT_SENTENCE_VOLUME, SNDLVL_NORM, 0, m_voicePitch);
				break;
			case 2: // question 
				SENTENCEG_PlayRndSz( edict(), "HG_ANSWER", HGRUNT_SENTENCE_VOLUME, SNDLVL_NORM, 0, m_voicePitch);
				break;
			}
			g_fGruntQuestion = 0;
		}
		JustSpoke();
	}
}

bool CNPC_HGrunt::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if (interactionType == g_interactionBarnacleVictimDangle)
	{
		// Force choosing of a new schedule
		ClearSchedule( "Soldier being eaten by a barnacle" );
		m_bInBarnacleMouth	= true;
		return true;
	}
	else if ( interactionType == g_interactionBarnacleVictimReleased )
	{
		SetState ( NPC_STATE_IDLE );
		m_bInBarnacleMouth	= false;
		SetAbsVelocity( vec3_origin );
		SetMoveType( MOVETYPE_STEP );
		return true;
	}
	else if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		if ( GetFlags() & FL_ONGROUND )
		{
			SetGroundEntity( NULL );
		}
		
		//Maybe this will break something else.
		if ( GetState() == NPC_STATE_SCRIPT )
		{
			 m_hCine->CancelScript();
			 ClearSchedule( "Soldier grabbed by a barnacle" );
		}

		SetState( NPC_STATE_PRONE );
		
		CTakeDamageInfo info;
		PainSound( info );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Combine needs to check ammo
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HGrunt::CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	 	 SetCondition( COND_NO_PRIMARY_AMMO );

}

//=========================================================
//=========================================================
CBaseEntity *CNPC_HGrunt::Kick( void )
{
	trace_t tr;

	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vecStart = GetAbsOrigin();
	vecStart.z += WorldAlignSize().z * 0.5;
	Vector vecEnd = vecStart + (forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, Vector(-16,-16,-18), Vector(16,16,18), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.m_pEnt )
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		return pEntity;
	}

	return NULL;
}

Vector CNPC_HGrunt::Weapon_ShootPosition( void )
{
	if ( m_fStanding )
		return GetAbsOrigin() + Vector( 0, 0, 60 );
	else
		return GetAbsOrigin() + Vector( 0, 0, 48 );
}

void CNPC_HGrunt::Event_Killed( const CTakeDamageInfo &info )
{
	Vector	vecGunPos;
	QAngle	vecGunAngles;

	GetAttachment( "0", vecGunPos, vecGunAngles );

	// switch to body group with no gun.
	SetBodygroup( GUN_GROUP, GUN_NONE );

	// If the gun would drop into a wall, spawn it at our origin
	if( UTIL_PointContents( vecGunPos ) & CONTENTS_SOLID )
	{
		vecGunPos = GetAbsOrigin();
	}

	// now spawn a gun.
	if (FBitSet( m_iWeapons, HGRUNT_SHOTGUN ))
	{
		 DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
	}
	else
	{
		 DropItem( "weapon_mp5", vecGunPos, vecGunAngles );
	}

	if (FBitSet( m_iWeapons, HGRUNT_GRENADELAUNCHER ))
	{
		DropItem( "ammo_ARgrenades", BodyTarget( GetAbsOrigin() ), vecGunAngles );
	}

	BaseClass::Event_Killed( info );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_HGrunt::HandleAnimEvent( animevent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
		case HGRUNT_AE_RELOAD:
		{
			CPASAttenuationFilter filter( this );
			EmitSound( filter, entindex(), "HGrunt.Reload" );

			m_cAmmoLoaded = m_iClipSize;
			ClearCondition( COND_NO_PRIMARY_AMMO);
		}
		break;

		case HGRUNT_AE_GREN_TOSS:
		{
			CHandGrenade *pGrenade = (CHandGrenade*)Create( "grenade_hand", GetAbsOrigin() + Vector(0,0,60), vec3_angle );
			if ( pGrenade )
			{
				pGrenade->ShootTimed( this, m_vecTossVelocity, 3.5 );
			}

			m_iLastGrenadeCondition =  COND_NONE;
			m_flNextGrenadeCheck = gpGlobals->curtime + 6;// wait six seconds before even looking again to see if a grenade can be thrown.

			Msg( "Tossing a grenade to flush you out!\n"	);
		}
		break;

		case HGRUNT_AE_GREN_LAUNCH:
		{
			CPASAttenuationFilter filter2( this );
			EmitSound( filter2, entindex(), "HGrunt.GrenadeLaunch" );
			
			Vector vecSrc;
			QAngle angAngles;

			GetAttachment( "0", vecSrc, angAngles );
		
			CGrenadeMP5 * m_pMyGrenade = (CGrenadeMP5*)Create( "grenade_mp5", vecSrc, angAngles, this );
			m_pMyGrenade->SetAbsVelocity( m_vecTossVelocity );
			m_pMyGrenade->SetLocalAngularVelocity( QAngle( random->RandomFloat( -100, -500 ), 0, 0 ) );
			m_pMyGrenade->SetMoveType( MOVETYPE_FLYGRAVITY ); 
			m_pMyGrenade->SetThrower( this );
			m_pMyGrenade->SetDamage( sk_plr_dmg_mp5_grenade.GetFloat() );

			if (g_iSkillLevel == SKILL_HARD)
				m_flNextGrenadeCheck = gpGlobals->curtime + random->RandomFloat( 2, 5 );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->curtime + 6;// wait six seconds before even looking again to see if a grenade can be thrown.

			m_iLastGrenadeCondition =  COND_NONE;

			Msg( "Using grenade launcer to flush you out!\n"	);
		}
		break;

		case HGRUNT_AE_GREN_DROP:
		{
			CHandGrenade *pGrenade = (CHandGrenade*)Create( "grenade_hand", Weapon_ShootPosition(), vec3_angle );
			if ( pGrenade )
			{
				pGrenade->ShootTimed( this, m_vecTossVelocity, 3.5 );
			}

			m_iLastGrenadeCondition =  COND_NONE;
			Msg( "Dropping a grenade!\n"	);
		}
		break;

		case HGRUNT_AE_BURST1:
		{
			if ( FBitSet( m_iWeapons, HGRUNT_9MMAR ) )
			{
				Shoot();

				CPASAttenuationFilter filter3( this );
				// the first round of the three round burst plays the sound and puts a sound in the world sound list.
				EmitSound( filter3, entindex(), "HGrunt.9MM" );
			}
			else
			{
				Shotgun( );

				CPASAttenuationFilter filter4( this );
				EmitSound( filter4, entindex(), "HGrunt.Shotgun" );
			}
		
			CSoundEnt::InsertSound ( SOUND_COMBAT, GetAbsOrigin(), 384, 0.3 );
		}
		break;

		case HGRUNT_AE_BURST2:
		case HGRUNT_AE_BURST3:
			Shoot();
			break;

		case HGRUNT_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if ( pHurt )
			{
				// SOUND HERE!
				Vector forward, up;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );

				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
					 pHurt->ViewPunch( QAngle( 15, 0, 0) );

				// Don't give velocity or damage to the world
				if( pHurt->entindex() > 0 )
				{
					pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );

					CTakeDamageInfo info( this, this, sk_hgrunt_kick.GetFloat(), DMG_CLUB );
					CalculateMeleeDamageForce( &info, forward, pHurt->GetAbsOrigin() );
					pHurt->TakeDamage( info );
				}			
			}
		}
		break;

		case HGRUNT_AE_CAUGHT_ENEMY:
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
				 JustSpoke();
			}

		}

		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
	}
}

void CNPC_HGrunt::SetAim( const Vector &aimDir )
{
	QAngle angDir;
	VectorAngles( aimDir, angDir );

	float curPitch = GetPoseParameter( "XR" );
	float newPitch = curPitch + UTIL_AngleDiff( UTIL_ApproachAngle( angDir.x, curPitch, 60 ), curPitch );

	SetPoseParameter( "XR", -newPitch );
}

//=========================================================
// Shoot
//=========================================================
void CNPC_HGrunt::Shoot ( void )
{
	if ( GetEnemy() == NULL )
		return;
	
	Vector vecShootOrigin = Weapon_ShootPosition();
	Vector vecShootDir = GetShootEnemyDir( vecShootOrigin );

	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	Vector	vecShellVelocity = right * random->RandomFloat(40,90) + up * random->RandomFloat( 75,200 ) + forward * random->RandomFloat( -40, 40 );
	EjectShell( vecShootOrigin - vecShootDir * 24, vecShellVelocity, GetAbsAngles().y, 0 );
	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 2048, m_iAmmoType ); // shoot +-5 degrees
	
	DoMuzzleFlash();
	
	m_cAmmoLoaded--;// take away a bullet!

	SetAim( vecShootDir );
}

//=========================================================
// Shoot
//=========================================================
void CNPC_HGrunt::Shotgun ( void )
{
	if ( GetEnemy() == NULL )
		return;

	Vector vecShootOrigin = Weapon_ShootPosition();
	Vector vecShootDir = GetShootEnemyDir( vecShootOrigin );

	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	Vector	vecShellVelocity = right * random->RandomFloat(40,90) + up * random->RandomFloat( 75,200 ) + forward * random->RandomFloat( -40, 40 );
	EjectShell( vecShootOrigin - vecShootDir * 24, vecShellVelocity, GetAbsAngles().y, 1 );
	FireBullets( sk_hgrunt_pellets.GetFloat(), vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES, 2048, m_iAmmoType, 0 ); // shoot +-7.5 degrees

	DoMuzzleFlash();
	
	m_cAmmoLoaded--;// take away a bullet!

	SetAim( vecShootDir );
}

//=========================================================
// start task
//=========================================================
void CNPC_HGrunt::StartTask ( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_GRUNT_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetCondition( COND_WEAPON_BLOCKED_BY_FRIEND );
		}
		TaskComplete();
		break;

	case TASK_GRUNT_SPEAK_SENTENCE:
		SpeakSentence();
		TaskComplete();
		break;
	
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		BaseClass ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		SetIdealActivity( ACT_RELOAD );
		break;

	case TASK_GRUNT_FACE_TOSS_DIR:
		break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		BaseClass::StartTask( pTask );
		if (GetMoveType() == MOVETYPE_FLYGRAVITY)
		{
			SetIdealActivity( ACT_GLIDE );
		}
		break;

	default: 
		BaseClass::StartTask( pTask );
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_HGrunt::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_GRUNT_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			GetMotor()->SetIdealYawToTargetAndUpdate( GetAbsOrigin() + m_vecTossVelocity * 64, AI_KEEP_YAW_SPEED );

			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		}
	default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}

//=========================================================
// PainSound
//=========================================================
void CNPC_HGrunt::PainSound( const CTakeDamageInfo &info )
{
	if ( gpGlobals->curtime > m_flNextPainTime )
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "HGrunt.Pain" );

		m_flNextPainTime = gpGlobals->curtime + 1;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CNPC_HGrunt::DeathSound( const CTakeDamageInfo &info )
{
	CPASAttenuationFilter filter( this, ATTN_IDLE );
	EmitSound( filter, entindex(), "HGrunt.Die" );	
}

//=========================================================
// SetActivity 
//=========================================================
Activity CNPC_HGrunt::NPC_TranslateActivity( Activity eNewActivity )
{
	switch ( eNewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if (FBitSet( m_iWeapons, HGRUNT_9MMAR))
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				return (Activity)ACT_GRUNT_MP5_STANDING;
			}
			else
			{
				// get crouching shoot
				return (Activity)ACT_GRUNT_MP5_CROUCHING;
			}
		}
		else
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				return (Activity)ACT_GRUNT_SHOTGUN_STANDING;
			}
			else
			{
				// get crouching shoot
				return (Activity)ACT_GRUNT_SHOTGUN_CROUCHING;
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		// grunt is going to a secondary long range attack. This may be a thrown 
		// grenade or fired grenade, we must determine which and pick proper sequence
 		if ( m_iWeapons & HGRUNT_HANDGRENADE )
		{
			// get toss anim
			return (Activity)ACT_GRUNT_TOSS_GRENADE;
		}
		else
		{
			// get launch anim
			return (Activity)ACT_GRUNT_LAUNCH_GRENADE;
		}
		break;
	case ACT_RUN:
		if ( m_iHealth <= HGRUNT_LIMP_HEALTH )
		{
			// limp!
			return ACT_RUN_HURT;
		}
		else
		{
			return eNewActivity;
		}
		break;
	case ACT_WALK:
		if ( m_iHealth <= HGRUNT_LIMP_HEALTH )
		{
			// limp!
			return ACT_WALK_HURT;
		}
		else
		{
			return eNewActivity;
		}
		break;
	case ACT_IDLE:
		if ( m_NPCState == NPC_STATE_COMBAT )
		{
			eNewActivity = ACT_IDLE_ANGRY;
		}
		
		break;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

void CNPC_HGrunt::ClearAttackConditions( void )
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

int CNPC_HGrunt::SelectSchedule( void )
{
		// clear old sentence
	m_iSentence = HGRUNT_SENT_NONE;

	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if ( GetMoveType() == MOVETYPE_FLYGRAVITY && m_NPCState != NPC_STATE_PRONE )
	{
		if (GetFlags() & FL_ONGROUND)
		{
			// just landed
			SetMoveType( MOVETYPE_STEP );
			SetGravity( 1.0 );
			return SCHED_GRUNT_REPEL_LAND;
		}
		else
		{
			// repel down a rope, 
			if ( m_NPCState == NPC_STATE_COMBAT )
				return SCHED_GRUNT_REPEL_ATTACK;
			else
				return SCHED_GRUNT_REPEL;
		}
	}

	// grunts place HIGH priority on running away from danger sounds.
	if ( HasCondition ( COND_HEAR_DANGER ) )
	{
		// dangerous sound nearby!
				
		//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
		// and the grunt should find cover from the blast
		// good place for "SHIT!" or some other colorful verbal indicator of dismay.
		// It's not safe to play a verbal order here "Scatter", etc cause 
		// this may only affect a single individual in a squad. 
				
		if (FOkToSpeak())
		{
			SENTENCEG_PlayRndSz( edict(), "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
			JustSpoke();
		}

		return SCHED_TAKE_COVER_FROM_BEST_SOUND;
	}

	switch	( m_NPCState )
	{

	case NPC_STATE_PRONE:
	{
		if (m_bInBarnacleMouth)
		{
			return SCHED_GRUNT_BARNACLE_CHOMP;
		}
		else
		{
			return SCHED_GRUNT_BARNACLE_HIT;
		}
	}

	case NPC_STATE_COMBAT:
		{
// dead enemy
			if ( HasCondition( COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return BaseClass::SelectSchedule();
			}

// new enemy
			if ( HasCondition( COND_NEW_ENEMY) )
			{
				if ( m_pSquad )
				{
					if ( !m_pSquad->IsLeader( this ) )
					{
						return SCHED_TAKE_COVER_FROM_ENEMY;
					}
					else 
					{
						//!!!KELLY - the leader of a squad of grunts has just seen the player or a 
						// monster and has made it the squad's enemy. You
						// can check pev->flags for FL_CLIENT to determine whether this is the player
						// or a monster. He's going to immediately start
						// firing, though. If you'd like, we can make an alternate "first sight" 
						// schedule where the leader plays a handsign anim
						// that gives us enough time to hear a short sentence or spoken command
						// before he starts pluggin away.
						if (FOkToSpeak())// && RANDOM_LONG(0,1))
						{
							if ((GetEnemy() != NULL) && GetEnemy()->IsPlayer())
								// player
								SENTENCEG_PlayRndSz( edict(), "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
							else if ((GetEnemy() != NULL) &&
									(GetEnemy()->Classify() != CLASS_PLAYER_ALLY) && 
									(GetEnemy()->Classify() != CLASS_HUMAN_PASSIVE) && 
									(GetEnemy()->Classify() != CLASS_MACHINE) )
								// monster
								SENTENCEG_PlayRndSz( edict(), "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);

							JustSpoke();
						}
						
						if ( HasCondition ( COND_CAN_RANGE_ATTACK1 ) )
						{
							return SCHED_GRUNT_SUPPRESS;
						}
						else
						{
							return SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE;
						}
					}
				}
			}
// no ammo
			else if ( HasCondition ( COND_NO_PRIMARY_AMMO ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return SCHED_GRUNT_HIDE_RELOAD;
			}
			
// damaged just a little
			else if ( HasCondition( COND_LIGHT_DAMAGE ) )
			{
				// if hurt:
				// 90% chance of taking cover
				// 10% chance of flinch.
				int iPercent = random->RandomInt(0,99);

				if ( iPercent <= 90 && GetEnemy() != NULL )
				{
					// only try to take cover if we actually have an enemy!

					//!!!KELLY - this grunt was hit and is going to run to cover.
					if (FOkToSpeak()) // && RANDOM_LONG(0,1))
					{
						//SENTENCEG_PlayRndSz( ENT(pev), "HG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
						m_iSentence = HGRUNT_SENT_COVER;
						//JustSpoke();
					}
					return SCHED_TAKE_COVER_FROM_ENEMY;
				}
				else
				{
					return SCHED_SMALL_FLINCH;
				}
			}
// can kick
			else if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
			{
				return SCHED_MELEE_ATTACK1;
			}
// can grenade launch

			else if ( FBitSet( m_iWeapons, HGRUNT_GRENADELAUNCHER) && HasCondition ( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE2 ) )
			{
				// shoot a grenade if you can
				return SCHED_RANGE_ATTACK2;
			}
// can shoot
			else if ( HasCondition ( COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( m_pSquad )
				{
					if ( m_pSquad->GetLeader() != NULL )
					{
					
						CAI_BaseNPC *pSquadLeader = m_pSquad->GetLeader()->MyNPCPointer();
						
						// if the enemy has eluded the squad and a squad member has just located the enemy
						// and the enemy does not see the squad member, issue a call to the squad to waste a 
						// little time and give the player a chance to turn.
						if ( pSquadLeader && pSquadLeader->EnemyHasEludedMe() && !HasCondition ( COND_ENEMY_FACING_ME ) )
						{
							return SCHED_GRUNT_FOUND_ENEMY;
						}
					}
				}

				if ( OccupyStrategySlotRange ( SQUAD_SLOT_ENGAGE1, SQUAD_SLOT_ENGAGE2 ) )
				{
					// try to take an available ENGAGE slot
					return SCHED_RANGE_ATTACK1;
				}
				else if ( HasCondition ( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE2 ) )
				{
					// throw a grenade if can and no engage slots are available
					return SCHED_RANGE_ATTACK2;
				}
				else
				{
					// hide!
					return SCHED_TAKE_COVER_FROM_ENEMY;
				}
			}
// can't see enemy
			else if ( HasCondition( COND_ENEMY_OCCLUDED ) )
			{
				if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE2 ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( edict(), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
						JustSpoke();
					}
					return SCHED_RANGE_ATTACK2;
				}
				else if ( OccupyStrategySlotRange ( SQUAD_SLOT_ENGAGE1, SQUAD_SLOT_ENGAGE2 ) )
				{
					//!!!KELLY - grunt cannot see the enemy and has just decided to 
					// charge the enemy's position. 
					if (FOkToSpeak())// && RANDOM_LONG(0,1))
					{
						//SENTENCEG_PlayRndSz( ENT(pev), "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
						m_iSentence = HGRUNT_SENT_CHARGE;
						//JustSpoke();
					}

					return SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE;
				}
				else
				{
					//!!!KELLY - grunt is going to stay put for a couple seconds to see if
					// the enemy wanders back out into the open, or approaches the
					// grunt's covered position. Good place for a taunt, I guess?
					if (FOkToSpeak() && random->RandomInt(0,1))
					{
						SENTENCEG_PlayRndSz( edict(), "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
						JustSpoke();
					}
					return SCHED_STANDOFF;
				}
			}
			
			if ( HasCondition( COND_SEE_ENEMY ) && !HasCondition ( COND_CAN_RANGE_ATTACK1 ) )
			{
				return SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE;
			}
		}
		case NPC_STATE_ALERT:
			if ( HasCondition( COND_ENEMY_DEAD ) && SelectWeightedSequence( ACT_VICTORY_DANCE ) != ACTIVITY_NOT_AVAILABLE )
			{
				// Scan around for new enemies
				return SCHED_VICTORY_DANCE;
			}
			break;
	}

	return BaseClass::SelectSchedule();
}

int CNPC_HGrunt::TranslateSchedule( int scheduleType )
{

	if ( scheduleType == SCHED_CHASE_ENEMY_FAILED )
	{
		 return SCHED_ESTABLISH_LINE_OF_FIRE;
	}
	switch	( scheduleType )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			if ( m_pSquad )
			{
				if ( g_iSkillLevel == SKILL_HARD && HasCondition( COND_CAN_RANGE_ATTACK2 ) && OccupyStrategySlotRange( SQUAD_SLOT_GRENADE1, SQUAD_SLOT_GRENADE2 ) )
				{
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( edict(), "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_SNDLVL, 0, m_voicePitch);
						JustSpoke();
					}
					return SCHED_GRUNT_TOSS_GRENADE_COVER;
				}
				else
				{
			  	    return SCHED_GRUNT_TAKE_COVER;
				}
			}
			else
			{
				if ( random->RandomInt(0,1) )
				{
					return SCHED_GRUNT_TAKE_COVER;
				}
				else
				{
					return SCHED_GRUNT_GRENADE_COVER;
				}
			}
		}
	case SCHED_GRUNT_TAKE_COVER_FAILED:
		{
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				return SCHED_RANGE_ATTACK1;
			}

			return SCHED_FAIL;
		}
		break;
	
	case SCHED_RANGE_ATTACK1:
		{
			// randomly stand or crouch
			if ( random->RandomInt( 0,9 ) == 0)
			{
				m_fStanding = random->RandomInt( 0, 1 ) != 0;
			}
		 
			if ( m_fStanding )
				return SCHED_GRUNT_RANGE_ATTACK1B;
			else
				return SCHED_GRUNT_RANGE_ATTACK1A;
		}

	case SCHED_RANGE_ATTACK2:
		{
			return SCHED_GRUNT_RANGE_ATTACK2;
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( m_pSquad )
			{
				if ( !m_pSquad->IsLeader( this ) )
				{
					return SCHED_GRUNT_FAIL;
				}
			}

			return SCHED_GRUNT_VICTORY_DANCE;
		}
	case SCHED_GRUNT_SUPPRESS:
		{
			if ( GetEnemy()->IsPlayer() && m_fFirstEncounter )
			{
				m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return SCHED_GRUNT_SIGNAL_SUPPRESS;
			}
			else
			{
				return SCHED_GRUNT_SUPPRESS;
			}
		}
	case SCHED_FAIL:
		{
			if ( GetEnemy() != NULL )
			{
				// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return SCHED_GRUNT_COMBAT_FAIL;
			}

			return SCHED_GRUNT_FAIL;
		}
	case SCHED_GRUNT_REPEL:
		{
			Vector vecVel = GetAbsVelocity();
			if ( vecVel.z > -128 )
			{
				vecVel.z -= 32;
				SetAbsVelocity( vecVel );
			}

			return SCHED_GRUNT_REPEL;
		}
	case SCHED_GRUNT_REPEL_ATTACK:
		{
			Vector vecVel = GetAbsVelocity();
			if ( vecVel.z > -128 )
			{
				vecVel.z -= 32;
				SetAbsVelocity( vecVel );
			}

			return SCHED_GRUNT_REPEL_ATTACK;
		}
	default:
		{
			return BaseClass::TranslateSchedule( scheduleType );
		}
	}
}

//=========================================================
// CHGruntRepel - when triggered, spawns a monster_human_grunt
// repelling down a line.
//=========================================================

class CNPC_HGruntRepel:public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_HGruntRepel, CAI_BaseNPC );
public:
	void Spawn( void );
	void Precache( void );
	void RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_grunt_repel, CNPC_HGruntRepel );



//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HGruntRepel )
	DEFINE_USEFUNC( RepelUse ),
	//DEFINE_FIELD( m_iSpriteTexture, FIELD_INTEGER ),
END_DATADESC()

void CNPC_HGruntRepel::Spawn( void )
{
	Precache( );
	SetSolid( SOLID_NONE );

	SetUse( &CNPC_HGruntRepel::RepelUse );
}

void CNPC_HGruntRepel::Precache( void )
{
	UTIL_PrecacheOther( "monster_human_grunt" );
	m_iSpriteTexture = PrecacheModel( "sprites/rope.vmt" );
}

void CNPC_HGruntRepel::RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -4096.0), MASK_NPCSOLID, this,COLLISION_GROUP_NONE, &tr);
	
	CBaseEntity *pEntity = Create( "monster_human_grunt", GetAbsOrigin(), GetAbsAngles() );
	CAI_BaseNPC *pGrunt = pEntity->MyNPCPointer( );
	pGrunt->SetMoveType( MOVETYPE_FLYGRAVITY );
	pGrunt->SetGravity( 0.001 );
	pGrunt->SetAbsVelocity( Vector( 0, 0, random->RandomFloat( -196, -128 ) ) );
	pGrunt->SetActivity( ACT_GLIDE );
	// UNDONE: position?
	pGrunt->m_vecLastPosition = tr.endpos;

	CBeam *pBeam = CBeam::BeamCreate( "sprites/rope.vmt", 10 );
	pBeam->PointEntInit( GetAbsOrigin() + Vector(0,0,112), pGrunt );
	pBeam->SetBeamFlags( FBEAM_SOLID );
	pBeam->SetColor( 255, 255, 255 );
	pBeam->SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + -4096.0 * tr.fraction / pGrunt->GetAbsVelocity().z + 0.5 );

	UTIL_Remove( this );
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( monster_human_grunt, CNPC_HGrunt )

	DECLARE_ACTIVITY( ACT_GRUNT_LAUNCH_GRENADE )
	DECLARE_ACTIVITY( ACT_GRUNT_TOSS_GRENADE )
	DECLARE_ACTIVITY( ACT_GRUNT_MP5_STANDING );
	DECLARE_ACTIVITY( ACT_GRUNT_MP5_CROUCHING );
	DECLARE_ACTIVITY( ACT_GRUNT_SHOTGUN_STANDING );
	DECLARE_ACTIVITY( ACT_GRUNT_SHOTGUN_CROUCHING );

	DECLARE_CONDITION( COND_GRUNT_NOFIRE )
	
	DECLARE_TASK( TASK_GRUNT_FACE_TOSS_DIR )
	DECLARE_TASK( TASK_GRUNT_SPEAK_SENTENCE )
	DECLARE_TASK( TASK_GRUNT_CHECK_FIRE )
		
	DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE1 )
	DECLARE_SQUADSLOT( SQUAD_SLOT_GRENADE2 )
	DECLARE_SQUADSLOT( SQUAD_SLOT_ENGAGE1 )
	DECLARE_SQUADSLOT( SQUAD_SLOT_ENGAGE2 )

	//=========================================================
	// > SCHED_GRUNT_FAIL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_FAIL,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT				0"
		"		TASK_WAIT_PVS			0"
		"	"
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
	)
	
	//=========================================================
	// > SCHED_GRUNT_COMBAT_FAIL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_COMBAT_FAIL,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT_FACE_ENEMY	2"
		"		TASK_WAIT_PVS			0"
		"	"
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		)
	
	//=========================================================
	// > SCHED_GRUNT_VICTORY_DANCE
	// Victory dance!
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_VICTORY_DANCE,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_WAIT						1.5"
		"		TASK_GET_PATH_TO_ENEMY_CORPSE	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_VICTORY_DANCE"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)
	
	//=========================================================
	// > SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE
	// Establish line of fire - move to a position that allows
	// the grunt to attack.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE_RETRY"
		"		TASK_GET_PATH_TO_ENEMY		0"
		"		TASK_GRUNT_SPEAK_SENTENCE	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// This is a schedule I added that borrows some HL2 technology
	// to be smarter in cases where HL1 was pretty dumb. I've wedged
	// this between ESTABLISH_LINE_OF_FIRE and TAKE_COVER_FROM_ENEMY (sjb)
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE_RETRY,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_GRUNT_TAKE_COVER_FROM_ENEMY"
		"		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
		"		TASK_GRUNT_SPEAK_SENTENCE		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_FOUND_ENEMY
	// Grunt established sight with an enemy
	// that was hiding from the squad.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_FOUND_ENEMY,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SIGNAL1"
		"	"
		"	Interrupts"
		"		COND_HEAR_DANGER"
	)
	
	
	//=========================================================
	// > SCHED_GRUNT_COMBAT_FACE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_COMBAT_FACE,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_FACE_ENEMY			0"
		"		TASK_WAIT				1.5"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_GRUNT_SWEEP"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
	)
	
	
	//=========================================================
	// > SCHED_GRUNT_SIGNAL_SUPPRESS
	// Suppressing fire - don't stop shooting until the clip is
	// empty or grunt gets hurt.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_SIGNAL_SUPPRESS,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SIGNAL2"
		"		TASK_FACE_ENEMY					0"
		"		TASK_GRUNT_CHECK_FIRE			0"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_GRUNT_CHECK_FIRE			0"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_GRUNT_CHECK_FIRE			0"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_GRUNT_CHECK_FIRE			0"
		"		TASK_RANGE_ATTACK1				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_GRUNT_CHECK_FIRE			0"
		"		TASK_RANGE_ATTACK1				0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_GRUNT_NOFIRE"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_HEAR_DANGER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_SUPPRESS
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_SUPPRESS,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_GRUNT_CHECK_FIRE		0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_GRUNT_CHECK_FIRE		0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_GRUNT_CHECK_FIRE		0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_GRUNT_CHECK_FIRE		0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_GRUNT_CHECK_FIRE		0"
		"		TASK_RANGE_ATTACK1			0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_GRUNT_NOFIRE"
		"		COND_NO_PRIMARY_AMMO"
		"		COND_HEAR_DANGER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_WAIT_IN_COVER
	// grunt wait in cover - we don't allow danger or the ability
	// to attack to break a grunt's run to cover schedule, but
	// when a grunt is in cover, we do want them to attack if they can.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_WAIT_IN_COVER,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT_FACE_ENEMY	1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
	)
	
	
	//=========================================================
	// > SCHED_GRUNT_TAKE_COVER
	// !!!BUGBUG - set a decent fail schedule here.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_TAKE_COVER,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_GRUNT_TAKE_COVER_FAILED"
		"		TASK_WAIT					0.2"
		"		TASK_FIND_COVER_FROM_ENEMY	0"
		"		TASK_GRUNT_SPEAK_SENTENCE	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_REMEMBER				MEMORY:INCOVER"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_GRUNT_WAIT_IN_COVER"
		"	"
		"	Interrupts"
	)
	
	
	//=========================================================
	// > SCHED_GRUNT_GRENADE_COVER
	// drop grenade then run to cover.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_GRENADE_COVER,
	
		"	Tasks"
		"		TASK_STOP_MOVING						0"
		"		TASK_FIND_COVER_FROM_ENEMY				99"
		"		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY		384"
		"		TASK_PLAY_SEQUENCE						ACTIVITY:ACT_SPECIAL_ATTACK1"
		"		TASK_CLEAR_MOVE_WAIT					0"
		"		TASK_RUN_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_GRUNT_WAIT_IN_COVER"
		"	"
		"	Interrupts"
	)
		
	//=========================================================
	// > SCHED_GRUNT_TOSS_GRENADE_COVER
	// drop grenade then run to cover.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_TOSS_GRENADE_COVER,
	
		"	Tasks"
		"		TASK_FACE_ENEMY				0"
		"		TASK_RANGE_ATTACK2			0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_GRUNT_TAKE_COVER_FROM_ENEMY"
		"	"
		"	Interrupts"
	)
	
	//=========================================================
	// > SCHED_GRUNT_HIDE_RELOAD
	// Grunt reload schedule
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_HIDE_RELOAD,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_GRUNT_RELOAD"
		"		TASK_FIND_COVER_FROM_ENEMY		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_REMEMBER					MEMORY:INCOVER"
		"		TASK_FACE_ENEMY					0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_RELOAD"
		"	"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_SWEEP
	// Do a turning sweep of the area
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_SWEEP,
	
		"	Tasks"
		"		TASK_TURN_LEFT			179"
		"		TASK_WAIT				1"
		"		TASK_TURN_LEFT			179"
		"		TASK_WAIT				1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_PLAYER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_RANGE_ATTACK1A
	// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
	// grunt's grenade toss requires the enemy be occluded.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_RANGE_ATTACK1A,
	
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_CROUCH"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		"		COND_HEAR_DANGER"
		"		COND_GRUNT_NOFIRE"
		"		COND_NO_PRIMARY_AMMO"
	)
	
	//=========================================================
	// > SCHED_GRUNT_RANGE_ATTACK1B
	// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
	// grunt's grenade toss requires the enemy be occluded.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_RANGE_ATTACK1B,
	
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_IDLE_ANGRY"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_GRUNT_CHECK_FIRE				0"
		"		TASK_RANGE_ATTACK1					0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		"		COND_HEAR_DANGER"
		"		COND_GRUNT_NOFIRE"
		"		COND_NO_PRIMARY_AMMO"
	)
	
	//=========================================================
	// > SCHED_GRUNT_RANGE_ATTACK2
	// secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
	// grunt's grenade toss requires the enemy be occluded.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_RANGE_ATTACK2,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_GRUNT_FACE_TOSS_DIR	0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_RANGE_ATTACK2"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_GRUNT_WAIT_IN_COVER" // don't run immediately after throwing grenade.
		"	"
		"	Interrupts"
	)
	
	//=========================================================
	// > SCHED_GRUNT_REPEL
	// secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
	// grunt's grenade toss requires the enemy be occluded.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_REPEL,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_IDEAL				0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_GLIDE"
		"	"
		"	Interrupts"
		"		COND_SEE_ENEMY"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_PLAYER"
		"		COND_HEAR_COMBAT"
	)
	
	//=========================================================
	// > SCHED_GRUNT_REPEL_ATTACK
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_REPEL_ATTACK,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_FLY"
		"	"
		"	Interrupts"
		"		COND_ENEMY_OCCLUDED"
	)
	
	//=========================================================
	// > SCHED_GRUNT_REPEL_LAND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_REPEL_LAND,
	
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_LAND"
		"		TASK_GET_PATH_TO_LASTPOSITION		0"
		"		TASK_RUN_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_CLEAR_LASTPOSITION				0"
		"	"
		"	Interrupts"
		"		COND_SEE_ENEMY"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_PLAYER"
	)
	
	//=========================================================
	// > SCHED_GRUNT_RELOAD
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_RELOAD,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_RELOAD"
		"	"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
	)
	
	//=========================================================
	// > SCHED_GRUNT_TAKE_COVER_FROM_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_TAKE_COVER_FROM_ENEMY,
	
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT						0.2"
		"		TASK_FIND_COVER_FROM_ENEMY		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_REMEMBER					MEMORY:INCOVER"
		"		TASK_FACE_ENEMY					0"
		"		TASK_WAIT						1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)
	
	//=========================================================
	// > SCHED_GRUNT_TAKE_COVER_FAILED
	// special schedule type that forces analysis of conditions and picks
	// the best possible schedule to recover from this type of failure.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_TAKE_COVER_FAILED,
		"	Tasks"
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_GRUNT_BARNACLE_HIT
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_BARNACLE_HIT,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_HIT"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_GRUNT_BARNACLE_PULL"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_GRUNT_BARNACLE_PULL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_BARNACLE_PULL,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_PULL"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_GRUNT_BARNACLE_CHOMP
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_BARNACLE_CHOMP,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_CHOMP"
		"		 TASK_SET_SCHEDULE			SCHEDULE:SCHED_GRUNT_BARNACLE_CHEW"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_GRUNT_BARNACLE_CHEW
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GRUNT_BARNACLE_CHEW,

		"	Tasks"
		"		 TASK_PLAY_SEQUENCE			ACTIVITY:ACT_BARNACLE_CHEW"
	)

AI_END_CUSTOM_NPC()


//=========================================================
// DEAD HGRUNT PROP
//=========================================================
class CNPC_DeadHGrunt : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_DeadHGrunt, CAI_BaseNPC );
public:
	void Spawn( void );
	Class_T	Classify ( void ) { return	CLASS_HUMAN_MILITARY; }
	float MaxYawSpeed( void ) { return 8.0f; }

	bool KeyValue( const char *szKeyName, const char *szValue );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[3];
};

char *CNPC_DeadHGrunt::m_szPoses[] = { "deadstomach", "deadside", "deadsitting" };

bool CNPC_DeadHGrunt::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "pose" ) )
		m_iPose = atoi( szValue );
	else 
		CAI_BaseNPC::KeyValue( szKeyName, szValue );

	return true;
}

LINK_ENTITY_TO_CLASS( monster_hgrunt_dead, CNPC_DeadHGrunt );

//=========================================================
// ********** DeadHGrunt SPAWN **********
//=========================================================
void CNPC_DeadHGrunt::Spawn( void )
{
	PrecacheModel("models/hgrunt.mdl");
	SetModel( "models/hgrunt.mdl" );

	ClearEffects();
	SetSequence( 0 );
	m_bloodColor		= BLOOD_COLOR_RED;

	SetSequence( LookupSequence( m_szPoses[m_iPose] ) );

	if ( GetSequence() == -1 )
	{
		Msg ( "Dead hgrunt with bad pose\n" );
	}

	// Corpses have less health
	m_iHealth			= 8;

	// map old bodies onto new bodies
	switch( m_nBody )
	{
	case 0: // Grunt with Gun
		m_nBody = 0;
		m_nSkin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_GRUNT );
		SetBodygroup( GUN_GROUP, GUN_MP5 );
		break;
	case 1: // Commander with Gun
		m_nBody = 0;
		m_nSkin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_COMMANDER );
		SetBodygroup( GUN_GROUP, GUN_MP5 );
		break;
	case 2: // Grunt no Gun
		m_nBody = 0;
		m_nSkin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_GRUNT );
		SetBodygroup( GUN_GROUP, GUN_NONE );
		break;
	case 3: // Commander no Gun
		m_nBody = 0;
		m_nSkin = 0;
		SetBodygroup( HEAD_GROUP, HEAD_COMMANDER );
		SetBodygroup( GUN_GROUP, GUN_NONE );
		break;
	}

	NPCInitDead();
}
