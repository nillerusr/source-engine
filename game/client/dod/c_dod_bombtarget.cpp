//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_dod_bombtarget.h"
#include "dod_shareddefs.h"
#include "c_dod_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODBombTarget, DT_DODBombTarget )

BEGIN_NETWORK_TABLE(C_DODBombTarget, DT_DODBombTarget )
	RecvPropInt( RECVINFO(m_iState) ),
	RecvPropInt( RECVINFO(m_iBombingTeam) ),

	RecvPropInt( RECVINFO(m_iTargetModel) ),
	RecvPropInt( RECVINFO(m_iUnavailableModel) ),
END_NETWORK_TABLE()

void C_DODBombTarget::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit( state );

	// Turn off
	if ( state == SHOULDTRANSMIT_END )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}

	// Turn on
	if ( state == SHOULDTRANSMIT_START )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void C_DODBombTarget::ClientThink( void )
{
	// if we are near the player, maybe give them a hint!

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer && pPlayer->IsAlive() )
	{
		Vector vecDist = GetAbsOrigin() - pPlayer->GetAbsOrigin();

		if ( vecDist.Length() < 200 )
		{
			int iOppositeTeam = ( m_iBombingTeam == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;
			if ( pPlayer->GetTeamNumber() == m_iBombingTeam && m_iState == BOMB_TARGET_ACTIVE )
			{
				pPlayer->CheckBombTargetPlantHint();
			}
			else if ( pPlayer->GetTeamNumber() == iOppositeTeam && m_iState == BOMB_TARGET_ARMED )
			{
				pPlayer->CheckBombTargetDefuseHint();
			}
		}
	}
		
	SetNextClientThink( gpGlobals->curtime + 0.5 );
}

int C_DODBombTarget::DrawModel( int flags )
{
	if ( m_iState == BOMB_TARGET_ACTIVE )
	{
		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

		int iOppositeTeam = ( m_iBombingTeam == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;

#ifdef _DEBUG
		if ( m_iBombingTeam == TEAM_UNASSIGNED )
			iOppositeTeam = TEAM_UNASSIGNED;
#endif

		if ( pPlayer && pPlayer->GetTeamNumber() == iOppositeTeam )
		{
			// draw a different model for the non-planting team

			if ( GetModelIndex() != m_iUnavailableModel	)
			{
				SetModelIndex( m_iUnavailableModel );
			}
		}
		else
		{
			if ( GetModelIndex() != m_iTargetModel	)
			{
				SetModelIndex( m_iTargetModel );
			}
		}
	}

	return BaseClass::DrawModel( flags );
}

// a player of the passed team is looking at us, see if we should
// play the hint telling them how to defuse
bool C_DODBombTarget::ShouldPlayDefuseHint( int team )
{
	return ( m_iState == BOMB_TARGET_ARMED && team != m_iBombingTeam );
}

// a player of the passed team is looking at us, see if we should
// play the hint telling them how to plant a bomb here
bool C_DODBombTarget::ShouldPlayPlantHint( int team )
{
	return ( m_iState == BOMB_TARGET_ACTIVE && team == m_iBombingTeam );
}
