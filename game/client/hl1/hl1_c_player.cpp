//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hl1_c_player.h"

ConVar cl_npc_speedmod_intime( "cl_npc_speedmod_intime", "0.25", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar cl_npc_speedmod_outtime( "cl_npc_speedmod_outtime", "1.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

#if defined( CHL1_Player )
#undef CHL1_Player	
#endif

IMPLEMENT_CLIENTCLASS_DT( C_HL1_Player, DT_HL1Player, CHL1_Player )
	RecvPropInt( RECVINFO( m_bHasLongJump ) ),
	RecvPropInt( RECVINFO( m_nFlashBattery ) ),
	RecvPropBool( RECVINFO( m_bIsPullingObject ) ),

	RecvPropFloat( RECVINFO( m_flStartCharge ) ),
	RecvPropFloat( RECVINFO( m_flAmmoStartCharge ) ),
	RecvPropFloat( RECVINFO( m_flPlayAftershock ) ),
	RecvPropFloat( RECVINFO( m_flNextAmmoBurn ) )
END_RECV_TABLE()

C_HL1_Player::C_HL1_Player()
{
	
}