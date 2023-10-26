#include "cbase.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_hack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Hack, DT_ASW_Hack, CASW_Hack)	
	RecvPropEHandle (RECVINFO(m_hHackerMarineResource)),
	RecvPropEHandle (RECVINFO(m_hHackTarget)),
	RecvPropInt		(RECVINFO(m_iShowOption)),
END_RECV_TABLE()

C_ASW_Hack::C_ASW_Hack()
{
	m_hHackerMarineResource = NULL;
	m_hHackTarget = NULL;
}

C_ASW_Marine_Resource* C_ASW_Hack::GetHackerMarineResource()
{
	return m_hHackerMarineResource.Get();
}

C_BaseEntity* C_ASW_Hack::GetHackTarget()
{
	return m_hHackTarget.Get();
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
C_BasePlayer* C_ASW_Hack::GetPredictionOwner()
{
	C_ASW_Marine_Resource *pMR = m_hHackerMarineResource.Get();
	if ( !pMR )
		return NULL;

	return pMR->GetCommander();
}

void C_ASW_Hack::PostDataUpdate( DataUpdateType_t updateType )
{
	bool bPredict = ShouldPredict();
	if ( bPredict )
	{
		SetSimulatedEveryTick( true );	
		SetPredictionEligible( true );
	}
	else
	{
		SetSimulatedEveryTick( false );
		SetPredictionEligible( false );
	}

	BaseClass::PostDataUpdate( updateType );

	if ( GetPredictable() && !bPredict )
	{
		MDLCACHE_CRITICAL_SECTION();
		ShutdownPredictable();
	}
}