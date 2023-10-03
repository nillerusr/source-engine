#include "cbase.h"
#include "C_ASW_Objective_Kill_Queen.h"
#include "asw_vgui_queen_health.h"
#include "c_asw_queen.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Objective_Kill_Queen, DT_ASW_Objective_Kill_Queen, CASW_Objective_Kill_Queen)
	RecvPropEHandle		( RECVINFO( m_hQueen ) ),
END_RECV_TABLE()


C_ASW_Objective_Kill_Queen::C_ASW_Objective_Kill_Queen()
{
	m_bLaunchedPanel = false;
}

C_ASW_Objective_Kill_Queen::~C_ASW_Objective_Kill_Queen()
{
	
}

void C_ASW_Objective_Kill_Queen::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);
}