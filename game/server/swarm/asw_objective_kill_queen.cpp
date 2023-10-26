#include "cbase.h"
#include "asw_objective_kill_queen.h"
#include "entitylist.h"
#include "asw_queen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_objective_kill_queen, CASW_Objective_Kill_Queen );

IMPLEMENT_SERVERCLASS_ST(CASW_Objective_Kill_Queen, DT_ASW_Objective_Kill_Queen)	
	SendPropEHandle (SENDINFO(m_hQueen) ),	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Objective_Kill_Queen )
	DEFINE_KEYFIELD( m_QueenName, FIELD_STRING, "queenname" ),
END_DATADESC()


CASW_Objective_Kill_Queen::CASW_Objective_Kill_Queen()
{
}


CASW_Objective_Kill_Queen::~CASW_Objective_Kill_Queen()
{
}

void CASW_Objective_Kill_Queen::Spawn()
{
	BaseClass::Spawn();

	// Find the queen
	m_hQueen = dynamic_cast<CASW_Queen*>(gEntList.FindEntityByName( m_hQueen.Get(), m_QueenName ));
	if (!m_hQueen.Get())
	{
		// if there was no queen with this name, then try to find any queen in the map
		m_hQueen = dynamic_cast<CASW_Queen*>(gEntList.FindEntityByClassname( NULL, "asw_queen" ));

		if (!m_hQueen.Get())
		{	
			Msg("Error: asw_objective_kill_queen with no asw_queen in the map");
		}
	}
}