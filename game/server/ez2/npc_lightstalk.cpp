//=============================================================================//
//
// Purpose:		Glowing Xen plants that retract into the ground and go dark
//				when threatened.
//
//				They are able to alert squadmates when an intruder is near.
//
// Author:		1upD
//
//=============================================================================//

#include "cbase.h"
#include "ai_squad.h"
#include "npcevent.h"
#include "npc_lightstalk.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS( npc_lightstalk, CNPC_LightStalk );
LINK_ENTITY_TO_CLASS( npc_xenlight, CNPC_LightStalk );

BEGIN_DATADESC( CNPC_LightStalk )
	DEFINE_KEYFIELD( m_LightColor, FIELD_COLOR32, "lightcolor" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Return the glow attributes for a given index
//-----------------------------------------------------------------------------
EyeGlow_t * CNPC_LightStalk::GetEyeGlowData( int i )
{
	if (i != 0)
		return NULL;

	EyeGlow_t * eyeGlow = new EyeGlow_t();

	eyeGlow->spriteName = AllocPooledString("sprites/light_glow02.vmt");
	eyeGlow->attachment = AllocPooledString("0");

	eyeGlow->alpha = m_LightColor.a;
	eyeGlow->red = m_LightColor.r;
	eyeGlow->green = m_LightColor.g;
	eyeGlow->blue = m_LightColor.b;
	eyeGlow->scale = 0.3f;
	eyeGlow->proxyScale = 3.0f;
	eyeGlow->renderMode = kRenderGlow;
	return eyeGlow;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_LightStalk::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/light.mdl" ) );
	}

	m_bIsRetracted = false;

	PrecacheScriptSound( "npc_lightstalk.extend" );
	PrecacheScriptSound( "npc_lightstalk.retract" );

	BaseClass::Precache();
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule.  
// OVERRIDDEN for custom lightstalk tasks
//=========================================================
void CNPC_LightStalk::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_LIGHTSTALK_EXTEND:
	{
		m_bIsRetracted = false;
		StartEye();

		CBaseEntity *pActivator = GetRetractActivator();
		variant_t Val;
		Val.Set( FIELD_EHANDLE, pActivator );
		m_OnRise.CBaseEntityOutput::FireOutput( Val, pActivator, this );

		SetHullType( HULL_HUMAN );
		break;
	}
	case TASK_LIGHTSTALK_RETRACT:
	{
		m_bIsRetracted = true;
		KillSprites( 1.0f );

		CBaseEntity *pActivator = GetRetractActivator();
		variant_t Val;
		Val.Set( FIELD_EHANDLE, pActivator );
		m_OnLower.CBaseEntityOutput::FireOutput( Val, pActivator, this );

		SetHullType( HULL_TINY );
		break;
	}
	default:
	{
		BaseClass::StartTask( pTask );
		break;
	}
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_LightStalk::RunTask ( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_LIGHTSTALK_EXTEND:
	case TASK_LIGHTSTALK_RETRACT:
	{
		if (IsActivityFinished())
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

Activity CNPC_LightStalk::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_CROUCH || eNewActivity == ACT_STAND )
	{
		return eNewActivity;
	}

	if ( m_bIsRetracted )
	{
		return ACT_CROUCHIDLE;
	}
	else
	{
		return ACT_IDLE;
	}
}

void CNPC_LightStalk::AlertSound( void )
{
	if (m_bIsRetracted)
	{
		EmitSound( "npc_lightstalk.extend" );
	}
	else
	{
		EmitSound( "npc_lightstalk.retract" );
	}
}


AI_BEGIN_CUSTOM_NPC( npc_lightstalk, CNPC_LightStalk )

DECLARE_TASK( TASK_LIGHTSTALK_EXTEND )
DECLARE_TASK( TASK_LIGHTSTALK_RETRACT )

//=========================================================
// > SCHED_LIGHTSTALK_RETRACT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_LIGHTSTALK_RETRACT,

	"	Tasks"
	"		TASK_SOUND_WAKE				0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_CROUCH"
	"		TASK_LIGHTSTALK_RETRACT		0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_CROUCHIDLE"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_LIGHTSTALK_EXTEND
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_LIGHTSTALK_EXTEND,

	"	Tasks"
	"		TASK_SOUND_WAKE				0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_STAND"
	"		TASK_LIGHTSTALK_EXTEND		0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"	"
	"	Interrupts"
)

AI_END_CUSTOM_NPC()