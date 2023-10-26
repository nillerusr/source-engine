#include "cbase.h"
#include "asw_objective_kill_aliens.h"
#include "asw_spawn_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_kill_aliens, CASW_Objective_Kill_Aliens );

BEGIN_DATADESC( CASW_Objective_Kill_Aliens )
	DEFINE_KEYFIELD( m_iTargetKills, FIELD_INTEGER,	"NumKills" ),
	DEFINE_KEYFIELD( m_AlienClassNum, FIELD_INTEGER, "AlienClass" ),
	DEFINE_FIELD( m_iCurrentKills, FIELD_INTEGER ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Objective_Kill_Aliens, DT_ASW_Objective_Kill_Aliens)
	SendPropInt		(SENDINFO(m_AlienClassNum)),
	SendPropInt		(SENDINFO(m_iTargetKills)),
	SendPropInt		(SENDINFO(m_iCurrentKills)),
END_SEND_TABLE()

CASW_Objective_Kill_Aliens::CASW_Objective_Kill_Aliens()
{
	m_iCurrentKills = 0;	// how many aliens killed so far
}


CASW_Objective_Kill_Aliens::~CASW_Objective_Kill_Aliens()
{
}

void CASW_Objective_Kill_Aliens::AlienKilled(CBaseEntity* pAlien)
{
	// check it's the right class
	if ( !pAlien || IsObjectiveComplete() || !ASWSpawnManager() )
		return;

	if ( m_AlienClassNum < 0 || m_AlienClassNum >= ASWSpawnManager()->GetNumAlienClasses() )
	{
		return;
	}

	if ( !FClassnameIs( pAlien, ASWSpawnManager()->GetAlienClass( m_AlienClassNum )->m_pszAlienClass ) )
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

float CASW_Objective_Kill_Aliens::GetObjectiveProgress()
{
	if ( m_iTargetKills <= 0 )
		return BaseClass::GetObjectiveProgress();

	float flProgress = (float) m_iCurrentKills.Get() / (float) m_iTargetKills.Get();
	flProgress = clamp<float>( flProgress, 0.0f, 1.0f );

	return flProgress;
}