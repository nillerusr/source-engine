#include "cbase.h"
#include "asw_objective_kill_goo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_destroy_goo, CASW_Objective_Kill_Goo );

BEGIN_DATADESC( CASW_Objective_Kill_Goo )
	DEFINE_KEYFIELD( m_iTargetKills, FIELD_INTEGER,	"NumGoo" ),
	DEFINE_FIELD( m_iCurrentKills, FIELD_INTEGER ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Objective_Kill_Goo, DT_ASW_Objective_Kill_Goo)	
	SendPropInt		(SENDINFO(m_iTargetKills)),
	SendPropInt		(SENDINFO(m_iCurrentKills)),
END_SEND_TABLE()

CASW_Objective_Kill_Goo::CASW_Objective_Kill_Goo()
{
	m_iCurrentKills = 0;	// how many Goo killed so far
}


CASW_Objective_Kill_Goo::~CASW_Objective_Kill_Goo()
{
}

void CASW_Objective_Kill_Goo::GooKilled(CASW_Alien_Goo* pGoo)
{
	if (IsObjectiveComplete())
		return;
	//Msg("CASW_Objective_Kill_Goo see Alien Goo killed!\n");	
	m_iCurrentKills++;
	//Msg(" Currently %d goos killed, target is %d\n", m_iCurrentKills, m_iTargetKills);
	if (m_iCurrentKills >= m_iTargetKills)
	{
		Msg("  we're done!\n");
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

float CASW_Objective_Kill_Goo::GetObjectiveProgress()
{
	if ( m_iTargetKills <= 0 )
		return BaseClass::GetObjectiveProgress();

	float flProgress = (float) m_iCurrentKills.Get() / (float) m_iTargetKills.Get();
	flProgress = clamp<float>( flProgress, 0.0f, 1.0f );

	return flProgress;
}