//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_walker_ministrider.h"
#include "beamdraw.h"
#include "view.h"


extern ConVar vehicle_free_pitch, vehicle_free_roll;


IMPLEMENT_CLIENTCLASS_DT( C_WalkerMiniStrider, DT_WalkerMiniStrider, CWalkerMiniStrider )
END_RECV_TABLE()


C_WalkerMiniStrider::C_WalkerMiniStrider()
{
}


C_WalkerMiniStrider::~C_WalkerMiniStrider()
{
}


void C_WalkerMiniStrider::ClientThink()
{
}


void C_WalkerMiniStrider::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
}

