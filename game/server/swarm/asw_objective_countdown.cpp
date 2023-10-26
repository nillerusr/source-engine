#include "cbase.h"
#include "asw_objective_countdown.h"
#include "asw_gamerules.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "triggers.h"
#include "util.h"
#include "te_effect_dispatch.h"
#include "effect_dispatch_data.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_countdown, CASW_Objective_Countdown );

IMPLEMENT_SERVERCLASS_ST(CASW_Objective_Countdown, DT_ASW_Objective_Countdown)
	SendPropFloat	(SENDINFO(m_fCountdownFinishTime), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Objective_Countdown )
	DEFINE_FIELD(m_bCountdownStarted, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fCountdownFinishTime, FIELD_TIME),
	DEFINE_KEYFIELD(m_fCountdownLength, FIELD_FLOAT, "CountdownLength"),
	DEFINE_THINKFUNC( ExplodeLevel ),	
	DEFINE_THINKFUNC( FailMission ),	
	DEFINE_INPUTFUNC( FIELD_VOID, "StartCountdown", InputStartCountdown ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CancelCountdown", InputCancelCountdown ),
	DEFINE_OUTPUT( m_OnCountdownFailed,		"OnCountdownFailed" ),
END_DATADESC()


// dummy objectives are complete automatically
CASW_Objective_Countdown::CASW_Objective_Countdown() : CASW_Objective()
{
	m_bVisible = false;
	m_bOptional = true;
}

void CASW_Objective_Countdown::Spawn()
{
	BaseClass::Spawn();

	Precache();
}

void CASW_Objective_Countdown::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound("ASW.WarheadExplosion");
	PrecacheScriptSound("ASW.WarheadExplosionLF");
}

CASW_Objective_Countdown::~CASW_Objective_Countdown()
{
}

void CASW_Objective_Countdown::InputStartCountdown( inputdata_t &inputdata )
{
	if (!m_bCountdownStarted)
	{
		m_bVisible = true;
		m_bCountdownStarted = true;
		m_bOptional = false;
		m_fCountdownFinishTime = gpGlobals->curtime + m_fCountdownLength;
		SetThink( &CASW_Objective_Countdown::ExplodeLevel );
		SetNextThink( m_fCountdownFinishTime );
	}
}

void CASW_Objective_Countdown::InputCancelCountdown( inputdata_t &inputdata )
{
	if (m_bCountdownStarted)
	{
		m_fCountdownFinishTime = 0;
		SetThink(NULL);
		SetComplete(true);
	}
}

void CASW_Objective_Countdown::ExplodeLevel()
{
	m_OnCountdownFailed.FireOutput(this, this);

	// spawn clientside effect to make explosion sound + graphics
	CEffectData	data;	
	CReliableBroadcastRecipientFilter filter;
	DispatchEffect( filter, 0.0, "ASWExplodeMap", data );

	SetThink( &CASW_Objective_Countdown::FailMission );
	SetNextThink( gpGlobals->curtime + 7);
}

void CASW_Objective_Countdown::FailMission()
{
	ASWGameRules()->ExplodedLevel();	
}