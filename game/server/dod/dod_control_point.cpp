//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_shareddefs.h"
#include "dod_control_point.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "dod_team.h"
#include "dod_objective_resource.h"
#include "dod_control_point_master.h"

LINK_ENTITY_TO_CLASS( dod_control_point, CControlPoint );

#define OBJ_ICON_NEUTRAL_FLAG 0
#define OBJ_ICON_ALLIES_FLAG 1
#define OBJ_ICON_AXIS_FLAG 2

BEGIN_DATADESC(CControlPoint)

	DEFINE_KEYFIELD( m_iszPrintName,		FIELD_STRING, "point_printname" ),

	DEFINE_KEYFIELD( m_iszAlliesCapSound,	FIELD_STRING, "point_allies_capsound" ),
	DEFINE_KEYFIELD( m_iszAxisCapSound,		FIELD_STRING, "point_axis_capsound" ),
	DEFINE_KEYFIELD( m_iszResetSound,		FIELD_STRING, "point_resetsound" ),

	DEFINE_KEYFIELD( m_iszAlliesModel,		FIELD_STRING, "point_allies_model" ),
	DEFINE_KEYFIELD( m_iszAxisModel,		FIELD_STRING, "point_axis_model" ),
	DEFINE_KEYFIELD( m_iszResetModel,		FIELD_STRING, "point_reset_model" ),

	DEFINE_KEYFIELD( m_iCPGroup,			FIELD_INTEGER, "point_group" ),

	DEFINE_KEYFIELD( m_iTimedPointsAllies,	FIELD_INTEGER, "point_timedpoints_allies" ),
	DEFINE_KEYFIELD( m_iTimedPointsAxis,	FIELD_INTEGER, "point_timedpoints_axis" ),

	DEFINE_KEYFIELD( m_iBombsRequired,		FIELD_INTEGER, "point_num_bombs" ),

	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetOwner", InputSetOwner ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ShowModel", InputShowModel ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HideModel", InputHideModel ),

	DEFINE_OUTPUT(	m_AlliesCapOutput,		"OnAlliesCap" ),	// these are fired whenever the point changes modes
	DEFINE_OUTPUT(	m_AxisCapOutput,		"OnAxisCap" ),
	DEFINE_OUTPUT(	m_PointResetOutput,		"OnCapReset" ),

	DEFINE_OUTPUT(	m_OwnerChangedToAllies,		"AlliesCapturePoint" ),	// these are fired when a team does the work to change the owner
	DEFINE_OUTPUT(	m_OwnerChangedToAxis,		"AxisCapturePoint" ),

END_DATADESC();
	

CControlPoint::CControlPoint( )
{
	m_iPointIndex = -1;
	m_bPointVisible = false;

	m_iAlliesIcon = 0;
	m_iAxisIcon = 0;
	m_iNeutralIcon = 0;	
	m_iNeutralIcon = 0;

	//default group is 0 for backwards compatibility
	m_iAlliesModelBodygroup = 0;
	m_iAxisModelBodygroup = 0;
	m_iResetModelBodygroup = 0;	

	m_bBombPlanted = false;
}

void CControlPoint::Spawn( void )
{
	Precache();

	SetTouch( NULL );

	InternalSetOwner( m_iDefaultOwner, false );	//init the owner of this point

	SetActive( !m_bStartDisabled );

	BaseClass::Spawn();

	UseClientSideAnimation();

	SetPlaybackRate( 1.0 );

	SetCycle( random->RandomFloat( 0, 1.0 ) );

	m_iAlliesRequired = 0;
	m_iAxisRequired = 0;

	if ( FBitSet( m_spawnflags, CAP_POINT_HIDE_MODEL ) )
	{
		AddEffects( EF_NODRAW );
	}

	m_iBombsRemaining = m_iBombsRequired;
}

void CControlPoint::SetNumCappersRequired( int alliesRequired, int axisRequired )
{
	m_iAlliesRequired = alliesRequired;
	m_iAxisRequired = axisRequired;
}

//================================
// InputReset
// Used by ControlMaster to this point to its default owner
//================================
void CControlPoint::InputReset( inputdata_t &input )
{
	InternalSetOwner( m_iDefaultOwner, false );
	g_pObjectiveResource->SetOwningTeam( GetPointIndex(), m_iTeam );
}

//================================
// InputReset
// Used by Area caps to set the owner
//================================
void CControlPoint::InputSetOwner( inputdata_t &input )
{
	int team = input.value.Int();

	Assert( team == TEAM_UNASSIGNED || team == TEAM_ALLIES || team == TEAM_AXIS );

	Assert( input.pCaller );
	Assert( input.pCaller->IsPlayer() );

	CDODPlayer *pPlayer = ToDODPlayer( input.pCaller );

	int iCappingPlayer = pPlayer->entindex();

	// Only allow cp caps when round is running ( but not when in warmup )
	if( DODGameRules()->State_Get() == STATE_RND_RUNNING &&
		!DODGameRules()->IsInWarmup() )
	{
		InternalSetOwner( team, true, 1, &iCappingPlayer );
		g_pObjectiveResource->SetOwningTeam( GetPointIndex(), m_iTeam );
	}	
}

void CControlPoint::SetOwner( int owner, bool bMakeSound, int iNumCappers, int *pCappingPlayers )
{
	// Only allow cp caps when round is running ( but not when in warmup )
	if( DODGameRules()->State_Get() == STATE_RND_RUNNING &&
		!DODGameRules()->IsInWarmup() )
	{
		InternalSetOwner( owner, bMakeSound, iNumCappers, pCappingPlayers );
		g_pObjectiveResource->SetOwningTeam( GetPointIndex(), m_iTeam );
	}
}

void CControlPoint::InputShowModel( inputdata_t &input )
{
	RemoveEffects( EF_NODRAW );
}

void CControlPoint::InputHideModel( inputdata_t &input )
{
	AddEffects( EF_NODRAW );
}

int CControlPoint::GetCurrentHudIconIndex( void )
{
	return GetHudIconIndexForTeam( GetOwner() );
}

int CControlPoint::GetHudIconIndexForTeam( int team )
{
	int icon = m_iNeutralIcon;

	switch( team )
	{
	case TEAM_ALLIES:
		icon = m_iAlliesIcon;
		break;
	case TEAM_AXIS:
		icon = m_iAxisIcon;
		break;
	case TEAM_UNASSIGNED:
		icon = m_iNeutralIcon;
		break;
	default:
		Assert( !"Bad team in GetHudIconIndexForTeam()" );
		break;
	}

	return icon;
}

int CControlPoint::GetTimerCapHudIcon( void )
{
	return m_iTimerCapIcon;
}

int CControlPoint::GetBombedHudIcon( void )
{
	return m_iBombedIcon;
}

// SetOwner
// ========
// Sets the new owner of the point 
// plays the appropriate sound
// and shows the right model

void CControlPoint::InternalSetOwner( int owner, bool bMakeSound, int iNumCappers, int *pCappingPlayers )
{
	m_iTeam = owner;

	switch ( m_iTeam )
	{
	case TEAM_UNASSIGNED: 
		{
			if( bMakeSound )
			{
				CBroadcastRecipientFilter filter;
				EmitSound( filter, entindex(), STRING(m_iszResetSound) );
			}
			
			SetModel( STRING(m_iszResetModel) );	
			SetBodygroup( 0, m_iResetModelBodygroup );
			
			m_PointResetOutput.FireOutput( this, this );
		}
		break;
	case TEAM_ALLIES: 
		{
			if( bMakeSound )
			{
				CBroadcastRecipientFilter filter;
				EmitSound( filter, entindex(), STRING(m_iszAlliesCapSound) );
			}
			
			SetModel( STRING(m_iszAlliesModel) );	
			SetBodygroup( 0, m_iAlliesModelBodygroup );
			
			m_AlliesCapOutput.FireOutput( this, this );
		}
		break;
	case TEAM_AXIS: 
		{
			if( bMakeSound )
			{
				CBroadcastRecipientFilter filter;
				EmitSound( filter, entindex(), STRING(m_iszAxisCapSound) );
			}
			
			SetModel( STRING(m_iszAxisModel) );	
			SetBodygroup( 0, m_iAxisModelBodygroup );
			
			m_AxisCapOutput.FireOutput( this, this );
		}		
		break;
	default:
		Assert(0);
		break;
	}

	if( bMakeSound )	//make sure this is a real cap and not a reset
	{
		for( int i=0;i<iNumCappers;i++ )
		{
			int playerIndex = pCappingPlayers[i];

			Assert( playerIndex > 0 && playerIndex <= gpGlobals->maxClients );

			CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( playerIndex ) );

			Assert( pPlayer );

			if ( pPlayer )
			{
				pPlayer->AddScore( PLAYER_POINTS_FOR_CAP );
				pPlayer->Stats_AreaCaptured();
				DODGameRules()->Stats_PlayerCap( pPlayer->GetTeamNumber(), pPlayer->m_Shared.DesiredPlayerClass() );

				// count bomb plants as point capture
				//if ( !m_bSetOwnerIsBombPlant )
				{
					pPlayer->StatEvent_PointCaptured();
				}
			}
		}

		if ( m_iTeam == TEAM_ALLIES )
		{
			m_OwnerChangedToAllies.FireOutput( this, this );
		}
		else if ( m_iTeam == TEAM_AXIS )
		{
			m_OwnerChangedToAxis.FireOutput( this, this );
		}
	}

	if( bMakeSound && m_iTeam != TEAM_UNASSIGNED && iNumCappers > 0 )	//dont print message if its a reset
		SendCapString( m_iTeam, iNumCappers, pCappingPlayers );

	// have control point master check the win conditions now!
	CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point_master" );

	while( pEnt )
	{
		CControlPointMaster *pMaster = dynamic_cast<CControlPointMaster *>( pEnt );

		if ( pMaster->IsActive() )
		{
			pMaster->CheckWinConditions();
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point_master" );
	}

	// Capping the last flag achievement
	if ( ( DODGameRules()->State_Get() == STATE_ALLIES_WIN && m_iTeam == TEAM_ALLIES ) ||
			( DODGameRules()->State_Get() == STATE_AXIS_WIN && m_iTeam == TEAM_AXIS ) )
	{
		// limit to flag cap maps
		if ( m_iBombsRequired == 0 )
		{
			for ( int i=0;i<iNumCappers;i++ )
			{
				CDODPlayer *pDODPlayer = ToDODPlayer( UTIL_PlayerByIndex( pCappingPlayers[i] ) );

				if ( pDODPlayer )
				{
					pDODPlayer->AwardAchievement( ACHIEVEMENT_DOD_CAP_LAST_FLAG );
				}
			}
		}
	}
}

void CControlPoint::SendCapString( int team, int iNumCappers, int *pCappingPlayers )
{
	if( strlen( STRING(m_iszPrintName) ) <= 0 )
		return;

	IGameEvent * event = gameeventmanager->CreateEvent( "dod_point_captured" );

	if ( event )
	{
		event->SetInt( "cp", m_iPointIndex );
		event->SetString( "cpname", STRING(m_iszPrintName) );

		char cappers[9];	// pCappingPlayers is max length 8
		int i;
		for( i=0;i<iNumCappers;i++ )
		{
			cappers[i] = (char)pCappingPlayers[i];
		}

		cappers[i] = '\0';

		// pCappingPlayers is a null terminated list of player indeces
		event->SetString( "cappers", cappers );
		event->SetInt( "priority", 9 );

		event->SetBool( "bomb", m_bSetOwnerIsBombPlant );

		gameeventmanager->FireEvent( event );
	}
}

void CControlPoint::CaptureBlocked( CDODPlayer *pPlayer )
{
	if( strlen( STRING(m_iszPrintName) ) <= 0 )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "dod_capture_blocked" );

	if ( event )
	{
		event->SetInt( "cp", m_iPointIndex );
		event->SetString( "cpname", STRING(m_iszPrintName) );
		event->SetInt( "blocker", pPlayer->entindex() );
		event->SetInt( "priority", 9 );
		event->SetBool( "bomb", GetBombsRemaining() > 0 );

		gameeventmanager->FireEvent( event );
	}

	pPlayer->AddScore( PLAYER_POINTS_FOR_BLOCK );

	if ( GetBombsRemaining() <= 0 )		// no bomb blocks ( kills planter or defuser )
	{
		pPlayer->StatEvent_CaptureBlocked();
	}

	pPlayer->Stats_AreaDefended();
	DODGameRules()->Stats_PlayerDefended( pPlayer->GetTeamNumber(), pPlayer->m_Shared.DesiredPlayerClass() );
}

// GetOwner
// ========
// returns the current owner
// 2 for allies
// 3 for axis
// 0 if noone owns it

int CControlPoint::GetOwner( void ) const
{ 
	return m_iTeam;
}

int CControlPoint::GetDefaultOwner( void ) const
{
	return m_iDefaultOwner;
}

int CControlPoint::GetCPGroup( void )
{
	return m_iCPGroup;
}

// PointValue
// ==========
// returns the time-based point value of this control point
int CControlPoint::PointValue( void )
{
	// teams earn 0 points for a flag they start with
	// after that its 1 additional point for each flag closer to the spawn.

	int value = 0;

	if ( FBitSet( m_spawnflags, CAP_POINT_TICK_FOR_BOMBS_REMAINING ) )
	{
		value = m_iBombsRemaining;
	}
	else if ( GetOwner() == m_iDefaultOwner )
	{
		value = 0;
	}
	else
	{
		if ( GetOwner() == TEAM_ALLIES )
		{
			value = m_iTimedPointsAllies;
		}
		else if ( GetOwner() == TEAM_AXIS )
		{
			value = m_iTimedPointsAxis;
		}
	}

	return value;
}

//precache the necessary models and sounds
void CControlPoint::Precache( void )
{
	if ( m_iszAlliesCapSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_iszAlliesCapSound) );
	}
	if ( m_iszAxisCapSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_iszAxisCapSound) );
	}
	if ( m_iszResetSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_iszResetSound) );	
	}

	PrecacheModel( STRING( m_iszAlliesModel ) );
	PrecacheModel( STRING( m_iszAxisModel ) );
	PrecacheModel( STRING( m_iszResetModel ) );

	PrecacheMaterial( STRING( m_iszAlliesIcon ) );
	m_iAlliesIcon = GetMaterialIndex( STRING( m_iszAlliesIcon ) );
	Assert( m_iAlliesIcon != 0 );

	PrecacheMaterial( STRING( m_iszAxisIcon ) );
	m_iAxisIcon = GetMaterialIndex( STRING( m_iszAxisIcon ) );
	Assert( m_iAxisIcon != 0 );

	PrecacheMaterial( STRING( m_iszNeutralIcon ) );
	m_iNeutralIcon = GetMaterialIndex( STRING( m_iszNeutralIcon ) );
	Assert( m_iNeutralIcon != 0 );

	if ( strlen( STRING( m_iszTimerCapIcon ) ) > 0 )
	{
		PrecacheMaterial( STRING( m_iszTimerCapIcon ) );
		m_iTimerCapIcon = GetMaterialIndex( STRING( m_iszTimerCapIcon ) );
	}

	if ( strlen( STRING( m_iszBombedIcon ) ) > 0 )
	{
		PrecacheMaterial( STRING( m_iszBombedIcon ) );
		m_iBombedIcon = GetMaterialIndex( STRING( m_iszBombedIcon ) );
	}

	if( !m_iNeutralIcon || !m_iAxisIcon || !m_iAlliesIcon )
	{
		Warning( "Invalid hud icon material in control point ( point index %d )\n", GetPointIndex() );
	}
}

// Keyvalue
// ========
// this function interfaces with the keyvalues set by the mapper
//
// Values:
// point_name - the name of the point displayed in the capture string ( and ControlStatus command )
// point_reset_time - time after which the point spontaneously resets - currently not used
// point_default_owner - the owner of this point at the start and after resets

// point_allies_capsound	|
// point_axis_capsound		|	the sounds that are made when the points are capped by each team
// point_resetsound			|
//
// point_allies_model		|
// point_axis_model			|	the models that the ent is set to after each team caps it
// point_reset_model		|

//point_team_points - number of team points to give to the capper's team for capping

bool CControlPoint::KeyValue( const char *szKeyName, const char *szValue )
{	
	if (FStrEq(szKeyName, "point_default_owner"))
	{
		m_iDefaultOwner = atoi(szValue);

		Assert( m_iDefaultOwner == TEAM_ALLIES || 
				m_iDefaultOwner == TEAM_AXIS || 
				m_iDefaultOwner == TEAM_UNASSIGNED );

		if( m_iDefaultOwner != TEAM_ALLIES &&
			m_iDefaultOwner != TEAM_AXIS && 
			m_iDefaultOwner != TEAM_UNASSIGNED )
		{
			Warning( "dod_control_point with bad point_default_owner - probably '1'\n" );
			m_iDefaultOwner = TEAM_UNASSIGNED;
		}
	}
	else if (FStrEq(szKeyName, "point_allies_model_bodygroup"))
	{
		m_iAlliesModelBodygroup = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "point_axis_model_bodygroup"))
	{
		m_iAxisModelBodygroup = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "point_reset_model_bodygroup"))
	{
		m_iResetModelBodygroup = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "point_index"))
	{
		m_iPointIndex = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "point_hud_icon_allies")) 
	{
		m_iszAlliesIcon = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "point_hud_icon_axis")) 
	{
		m_iszAxisIcon = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "point_hud_icon_neutral")) 
	{
		m_iszNeutralIcon = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "point_hud_icon_timercap")) 
	{
		m_iszTimerCapIcon = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "point_hud_icon_bombed")) 
	{
		m_iszBombedIcon = AllocPooledString(szValue);
	}
	else
		return CBaseEntity::KeyValue( szKeyName, szValue );

	return true;
}

void CControlPoint::SetActive( bool active )
{
	m_bActive = active;
	
	if( active )
	{
		RemoveEffects( EF_NODRAW );
	}
	else
	{
		AddEffects( EF_NODRAW );
	}
}

void CControlPoint::BombPlanted( float flTimerLength, CDODPlayer *pPlantingPlayer )
{
	if ( m_bBombPlanted == true )
	{
		Warning( "ERROR - dod_control_point only supports one bomb being planted at once\n" );
		return;
	}

	//send it to everyone
	IGameEvent *event = gameeventmanager->CreateEvent( "dod_bomb_planted" );
	if ( event )
	{
		int iOppositeTeam = ( pPlantingPlayer->GetTeamNumber() == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;

		event->SetInt( "team", iOppositeTeam );

		event->SetInt( "cp", m_iPointIndex );
		event->SetString( "cpname", STRING(m_iszPrintName) );
		event->SetInt( "userid", pPlantingPlayer->GetUserID() );

		gameeventmanager->FireEvent( event );
	}

	m_bBombPlanted = true;
	m_flBombExplodeTime = gpGlobals->curtime + flTimerLength;

	// give the planter a point
	pPlantingPlayer->AddScore( PLAYER_POINTS_FOR_BOMB_PLANT );

	pPlantingPlayer->StatEvent_BombPlanted();

	// set hud display
	// this triggers the countdown ( using a shared, constant bomb timer )
	g_pObjectiveResource->SetBombPlanted( GetPointIndex(), true );
}

void CControlPoint::BombExploded( CDODPlayer *pPlantingPlayer /* = NULL */, int iPlantingTeam /* = TEAM_UNASSIGNED */ )
{
	m_bBombPlanted = false;

	m_iBombsRemaining -= 1;
	g_pObjectiveResource->SetBombsRemaining( GetPointIndex(), m_iBombsRemaining );

	if ( pPlantingPlayer )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_bomb_exploded" );
		if ( event )
		{
			event->SetInt( "cp", m_iPointIndex );
			event->SetString( "cpname", STRING(m_iszPrintName) );
			event->SetInt( "userid", pPlantingPlayer->GetUserID() );

			gameeventmanager->FireEvent( event );
		}

		pPlantingPlayer->Stats_BombDetonated();

		int iCappingPlayer = pPlantingPlayer->entindex();

		DODGameRules()->CapEvent( CAP_EVENT_BOMB, iPlantingTeam );

		if ( m_iBombsRemaining <= 0 )
		{
			m_bSetOwnerIsBombPlant = true;

			InternalSetOwner( iPlantingTeam, true, 1, &iCappingPlayer );
			g_pObjectiveResource->SetOwningTeam( GetPointIndex(), GetOwner() );

			m_bSetOwnerIsBombPlant = false;

			// top up points so it equals PLAYER_POINTS_FOR_BOMB_EXPLODED
			pPlantingPlayer->AddScore( PLAYER_POINTS_FOR_BOMB_EXPLODED - PLAYER_POINTS_FOR_CAP );
		}
		else
		{
			// give that bomber a point!
			pPlantingPlayer->AddScore( PLAYER_POINTS_FOR_BOMB_EXPLODED );

			pPlantingPlayer->Stats_AreaCaptured();

			pPlantingPlayer->StatEvent_PointCaptured();

			// send something to death notices to show that something was accomplished

			IGameEvent * event = gameeventmanager->CreateEvent( "dod_point_captured" );

			if ( event )
			{
				event->SetInt( "cp", m_iPointIndex );
				event->SetString( "cpname", STRING(m_iszPrintName) );

				char cappers[3];
				cappers[0] = iCappingPlayer;
				cappers[1] = '\0';

				// pCappingPlayers is a null terminated list of player indeces
				event->SetString( "cappers", cappers );
				event->SetInt( "priority", 9 );

				event->SetBool( "bomb", true );

				gameeventmanager->FireEvent( event );
			}
		}
	}
	else
	{
		if ( m_iBombsRemaining <= 0 )
		{
			// get the opposite team
			int team = ( GetOwner() == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES;

			InternalSetOwner( team, true );
			g_pObjectiveResource->SetOwningTeam( GetPointIndex(), GetOwner() );
		}
	}

	g_pObjectiveResource->SetBombPlanted( GetPointIndex(), false );
}

void CControlPoint::BombDisarmed( CDODPlayer *pDisarmingPlayer )
{
	CancelBombPlanted();

	CaptureBlocked( pDisarmingPlayer );

	IGameEvent *event = gameeventmanager->CreateEvent( "dod_bomb_defused" );
	if ( event )
	{
		event->SetInt( "cp", m_iPointIndex );
		event->SetString( "cpname", STRING(m_iszPrintName) );
		event->SetInt( "userid", pDisarmingPlayer->GetUserID() );
		event->SetInt( "team", pDisarmingPlayer->GetTeamNumber() );
		gameeventmanager->FireEvent( event );
	}
}

void CControlPoint::CancelBombPlanted( void )
{
	m_bBombPlanted = false;

	// reset hud display
	g_pObjectiveResource->SetBombPlanted( GetPointIndex(), false );
}