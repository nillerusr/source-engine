//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_neurotoxin_countdown.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CUtlVector< C_NeurotoxinCountdown* > g_NeurotoxinCountdowns;


IMPLEMENT_CLIENTCLASS_DT(C_NeurotoxinCountdown, DT_NeurotoxinCountdown, CNeurotoxinCountdown)

	RecvPropBool( RECVINFO(m_bEnabled) ),

END_RECV_TABLE()


C_NeurotoxinCountdown::C_NeurotoxinCountdown()
{
	g_NeurotoxinCountdowns.AddToTail( this );
}

C_NeurotoxinCountdown::~C_NeurotoxinCountdown()
{
	g_NeurotoxinCountdowns.FindAndRemove( this );
}

int C_NeurotoxinCountdown::GetMinutes( void )
{
	C_BasePlayer *player = UTIL_PlayerByIndex( 1 );
	if ( player )
		return player->GetBonusProgress() / 60;
	
	return 0.0f;
}

int C_NeurotoxinCountdown::GetSeconds( void )
{
	C_BasePlayer *player = UTIL_PlayerByIndex( 1 );
	if ( player )
		return player->GetBonusProgress() % 60;

	return 0.0f;
}

int C_NeurotoxinCountdown::GetMilliseconds( void )
{
	return static_cast<int>( gpGlobals->curtime * 100.0f ) % 100;;
}
