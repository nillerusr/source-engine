//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

#include "ai_agent.h"
#include "datacache/imdlcache.h"
#include "isaverestore.h"
#include "game.h"
#include "env_debughistory.h"
#include "checksum_crc.h"

#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
//
// Crude frame timings
//

extern CFastTimer g_AIRunTimer;
extern CFastTimer g_AIPostRunTimer;

extern CFastTimer g_AIConditionsTimer;
extern CFastTimer g_AIPrescheduleThinkTimer;
extern CFastTimer g_AIMaintainScheduleTimer;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// ================================================================
//  Init static data
// ================================================================
CAI_ClassScheduleIdSpace	CAI_Agent::gm_ClassScheduleIdSpace( true );
CAI_GlobalScheduleNamespace CAI_Agent::gm_SchedulingSymbols;

// ================================================================
//  Class Methods
// ================================================================

//---------------------------------------------------------
//---------------------------------------------------------
#define InterruptFromCondition( iCondition ) \
	AI_RemapFromGlobal( ( AI_IdIsLocal( iCondition ) ? GetClassScheduleIdSpace()->ConditionLocalToGlobal( iCondition ) : iCondition ) )
	
void CAI_Agent::SetCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_Conditions.Set( interrupt );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_Agent::HasCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	bool bReturn = m_Conditions.IsBitSet(interrupt);
	return (bReturn);
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_Agent::HasCondition( int iCondition, bool bUseIgnoreConditions )
{
	if ( bUseIgnoreConditions )
		return HasCondition( iCondition );
	
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	bool bReturn = m_ConditionsPreIgnore.IsBitSet(interrupt);
	return (bReturn);
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_Agent::ClearCondition( int iCondition )
{
	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_Conditions.Clear(interrupt);
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_Agent::ClearConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_Conditions.Clear( interrupt );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_Agent::SetIgnoreConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_InverseIgnoreConditions.Clear( interrupt ); // clear means ignore
	}
}

void CAI_Agent::ClearIgnoreConditions( int *pConditions, int nConditions )
{
	for ( int i = 0; i < nConditions; ++i )
	{
		int iCondition = pConditions[i];
		int interrupt = InterruptFromCondition( iCondition );
		
		if ( interrupt == -1 )
		{
			Assert(0);
			continue;
		}
		
		m_InverseIgnoreConditions.Set( interrupt ); // set means don't ignore
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_Agent::HasInterruptCondition( int iCondition )
{
	if( !GetCurSchedule() )
	{
		return false;
	}

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( m_Conditions.IsBitSet( interrupt ) && GetCurSchedule()->HasInterrupt( interrupt ) );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_Agent::ConditionInterruptsCurSchedule( int iCondition )
{	
	if( !GetCurSchedule() )
	{
		return false;
	}

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( GetCurSchedule()->HasInterrupt( interrupt ) );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CAI_Agent::ConditionInterruptsSchedule( int localScheduleID, int iCondition )
{
	CAI_Schedule *pSchedule = GetSchedule( localScheduleID );
	if ( !pSchedule )
		return false;

	int interrupt = InterruptFromCondition( iCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	return ( pSchedule->HasInterrupt( interrupt ) );
}


//-----------------------------------------------------------------------------
// Returns whether we currently have any interrupt conditions that would
// interrupt the given schedule.
//-----------------------------------------------------------------------------
bool CAI_Agent::HasConditionsToInterruptSchedule( int nLocalScheduleID )
{
	CAI_Schedule *pSchedule = GetSchedule( nLocalScheduleID );
	if ( !pSchedule )
		return false;

	CAI_ScheduleBits bitsMask;
	pSchedule->GetInterruptMask( &bitsMask );

	CAI_ScheduleBits bitsOut;
	AccessConditionBits().And( bitsMask, &bitsOut );
	
	return !bitsOut.IsAllClear();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CAI_Agent::IsCustomInterruptConditionSet( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return false;
	}
	
	return m_CustomInterruptConditions.IsBitSet( interrupt );
}

//-----------------------------------------------------------------------------
// Purpose: Sets a flag in the custom interrupt flags, translating the condition
//			to the proper global space, if necessary
//-----------------------------------------------------------------------------
void CAI_Agent::SetCustomInterruptCondition( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_CustomInterruptConditions.Set( interrupt );
}

//-----------------------------------------------------------------------------
// Purpose: Clears a flag in the custom interrupt flags, translating the condition
//			to the proper global space, if necessary
//-----------------------------------------------------------------------------
void CAI_Agent::ClearCustomInterruptCondition( int nCondition )
{
	int interrupt = InterruptFromCondition( nCondition );
	
	if ( interrupt == -1 )
	{
		Assert(0);
		return;
	}
	
	m_CustomInterruptConditions.Clear( interrupt );
}


//-----------------------------------------------------------------------------
// Purpose: Clears all the custom interrupt flags.
//-----------------------------------------------------------------------------
void CAI_Agent::ClearCustomInterruptConditions()
{
	m_CustomInterruptConditions.ClearAll();
}

//-----------------------------------------------------------------------------

bool CAI_Agent::PreThink( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// NPC Think - calls out to core AI functions and handles this
// npc's specific animation events
//

void CAI_Agent::Think( void )
{
	if ( PreThink() )
	{
		RunAI();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Virtual function that allows us to have any npc ignore a set of
// shared conditions.
//
//-----------------------------------------------------------------------------
void CAI_Agent::RemoveIgnoredConditions( void )
{
	m_ConditionsPreIgnore = m_Conditions;
	m_Conditions.And( m_InverseIgnoreConditions, &m_Conditions );
}

//-----------------------------------------------------------------------------

void CAI_Agent::GatherConditions( void )
{
	m_bConditionsGathered = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_Agent::PrescheduleThink( void )
{
}

//-----------------------------------------------------------------------------
// Main entry point for processing AI
//-----------------------------------------------------------------------------

void CAI_Agent::RunAI( void )
{
	AI_PROFILE_SCOPE(CAI_Agent_RunAI);
	g_AIRunTimer.Start();

	m_bConditionsGathered = false;

	AI_PROFILE_SCOPE_BEGIN(CAI_Agent_RunAI_GatherConditions);
	GatherConditions();
	RemoveIgnoredConditions();
	AI_PROFILE_SCOPE_END();

	if ( !m_bConditionsGathered )
		m_bConditionsGathered = true; // derived class didn't call to base

	g_AIPrescheduleThinkTimer.Start();

	AI_PROFILE_SCOPE_BEGIN(CAI_RunAI_PrescheduleThink);
	PrescheduleThink();
	AI_PROFILE_SCOPE_END();

	g_AIPrescheduleThinkTimer.End();
	
	MaintainSchedule();

	PostscheduleThink();
				  
	ClearTransientConditions();

	g_AIRunTimer.End();
}

//-----------------------------------------------------------------------------
void CAI_Agent::ClearTransientConditions()
{
}



//=========================================================
// NPCInit - after a npc is spawned, it needs to
// be dropped into the world, checked for mobility problems,
// and put on the proper path, if any. This function does
// all of those things after the npc spawns. Any
// initialization that should take place for all npcs
// goes here.
//=========================================================
void CAI_Agent::Init( void )
{
	// Clear conditions
	m_Conditions.ClearAll();

	// NOTE: Can't call NPC Init Think directly... logic changed about
	// what time it is when worldspawn happens..

	// We must put off the rest of our initialization
	// until we're sure everything else has had a chance to spawn. Otherwise
	// we may try to reference entities that haven't spawned yet.(sjb)
	ForceGatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_Agent::TaskComplete(  bool fIgnoreSetFailedCondition )
{
// 	EndTaskOverlay();

	// Handy thing to use for debugging
	//if (IsCurSchedule(SCHED_PUT_HERE) &&
	//	GetTask()->iTask == TASK_PUT_HERE)
	//{
	//	int put_breakpoint_here = 5;
	//}

	if ( fIgnoreSetFailedCondition || !HasCondition(COND_TASK_FAILED) )
	{
		SetTaskStatus( TASKSTATUS_COMPLETE );
	}
}

void CAI_Agent::TaskMovementComplete( void )
{
	switch( GetTaskStatus() )
	{
	case TASKSTATUS_NEW:
	case TASKSTATUS_RUN_MOVE_AND_TASK:
		SetTaskStatus( TASKSTATUS_RUN_TASK );
		break;

	case TASKSTATUS_RUN_MOVE:
		TaskComplete();
		break;

	case TASKSTATUS_RUN_TASK:
		// FIXME: find out how to safely restart movement
		//Warning( "Movement completed twice!\n" );
		//Assert( 0 );
		break;

	case TASKSTATUS_COMPLETE:
		break;
	}
}


int CAI_Agent::TaskIsRunning( void )
{
	if ( GetTaskStatus() != TASKSTATUS_COMPLETE &&
		 GetTaskStatus() != TASKSTATUS_RUN_MOVE )
		 return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_Agent::TaskFail( AI_TaskFailureCode_t code )
{
// 	EndTaskOverlay();

	// Handy tool for debugging
	//if (IsCurSchedule(SCHED_PUT_NAME_HERE))
	//{
	//	int put_breakpoint_here = 5;
	//}

	// If in developer mode save the fail text for debug output
	if (g_pDeveloper->GetInt())
	{
		m_failText = TaskFailureToString( code );

		m_interuptSchedule	= NULL;
		m_failedSchedule    = GetCurSchedule();

		if (GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
		{
			DevMsg(this, AIMF_IGNORE_SELECTED, "      TaskFail -> %s\n", m_failText );
		}

		ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d):       TaskFail -> %s\n", GetDebugName(), entindex(), m_failText ) );

		//AddTimedOverlay( fail_text, 5);
	}

	m_ScheduleState.taskFailureCode = code;
	SetCondition(COND_TASK_FAILED);
}



//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CAI_Agent::DrawDebugTextOverlays( int text_offset )
{
	if (GetDebugOverlayFlags() & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];

		// --------------
		// Print Schedule
		// --------------
		if ( GetCurSchedule() )
		{
			const char *pName = NULL;
			pName = GetCurSchedule()->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}
			Q_snprintf(tempstr,sizeof(tempstr),"Schd: %s, ", pName );
			EntityText(text_offset,tempstr,0);
			text_offset++;

			if (GetDebugOverlayFlags() & OVERLAY_NPC_TASK_BIT)
			{
				for (int i = 0 ; i < GetCurSchedule()->NumTasks(); i++)
				{
					Q_snprintf(tempstr,sizeof(tempstr),"%s%s%s%s",
						((i==0)					? "Task:":"       "),
						((i==GetScheduleCurTaskIndex())	? "->"   :"   "),
						TaskName(GetCurSchedule()->GetTaskList()[i].iTask),
						((i==GetScheduleCurTaskIndex())	? "<-"   :""));

					EntityText(text_offset,tempstr,0);
					text_offset++;
				}
			}
			else
			{
				const Task_t *pTask = GetTask();
				if ( pTask )
				{
					Q_snprintf(tempstr,sizeof(tempstr),"Task: %s (#%d), ", TaskName(pTask->iTask), GetScheduleCurTaskIndex() );
				}
				else
				{
					Q_strncpy(tempstr,"Task: None",sizeof(tempstr));
				}
				EntityText(text_offset,tempstr,0);
				text_offset++;
			}
		}

		//
		// Print all the current conditions.
		//
		if (GetDebugOverlayFlags() & OVERLAY_NPC_CONDITIONS_BIT)
		{
			bool bHasConditions = false;
			for (int i = 0; i < MAX_CONDITIONS; i++)
			{
				if (m_Conditions.IsBitSet(i))
				{
					Q_snprintf(tempstr, sizeof(tempstr), "Cond: %s\n", ConditionName(AI_RemapToGlobal(i)));
					EntityText(text_offset, tempstr, 0);
					text_offset++;
					bHasConditions = true;
				}
			}
			if (!bHasConditions)
			{
				Q_snprintf(tempstr,sizeof(tempstr),"(no conditions)");
				EntityText(text_offset,tempstr,0);
				text_offset++;
			}
		}

		// --------------
		// Print Interrupte
		// --------------
		if (m_interuptSchedule)
		{
			const char *pName = NULL;
			pName = m_interuptSchedule->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}

			Q_snprintf(tempstr,sizeof(tempstr),"Intr: %s (%s)\n", pName, m_interruptText );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

		// --------------
		// Print Failure
		// --------------
		if (m_failedSchedule)
		{
			const char *pName = NULL;
			pName = m_failedSchedule->GetName();
			if ( !pName )
			{
				pName = "Unknown";
			}
			Q_snprintf(tempstr,sizeof(tempstr),"Fail: %s (%s)\n", pName,m_failText );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}

	}
	return text_offset;
}

//------------------------------------------------------------------------------
// Purpose : Add new entity positioned overlay text
// Input   : How many lines to offset text from origin
//			 The text to print
//			 How long to display text
//			 The color of the text
// Output  :
//------------------------------------------------------------------------------
void CAI_Agent::EntityText( int text_offset, const char *text, float duration, int r, int g, int b, int a )
{
	NDebugOverlay::EntityTextAtPosition( m_vecAgentDebugOverlaysPos, text_offset, text, duration, r, g, b, a );
}


//=========================================================
//=========================================================
void CAI_Agent::OnScheduleChange ( void )
{
// 	EndTaskOverlay();
}




// Global Savedata for npc
//
// This should be an exact copy of the var's in the header.  Fields
// that aren't save/restored are commented out

BEGIN_SIMPLE_DATADESC( CAI_Agent )

	//								m_pSchedule  (reacquired on restore)
	DEFINE_EMBEDDED( m_ScheduleState ),
	DEFINE_FIELD( m_IdealSchedule,				FIELD_INTEGER ), // handled specially but left in for "virtual" schedules
	DEFINE_FIELD( m_failSchedule,				FIELD_INTEGER ), // handled specially but left in for "virtual" schedules
	//								m_Conditions (custom save)
	//								m_CustomInterruptConditions (custom save)
	//								m_ConditionsPreIgnore (custom save)
	//								m_InverseIgnoreConditions (custom save)
	DEFINE_FIELD( m_bForceConditionsGather,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bConditionsGathered,		FIELD_BOOLEAN ),

	//							m_fIsUsingSmallHull			TODO -- This needs more consideration than simple save/load
	// 							m_failText					DEBUG
	// 							m_interruptText				DEBUG
	// 							m_failedSchedule			DEBUG
	// 							m_interuptSchedule			DEBUG
	// 							m_nDebugCurIndex			DEBUG

	// 							m_LastShootAccuracy			DEBUG
	// 							m_RecentShotAccuracy		DEBUG
	// 							m_TotalShots				DEBUG
	// 							m_TotalHits					DEBUG
	//							m_bSelected					DEBUG
	// 							m_TimeLastShotMark			DEBUG
	//							m_bDeferredNavigation

END_DATADESC()

BEGIN_SIMPLE_DATADESC( AIAgentScheduleState_t )
	DEFINE_FIELD( iCurTask,				FIELD_INTEGER ),
	DEFINE_FIELD( fTaskStatus,			FIELD_INTEGER ),
	DEFINE_FIELD( timeStarted,			FIELD_TIME ),
	DEFINE_FIELD( timeCurTaskStarted,	FIELD_TIME ),
	DEFINE_FIELD( taskFailureCode,		FIELD_INTEGER ),
	DEFINE_FIELD( iTaskInterrupt,		FIELD_INTEGER ),
	DEFINE_FIELD( bScheduleWasInterrupted, FIELD_BOOLEAN ),
END_DATADESC()

//-----------------------------------------------------------------------------

const short AI_EXTENDED_SAVE_HEADER_VERSION = 5;
const short AI_EXTENDED_SAVE_HEADER_RESET_VERSION = 3;

const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_CONDITIONS = 2;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SCHEDULE_ID_FIXUP = 3;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SEQUENCE = 4;
const short AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_NAVIGATOR_SAVE = 5;

struct AIAgentSaveHeader_t
{
	AIAgentSaveHeader_t()
	 :	version(AI_EXTENDED_SAVE_HEADER_VERSION), 
		flags(0),
		scheduleCrc(0)
	{
		szSchedule[0] = 0;
		szIdealSchedule[0] = 0;
		szFailSchedule[0] = 0;
		szSequence[0] = 0;
	}

	short version;
	unsigned flags;
	char szSchedule[128];
	CRC32_t scheduleCrc;
	char szIdealSchedule[128];
	char szFailSchedule[128];
	char szSequence[128];
	
	DECLARE_SIMPLE_DATADESC();
};

//-------------------------------------

BEGIN_SIMPLE_DATADESC( AIAgentSaveHeader_t )
	DEFINE_FIELD( 		version,		FIELD_SHORT ),
	DEFINE_FIELD( 		flags,			FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY(	szSchedule,		FIELD_CHARACTER ),
	DEFINE_FIELD( 		scheduleCrc,	FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY(	szIdealSchedule,	FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY(	szFailSchedule,		FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY(	szSequence,		FIELD_CHARACTER ),
END_DATADESC()

//-------------------------------------

int CAI_Agent::Save( ISave &save )
{
	AIAgentSaveHeader_t saveHeader;
	
	if ( m_pSchedule )
	{
		const char *pszSchedule = m_pSchedule->GetName();

		Assert( Q_strlen( pszSchedule ) < sizeof( saveHeader.szSchedule ) - 1 );
		Q_strncpy( saveHeader.szSchedule, pszSchedule, sizeof( saveHeader.szSchedule ) );

		CRC32_Init( &saveHeader.scheduleCrc );
		CRC32_ProcessBuffer( &saveHeader.scheduleCrc, (void *)m_pSchedule->GetTaskList(), m_pSchedule->NumTasks() * sizeof(Task_t) );
		CRC32_Final( &saveHeader.scheduleCrc );
	}
	else
	{
		saveHeader.szSchedule[0] = 0;
		saveHeader.scheduleCrc = 0;
	}

	int idealSchedule = GetGlobalScheduleId( m_IdealSchedule );

	if ( idealSchedule != -1 && idealSchedule != AI_RemapToGlobal( SCHED_NONE ) )
	{
		CAI_Schedule *pIdealSchedule = GetSchedule( m_IdealSchedule );
		if ( pIdealSchedule )
		{
			const char *pszIdealSchedule = pIdealSchedule->GetName();
			Assert( Q_strlen( pszIdealSchedule ) < sizeof( saveHeader.szIdealSchedule ) - 1 );
			Q_strncpy( saveHeader.szIdealSchedule, pszIdealSchedule, sizeof( saveHeader.szIdealSchedule ) );
		}
	}

	int failSchedule = GetGlobalScheduleId( m_failSchedule );
	if ( failSchedule != -1 && failSchedule != AI_RemapToGlobal( SCHED_NONE ) )
	{
		CAI_Schedule *pFailSchedule = GetSchedule( m_failSchedule );
		if ( pFailSchedule )
		{
			const char *pszFailSchedule = pFailSchedule->GetName();
			Assert( Q_strlen( pszFailSchedule ) < sizeof( saveHeader.szFailSchedule ) - 1 );
			Q_strncpy( saveHeader.szFailSchedule, pszFailSchedule, sizeof( saveHeader.szFailSchedule ) );
		}
	}

	save.WriteAll( &saveHeader );

	save.StartBlock();
	SaveConditions( save, m_Conditions );
	SaveConditions( save, m_CustomInterruptConditions );
	SaveConditions( save, m_ConditionsPreIgnore );
	CAI_ScheduleBits ignoreConditions;
	m_InverseIgnoreConditions.Not( &ignoreConditions );
	SaveConditions( save, ignoreConditions );
	save.EndBlock();

	return 1;
}

//-------------------------------------

void CAI_Agent::DiscardScheduleState()
{
	// We don't save/restore schedules yet
	ClearSchedule( "Restoring NPC" );

	m_Conditions.ClearAll();
}

//-------------------------------------

int CAI_Agent::Restore( IRestore &restore )
{
	AIAgentSaveHeader_t saveHeader;
	restore.ReadAll( &saveHeader );

	restore.StartBlock();
	RestoreConditions( restore, &m_Conditions );
	RestoreConditions( restore, &m_CustomInterruptConditions );
	RestoreConditions( restore, &m_ConditionsPreIgnore );
	CAI_ScheduleBits ignoreConditions;
	RestoreConditions( restore, &ignoreConditions );
	ignoreConditions.Not( &m_InverseIgnoreConditions );
	restore.EndBlock();

#ifdef TODO
	// do a normal restore
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;
#else
	int status = 1;
#endif

	// Do schedule fix-up
	if ( saveHeader.version >= AI_EXTENDED_SAVE_HEADER_FIRST_VERSION_WITH_SCHEDULE_ID_FIXUP )
	{
		if ( saveHeader.szIdealSchedule[0] )
		{
			CAI_Schedule *pIdealSchedule = g_AI_AgentSchedulesManager.GetScheduleByName( saveHeader.szIdealSchedule );
			m_IdealSchedule = ( pIdealSchedule ) ? pIdealSchedule->GetId() : SCHED_NONE;
		}

		if ( saveHeader.szFailSchedule[0] )
		{
			CAI_Schedule *pFailSchedule = g_AI_AgentSchedulesManager.GetScheduleByName( saveHeader.szFailSchedule );
			m_failSchedule = ( pFailSchedule ) ? pFailSchedule->GetId() : SCHED_NONE;
		}
	}

	bool bDiscardScheduleState = ( saveHeader.szSchedule[0] == 0 ||
								   saveHeader.version < AI_EXTENDED_SAVE_HEADER_RESET_VERSION );

	if ( m_ScheduleState.taskFailureCode >= NUM_FAIL_CODES )
		m_ScheduleState.taskFailureCode = FAIL_NO_TARGET; // must have been a string, gotta punt

	if ( !bDiscardScheduleState )
	{
		m_pSchedule = g_AI_AgentSchedulesManager.GetScheduleByName( saveHeader.szSchedule );
		if ( m_pSchedule )
		{
			CRC32_t scheduleCrc;
			CRC32_Init( &scheduleCrc );
			CRC32_ProcessBuffer( &scheduleCrc, (void *)m_pSchedule->GetTaskList(), m_pSchedule->NumTasks() * sizeof(Task_t) );
			CRC32_Final( &scheduleCrc );

			if ( scheduleCrc != saveHeader.scheduleCrc )
			{
				m_pSchedule = NULL;
			}
		}
	}

	if ( !m_pSchedule )
		bDiscardScheduleState = true;

	if ( bDiscardScheduleState )
	{
		DiscardScheduleState();
	}

	return status;
}

//-------------------------------------

void CAI_Agent::SaveConditions( ISave &save, const CAI_ScheduleBits &conditions )
{
	for (int i = 0; i < MAX_CONDITIONS; i++)
	{
		if (conditions.IsBitSet(i))
		{
			const char *pszConditionName = ConditionName(AI_RemapToGlobal(i));
			if ( !pszConditionName )
				break;
			save.WriteString( pszConditionName );
		}
	}
	save.WriteString( "" );
}

//-------------------------------------

void CAI_Agent::RestoreConditions( IRestore &restore, CAI_ScheduleBits *pConditions )
{
	pConditions->ClearAll();
	char szCondition[256];
	for (;;)
	{
		restore.ReadString( szCondition, sizeof(szCondition), 0 );
		if ( !szCondition[0] )
			break;
		int iCondition = GetSchedulingSymbols()->ConditionSymbolToId( szCondition );
		if ( iCondition != -1 )
			pConditions->Set( AI_RemapFromGlobal( iCondition ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Written by subclasses macro to load schedules
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CAI_Agent::LoadSchedules(void)
{
	return true;
}

//-----------------------------------------------------------------------------

bool CAI_Agent::LoadedSchedules(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAI_Agent::CAI_Agent(void)
{
	m_pSchedule = NULL;
	m_IdealSchedule = SCHED_NONE;

	// ----------------------------
	//  Debugging fields
	// ----------------------------
	m_interruptText				= NULL;
	m_failText					= NULL;
	m_failedSchedule			= NULL;
	m_interuptSchedule			= NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAI_Agent::~CAI_Agent(void)
{
}


//-----------------------------------------------------------------------------

static void AIMsgGuts( CAI_Agent *pAI, unsigned flags, const char *pszMsg )
{
// 	int			len		= strlen( pszMsg );
// 	const char *pszFmt2 = NULL;
// 
// 	if ( len && pszMsg[len-1] == '\n' )
// 	{
// 		(const_cast<char *>(pszMsg))[len-1] = 0;
// 		pszFmt2 = "%s (%s: %d/%s) [%d]\n";
// 	}
// 	else
// 		pszFmt2 = "%s (%s: %d/%s) [%d]";
// 	
// 	DevMsg( pszFmt2, 
// 		 pszMsg, 
// 		 pAI->GetClassname(),
// 		 pAI->entindex(),
// 		 ( pAI->GetEntityName() == NULL_STRING ) ? "<unnamed>" : STRING(pAI->GetEntityName()),
// 		 gpGlobals->tickcount );
}

void DevMsg( CAI_Agent *pAI, unsigned flags, const char *pszFormat, ... )
{
	if ( (flags & AIMF_IGNORE_SELECTED) || (pAI->GetDebugOverlayFlags() & OVERLAY_NPC_SELECTED_BIT) )
	{
		AIMsgGuts( pAI, flags, CFmtStr( &pszFormat ) );
	}
}

//-----------------------------------------------------------------------------

void DevMsg( CAI_Agent *pAI, const char *pszFormat, ... )
{
	if ( (pAI->GetDebugOverlayFlags() & OVERLAY_NPC_SELECTED_BIT) )
	{
		AIMsgGuts( pAI, 0, CFmtStr( &pszFormat ) );
	}
}


//=========================================================
// GetScheduleOfType - returns a pointer to one of the
// NPC's available schedules of the indicated type.
//=========================================================
CAI_Schedule *CAI_Agent::GetScheduleOfType( int scheduleType )
{
	// allow the derived classes to pick an appropriate version of this schedule or override
	// base schedule types.
	AI_PROFILE_SCOPE_BEGIN(CAI_BaseNPC_TranslateSchedule);
	scheduleType = TranslateSchedule( scheduleType );
	AI_PROFILE_SCOPE_END();

	// Get a pointer to that schedule
	CAI_Schedule *schedule = GetSchedule(scheduleType);

	if (!schedule)
	{
		//DevMsg( "GetScheduleOfType(): No CASE for Schedule Type %d!\n", scheduleType );
		return GetSchedule(SCHED_NONE);
	}
	return schedule;
}

CAI_Schedule *CAI_Agent::GetSchedule(int schedule)
{
	if ( schedule < NEXT_SCHEDULE )
	{
		return NULL;
	}

	if (!GetClassScheduleIdSpace()->IsGlobalBaseSet())
	{
		Warning("ERROR: %s missing schedule!\n", GetSchedulingErrorName());
		return g_AI_AgentSchedulesManager.GetScheduleFromID(SCHED_NONE);
	}
	if ( AI_IdIsLocal( schedule ) )
	{
		schedule = GetClassScheduleIdSpace()->ScheduleLocalToGlobal(schedule);
	}

	return g_AI_AgentSchedulesManager.GetScheduleFromID( schedule );
}

bool CAI_Agent::IsCurSchedule( int schedId, bool fIdeal )	
{ 
	if ( !m_pSchedule )
		return ( schedId == SCHED_NONE || schedId == AI_RemapToGlobal(SCHED_NONE) );

	schedId = ( AI_IdIsLocal( schedId ) ) ? GetClassScheduleIdSpace()->ScheduleLocalToGlobal(schedId) : schedId;
	if ( fIdeal )
		return ( schedId == m_IdealSchedule );

	return ( m_pSchedule->GetId() == schedId ); 
}


const char* CAI_Agent::ConditionName(int conditionID)
{
	if ( AI_IdIsLocal( conditionID ) )
		conditionID = GetClassScheduleIdSpace()->ConditionLocalToGlobal(conditionID);
	return GetSchedulingSymbols()->ConditionIdToSymbol(conditionID);
}

const char *CAI_Agent::TaskName(int taskID)
{
	if ( AI_IdIsLocal( taskID ) )
		taskID = GetClassScheduleIdSpace()->TaskLocalToGlobal(taskID);
	return GetSchedulingSymbols()->TaskIdToSymbol( taskID );
}



extern ConVar ai_task_pre_script;
extern ConVar ai_use_efficiency;
extern ConVar ai_use_think_optimizations;
#define ShouldUseEfficiency() ( ai_use_think_optimizations.GetBool() && ai_use_efficiency.GetBool() )

extern ConVar	ai_simulate_task_overtime;

#define MAX_TASKS_RUN 10

struct AgentTaskTimings
{
	const char *pszTask;
	CFastTimer selectSchedule;
	CFastTimer startTimer;
	CFastTimer runTimer;
};

AgentTaskTimings g_AIAgentTaskTimings[MAX_TASKS_RUN];
int			g_nAIAgentTasksRun;

void CAI_Agent::DumpTaskTimings()
{
	DevMsg(" Tasks timings:\n" );
	for ( int i = 0; i < g_nAIAgentTasksRun; ++i )
	{
		DevMsg( "   %32s -- select %5.2f, start %5.2f, run %5.2f\n", g_AIAgentTaskTimings[i].pszTask,
			g_AIAgentTaskTimings[i].selectSchedule.GetDuration().GetMillisecondsF(),
			g_AIAgentTaskTimings[i].startTimer.GetDuration().GetMillisecondsF(),
			g_AIAgentTaskTimings[i].runTimer.GetDuration().GetMillisecondsF() );

	}
}


//=========================================================
// FHaveSchedule - Returns true if NPC's GetCurSchedule()
// is anything other than NULL.
//=========================================================
bool CAI_Agent::FHaveSchedule( void )
{
	if ( GetCurSchedule() == NULL )
	{
		return false;
	}

	return true;
}

//=========================================================
// ClearSchedule - blanks out the caller's schedule pointer
// and index.
//=========================================================
void CAI_Agent::ClearSchedule( const char *szReason )
{
	if (szReason && GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
	{
		DevMsg( this, AIMF_IGNORE_SELECTED, "  Schedule cleared: %s\n", szReason );
	}

	if ( szReason )
	{
		ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs( "%s(%d):  Schedule cleared: %s\n", GetDebugName(), entindex(), szReason ) );
	}

	m_ScheduleState.timeCurTaskStarted = m_ScheduleState.timeStarted = 0;
	m_ScheduleState.bScheduleWasInterrupted = true;
	SetTaskStatus( TASKSTATUS_NEW );
	m_IdealSchedule = SCHED_NONE;
	m_pSchedule =  NULL;
	ResetScheduleCurTaskIndex();
	m_InverseIgnoreConditions.SetAll();
}

//=========================================================
// FScheduleDone - Returns true if the caller is on the
// last task in the schedule
//=========================================================
bool CAI_Agent::FScheduleDone ( void )
{
	Assert( GetCurSchedule() != NULL );

	if ( GetScheduleCurTaskIndex() == GetCurSchedule()->NumTasks() )
	{
		return true;
	}

	return false;
}

//=========================================================

bool CAI_Agent::SetSchedule( int localScheduleID ) 			
{ 
	CAI_Schedule *pNewSchedule = GetScheduleOfType( localScheduleID );
	if ( pNewSchedule )
	{
		m_IdealSchedule = GetGlobalScheduleId( localScheduleID );
		SetSchedule( pNewSchedule ); 
		return true;
	}
	return false;
}


//=========================================================
// SetSchedule - replaces the NPC's schedule pointer
// with the passed pointer, and sets the ScheduleIndex back
// to 0
//=========================================================
#define SCHEDULE_HISTORY_SIZE	10
void CAI_Agent::SetSchedule( CAI_Schedule *pNewSchedule )
{
	//Assert( pNewSchedule != NULL );

	m_ScheduleState.timeCurTaskStarted = m_ScheduleState.timeStarted = gpGlobals->curtime;
	m_ScheduleState.bScheduleWasInterrupted = false;

	m_pSchedule = pNewSchedule ;
	ResetScheduleCurTaskIndex();
	SetTaskStatus( TASKSTATUS_NEW );
	m_failSchedule = SCHED_NONE;
	m_Conditions.ClearAll();
	m_bConditionsGathered = false;
	m_InverseIgnoreConditions.SetAll();

	// this is very useful code if you can isolate a test case in a level with a single NPC. It will notify
	// you of every schedule selection the NPC makes.

	if( pNewSchedule != NULL )
	{
		if (GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
		{
			DevMsg(this, AIMF_IGNORE_SELECTED, "Schedule: %s (time: %.2f)\n", pNewSchedule->GetName(), gpGlobals->curtime );
		}

		ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d): Schedule: %s (time: %.2f)\n", GetDebugName(), entindex(), pNewSchedule->GetName(), gpGlobals->curtime ) );
	}
}

//=========================================================
// NextScheduledTask - increments the ScheduleIndex
//=========================================================
void CAI_Agent::NextScheduledTask ( void )
{
	Assert( GetCurSchedule() != NULL );

	SetTaskStatus( TASKSTATUS_NEW );
	IncScheduleCurTaskIndex();

	if ( FScheduleDone() )
	{
		// Reset memory of failed schedule 
		m_failedSchedule   = NULL;
		m_interuptSchedule = NULL;

		// just completed last task in schedule, so make it invalid by clearing it.
		SetCondition( COND_SCHEDULE_DONE );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This function allows NPCs to modify the interrupt mask for the
//			current schedule. This enables them to use base schedules but with
//			different interrupt conditions. Implement this function in your
//			derived class, and Set or Clear condition bits as you please.
//
//			NOTE: Always call the base class in your implementation, but be
//				  aware of the difference between changing the bits before vs.
//				  changing the bits after calling the base implementation.
//
// Input  : pBitString - Receives the updated interrupt mask.
//-----------------------------------------------------------------------------
void CAI_Agent::BuildScheduleTestBits( void )
{
	//NOTENOTE: Always defined in the leaf classes
}


//=========================================================
// IsScheduleValid - returns true as long as the current
// schedule is still the proper schedule to be executing,
// taking into account all conditions
//=========================================================
bool CAI_Agent::IsScheduleValid()
{
	if ( GetCurSchedule() == NULL || GetCurSchedule()->NumTasks() == 0 )
	{
		return false;
	}

	//Start out with the base schedule's set interrupt conditions
	GetCurSchedule()->GetInterruptMask( &m_CustomInterruptConditions );

	if ( !m_CustomInterruptConditions.IsBitSet( COND_NO_CUSTOM_INTERRUPTS ) )
	{
		BuildScheduleTestBits();
	}

	// This is like: m_CustomInterruptConditions &= m_Conditions;
	CAI_ScheduleBits testBits;
	m_CustomInterruptConditions.And( m_Conditions, &testBits  );

	if (!testBits.IsAllClear()) 
	{
		// If in developer mode save the interrupt text for debug output
		if (g_pDeveloper->GetInt()) 
		{
			// Reset memory of failed schedule 
			m_failedSchedule   = NULL;
			m_interuptSchedule = GetCurSchedule();

			// Find the first non-zero bit
			for (int i=0;i<MAX_CONDITIONS;i++)
			{
				if (testBits.IsBitSet(i))
				{
					m_interruptText = ConditionName( AI_RemapToGlobal( i ) );
					if (!m_interruptText)
					{
						m_interruptText = "(UNKNOWN CONDITION)";
						/*
						static const char *pError = "ERROR: Unknown condition!";
						DevMsg("%s (%s)\n", pError, GetDebugName());
						m_interruptText = pError;
						*/
					}

					if (GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
					{
						DevMsg( this, AIMF_IGNORE_SELECTED, "      Break condition -> %s\n", m_interruptText );
					}

					ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d):      Break condition -> %s\n", GetDebugName(), entindex(), m_interruptText ) );

					break;
				}
			}
		}

		return false;
	}

	if ( HasCondition(COND_SCHEDULE_DONE) || 
		HasCondition(COND_TASK_FAILED)   )
	{
		// some condition has interrupted the schedule, or the schedule is done
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether or not SelectIdealState() should be called before
//			a NPC selects a new schedule. 
//
//			NOTE: This logic was a source of pure, distilled trouble in Half-Life.
//			If you change this function, please supply good comments.
//
// Output : Returns true if yes, false if no
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Returns a new schedule based on current condition bits.
//-----------------------------------------------------------------------------
CAI_Schedule *CAI_Agent::GetNewSchedule( void )
{
	int scheduleType;

	//
	// Schedule selection code here overrides all leaf schedule selection.
	//
	scheduleType = SelectSchedule();

	m_IdealSchedule = GetGlobalScheduleId( scheduleType );

	return GetScheduleOfType( scheduleType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CAI_Schedule *CAI_Agent::GetFailSchedule( void )
{
	int prevSchedule;
	int failedTask;

	if ( GetCurSchedule() )
		prevSchedule = GetLocalScheduleId( GetCurSchedule()->GetId() );
	else
		prevSchedule = SCHED_NONE;

	const Task_t *pTask = GetTask();
	if ( pTask )
		failedTask = pTask->iTask;
	else
		failedTask = TASK_INVALID;

	Assert( AI_IdIsLocal( prevSchedule ) );
	Assert( AI_IdIsLocal( failedTask ) );

	int scheduleType = SelectFailSchedule( prevSchedule, failedTask, m_ScheduleState.taskFailureCode );
	return GetScheduleOfType( scheduleType );
}


//=========================================================
// MaintainSchedule - does all the per-think schedule maintenance.
// ensures that the NPC leaves this function with a valid
// schedule!
//=========================================================

static bool ShouldStopProcessingTasks( CAI_Agent *pNPC, int taskTime, int timeLimit )
{
#ifdef DEBUG
	if( ai_simulate_task_overtime.GetBool() )
		return true;
#endif
	return false;
}

//-------------------------------------

void CAI_Agent::MaintainSchedule ( void )
{
	AI_PROFILE_SCOPE(CAI_Agent_RunAI_MaintainSchedule);
	extern CFastTimer g_AIMaintainScheduleTimer;
	CTimeScope timeScope(&g_AIMaintainScheduleTimer);

	//---------------------------------

	CAI_Schedule	*pNewSchedule;
	int			i;
	bool		runTask = true;

#if defined( VPROF_ENABLED )
#if defined(DISABLE_DEBUG_HISTORY)
	bool bDebugTaskNames = ( developer.GetBool() || ( VProfAI() && g_VProfCurrentProfile.IsEnabled() ) );
#else
	bool bDebugTaskNames = true;
#endif
#else
	bool bDebugTaskNames = false;
#endif

	memset( g_AIAgentTaskTimings, 0, sizeof(g_AIAgentTaskTimings) );

	g_nAIAgentTasksRun = 0;

	const int timeLimit = ( IsDebug() ) ? 16 : 8;
	int taskTime = Plat_MSTime();

	// 	// Reset this at the beginning of the frame
	// 	Forget( bits_MEMORY_TASK_EXPENSIVE );

	// UNDONE: Tune/fix this MAX_TASKS_RUN... This is just here so infinite loops are impossible
	bool bStopProcessing = false;
	for ( i = 0; i < MAX_TASKS_RUN && !bStopProcessing; i++ )
	{
		if ( GetCurSchedule() != NULL && TaskIsComplete() )
		{
			// Schedule is valid, so advance to the next task if the current is complete.
			NextScheduledTask();

			// If we finished the current schedule, clear our ignored conditions so they
			// aren't applied to the next schedule selection.
			if ( HasCondition( COND_SCHEDULE_DONE ) )
			{
				// Put our conditions back the way they were after GatherConditions,
				// but add in COND_SCHEDULE_DONE.
				m_Conditions = m_ConditionsPreIgnore;
				SetCondition( COND_SCHEDULE_DONE );

				m_InverseIgnoreConditions.SetAll();
			}
		}

		int curTiming = g_nAIAgentTasksRun;
		g_nAIAgentTasksRun++;

		// validate existing schedule 
		if ( !IsScheduleValid() /* || m_NPCState != m_IdealNPCState */ )
		{
			// Notify the NPC that his schedule is changing
			m_ScheduleState.bScheduleWasInterrupted = true;
			OnScheduleChange();

			if ( !m_bConditionsGathered )
			{
				// occurs if a schedule is exhausted within one think
				GatherConditions();
			}
			// 
			// 			if ( ShouldSelectIdealState() )
			// 			{
			// 				NPC_STATE eIdealState = SelectIdealState();
			// 				SetIdealState( eIdealState );
			// 			}

			if ( HasCondition( COND_TASK_FAILED ) /*&& m_NPCState == m_IdealNPCState*/ )
			{
				// Get a fail schedule if the previous schedule failed during execution and 
				// the NPC is still in its ideal state. Otherwise, the NPC would immediately
				// select the same schedule again and fail again.
				if (GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
				{
					DevMsg( this, AIMF_IGNORE_SELECTED, "      (failed)\n" );
				}

				ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d):      (failed)\n", GetDebugName(), entindex() ) );

				pNewSchedule = GetFailSchedule();
				m_IdealSchedule = pNewSchedule->GetId();
				DevWarning( 2, "(%s) Schedule (%s) Failed at %d!\n", STRING( GetEntityName() ), GetCurSchedule() ? GetCurSchedule()->GetName() : "GetCurSchedule() == NULL", GetScheduleCurTaskIndex() );
				SetSchedule( pNewSchedule );
			}
			else
			{
				// 				// If the NPC is supposed to change state, it doesn't matter if the previous
				// 				// schedule failed or completed. Changing state means selecting an entirely new schedule.
				// 				SetState( m_IdealNPCState );
				// 				
				g_AIAgentTaskTimings[curTiming].selectSchedule.Start();

				pNewSchedule = GetNewSchedule();

				g_AIAgentTaskTimings[curTiming].selectSchedule.End();

				SetSchedule( pNewSchedule );
			}
		}

		if (!GetCurSchedule())
		{
			g_AIAgentTaskTimings[curTiming].selectSchedule.Start();

			pNewSchedule = GetNewSchedule();

			g_AIAgentTaskTimings[curTiming].selectSchedule.End();

			if (pNewSchedule)
			{
				SetSchedule( pNewSchedule );
			}
		}

		if ( !GetCurSchedule() || GetCurSchedule()->NumTasks() == 0 )
		{
			return;
		}

		AI_PROFILE_SCOPE_BEGIN_( CAI_Agent::GetSchedulingSymbols()->ScheduleIdToSymbol( GetCurSchedule()->GetId() ) );

		if ( GetTaskStatus() == TASKSTATUS_NEW )
		{	
			if ( GetScheduleCurTaskIndex() == 0 )
			{
				int globalId = GetCurSchedule()->GetId();
				int localId = GetLocalScheduleId( globalId ); // if localId == -1, then it came from a behavior
				OnStartSchedule( (localId != -1)? localId : globalId );
			}

			g_AIAgentTaskTimings[curTiming].startTimer.Start();
			const Task_t *pTask = GetTask();
			const char *pszTaskName = ( bDebugTaskNames ) ? TaskName( pTask->iTask ) : "ai_task";
			Assert( pTask != NULL );
			g_AIAgentTaskTimings[i].pszTask = pszTaskName;

			if (GetDebugOverlayFlags() & OVERLAY_TASK_TEXT_BIT)
			{
				DevMsg(this, AIMF_IGNORE_SELECTED, "  Task: %s\n", pszTaskName );
			}

			ADD_DEBUG_HISTORY( HISTORY_AI_DECISIONS, UTIL_VarArgs("%s(%d):  Task: %s\n", GetDebugName(), entindex(), pszTaskName ) );

			OnStartTask();

			m_ScheduleState.taskFailureCode    = NO_TASK_FAILURE;
			m_ScheduleState.timeCurTaskStarted = gpGlobals->curtime;

			AI_PROFILE_SCOPE_BEGIN_( pszTaskName );
			AI_PROFILE_SCOPE_BEGIN(CAI_Agent_StartTask);

			StartTask( pTask );

			AI_PROFILE_SCOPE_END();
			AI_PROFILE_SCOPE_END();

			// 			if ( TaskIsRunning() && !HasCondition(COND_TASK_FAILED) )
			// 				StartTaskOverlay();

			g_AIAgentTaskTimings[curTiming].startTimer.End();
			// DevMsg( "%.2f StartTask( %s )\n", gpGlobals->curtime, m_pTaskSR->GetStringText( pTask->iTask ) );
		}

		AI_PROFILE_SCOPE_END();

		// 		// UNDONE: Twice?!!!
		// 		MaintainActivity();

		AI_PROFILE_SCOPE_BEGIN_( CAI_Agent::GetSchedulingSymbols()->ScheduleIdToSymbol( GetCurSchedule()->GetId() ) );

		if ( !TaskIsComplete() && GetTaskStatus() != TASKSTATUS_NEW )
		{
			if ( TaskIsRunning() && !HasCondition(COND_TASK_FAILED) && runTask )
			{
				const Task_t *pTask = GetTask();
				const char *pszTaskName = ( bDebugTaskNames ) ? TaskName( pTask->iTask ) : "ai_task";
				Assert( pTask != NULL );
				g_AIAgentTaskTimings[i].pszTask = pszTaskName;
				// DevMsg( "%.2f RunTask( %s )\n", gpGlobals->curtime, m_pTaskSR->GetStringText( pTask->iTask ) );
				g_AIAgentTaskTimings[curTiming].runTimer.Start();

				AI_PROFILE_SCOPE_BEGIN_( pszTaskName );
				AI_PROFILE_SCOPE_BEGIN(CAI_Agent_RunTask);

				int j;
				for (j = 0; j < 8; j++)
				{
					RunTask( pTask );

					if ( GetTaskInterrupt() == 0 || TaskIsComplete() || HasCondition(COND_TASK_FAILED) )
						break;

					if ( ShouldUseEfficiency() && ShouldStopProcessingTasks( this, Plat_MSTime() - taskTime, timeLimit ) )
					{
						bStopProcessing = true;
						break;
					}
				}
				AssertMsg( j < 8, "Runaway task interrupt\n" );

				AI_PROFILE_SCOPE_END();
				AI_PROFILE_SCOPE_END();

				if ( TaskIsRunning() && !HasCondition(COND_TASK_FAILED) )
				{
					// 	EndTaskOverlay();
					// 
				}

				g_AIAgentTaskTimings[curTiming].runTimer.End();

				// don't do this again this frame
				// FIXME: RunTask() should eat some of the clock, depending on what it has done
				// runTask = false;

				if ( !TaskIsComplete() )
				{
					bStopProcessing = true;
				}
			}
			else
			{
				bStopProcessing = true;
			}
		}

		AI_PROFILE_SCOPE_END();

		// Decide if we should continue on this frame
		if ( !bStopProcessing && ShouldStopProcessingTasks( this, Plat_MSTime() - taskTime, timeLimit ) )
			bStopProcessing = true;
	}
}



//-----------------------------------------------------------------------------
// Start task!
//-----------------------------------------------------------------------------
void CAI_Agent::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SET_SCHEDULE:
		if ( !SetSchedule( pTask->flTaskData ) )
			TaskFail(FAIL_SCHEDULE_NOT_FOUND);
		break;

	default:
		DevMsg( "No StartTask entry for %s\n", TaskName( pTask->iTask ) );
	};
}


//=========================================================
// RunTask 
//=========================================================
void CAI_Agent::RunTask( const Task_t *pTask )
{
	DevMsg( "No RunTask entry for %s\n", TaskName( pTask->iTask ) );
	TaskComplete();
}

//=========================================================
// GetTask - returns a pointer to the current 
// scheduled task. NULL if there's a problem.
//=========================================================
const Task_t *CAI_Agent::GetTask( void ) 
{
	int iScheduleIndex = GetScheduleCurTaskIndex();
	if ( !GetCurSchedule() ||  iScheduleIndex < 0 || iScheduleIndex >= GetCurSchedule()->NumTasks() )
		// iScheduleIndex is not within valid range for the NPC's current schedule.
		return NULL;

	return &GetCurSchedule()->GetTaskList()[ iScheduleIndex ];
}



//-----------------------------------------------------------------------------
// Purpose: Decides which type of schedule best suits the NPC's current 
// state and conditions. Then calls NPC's member function to get a pointer 
// to a schedule of the proper type.
//-----------------------------------------------------------------------------
int CAI_Agent::SelectSchedule( void )
{
	return SCHED_FAIL;
}


//-----------------------------------------------------------------------------

int CAI_Agent::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	return ( m_failSchedule != SCHED_NONE ) ? m_failSchedule : SCHED_FAIL;
}

void CAI_Agent::InitDefaultTaskSR(void)
{
#define ADD_DEF_TASK( name ) idSpace.AddTask(#name, name, "CAI_Agent" )

	CAI_ClassScheduleIdSpace &idSpace = CAI_Agent::AccessClassScheduleIdSpaceDirect();

	ADD_DEF_TASK( TASK_INVALID );
	ADD_DEF_TASK( TASK_SET_SCHEDULE );
}

void CAI_Agent::InitDefaultConditionSR(void)
{
#define ADD_CONDITION_TO_SR( _n ) idSpace.AddCondition( #_n, _n, "CAI_Agent" )

	CAI_ClassScheduleIdSpace &idSpace = CAI_Agent::AccessClassScheduleIdSpaceDirect();

	ADD_CONDITION_TO_SR( COND_NONE );
	ADD_CONDITION_TO_SR( COND_TASK_FAILED );
	ADD_CONDITION_TO_SR( COND_SCHEDULE_DONE );
	ADD_CONDITION_TO_SR( COND_NO_CUSTOM_INTERRUPTS );		// Don't call BuildScheduleTestBits for this schedule. Used for schedules that must strictly control their interruptibility.
}

void CAI_Agent::InitDefaultScheduleSR(void)
{
#define ADD_DEF_SCHEDULE( name, localId ) idSpace.AddSchedule(name, localId, "CAI_Agent" )

	CAI_ClassScheduleIdSpace &idSpace = CAI_Agent::AccessClassScheduleIdSpaceDirect();

	ADD_DEF_SCHEDULE( "SCHED_NONE",							SCHED_NONE);
	ADD_DEF_SCHEDULE( "SCHED_FAIL",							SCHED_FAIL );
}

bool CAI_Agent::LoadDefaultSchedules(void)
{
	//	AI_LOAD_DEF_SCHEDULE( CAI_Agent,					SCHED_NONE);
	//AI_LOAD_DEF_SCHEDULE( CAI_Agent,					SCHED_FAIL);
	return true;
}
