#include "cbase.h"

#ifdef GAME_DLL
	
	#include "player.h"
	#include "usercmd.h"
	#include "igamemovement.h"
	#include "mathlib/mathlib.h"
	#include "client.h"
	#include "player_command.h"
	#include "asw_marine_command.h"
	#include "movehelper_server.h"
	#include "iservervehicle.h"
	#include "ilagcompensationmanager.h"
	#include "tier0/vprof.h"
	#include "asw_marine.h"
#else
	#include "c_baseplayer.h"	
	#include "usercmd.h"
	#include "igamemovement.h"
	#include "asw_marine_command.h"
	#include "tier0/vprof.h"
	#include "c_asw_marine.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameMovement *g_pGameMovement;
extern CMoveData *g_pMoveData;	// This is a global because it is subclassed by each game.
extern ConVar sv_noclipduringpause;
extern ConVar asw_controls;

// PlayerMove Interface
static CMarineMove g_MarineMove;

// For shared code.
#ifdef CLIENT_DLL
	#define CASW_Marine C_ASW_Marine
#endif

// this code is based on player_command.cpp/.h, but is altered to drive a marine AI
//  lots of commented code in here used to track down prediction errors

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CMarineMove *MarineMove()
{
	return &g_MarineMove;
}

extern void DiffPrint( bool bServer, int nCommandNumber, char const *fmt, ... );

//-----------------------------------------------------------------------------
// Purpose: Finishes running movement
// Input  : *player - 
//			*move - 
//			*ucmd - 
//			time - 
//-----------------------------------------------------------------------------
void CMarineMove::FinishMarineMove( const CBasePlayer *player, CBaseEntity *marine, CUserCmd *ucmd, CMoveData *move )
{
	VPROF( "CMarineMove::FinishMarineMove" );
	CASW_Marine *aswmarine = static_cast< CASW_Marine * >(marine);
	if (aswmarine == NULL)
		return;
	
	marine->SetAbsVelocity( move->m_vecVelocity );

#ifdef CLIENT_DLL
	marine->SetNetworkOrigin( move->GetAbsOrigin() );
	marine->SetLocalOrigin( move->GetAbsOrigin() );
#else
	marine->SetAbsOrigin( move->GetAbsOrigin() );
#endif

	aswmarine->m_nOldButtons = move->m_nButtons;
//	player->m_Local.m_nOldButtons			= move->m_nButtons;

	// Convert final pitch to body pitch
	float pitch = move->m_vecAngles[ PITCH ];
	if ( pitch > 180.0f )
	{
		pitch -= 360.0f;
	}
	pitch = clamp( pitch, -90, 90 );

	move->m_vecAngles[ PITCH ] = pitch;
/*
	int pitch_param = player->LookupPoseParameter( "body_pitch" );
	if ( pitch_param >= 0 )
	{
		player->SetPoseParameter( pitch_param, pitch );
	}*/

	marine->SetLocalAngles( move->m_vecAngles );
	
	QAngle angles = marine->GetAbsAngles();
	angles[YAW] = ucmd->viewangles[YAW];
	marine->SetAbsAngles( angles );

	/*
	// The class had better not have changed during the move!!
	
	if ( player->m_hConstraintEntity )
		Assert( move->m_vecConstraintCenter == player->m_hConstraintEntity.Get()->GetAbsOrigin() );
	else
		Assert( move->m_vecConstraintCenter == player->m_vecConstraintCenter );
	Assert( move->m_flConstraintRadius == player->m_flConstraintRadius );
	Assert( move->m_flConstraintWidth == player->m_flConstraintWidth );
	Assert( move->m_flConstraintSpeedFactor == player->m_flConstraintSpeedFactor );
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Prepares for running movement
// Input  : *player - 
//			*ucmd - 
//			*pHelper - 
//			*move - 
//			time - 
//-----------------------------------------------------------------------------

void CMarineMove::SetupMarineMove( const CBasePlayer *player, CBaseEntity *marine, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	VPROF( "CMarineMove::SetupMarineMove" );

	// hm, we need it to be a specific swarm marine here, not a generic entity
	CASW_Marine *aswmarine = static_cast< CASW_Marine * >(marine);
	if (aswmarine == NULL)
		return;

	// Prepare the usercmd fields
	move->m_nImpulseCommand		= ucmd->impulse;	
	move->m_vecViewAngles		= ucmd->viewangles;
	move->m_vecViewAngles.x = 0;	// asw always walking horizontally

	CBaseEntity *pMoveParent = marine->GetMoveParent();
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

	move->m_nButtons			= ucmd->buttons;

	// Ingore buttons for movement if at controls
	if ( marine->GetFlags() & FL_ATCONTROLS )
	{
		move->m_flForwardMove		= 0;
		move->m_flSideMove			= 0;
		move->m_flUpMove				= 0;
	}
	else
	{
		move->m_flForwardMove		= ucmd->forwardmove;
		move->m_flSideMove			= ucmd->sidemove;
		move->m_flUpMove				= ucmd->upmove;
	}

	// Prepare remaining fields
	move->m_flClientMaxSpeed		= aswmarine->MaxSpeed(); //player->MaxSpeed();
#ifdef CLIENT_DLL
	//Msg("maxspeed = %f\n", move->m_flClientMaxSpeed);
#endif
	//move->m_nOldButtons			= player->m_Local.m_nOldButtons;
	move->m_nOldButtons = aswmarine->m_nOldButtons;
	move->m_vecAngles			= marine->GetAbsAngles(); //player->pl.v_angle;
#ifdef GAME_DLL
	//Msg("S ucmd %d vel %f %f %f\n", ucmd->command_number, move->m_vecVelocity.x, move->m_vecVelocity.y, move->m_vecVelocity.z);
#else
	//Msg("C ucmd %d vel %f %f %f\n", ucmd->command_number, move->m_vecVelocity.x, move->m_vecVelocity.y, move->m_vecVelocity.z);
#endif
	move->m_vecVelocity			= marine->GetAbsVelocity();
	
	move->m_nPlayerHandle		= marine;//player;

#ifdef GAME_DLL
	move->SetAbsOrigin( marine->GetAbsOrigin() );
#else
	move->SetAbsOrigin( marine->GetNetworkOrigin() );
	/*
	C_BaseEntity *pEnt = cl_entitylist->FirstBaseEntity();
	while (pEnt)
	{
		if (FClassnameIs(pEnt, "class C_DynamicProp"))
		{
			Msg("Setting z to %f\n", pEnt->GetAbsOrigin().z + 10);
			move->m_vecAbsOrigin.z = pEnt->GetAbsOrigin().z + 10;
			marine->SetNetworkOrigin(pEnt->GetAbsOrigin() + Vector(0,0,10));
			break;
		}
		pEnt = cl_entitylist->NextBaseEntity( pEnt );
	}
	*/
#endif

	//Msg("Move X velocity set to %f forward move = %f  origin = %f\n",
//		move->m_vecVelocity.x, move->m_flForwardMove, move->m_vecAbsOrigin.x);

	// Copy constraint information
	/*
	if ( player->m_hConstraintEntity.Get() )
		move->m_vecConstraintCenter = player->m_hConstraintEntity.Get()->GetAbsOrigin();
	else
		move->m_vecConstraintCenter = player->m_vecConstraintCenter;
	move->m_flConstraintRadius = player->m_flConstraintRadius;
	move->m_flConstraintWidth = player->m_flConstraintWidth;
	move->m_flConstraintSpeedFactor = player->m_flConstraintSpeedFactor;
	*/
}

CMarineMove::CMarineMove( void )
{
}