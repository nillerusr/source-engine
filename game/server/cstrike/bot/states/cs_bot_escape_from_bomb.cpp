//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->DestroyPath();
	me->EquipKnife();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnUpdate( CCSBot *me )
{
	const Vector *bombPos = me->GetGameState()->GetBombPosition();

	// if we don't know where the bomb is, we shouldn't be in this state
	if (bombPos == NULL)
	{
		me->Idle();
		return;
	}

	// grab our knife to move quickly
	me->EquipKnife();

	// look around
	me->UpdateLookAround();

	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		// we have no path, or reached the end of one - create a new path far away from the bomb
		FarAwayFromPositionFunctor func( *bombPos );
		CNavArea *goalArea = FindMinimumCostArea( me->GetLastKnownArea(), func );

		// if this fails, we'll try again next time
		me->ComputePath( goalArea->GetCenter(), FASTEST_ROUTE );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Escape from the bomb.
 */
void EscapeFromBombState::OnExit( CCSBot *me )
{
	me->EquipBestWeapon();
}
