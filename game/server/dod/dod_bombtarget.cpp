//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "dod_shareddefs.h"
#include "dod_bombtarget.h"
#include "basecombatweapon.h"
#include "weapon_dodbasebomb.h"
#include "dod_team.h"
#include "dod_shareddefs.h"
#include "dod_objective_resource.h"

BEGIN_DATADESC(CDODBombTarget)

	DEFINE_KEYFIELD( m_iszCapPointName,		FIELD_STRING, "target_control_point" ),

	DEFINE_KEYFIELD( m_iBombingTeam,		FIELD_INTEGER, "bombing_team" ),

	DEFINE_KEYFIELD( m_iTimerAddSeconds,	FIELD_INTEGER, "add_timer_seconds" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	DEFINE_OUTPUT(	m_OnBecomeActive,		"OnBombTargetActivated" ),
	DEFINE_OUTPUT(	m_OnBecomeInactive,		"OnBombTargetDeactivated" ),
	DEFINE_OUTPUT(	m_OnBombPlanted,		"OnBombPlanted" ),
	DEFINE_OUTPUT(	m_OnBombExploded,		"OnBombExploded" ),
	DEFINE_OUTPUT(	m_OnBombDisarmed,		"OnBombDefused" ),

	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_INTEGER, "StartDisabled" ),

	DEFINE_USEFUNC( State_Use ),
	DEFINE_THINKFUNC( State_Think ),

END_DATADESC();

IMPLEMENT_SERVERCLASS_ST(CDODBombTarget, DT_DODBombTarget)
	SendPropInt( SENDINFO(m_iState), 3 ),
	SendPropInt( SENDINFO(m_iBombingTeam), 3 ),

	SendPropModelIndex( SENDINFO(m_iTargetModel) ),
	SendPropModelIndex( SENDINFO(m_iUnavailableModel) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( dod_bomb_target, CDODBombTarget );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::Spawn( void )
{
	m_pControlPoint = NULL;

	Precache();

	SetTouch( NULL );
	SetUse( &CDODBombTarget::State_Use );
	SetThink( &CDODBombTarget::State_Think );
	SetNextThink( gpGlobals->curtime + 0.1 );

	m_pCurStateInfo = NULL;
	if ( m_bStartDisabled )
		State_Transition( BOMB_TARGET_INACTIVE );
	else
		State_Transition( BOMB_TARGET_ACTIVE );
	
	BaseClass::Spawn();

	// incase we have any animating bomb models
	SetPlaybackRate( 1.0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( DOD_BOMB_TARGET_MODEL_ARMED );
	m_iTargetModel = PrecacheModel( DOD_BOMB_TARGET_MODEL_TARGET );
	m_iUnavailableModel = PrecacheModel( DOD_BOMB_TARGET_MODEL_UNAVAILABLE );
}

//-----------------------------------------------------------------------------
// Purpose: change use flags based on state
//-----------------------------------------------------------------------------
int	CDODBombTarget::ObjectCaps()
{ 
	int caps = BaseClass::ObjectCaps();

	if ( State_Get() != BOMB_TARGET_INACTIVE )
	{
		caps |= (FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS);
	}

	return caps;
}

//-----------------------------------------------------------------------------
// Purpose: timer length accessor
//-----------------------------------------------------------------------------
float CDODBombTarget::GetBombTimerLength( void )
{
	return DOD_BOMB_TIMER_LENGTH;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Transition( BombTargetState newState )
{
	State_Leave();
	State_Enter( newState );
}	

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Enter( BombTargetState newState )
{
	m_pCurStateInfo = State_LookupInfo( newState );

	if ( 0 )
	{
		if ( m_pCurStateInfo )
			Msg( "DODRoundState: entering '%s'\n", m_pCurStateInfo->m_pStateName );
		else
			Msg( "DODRoundState: entering #%d\n", newState );
	}

	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState)();

	m_iState = (int)newState;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState)();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Think()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnThink )
	{
		(this->*m_pCurStateInfo->pfnThink)();
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnUse )
	{
		(this->*m_pCurStateInfo->pfnUse)( pActivator, pCaller, useType, value );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDODBombTargetStateInfo* CDODBombTarget::State_LookupInfo( BombTargetState state )
{
	static CDODBombTargetStateInfo bombTargetStateInfos[] =
	{
		{ BOMB_TARGET_INACTIVE,	"BOMB_TARGET_INACTIVE",		&CDODBombTarget::State_Enter_INACTIVE, NULL, NULL, NULL },
		{ BOMB_TARGET_ACTIVE,	"BOMB_TARGET_ACTIVE",		&CDODBombTarget::State_Enter_ACTIVE, NULL, NULL, &CDODBombTarget::State_Use_ACTIVE },
		{ BOMB_TARGET_ARMED,	"BOMB_TARGET_ARMED",		&CDODBombTarget::State_Enter_ARMED, &CDODBombTarget::State_Leave_Armed, &CDODBombTarget::State_Think_ARMED, &CDODBombTarget::State_Use_ARMED }
	};

	for ( int i=0; i < ARRAYSIZE( bombTargetStateInfos ); i++ )
	{
		if ( bombTargetStateInfos[i].m_iState == state )
			return &bombTargetStateInfos[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Enter_INACTIVE( void )
{
	// go invisible
	AddEffects( EF_NODRAW );

	m_OnBecomeInactive.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Enter_ACTIVE( void )
{
	RemoveEffects( EF_NODRAW );

	// set transparent model
	SetModel( DOD_BOMB_TARGET_MODEL_TARGET );

	m_OnBecomeActive.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Enter_ARMED( void )
{
	RemoveEffects( EF_NODRAW );

	// set solid model
	SetModel( DOD_BOMB_TARGET_MODEL_ARMED );

	// start count down
	m_flExplodeTime = gpGlobals->curtime + GetBombTimerLength();

	m_OnBombPlanted.FireOutput( this, this );

	// tell CP our time until detonation
	CControlPoint *pCP = GetControlPoint();
	if ( pCP )
	{
		pCP->BombPlanted( GetBombTimerLength(), m_pPlantingPlayer );
	}	

	EmitSound( "Weapon_C4.Fuse" );

	static int iWickSeq = LookupSequence( "w_tnt_wick" );
	ResetSequence( iWickSeq );
}

void CDODBombTarget::State_Leave_Armed( void )
{
	StopSound( "Weapon_C4.Fuse" );
}

void CDODBombTarget::ResetDefuse( int index )
{
	DefusingPlayer *pRec = &m_DefusingPlayers[index];

	Assert( pRec );

	if ( pRec && pRec->m_pPlayer )
	{
		pRec->m_pPlayer->SetDefusing( NULL );

		//cancel the progress bar
		pRec->m_pPlayer->SetProgressBarTime( 0 );
	}

	m_DefusingPlayers.Remove( index );

	// if noone else is defusing, set objective resource to not be defusing
	if ( m_DefusingPlayers.Count() <= 0 )
	{
		CControlPoint *pCP = GetControlPoint();
		if ( pCP )
		{
			g_pObjectiveResource->SetBombBeingDefused( pCP->GetPointIndex(), false );
		}
	}
}

#include "IEffects.h"
void TE_Sparks( IRecipientFilter& filter, float delay,
			   const Vector *pos, int nMagnitude, int nTrailLength, const Vector *pDir );

extern short g_sModelIndexFireball;

void CDODBombTarget::Explode( void )
{
	// output the explosion
	EmitSound( "Weapon_C4.Explode" );

	Vector origin = GetAbsOrigin();

	CPASFilter filter( origin );

	te->Explosion( filter, -1.0, // don't apply cl_interp delay
		&origin,
		g_sModelIndexFireball,
		20,	//scale
		25,
		TE_EXPLFLAG_NONE,
		0,
		0 );

	float flDamage = 200;
	float flDmgRadius = flDamage * 2.5;
	// do a separate radius damage that ignores the world for added damage!
	CTakeDamageInfo dmgInfo( this, m_pPlantingPlayer, vec3_origin, origin, flDamage, DMG_BLAST | DMG_BOMB ); 
	DODGameRules()->RadiusDamage( dmgInfo, origin, flDmgRadius, CLASS_NONE, NULL, true );

	// stun players in a radius
	const float flStunDamage = 100;

	CTakeDamageInfo stunInfo( this, this, vec3_origin, GetAbsOrigin(), flStunDamage, DMG_STUN );
	DODGameRules()->RadiusStun( stunInfo, GetAbsOrigin(), flDmgRadius );

	State_Transition( BOMB_TARGET_INACTIVE );

	m_OnBombExploded.FireOutput( this, this );

	// tell CP bomb is no longer active
	CControlPoint *pCP = GetControlPoint();
	if ( pCP )
	{
		CDODPlayer *pPlayer = m_pPlantingPlayer;

		if ( !pPlayer || !pPlayer->IsConnected() )
		{
			// pick a random player from the bombing team

			// hax - if we are debug and team is 0, use allies
			if ( m_iBombingTeam == TEAM_UNASSIGNED )
				m_iBombingTeam = TEAM_ALLIES;

			CDODTeam *pTeam = GetGlobalDODTeam( m_iBombingTeam );

			pPlayer = NULL;

			if ( pTeam->GetNumPlayers() > 0 )
			{
				pPlayer = ToDODPlayer( pTeam->GetPlayer( 0 ) );

				if ( !pPlayer || !pPlayer->IsConnected() )
				{
					// bad situation, abandon here
					pPlayer = NULL;
				}
			}				
		}

		pCP->BombExploded( pPlayer, m_iBombingTeam );

		g_pObjectiveResource->SetBombBeingDefused( pCP->GetPointIndex(), false );
	}	

	// If we add time to the round timer, tell Gamerules
	// Don't do this if this bomb ended the game
	if ( m_iTimerAddSeconds > 0 )
	{
		if ( m_pPlantingPlayer && FStrEq( STRING(gpGlobals->mapname), "dod_jagd" ) )
		{
			// if the timer is 0:00 or less, achievement time
			if ( DODGameRules()->GetTimerSeconds() <= 0 )
			{
				m_pPlantingPlayer->AwardAchievement( ACHIEVEMENT_DOD_JAGD_OVERTIME_CAP );
			}
		}

		if ( DODGameRules()->State_Get() == STATE_RND_RUNNING )
		{
			DODGameRules()->AddTimerSeconds( m_iTimerAddSeconds );
		}
	}
}

void CDODBombTarget::BombDefused( CDODPlayer *pDefuser )
{
	// tell CP bomb is no longer active
	CControlPoint *pCP = GetControlPoint();
	if ( pCP )
	{
		pCP->BombDisarmed( pDefuser );
	}	

	if ( pDefuser )
	{
		pDefuser->StatEvent_BombDefused();
	}

	State_Transition( BOMB_TARGET_ACTIVE );

	m_OnBombDisarmed.FireOutput( this, this );
}

bool CDODBombTarget::CanPlayerStartDefuse( CDODPlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() || !pPlayer->IsConnected() )
	{
		// if the defuser is not alive or has disconnected, reset
		return false;
	}

	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
	{
		// they are not on the ground, remove them
		pPlayer->HintMessage( HINT_BOMB_DEFUSE_ONGROUND );

		return false;
	}

	Vector vecDist = ( pPlayer->GetAbsOrigin() - GetAbsOrigin() );
	float flDist = vecDist.Length();

	if ( flDist > DOD_BOMB_DEFUSE_MAXDIST )	// PLAYER_USE_RADIUS is not actually used by the playerUse code!!
	{
		// they are too far away to continue ( or start ) defusing
		return false;
	}

	return true;
}

bool CDODBombTarget::CanPlayerContinueDefusing( CDODPlayer *pPlayer, DefusingPlayer *pDefuseRecord )
{
	if ( !pDefuseRecord || pDefuseRecord->m_flDefuseTimeoutTime < gpGlobals->curtime )
	{
		// they have not updated their +use in a while, assume they stopped
		return false;
	}

	return CanPlayerStartDefuse( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Think_ARMED( void )
{
	// count down

	if ( DODGameRules()->State_Get() != STATE_RND_RUNNING )
	{
		State_Transition( BOMB_TARGET_ACTIVE );

		// reset the timer
		CControlPoint *pCP = GetControlPoint();
		if ( pCP )
		{
			pCP->CancelBombPlanted();
		}	
	}

	// manually advance frame so that it matches bomb timer length.
	float flTimerLength = GetBombTimerLength();
	float flTimeLeft = m_flExplodeTime - gpGlobals->curtime;
	SetCycle( clamp( 1.0 - ( flTimeLeft / flTimerLength ), 0.0, 1.0 ) );

	static int iAttachment = LookupAttachment( "wick" );//ed awesome

	Vector pos;
	QAngle ang;
	GetAttachment( iAttachment, pos, ang );

	Vector forward;
	AngleVectors( ang, &forward );

	CPVSFilter filter( pos );
	TE_Sparks( filter, 0.0, &pos, 1, 1, &forward );

	// So long as we have valid defusers, we will not explode

	if ( m_DefusingPlayers.Count() > 0 )
	{
		// remove the undesirables
		// make sure they are on ground still
		// see if they have completed the defuse

		for ( int i=m_DefusingPlayers.Count()-1;i>=0;i-- )
		{
			DefusingPlayer *pDefuseRecord = &m_DefusingPlayers[i];

			CDODPlayer *pPlayer = pDefuseRecord->m_pPlayer;

			if ( !CanPlayerContinueDefusing( pPlayer, pDefuseRecord ) )
			{
				ResetDefuse( i );
			}
			else
			{
				// they are still a valid defuser

				// if their defuse complete time has passed
				if ( pDefuseRecord->m_flDefuseCompleteTime < gpGlobals->curtime )
				{
					// Defuse Complete
					BombDefused( pPlayer );		

					// remove everyone from the list
					for ( int j=m_DefusingPlayers.Count()-1;j>=0;j-- )
					{
						ResetDefuse( j );
					}

					// break out of this loop
					i = -1;
				}
			}
		}
	}
	else if ( gpGlobals->curtime > m_flExplodeTime )
	{
		// no defusers, time is up
		Explode();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Use_ACTIVE( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CDODPlayer *pPlayer = ToDODPlayer( pActivator );

	if ( !CanPlantHere( pPlayer ) )
		return;

	Vector pos = pPlayer->WorldSpaceCenter();

	float flDist = ( pos - GetAbsOrigin() ).Length();

	if ( flDist > DOD_BOMB_PLANT_RADIUS )
	{
		return;
	}

	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
	{
		pPlayer->HintMessage( HINT_BOMB_DEFUSE_ONGROUND, true );
		return;
	}

	CBaseCombatWeapon *pWeapon = NULL;

	if ( ( pWeapon = pPlayer->Weapon_OwnsThisType( "weapon_basebomb" ) ) != NULL )
	{
		CDODBaseBombWeapon *pBomb = dynamic_cast<CDODBaseBombWeapon *>( pWeapon );

		if ( pBomb )
		{
			// switch to their bomb, they will have to hit primary attack
			pPlayer->Weapon_Switch( pBomb );

			if ( pBomb == pPlayer->GetActiveWeapon() )
			{
				pBomb->PrimaryAttack();
			}
		}
	}
	else
	{
		// they don't have a bomb - play hint message
		pPlayer->HintMessage( HINT_NEED_BOMB );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::CompletePlanting( CDODPlayer *pPlantingPlayer )
{
	if ( pPlantingPlayer && ( pPlantingPlayer->GetTeamNumber() == m_iBombingTeam || m_iBombingTeam == TEAM_UNASSIGNED ) )
	{
		m_pPlantingPlayer = pPlantingPlayer;

		State_Transition( BOMB_TARGET_ARMED );
	}	
}

DefusingPlayer *CDODBombTarget::FindDefusingPlayer( CDODPlayer *pPlayer )
{
	DefusingPlayer *pRec = NULL;

	for ( int i=0;i<m_DefusingPlayers.Count();i++ )
	{
		if ( m_DefusingPlayers[i].m_pPlayer == pPlayer )
		{
			pRec = &m_DefusingPlayers[i];
			break;
		}
	}

	return pRec;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::State_Use_ARMED( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// check for people disarming
	CDODPlayer *pPlayer = ToDODPlayer( pActivator );

	if ( !pPlayer || pPlayer->GetTeamNumber() == m_iBombingTeam || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return;

	// check for distance, offground etc
	if ( CanPlayerStartDefuse( pPlayer ) == false )
	{
		return;
	}

	DefusingPlayer *pDefusingPlayerRecord = FindDefusingPlayer( pPlayer );

	// See if we already added this player to the defusing list
	if ( pDefusingPlayerRecord )
	{
		// They are still defusing for the next 0.2 seconds
		pDefusingPlayerRecord->m_flDefuseTimeoutTime = gpGlobals->curtime + 0.2;
	}
	else
	{
		// add player to the list
		DefusingPlayer defusingPlayer;		

		defusingPlayer.m_pPlayer = pPlayer;
		defusingPlayer.m_flDefuseCompleteTime = gpGlobals->curtime + DOD_BOMB_DEFUSE_TIME;
		defusingPlayer.m_flDefuseTimeoutTime = gpGlobals->curtime + 0.2;

		m_DefusingPlayers.AddToTail( defusingPlayer );

		// tell the player they are defusing
		pPlayer->SetDefusing( this );
		pPlayer->SetProgressBarTime( DOD_BOMB_DEFUSE_TIME );

		CControlPoint *pCP = GetControlPoint();
		if ( pCP )
		{
            g_pObjectiveResource->SetBombBeingDefused( pCP->GetPointIndex(), true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::InputEnable( inputdata_t &inputdata )
{
	if ( m_pCurStateInfo && m_pCurStateInfo->m_iState == BOMB_TARGET_INACTIVE )
	{
		State_Transition( BOMB_TARGET_ACTIVE );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::InputDisable( inputdata_t &inputdata )
{
	if ( m_pCurStateInfo && m_pCurStateInfo->m_iState != BOMB_TARGET_INACTIVE )
	{
		if ( m_pCurStateInfo->m_iState == BOMB_TARGET_ARMED )
		{
			// if planting, tell our cp we're not planting anymore
			CControlPoint *pCP = GetControlPoint();
			if ( pCP )
			{
				pCP->CancelBombPlanted();
			}			
		}

		State_Transition( BOMB_TARGET_INACTIVE );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CControlPoint *CDODBombTarget::GetControlPoint( void )
{
	if ( !m_pControlPoint )
	{
		if ( m_iszCapPointName == NULL_STRING )
			return NULL;

		// try to find it
		m_pControlPoint = dynamic_cast<CControlPoint *>( gEntList.FindEntityByName( NULL, STRING(m_iszCapPointName) ) );

		if ( !m_pControlPoint )
			Warning( "Could not find dod_control_point named \"%s\" for dod_bomb_target \"%s\"\n", STRING(m_iszCapPointName), STRING( GetEntityName() ) );
	}

	return m_pControlPoint;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDODBombTarget::CanPlantHere( CDODPlayer *pPlayer )
{
	if ( !m_pCurStateInfo )
		return false;

	if ( m_pCurStateInfo->m_iState != BOMB_TARGET_ACTIVE )
		return false;

	if ( pPlayer->GetTeamNumber() != m_iBombingTeam && m_iBombingTeam != TEAM_UNASSIGNED )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODBombTarget::DefuseBlocked( CDODPlayer *pAttacker )
{
	CControlPoint *pPoint = GetControlPoint();

	if ( !pPoint )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "dod_capture_blocked" );

	if ( event )
	{
		event->SetInt( "cp", pPoint->GetPointIndex() );
		event->SetString( "cpname", pPoint->GetName() );
		event->SetInt( "blocker", pAttacker->entindex() );
		event->SetInt( "priority", 9 );
		event->SetBool( "bomb", true );

		gameeventmanager->FireEvent( event );
	}

	pAttacker->AddScore( PLAYER_POINTS_FOR_BLOCK );

	pAttacker->Stats_AreaDefended();

}

void CDODBombTarget::PlantBlocked( CDODPlayer *pAttacker )
{
	CControlPoint *pPoint = GetControlPoint();

	if ( !pPoint )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "dod_capture_blocked" );

	if ( event )
	{
		event->SetInt( "cp", pPoint->GetPointIndex() );
		event->SetString( "cpname", pPoint->GetName() );
		event->SetInt( "blocker", pAttacker->entindex() );
		event->SetInt( "priority", 9 );
		event->SetBool( "bomb", true );

		gameeventmanager->FireEvent( event );
	}

	pAttacker->AddScore( PLAYER_POINTS_FOR_BLOCK );

	pAttacker->Stats_AreaDefended();
}
