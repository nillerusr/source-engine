//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//=========================================================
// GMan - misunderstood servant of the people
//=========================================================

#include	"cbase.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include "ai_baseactor.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CNPC_GMan : public CAI_BaseActor
{
	DECLARE_CLASS( CNPC_GMan, CAI_BaseActor );
public:

	void Spawn( void );
	void Precache( void );
	float MaxYawSpeed( void ){ return 90.0f; }
	Class_T  Classify ( void );
	void HandleAnimEvent( animevent_t *pEvent );
	int GetSoundInterests ( void );

	bool IsInC5A1();

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );
	int	 OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
	void TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType);

	virtual int PlayScriptedSentence( const char *pszSentence, float duration, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener );
	
	EHANDLE m_hPlayer;
	EHANDLE m_hTalkTarget;
	float   m_flTalkTime;
};

LINK_ENTITY_TO_CLASS( monster_gman, CNPC_GMan );

//=========================================================
// Hack that tells us whether the GMan is in the final map
//=========================================================
bool CNPC_GMan::IsInC5A1()
{
	const char *pMapName = STRING(gpGlobals->mapname);

	if( pMapName )
	{
		return !Q_strnicmp( pMapName, "c5a1", 4 );
	}

	return false;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
Class_T	CNPC_GMan::Classify ( void )
{
	return	CLASS_NONE;
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_GMan::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 1:
	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// GetSoundInterests - generic monster can't hear.
//=========================================================
int CNPC_GMan::GetSoundInterests ( void )
{
	return	NULL;
}

//=========================================================
// Spawn
//=========================================================
void CNPC_GMan::Spawn()
{
	Precache();

	BaseClass::Spawn();

	SetModel( "models/gman.mdl" );

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_MECH );
	m_iHealth			= 8;
	m_flFieldOfView		= 0.5;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_OPEN_DOORS | bits_CAP_USE_WEAPONS | bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD);

	NPCInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_GMan::Precache()
{
	PrecacheModel( "models/gman.mdl" );
}	


//=========================================================
// AI Schedules Specific to this monster
//=========================================================


void CNPC_GMan::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_WAIT:
		if (m_hPlayer == NULL)
		{
			m_hPlayer = gEntList.FindEntityByClassname( NULL, "player" );
		}
		break;
	}

	BaseClass::StartTask( pTask );
}

void CNPC_GMan::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_WAIT:
		// look at who I'm talking to
		if (m_flTalkTime > gpGlobals->curtime && m_hTalkTarget != NULL)
		{
			AddLookTarget( m_hTalkTarget->GetAbsOrigin(), 1.0, 2.0 );
		}
		// look at player, but only if playing a "safe" idle animation
		else if (m_hPlayer != NULL && (GetSequence() == 0 || IsInC5A1()) )
		{
			 AddLookTarget( m_hPlayer->EyePosition(), 1.0, 3.0 );
		}
		else 
		{
			// Just center the head forward.
			Vector forward;
			GetVectors( &forward, NULL, NULL );

			AddLookTarget( GetAbsOrigin() + forward * 12.0f, 1.0, 1.0 );
			SetBoneController( 0, 0 );
		}
		BaseClass::RunTask( pTask );
		break;
	}

	SetBoneController( 0, 0 );
	BaseClass::RunTask( pTask );
}


//=========================================================
// Override all damage
//=========================================================
int CNPC_GMan::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	m_iHealth = m_iMaxHealth / 2; // always trigger the 50% damage aitrigger

	if ( inputInfo.GetDamage() > 0 )
		 SetCondition( COND_LIGHT_DAMAGE );
	
	if ( inputInfo.GetDamage() >= 20 )
		 SetCondition( COND_HEAVY_DAMAGE );
	
	return TRUE;
}


void CNPC_GMan::TraceAttack( CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType)
{
	g_pEffects->Ricochet( ptr->endpos, ptr->plane.normal );
//	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

int CNPC_GMan::PlayScriptedSentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, bool bConcurrent, CBaseEntity *pListener )
{
	BaseClass::PlayScriptedSentence( pszSentence, delay, volume, soundlevel, bConcurrent, pListener );

	m_flTalkTime = gpGlobals->curtime + delay;
	m_hTalkTarget = pListener;

	return 1;
}
