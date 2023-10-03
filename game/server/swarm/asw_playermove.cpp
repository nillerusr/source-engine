//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "player_command.h"
#include "igamemovement.h"
#include "in_buttons.h"
#include "ipredictionsystem.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_command.h"
#include "asw_imarinegamemovement.h"
#include "asw_marine_gamemovement.h"
#include "ilagcompensationmanager.h"
#include "asw_lag_compensation.h"
#include "iservervehicle.h"
#include "asw_movedata.h"

static CASW_MoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = NULL;

extern IGameMovement *g_pGameMovement;
extern ConVar sv_noclipduringpause;

extern IMarineGameMovement *g_pMarineGameMovement;
extern ConVar asw_allow_detach;
ConVar asw_move_marine("asw_move_marine", "1", FCVAR_CHEAT, "Activate remote control of named entity");

void CommentarySystem_PePlayerRunCommand( CBasePlayer *player, CUserCmd *ucmd );

//-----------------------------------------------------------------------------
// Sets up the move data for Infested
//-----------------------------------------------------------------------------
class CASW_PlayerMove : public CPlayerMove
{
DECLARE_CLASS( CASW_PlayerMove, CPlayerMove );

public:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper );
};

// PlayerMove Interface
static CASW_PlayerMove g_PlayerMove;

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove()
{
	return &g_PlayerMove;
}

//-----------------------------------------------------------------------------
// Purpose: This is called pre player movement and copies all the data necessary
//          from the player for movement. (Server-side, the client-side version
//          of this code can be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CASW_PlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	player->AvoidPhysicsProps( ucmd );

	BaseClass::SetupMove( player, ucmd, pHelper, move );

	// this forces horizontal movement
	CASW_Player *pASWPlayer = dynamic_cast<CASW_Player*>(player);
	if (pASWPlayer && pASWPlayer->GetMarine() && !asw_allow_detach.GetBool())
	{
			move->m_vecAngles.x = 0;
		move->m_vecViewAngles.x = 0;

		CBaseEntity *pMoveParent = player->GetMoveParent();
		if (!pMoveParent)
		{
			move->m_vecAbsViewAngles = move->m_vecViewAngles;
		}
		else
		{
			matrix3x4_t viewToParent, viewToWorld;
			AngleMatrix( move->m_vecViewAngles, viewToParent );
			ConcatTransforms( pMoveParent->EntityToWorldTransform(), viewToParent, viewToWorld );
			MatrixAngles( viewToWorld, move->m_vecAbsViewAngles );
		}
	}
	CASW_MoveData *pASWMove = static_cast<CASW_MoveData*>( move );
	pASWMove->m_iForcedAction = ucmd->forced_action;
	// setup trace optimization
	g_pGameMovement->SetupMovementBounds( move );
}

extern void DiffPrint( bool bServer, int nCommandNumber, char const *fmt, ... );
extern ConVar asw_rts_controls;

void CASW_PlayerMove::RunCommand( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	CASW_Player *pASWPlayer = static_cast<CASW_Player*>( player );
	Assert( pASWPlayer );

	StartCommand( player, ucmd );

	pASWPlayer->SetHighlightEntity( CBaseEntity::Instance( ucmd->crosshair_entity ) );

	// Set globals appropriately
	gpGlobals->curtime		=  player->m_nTickBase * TICK_INTERVAL;
	gpGlobals->frametime	=  player->m_bGamePaused ? 0 : TICK_INTERVAL;

	// Add and subtract buttons we're forcing on the player
	ucmd->buttons |= player->m_afButtonForced;
	ucmd->buttons &= ~player->m_afButtonDisabled;

	if ( player->m_bGamePaused )
	{
		// If no clipping and cheats enabled and noclipduring game enabled, then leave
		//  forwardmove and angles stuff in usercmd
		if ( player->GetMoveType() == MOVETYPE_NOCLIP &&
			sv_cheats->GetBool() && 
			sv_noclipduringpause.GetBool() )
		{
			gpGlobals->frametime = TICK_INTERVAL;
		}
	}

	/*
	// TODO:  We can check whether the player is sending more commands than elapsed real time
	cmdtimeremaining -= ucmd->msec;
	if ( cmdtimeremaining < 0 )
	{
	//	return;
	}
	*/

	g_pGameMovement->StartTrackPredictionErrors( player );

	CommentarySystem_PePlayerRunCommand( player, ucmd );

	// Do weapon selection
	if ( ucmd->weaponselect != 0 )
	{
		CBaseCombatWeapon *weapon = dynamic_cast< CBaseCombatWeapon * >( CBaseEntity::Instance( ucmd->weaponselect ) );
		if (weapon)
			pASWPlayer->ASWSelectWeapon(weapon, 0); // ucmd->weaponsubtype);  // infested - subtype var used for sending marine profile index instead
	}

	IServerVehicle *pVehicle = player->GetVehicle();

	// Latch in impulse.
	if ( ucmd->impulse )
	{
		// Discard impulse commands unless the vehicle allows them.
		// FIXME: UsingStandardWeapons seems like a bad filter for this. The flashlight is an impulse command, for example.
		if ( !pVehicle || player->UsingStandardWeaponsInVehicle() )
		{
			player->m_nImpulse = ucmd->impulse;
		}
	}

	// Update player input button states
	VPROF_SCOPE_BEGIN( "player->UpdateButtonState" );
	player->UpdateButtonState( ucmd->buttons );
	VPROF_SCOPE_END();

	CheckMovingGround( player, TICK_INTERVAL );

	g_pMoveData->m_vecOldAngles = player->pl.v_angle;

	// Copy from command to player unless game .dll has set angle using fixangle
	if ( player->pl.fixangle == FIXANGLE_NONE )
	{
		player->pl.v_angle = ucmd->viewangles;
	}
	else if( player->pl.fixangle == FIXANGLE_RELATIVE )
	{
		player->pl.v_angle = ucmd->viewangles + player->pl.anglechange;
	}

	// TrackIR
	player->SetEyeAngleOffset(ucmd->headangles);
	player->SetEyeOffset(ucmd->headoffset);
	// TrackIR

	// Call standard client pre-think
	RunPreThink( player );

	// Call Think if one is set
	RunThink( player, TICK_INTERVAL );

	// Setup input.
	SetupMove( player, ucmd, moveHelper, g_pMoveData );

	// Let the game do the movement.
	if (asw_allow_detach.GetBool())
	{
		if ( !pVehicle )
		{
			VPROF( "g_pGameMovement->ProcessMovement()" );
			Assert( g_pGameMovement );
			g_pGameMovement->ProcessMovement( player, g_pMoveData );
		}
		else
		{
			VPROF( "pVehicle->ProcessMovement()" );
			pVehicle->ProcessMovement( player, g_pMoveData );
		}
	}

	if ( asw_rts_controls.GetBool() )
	{
		Vector vecNewPos = g_pMoveData->GetAbsOrigin();

		vecNewPos.y += ucmd->forwardmove * gpGlobals->frametime * 2;
		vecNewPos.x += ucmd->sidemove * gpGlobals->frametime * 2;

		g_pMoveData->SetAbsOrigin( vecNewPos );
	}

	pASWPlayer->SetCrosshairTracePos( ucmd->crosshairtrace );
	// Copy output
	FinishMove( player, ucmd, g_pMoveData );

	// Let server invoke any needed impact functions
	VPROF_SCOPE_BEGIN( "moveHelper->ProcessImpacts" );
	moveHelper->ProcessImpacts();
	VPROF_SCOPE_END();

	// put lag compensation here so it affects weapons
	lagcompensation->StartLagCompensation( player, LAG_COMPENSATE_BOUNDS );
	RunPostThink( player );
	lagcompensation->FinishLagCompensation( player );

	// let the player drive marine movement here
	pASWPlayer->DriveMarineMovement( ucmd, moveHelper );

	g_pGameMovement->FinishTrackPredictionErrors( player );

	FinishCommand( player );

	// Let time pass
	player->m_nTickBase++;
}