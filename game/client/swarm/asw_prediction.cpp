//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "prediction.h"
#include "c_baseplayer.h"
#include "igamemovement.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "prediction_private.h"
#include "tier0/vprof.h"
#include "con_nprint.h"
#include "IClientVehicle.h"
#include "asw_movedata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CASW_MoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;
extern IGameMovement *g_pGameMovement;

extern ConVar asw_allow_detach;
extern ConVar cl_showerror;
typedescription_t *FindFieldByName( const char *fieldname, datamap_t *dmap );

class CASW_Prediction : public CPrediction
{
DECLARE_CLASS( CASW_Prediction, CPrediction );

public:
	CASW_Prediction();

	virtual void	SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	RunCommand( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper );

	virtual void	CheckError( int nSlot, C_BasePlayer *player, int commands_acknowledged );
	virtual void	CheckMarineError( int nSlot, int commands_acknowledged );

protected:

	bool			m_bMarineOriginTypedescriptionSearched;
	CUtlVector< const typedescription_t * >	m_MarineOriginTypeDescription; // A vector in cases where the .x, .y, and .z are separately listed

};

CASW_Prediction::CASW_Prediction() : 
	m_bMarineOriginTypedescriptionSearched( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Prediction::CheckError( int nSlot, C_BasePlayer *player, int commands_acknowledged )
{
#if !defined( NO_ENTITY_PREDICTION )
	// Infesed - check marine prediction error as well as player's:
	CheckMarineError(nSlot, commands_acknowledged);
#endif

	BaseClass::CheckError( nSlot, player, commands_acknowledged );
}

void CASW_Prediction::CheckMarineError( int nSlot, int commands_acknowledged )
{
	C_ASW_Player	*player;
	Vector		origin;
	Vector		delta;
	float		len;
	static int	pos = 0;

	// Not in the game yet
	if ( !engine->IsInGame() )
		return;

	// Not running prediction
	if ( !cl_predict->GetInt() )
		return;

	player = C_ASW_Player::GetLocalASWPlayer( nSlot );
	if ( !player )
		return;

	C_ASW_Marine* pMarine = player->GetMarine();
	if (!pMarine)
		return;

	// Not predictable yet (flush entity packet?)
	if ( !pMarine->IsIntermediateDataAllocated() )
		return;

	origin = pMarine->GetNetworkOrigin();

	const void *slot = pMarine->GetPredictedFrame( commands_acknowledged - 1 );
	if ( !slot )
		return;

	if ( !m_bMarineOriginTypedescriptionSearched )
	{
		m_bMarineOriginTypedescriptionSearched = true;
		const typedescription_t *td = CPredictionCopy::FindFlatFieldByName( "m_vecNetworkOrigin", pMarine->GetPredDescMap() );
		if ( td ) 
		{
			m_MarineOriginTypeDescription.AddToTail( td );
		}
	}

	if ( !m_MarineOriginTypeDescription.Count() )
		return;

	Vector predicted_origin;

	memcpy( (Vector *)&predicted_origin, (Vector *)( (byte *)slot + m_MarineOriginTypeDescription[ 0 ]->flatOffset[ TD_OFFSET_PACKED ] ), sizeof( Vector ) );

	// Compare what the server returned with what we had predicted it to be
	VectorSubtract ( predicted_origin, origin, delta );

	len = VectorLength( delta );
	if (len > MAX_PREDICTION_ERROR )
	{	
		// A teleport or something, clear out error
		len = 0;
	}
	else
	{
		if ( len > MIN_PREDICTION_EPSILON )
		{
			pMarine->NotePredictionError( delta );

			if ( cl_showerror.GetInt() >= 1 )
			{
				con_nprint_t np;
				np.fixed_width_font = true;
				np.color[0] = 1.0f;
				np.color[1] = 0.95f;
				np.color[2] = 0.7f;
				np.index = 20 + ( ++pos % 20 );
				np.time_to_live = 2.0f;

				engine->Con_NXPrintf( &np, "marine pred error %6.3f units (%6.3f %6.3f %6.3f)", len, delta.x, delta.y, delta.z );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Prediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, 
	CMoveData *move )
{
	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );
	
	CASW_Player *pASWPlayer = static_cast<CASW_Player*>( player );
	if ( !asw_allow_detach.GetBool() )
	{		
		if ( pASWPlayer && pASWPlayer->GetMarine() )
		{
			// this forces horizontal movement
			move->m_vecAngles.x = 0;
			move->m_vecViewAngles.x = 0;
		}
	}

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
	CASW_MoveData *pASWMove = static_cast<CASW_MoveData*>( move );
	pASWMove->m_iForcedAction = ucmd->forced_action;
	// setup trace optimization
	g_pGameMovement->SetupMovementBounds( move );
}

extern void DiffPrint( bool bServer, int nCommandNumber, char const *fmt, ... );

//-----------------------------------------------------------------------------
// Purpose: Predicts a single movement command for player
// Input  : *moveHelper - 
//			*player - 
//			*u - 
//-----------------------------------------------------------------------------
void CASW_Prediction::RunCommand( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper )
{
#if !defined( NO_ENTITY_PREDICTION )
	VPROF( "CPrediction::RunCommand" );
#if defined( _DEBUG )
	char sz[ 32 ];
	Q_snprintf( sz, sizeof( sz ), "runcommand%04d", ucmd->command_number );
	PREDICTION_TRACKVALUECHANGESCOPE( sz );
#endif

	C_ASW_Player *pASWPlayer = (C_ASW_Player*)player;
	Assert( pASWPlayer );

	StartCommand( player, ucmd );

	pASWPlayer->SetHighlightEntity( C_BaseEntity::Instance( ucmd->crosshair_entity ) );

	// Set globals appropriately
	gpGlobals->curtime		= player->m_nTickBase * TICK_INTERVAL;
	gpGlobals->frametime	= TICK_INTERVAL;
	
	g_pGameMovement->StartTrackPredictionErrors( player );

	// TODO
	// TODO:  Check for impulse predicted?

	// Do weapon selection
	if ( ucmd->weaponselect != 0 )
	{
		C_BaseCombatWeapon *weapon = dynamic_cast< C_BaseCombatWeapon * >( CBaseEntity::Instance( ucmd->weaponselect ) );
		if (weapon)
		{
			pASWPlayer->ASWSelectWeapon(weapon, 0); //ucmd->weaponsubtype);		// asw - subtype var used for sending marine profile index instead
		}
	}

	// Latch in impulse.
	IClientVehicle *pVehicle = player->GetVehicle();
	if ( ucmd->impulse )
	{
		// Discard impulse commands unless the vehicle allows them.
		// FIXME: UsingStandardWeapons seems like a bad filter for this. 
		// The flashlight is an impulse command, for example.
		if ( !pVehicle || player->UsingStandardWeaponsInVehicle() )
		{
			player->m_nImpulse = ucmd->impulse;
		}
	}

	// Get button states
	player->UpdateButtonState( ucmd->buttons );

	// TODO
	//	CheckMovingGround( player, ucmd->frametime );

	// TODO
	//	g_pMoveData->m_vecOldAngles = player->pl.v_angle;

	// Copy from command to player unless game .dll has set angle using fixangle
	// if ( !player->pl.fixangle )
	{
		player->SetLocalViewAngles( ucmd->viewangles );
	}

	// Call standard client pre-think
	RunPreThink( player );

	// Call Think if one is set
	RunThink( player, TICK_INTERVAL );

	// Setup input.
	{

		SetupMove( player, ucmd, moveHelper, g_pMoveData );
	}

	// Run regular player movement if we're not controlling a marine
	if ( asw_allow_detach.GetBool() )
	{
		if ( !pVehicle )
		{
			Assert( g_pGameMovement );
			g_pGameMovement->ProcessMovement( player, g_pMoveData );
		}
		else
		{
			pVehicle->ProcessMovement( player, g_pMoveData );
		}
	}

// 	if ( !asw_allow_detach.GetBool() && pASWPlayer->GetMarine() )
// 	{
// 		g_pMoveData->SetAbsOrigin( pASWPlayer->GetMarine()->GetAbsOrigin() );
// 	}

	pASWPlayer->SetCrosshairTracePos( ucmd->crosshairtrace );

	FinishMove( player, ucmd, g_pMoveData );

	RunPostThink( player );

	// let the player drive marine movement here
	pASWPlayer->DriveMarineMovement( ucmd, moveHelper );

	g_pGameMovement->FinishTrackPredictionErrors( player );	

	FinishCommand( player );

	player->m_nTickBase++;
#endif
}

// Expose interface to engine
// Expose interface to engine
static CASW_Prediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CASW_Prediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;

