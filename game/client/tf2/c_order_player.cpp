//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_player.h"
#include "cliententitylist.h"
#include "c_basetfplayer.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderPlayer, DT_OrderPlayer, COrderPlayer )
END_RECV_TABLE()


void C_OrderPlayer::GetTargetDescription( char *pDest, int bufferSize )
{
	pDest[0] = 0;
	
	// Order target
	if ( !m_iTargetEntIndex )
		return;

	C_BaseEntity *pEnt = cl_entitylist->GetEnt(m_iTargetEntIndex);
	if ( !pEnt )
		return;

	C_BaseTFPlayer *pPlayer = dynamic_cast<C_BaseTFPlayer*>(pEnt);
	if ( pPlayer )
	{
		pPlayer->GetTargetDescription( pDest, bufferSize );
	}
}




