//=//=============================================================================//
//
// Purpose:		Passive plants that retract when threatened
//
// Author:		1upD
//
//=============================================================================//

#include "cbase.h"
#include "ai_squad.h"
#include "npcevent.h"
#include "npc_xenhair.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CNPC_XenHair )
DEFINE_FIELD( m_flHairVelocity, FIELD_FLOAT ),
DEFINE_FIELD( m_flIdealHeight, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_xenhair, CNPC_XenHair );

void CNPC_XenHair::Spawn()
{
	BaseClass::Spawn();
	SetHullType( HULL_TINY );
	// Hairs don't collide
	SetSolid( SOLID_NONE );
	SetGravity( 0.0f );

	m_iMaxHealth = 1000;
	m_iHealth = m_iMaxHealth;
	m_flHairVelocity = 0.0f;
	m_flIdealHeight = GetAbsOrigin().z;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_XenHair::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/hair.mdl" ) );
	}

	PrecacheScriptSound( "npc_xenhair.extend" );
	PrecacheScriptSound( "npc_xenhair.retract" );

	BaseClass::Precache();
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule.  
// OVERRIDDEN for custom lightstalk tasks
//=========================================================
void CNPC_XenHair::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_HAIR_EXTEND:
	{
		m_bIsRetracted = false;
		m_flHairVelocity = 16.0f;
		m_flIdealHeight = GetAbsOrigin().z + 64.0f;

		CBaseEntity *pActivator = GetRetractActivator();
		variant_t Val;
		Val.Set( FIELD_EHANDLE, pActivator );
		m_OnRise.CBaseEntityOutput::FireOutput( Val, pActivator, this );

		break;
	}
	case TASK_HAIR_RETRACT:
	{
		m_bIsRetracted = true;
		m_flHairVelocity = -16.0f;
		m_flIdealHeight = GetAbsOrigin().z - 64.0f;

		CBaseEntity *pActivator = GetRetractActivator();
		variant_t Val;
		Val.Set( FIELD_EHANDLE, pActivator );
		m_OnLower.CBaseEntityOutput::FireOutput( Val, pActivator, this );

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
void CNPC_XenHair::RunTask ( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_HAIR_EXTEND:
	case TASK_HAIR_RETRACT:
	{
		// If the velocity is positive, check if the current height is above or equal to the ideal height
		// If the velocity is negative, check if the current height is below or equal to the ideal height
		float currentHeight = GetAbsOrigin().z;
		if ( m_flHairVelocity > 0 ? currentHeight >=  m_flIdealHeight : currentHeight <= m_flIdealHeight)
		{
			TaskComplete();
		}

		SetAbsOrigin( GetAbsOrigin() + Vector( 0, 0, m_flHairVelocity ) );

		break;
	}
	default:
	{
		BaseClass::RunTask( pTask );
		break;
	}
	}
}

void CNPC_XenHair::AlertSound( void )
{
	if (m_bIsRetracted)
	{
		EmitSound( "npc_xenhair.extend" );
	}
	else
	{
		EmitSound( "npc_xenhair.retract" );
	}
}

AI_BEGIN_CUSTOM_NPC( npc_xenhair, CNPC_XenHair )

DECLARE_TASK( TASK_HAIR_EXTEND )
DECLARE_TASK( TASK_HAIR_RETRACT )

//=========================================================
// > SCHED_HAIR_RETRACT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HAIR_RETRACT,

	"	Tasks"
	"		TASK_SOUND_WAKE				0"
	"		TASK_HAIR_RETRACT		0"
	"	"
	"	Interrupts"
)

//=========================================================
// > SCHED_HAIR_EXTEND
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HAIR_EXTEND,

	"	Tasks"
	"		TASK_SOUND_WAKE				0"
	"		TASK_HAIR_EXTEND		0"
	"	"
	"	Interrupts"
)

AI_END_CUSTOM_NPC()