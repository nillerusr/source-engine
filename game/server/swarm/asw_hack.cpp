#include "cbase.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_hack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST(CASW_Hack, DT_ASW_Hack)	
	SendPropEHandle (SENDINFO(m_hHackerMarineResource) ),
	SendPropEHandle (SENDINFO(m_hHackTarget) ),	
	SendPropInt		(SENDINFO(m_iShowOption)),
END_SEND_TABLE()


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Hack )
	DEFINE_FIELD( m_hHackerMarineResource, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hHackTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hHackingPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hHackingMarine, FIELD_EHANDLE ),
END_DATADESC()


CASW_Hack::CASW_Hack()
{
}

bool CASW_Hack::InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget)
{
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	if (!pHackingPlayer || !pHackingMarine || !pHackTarget || !pHackingMarine->GetMarineResource())
	{
		return false;
	}
	
	m_hHackingPlayer = pHackingPlayer;
	m_hHackingMarine = pHackingMarine;
	m_hHackTarget = pHackTarget;
	m_hHackerMarineResource = pHackingMarine->GetMarineResource();	
	pHackingMarine->m_hCurrentHack = this;

	return true;
}


int CASW_Hack::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

CASW_Player* CASW_Hack::GetHackingPlayer()
{
	return dynamic_cast<CASW_Player*>(m_hHackingPlayer.Get());
}
	
CASW_Marine* CASW_Hack::GetHackingMarine()
{
	return dynamic_cast<CASW_Marine*>(m_hHackingMarine.Get());
}


void CASW_Hack::MarineStoppedUsing(CASW_Marine* pMarine)
{
	if (pMarine->m_hCurrentHack.Get() == this)
	{
		pMarine->m_hCurrentHack = NULL;
	}
	// finish hacking
	m_hHackingMarine = NULL;
	m_hHackingPlayer = NULL;
	m_hHackerMarineResource = NULL;
}