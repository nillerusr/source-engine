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
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnEnter( CCSBot *me )
{
	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the bomb on the floor and pick it up
 */
void FetchBombState::OnUpdate( CCSBot *me )
{
	if (me->HasC4())
	{
		me->PrintIfWatched( "I picked up the bomb\n" );
		me->Idle();
		return;
	}


	CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
	if (bomb)
	{
		if (!me->HasPath())
		{
			// build a path to the bomb
			if (me->ComputePath( bomb->GetAbsOrigin() ) == false)
			{
				me->PrintIfWatched( "Fetch bomb pathfind failed\n" );

				// go Hunt instead of Idle to prevent continuous re-pathing to inaccessible bomb
				me->Hunt();
				return;
			}
		}
	}
	else
	{
		// someone picked up the bomb
		me->PrintIfWatched( "Someone else picked up the bomb.\n" );
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround();

	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
		me->Idle();
}
