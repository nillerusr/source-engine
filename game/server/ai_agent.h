//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base NPC character with AI
//
//=============================================================================//

#ifndef AI_AGENT_H
#define AI_AGENT_H

#ifdef _WIN32
#pragma once
#endif

#include "ai_debug.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_condition.h"
#include "ai_task.h"
#include "ai_namespaces.h"
#include "bitstring.h"


class CAI_Agent;

void DevMsg( CAI_Agent *pAI, unsigned flags, const char *pszFormat, ... );
void DevMsg( CAI_Agent *pAI, const char *pszFormat, ... );

typedef CBitVec<MAX_CONDITIONS> CAI_ScheduleBits;


//=============================================================================
//
// Constants & enumerations
//
//=============================================================================

//
// Debug bits
//
//-------------------------------------

#ifdef AI_MONITOR_FOR_OSCILLATION
struct AIScheduleChoice_t 
{
	float			m_flTimeSelected;
	CAI_Schedule	*m_pScheduleSelected;
};
#endif//AI_MONITOR_FOR_OSCILLATION

//=============================================================================
//
// Types used by CAI_Agent
//
//=============================================================================

struct AIAgentScheduleState_t
{
	int					 iCurTask;
	TaskStatus_e		 fTaskStatus;
	float				 timeStarted;
	float				 timeCurTaskStarted;
	AI_TaskFailureCode_t taskFailureCode;
	int					 iTaskInterrupt;
	bool				 bScheduleWasInterrupted;

	DECLARE_SIMPLE_DATADESC();
};

//=============================================================================
//
//	class CAI_Agent
//
//=============================================================================

class CAI_Agent
{
public:
	//-----------------------------------------------------
	//
	// Initialization, cleanup, serialization, identity
	//
	
	CAI_Agent();
	~CAI_Agent();

	//---------------------------------
	
	DECLARE_SIMPLE_DATADESC();

	virtual int			Save( ISave &save ); 
	virtual int			Restore( IRestore &restore );
	void				SaveConditions( ISave &save, const CAI_ScheduleBits &conditions );
	void				RestoreConditions( IRestore &restore, CAI_ScheduleBits *pConditions );

	//---------------------------------
	
	virtual void		Init( void ); // derived calls after Spawn()

	// Flaccid implementations to satisfy boilerplate debug code
	virtual const char *GetDebugName()	{ return "CAI_Agent"; }
	virtual int entindex()				{ return -1; }

public:
	//-----------------------------------------------------
	//
	// AI processing - thinking, schedule selection and task running
	//
	//-----------------------------------------------------
	// Thinking, including core thinking, movement, animation
	virtual void		Think( void );

	// Core thinking (schedules & tasks)
	virtual void		RunAI( void );// core ai function!	

	// Called to gather up all relevant conditons
	virtual void		GatherConditions( void );

	// Called immediately prior to schedule processing
	virtual void		PrescheduleThink( void );

	// Called immediately after schedule processing
	virtual void		PostscheduleThink( void ) { return; };

	// Notification that the current schedule, if any, is ending and a new one is being selected
	virtual void		OnScheduleChange( void );

	// Notification that a new schedule is about to run its first task
	virtual void		OnStartSchedule( int scheduleType ) {};

	// This function implements a decision tree for the NPC.  It is responsible for choosing the next behavior (schedule)
	// based on the current conditions and state.
	virtual int			SelectSchedule( void );
	virtual int			SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

	// After the schedule has been selected, it will be processed by this function so child NPC classes can 
	// remap base schedules into child-specific behaviors
	virtual int			TranslateSchedule( int scheduleType ) { return scheduleType; }

	virtual void		StartTask( const Task_t *pTask );
	virtual void		RunTask( const Task_t *pTask );

	virtual void		ClearTransientConditions();

	void				ForceGatherConditions()	{ m_bForceConditionsGather = true; }	// Force an NPC out of PVS to call GatherConditions on next think

	enum 
	{
		SCHED_NONE = 0,
		SCHED_FAIL,
		NEXT_SCHEDULE,

		TASK_INVALID = 0,
		TASK_SET_SCHEDULE,
		NEXT_TASK,

		COND_NONE = 0,				// A way for a function to return no condition to get
		COND_TASK_FAILED,
		COND_SCHEDULE_DONE,
		COND_NO_CUSTOM_INTERRUPTS,		// Don't call BuildScheduleTestBits for this schedule. Used for schedules that must strictly control their interruptibility.
		NEXT_CONDITION,
	};

protected:
	// Used by derived classes to chain a task to a task that might not be the 
	// one they are currently handling:
	void				ChainStartTask( int task, float taskData = 0 )	{ Task_t tempTask = { task, taskData }; StartTask( (const Task_t *)&tempTask ); }
	void				ChainRunTask( int task, float taskData = 0 )	{ Task_t tempTask = { task, taskData }; RunTask( (const Task_t *)	&tempTask );	}

private:

	bool				PreThink( void );
	void				MaintainSchedule( void );
	
	virtual int			StartTask ( Task_t *pTask ) { DevMsg( "Called wrong StartTask()\n" ); StartTask( (const Task_t *)pTask ); return 0; } // to ensure correct signature in derived classes
	virtual int			RunTask ( Task_t *pTask )	{ DevMsg( "Called wrong RunTask()\n" ); RunTask( (const Task_t *)pTask ); return 0; } // to ensure correct signature in derived classes

public:
	//-----------------------------------------------------
	//
	// Schedules & tasks
	//
	//-----------------------------------------------------
	
	void				SetSchedule( CAI_Schedule *pNewSchedule );
	bool				SetSchedule( int localScheduleID );
	
	void				SetDefaultFailSchedule( int failSchedule )	{ m_failSchedule = failSchedule; }
	
	void				ClearSchedule( const char *szReason );
	
	CAI_Schedule *		GetCurSchedule()							{ return m_pSchedule; }
	bool				IsCurSchedule( int schedId, bool fIdeal = true );
	virtual CAI_Schedule *GetSchedule(int localScheduleID);
	virtual int			GetLocalScheduleId( int globalScheduleID )	{ return AI_IdIsLocal( globalScheduleID ) ? globalScheduleID : GetClassScheduleIdSpace()->ScheduleGlobalToLocal( globalScheduleID ); }
	virtual int			GetGlobalScheduleId( int localScheduleID )	{ return AI_IdIsGlobal( localScheduleID ) ? localScheduleID : GetClassScheduleIdSpace()->ScheduleLocalToGlobal( localScheduleID ); }

	float				GetTimeScheduleStarted() const				{ return m_ScheduleState.timeStarted; }
	
	//---------------------------------
	
	const Task_t*		GetTask( void );
	int					TaskIsRunning( void );
	
	virtual void		TaskFail( AI_TaskFailureCode_t );
	void				TaskFail( const char *pszGeneralFailText )	{ TaskFail( MakeFailCode( pszGeneralFailText ) ); }
	void				TaskComplete( bool fIgnoreSetFailedCondition = false );

	void				TaskInterrupt()								{ m_ScheduleState.iTaskInterrupt++; }
	void				ClearTaskInterrupt()						{ m_ScheduleState.iTaskInterrupt = 0; }
	int					GetTaskInterrupt() const					{ return m_ScheduleState.iTaskInterrupt; }
	
	void				TaskMovementComplete( void );
	inline int			TaskIsComplete( void ) 						{ return (GetTaskStatus() == TASKSTATUS_COMPLETE); }

	virtual const char *TaskName(int taskID);

	float				GetTimeTaskStarted() const					{ return m_ScheduleState.timeCurTaskStarted; }
	virtual int			GetLocalTaskId( int globalTaskId)			{ return GetClassScheduleIdSpace()->TaskGlobalToLocal( globalTaskId ); }

	virtual const char *GetSchedulingErrorName()					{ return "CAI_Agent"; }

protected:
	static bool			LoadSchedules(void);
	virtual bool		LoadedSchedules(void);
	virtual void		BuildScheduleTestBits( void );

	//---------------------------------

	// This is the main call to select/translate a schedule
	virtual CAI_Schedule *GetNewSchedule( void );
	virtual CAI_Schedule *GetFailSchedule( void );

private:
	// This function maps the type through TranslateSchedule() and then retrieves the pointer
	// to the actual CAI_Schedule from the database of schedules available to this class.
	CAI_Schedule *		GetScheduleOfType( int scheduleType );
	
	bool				FHaveSchedule( void );
	bool				FScheduleDone ( void );
	CAI_Schedule *		ScheduleInList( const char *pName, CAI_Schedule **pList, int listCount );

	int 				GetScheduleCurTaskIndex() const			{ return m_ScheduleState.iCurTask;		}
	inline int			IncScheduleCurTaskIndex();
	inline void			ResetScheduleCurTaskIndex();
	void				NextScheduledTask ( void );
	bool				IsScheduleValid ( void );
	
	// Selecting the ideal state

	// Various schedule selections based on NPC_STATE
	void				OnStartTask( void ) 					{ SetTaskStatus( TASKSTATUS_RUN_MOVE_AND_TASK ); }
	void 				SetTaskStatus( TaskStatus_e status )	{ m_ScheduleState.fTaskStatus = status; 	}
	TaskStatus_e 		GetTaskStatus() const					{ return m_ScheduleState.fTaskStatus; 	}

	void				DiscardScheduleState();

	//---------------------------------

	CAI_Schedule *		m_pSchedule;
	int					m_IdealSchedule;
	AIAgentScheduleState_t	m_ScheduleState;
	int					m_failSchedule;				// Schedule type to choose if current schedule fails

public:
	//-----------------------------------------------------
	//
	// Conditions
	//
	//-----------------------------------------------------

	virtual const char*	ConditionName(int conditionID);
	
	virtual void		RemoveIgnoredConditions ( void );
	void				SetCondition( int iCondition /*, bool state = true*/ );
	bool				HasCondition( int iCondition );
	bool				HasCondition( int iCondition, bool bUseIgnoreConditions );
	bool				HasInterruptCondition( int iCondition );
	bool				HasConditionsToInterruptSchedule( int nLocalScheduleID );

	void				ClearCondition( int iCondition );
	void				ClearConditions( int *pConditions, int nConditions );
	void				SetIgnoreConditions( int *pConditions, int nConditions );
	void				ClearIgnoreConditions( int *pConditions, int nConditions );
	bool				ConditionInterruptsCurSchedule( int iCondition );
	bool				ConditionInterruptsSchedule( int schedule, int iCondition );

	void				SetCustomInterruptCondition( int nCondition );
	bool				IsCustomInterruptConditionSet( int nCondition );
	void				ClearCustomInterruptCondition( int nCondition );
	void				ClearCustomInterruptConditions( void );

	bool				ConditionsGathered() const		{ return m_bConditionsGathered; }
	const CAI_ScheduleBits &AccessConditionBits() const { return m_Conditions; }
	CAI_ScheduleBits &	AccessConditionBits()			{ return m_Conditions; }

private:
	CAI_ScheduleBits	m_Conditions;
	CAI_ScheduleBits	m_CustomInterruptConditions;	//Bit string assembled by the schedule running, then 
														//modified by leaf classes to suit their needs
	CAI_ScheduleBits	m_ConditionsPreIgnore;
	CAI_ScheduleBits	m_InverseIgnoreConditions;

	bool				m_bForceConditionsGather;
	bool				m_bConditionsGathered;


public:
	//-----------------------------------------------------
	//
	// Core mapped data structures 
	//
	// String Registries for default AI Shared by all CBaseNPCs
	//	These are used only during initialization and in debug
	//-----------------------------------------------------

	static void InitSchedulingTables();

	static CAI_GlobalScheduleNamespace *GetSchedulingSymbols()		{ return &gm_SchedulingSymbols; }
	static CAI_ClassScheduleIdSpace &AccessClassScheduleIdSpaceDirect() { return gm_ClassScheduleIdSpace; }
	virtual CAI_ClassScheduleIdSpace *	GetClassScheduleIdSpace()	{ return &gm_ClassScheduleIdSpace; }

	static int			GetScheduleID	(const char* schedName);
	static int			GetConditionID	(const char* condName);
	static int			GetTaskID		(const char* taskName);

private:
	friend class CAI_SystemHook;
	friend class CAI_SchedulesManager;
	
	static bool			LoadDefaultSchedules(void);

	static void			InitDefaultScheduleSR(void);
	static void			InitDefaultTaskSR(void);
	static void			InitDefaultConditionSR(void);
	
	static CAI_GlobalScheduleNamespace	gm_SchedulingSymbols;
	static CAI_ClassScheduleIdSpace		gm_ClassScheduleIdSpace;

public:
	//----------------------------------------------------
	// Debugging tools
	//
	
	// -----------------------------
	//  Debuging Fields and Methods
	// -----------------------------
	int					m_AgentDebugOverlays;
	Vector				m_vecAgentDebugOverlaysPos;
	const char*			m_failText;					// Text of why it failed
	const char*			m_interruptText;			// Text of why schedule interrupted
	CAI_Schedule*		m_failedSchedule;			// The schedule that failed last
	CAI_Schedule*		m_interuptSchedule;			// The schedule that was interrupted last
	int					m_nDebugCurIndex;			// Index used for stepping through AI
	void 				DumpTaskTimings();
	virtual int			DrawDebugTextOverlays( int text_offset );
	void		 EntityText( int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255 );
	int GetDebugOverlayFlags() {return m_AgentDebugOverlays;}
	string_t GetEntityName() { return NULL_STRING; }
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CAI_Agent::IncScheduleCurTaskIndex()
{
	m_ScheduleState.iTaskInterrupt = 0;
	return ++m_ScheduleState.iCurTask;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CAI_Agent::ResetScheduleCurTaskIndex()
{
	m_ScheduleState.iCurTask = 0;
	m_ScheduleState.iTaskInterrupt = 0;
}


// ============================================================================
//	Macros for introducing new schedules in sub-classes
//
// Strings registries and schedules use unique ID's for each item, but 
// sub-class enumerations are non-unique, so we translate between the 
// enumerations and unique ID's
// ============================================================================

#define AI_BEGIN_AGENT_( derivedClass, baseClass ) \
	IMPLEMENT_AGENT(derivedClass, baseClass ) \
	void derivedClass::InitCustomSchedules( void ) \
	{ \
		typedef derivedClass CNpc; \
		typedef baseClass CAgentBase; \
		const char *pszClassName = #derivedClass; \
		\
		CUtlVector<char *> schedulesToLoad; \
		CUtlVector<AIScheduleLoadFunc_t> reqiredOthers; \
		CAI_AgentNamespaceInfos scheduleIds; \
		CAI_AgentNamespaceInfos taskIds; \
		CAI_AgentNamespaceInfos conditionIds;
		

#define AI_BEGIN_AGENT( derivedClass ) \
	AI_BEGIN_AGENT_( derivedClass, BaseClass )
		

//-----------------

#define DEFINE_SCHEDULE( id, text ) \
	scheduleIds.PushBack( #id, id ); \
	char * g_psz##id = \
		"\n	Schedule" \
		"\n		" #id \
		text \
		"\n"; \
	schedulesToLoad.AddToTail( (char *)g_psz##id );
	
//-----------------

#define DECLARE_CONDITION( id ) \
	conditionIds.PushBack( #id, id );

//-----------------

#define DECLARE_TASK( id ) \
	taskIds.PushBack( #id, id );

//-----------------

// IDs are stored and then added in order due to constraints in the namespace implementation
#define AI_END_AGENT() \
		\
		int i; \
		\
		CNpc::AccessClassScheduleIdSpaceDirect().Init( pszClassName, CAgentBase::GetSchedulingSymbols(), &CAgentBase::AccessClassScheduleIdSpaceDirect() ); \
		\
		scheduleIds.Sort(); \
		taskIds.Sort(); \
		conditionIds.Sort(); \
		\
		for ( i = 0; i < scheduleIds.Count(); i++ ) \
		{ \
			ADD_CUSTOM_SCHEDULE_NAMED( CNpc, scheduleIds[i].pszName, scheduleIds[i].localId );  \
		} \
		\
		for ( i = 0; i < taskIds.Count(); i++ ) \
		{ \
			ADD_CUSTOM_TASK_NAMED( CNpc, taskIds[i].pszName, taskIds[i].localId );  \
		} \
		\
		for ( i = 0; i < conditionIds.Count(); i++ ) \
		{ \
			if ( AIAgentValidateConditionLimits( conditionIds[i].pszName ) ) \
			{ \
				ADD_CUSTOM_CONDITION_NAMED( CNpc, conditionIds[i].pszName, conditionIds[i].localId );  \
			} \
		} \
		\
		for ( i = 0; i < reqiredOthers.Count(); i++ ) \
		{ \
			(*reqiredOthers[i])();  \
		} \
		\
		for ( i = 0; i < schedulesToLoad.Count(); i++ ) \
		{ \
			if ( CNpc::gm_SchedLoadStatus.fValid ) \
			{ \
				CNpc::gm_SchedLoadStatus.fValid = g_AI_AgentSchedulesManager.LoadSchedulesFromBuffer( pszClassName, schedulesToLoad[i], &AccessClassScheduleIdSpaceDirect(), GetSchedulingSymbols() ); \
			} \
			else \
				break; \
		} \
	}

inline bool AIAgentValidateConditionLimits( const char *pszNewCondition )
{
	int nGlobalConditions = CAI_Agent::GetSchedulingSymbols()->NumConditions();
	if ( nGlobalConditions >= MAX_CONDITIONS )
	{ 
		AssertMsg2( 0, "Exceeded max number of conditions (%d), ignoring condition %s\n", MAX_CONDITIONS, pszNewCondition ); 
		DevWarning( "Exceeded max number of conditions (%d), ignoring condition %s\n", MAX_CONDITIONS, pszNewCondition ); 
		return false;
	}
	return true;
}


//-------------------------------------

struct AI_AgentNamespaceAddInfo_t
{
	AI_AgentNamespaceAddInfo_t( const char *pszName, int localId )
	 :	pszName( pszName ),
		localId( localId )
	{
	}
	
	const char *pszName;
	int			localId;
};

class CAI_AgentNamespaceInfos : public CUtlVector<AI_AgentNamespaceAddInfo_t>
{
public:
	void PushBack(  const char *pszName, int localId )
	{
		AddToTail( AI_AgentNamespaceAddInfo_t( pszName, localId ) );
	}

	void Sort()
	{
		CUtlVector<AI_AgentNamespaceAddInfo_t>::Sort( Compare );
	}
	
private:
	static int __cdecl Compare(const AI_AgentNamespaceAddInfo_t *pLeft, const AI_AgentNamespaceAddInfo_t *pRight )
	{
		return pLeft->localId - pRight->localId;
	}
	
};

//-------------------------------------

// Declares the static variables that hold the string registry offset for the new subclass
// as well as the initialization in schedule load functions

struct AI_AgentSchedLoadStatus_t
{
	bool fValid;
	int  signature;
};

// Load schedules pulled out to support stepping through with debugger
inline bool AI_DoLoadSchedules( bool (*pfnBaseLoad)(), void (*pfnInitCustomSchedules)(),
								AI_AgentSchedLoadStatus_t *pLoadStatus )
{
	(*pfnBaseLoad)();
	
	if (pLoadStatus->signature != g_AI_AgentSchedulesManager.GetScheduleLoadSignature())
	{
		(*pfnInitCustomSchedules)();
		pLoadStatus->fValid	   = true;
		pLoadStatus->signature = g_AI_AgentSchedulesManager.GetScheduleLoadSignature();
	}
	return pLoadStatus->fValid;
}

//-------------------------------------

typedef bool (*AIScheduleLoadFunc_t)();

// @Note (toml 02-16-03): The following class exists to allow us to establish an anonymous friendship
// in DEFINE_AGENT. The particulars of this implementation is almost entirely
// defined by bugs in MSVC 6.0
class AgentScheduleLoadHelperImpl
{
public:
	template <typename T> 
	static AIScheduleLoadFunc_t AccessScheduleLoadFunc(T *)
	{
		return (&T::LoadSchedules);
	}
};

//-------------------------------------

#define DEFINE_AGENT()\
	static AI_AgentSchedLoadStatus_t 		gm_SchedLoadStatus; \
	static CAI_ClassScheduleIdSpace 	gm_ClassScheduleIdSpace; \
	static const char *					gm_pszErrorClassName;\
	\
	static CAI_ClassScheduleIdSpace &	AccessClassScheduleIdSpaceDirect() 	{ return gm_ClassScheduleIdSpace; } \
	virtual CAI_ClassScheduleIdSpace *	GetClassScheduleIdSpace()			{ return &gm_ClassScheduleIdSpace; } \
	virtual const char *				GetSchedulingErrorName()			{ return gm_pszErrorClassName; } \
	\
	static void							InitCustomSchedules(void);\
	\
	static bool							LoadSchedules(void);\
	virtual bool						LoadedSchedules(void); \
	\
	friend class AgentScheduleLoadHelperImpl;	\
	\
	class CScheduleLoader \
	{ \
	public: \
		CScheduleLoader(); \
	} m_ScheduleLoader; \
	\
	friend class CScheduleLoader;

//-------------------------------------

#define IMPLEMENT_AGENT(derivedClass, baseClass)\
	AI_AgentSchedLoadStatus_t		derivedClass::gm_SchedLoadStatus = { true, -1 }; \
	CAI_ClassScheduleIdSpace 	derivedClass::gm_ClassScheduleIdSpace; \
	const char *				derivedClass::gm_pszErrorClassName = #derivedClass; \
	\
	derivedClass::CScheduleLoader::CScheduleLoader()\
	{ \
		derivedClass::LoadSchedules(); \
	} \
	\
	/* --------------------------------------------- */ \
	/* Load schedules for this type of NPC           */ \
	/* --------------------------------------------- */ \
	bool derivedClass::LoadSchedules(void)\
	{\
		return AI_DoLoadSchedules( derivedClass::baseClass::LoadSchedules, \
								   derivedClass::InitCustomSchedules, \
								   &derivedClass::gm_SchedLoadStatus ); \
	}\
	\
	bool derivedClass::LoadedSchedules(void) \
	{ \
		return derivedClass::gm_SchedLoadStatus.fValid;\
	} 


//-------------------------------------

#define ADD_CUSTOM_SCHEDULE_NAMED(derivedClass,schedName,schedEN)\
	if ( !derivedClass::AccessClassScheduleIdSpaceDirect().AddSchedule( schedName, schedEN, derivedClass::gm_pszErrorClassName ) ) return;

#define ADD_CUSTOM_SCHEDULE(derivedClass,schedEN) ADD_CUSTOM_SCHEDULE_NAMED(derivedClass,#schedEN,schedEN)

#define ADD_CUSTOM_TASK_NAMED(derivedClass,taskName,taskEN)\
	if ( !derivedClass::AccessClassScheduleIdSpaceDirect().AddTask( taskName, taskEN, derivedClass::gm_pszErrorClassName ) ) return;

#define ADD_CUSTOM_TASK(derivedClass,taskEN) ADD_CUSTOM_TASK_NAMED(derivedClass,#taskEN,taskEN)

#define ADD_CUSTOM_CONDITION_NAMED(derivedClass,condName,condEN)\
	if ( !derivedClass::AccessClassScheduleIdSpaceDirect().AddCondition( condName, condEN, derivedClass::gm_pszErrorClassName ) ) return;

#define ADD_CUSTOM_CONDITION(derivedClass,condEN) ADD_CUSTOM_CONDITION_NAMED(derivedClass,#condEN,condEN)



#endif // AI_AGENT_H
