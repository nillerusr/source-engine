#include "cbase.h"
#include "asw_objective_kill_eggs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_kill_eggs, CASW_Objective_Kill_Eggs );

BEGIN_DATADESC( CASW_Objective_Kill_Eggs )
	DEFINE_KEYFIELD( m_iTargetKills, FIELD_INTEGER,	"NumEggs" ),
	DEFINE_FIELD( m_iCurrentKills, FIELD_INTEGER ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Objective_Kill_Eggs, DT_ASW_Objective_Kill_Eggs)	
	SendPropInt		(SENDINFO(m_iTargetKills)),
	SendPropInt		(SENDINFO(m_iCurrentKills)),
END_SEND_TABLE()

CASW_Objective_Kill_Eggs::CASW_Objective_Kill_Eggs()
{
	m_iCurrentKills = 0;	// how many eggs killed so far
}


CASW_Objective_Kill_Eggs::~CASW_Objective_Kill_Eggs()
{
}

void CASW_Objective_Kill_Eggs::EggKilled(CASW_Egg* pEgg)
{
	if (IsObjectiveComplete())
		return;

	m_iCurrentKills++;
	if (m_iCurrentKills >= m_iTargetKills)
	{
		SetComplete(true);
	}
	else
	{
		CReliableBroadcastRecipientFilter filter;
		UserMessageBegin( filter, "ShowObjectives" );
		WRITE_FLOAT( 5.0f );
		MessageEnd();
	}
}

float CASW_Objective_Kill_Eggs::GetObjectiveProgress()
{
	if ( m_iTargetKills <= 0 )
		return BaseClass::GetObjectiveProgress();

	float flProgress = (float) m_iCurrentKills.Get() / (float) m_iTargetKills.Get();
	flProgress = clamp<float>( flProgress, 0.0f, 1.0f );

	return flProgress;
}