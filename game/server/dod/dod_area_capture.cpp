//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_area_capture.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "dod_control_point.h"
#include "dod_team.h"
#include "dod_objective_resource.h"


//-------------------------------------------------------------------
// CAreaCapture
// An area entity that players must remain in in order to active another entity
// Triggers are fired on start of capture, on end of capture and on broken capture
// Can either be capped by both teams at once, or just by one
// Time to capture and number of people required to capture are both passed by the mapper



BEGIN_DATADESC(CAreaCapture)

	// Touch functions
	DEFINE_FUNCTION( AreaTouch ),

	// Think functions
	DEFINE_THINKFUNC( Think ),

	// Keyfields
	DEFINE_KEYFIELD( m_iszCapPointName,			FIELD_STRING,	"area_cap_point" ),

	DEFINE_KEYFIELD( m_nAlliesNumCap,			FIELD_INTEGER,	"area_allies_numcap" ),
	DEFINE_KEYFIELD( m_nAxisNumCap,				FIELD_INTEGER,	"area_axis_numcap" ),
	DEFINE_KEYFIELD( m_flCapTime,				FIELD_FLOAT,	"area_time_to_cap" ),

	DEFINE_KEYFIELD( m_bAlliesCanCap,			FIELD_BOOLEAN,	"area_allies_cancap" ),
	DEFINE_KEYFIELD( m_bAxisCanCap,				FIELD_BOOLEAN,	"area_axis_cancap" ),

	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundInit", InputRoundInit ),

	// Outputs
	DEFINE_OUTPUT( m_AlliesStartOutput, "OnAlliesStartCap" ),
	DEFINE_OUTPUT( m_AlliesBreakOutput, "OnAlliesBreakCap" ),
	DEFINE_OUTPUT( m_AlliesCapOutput,	"OnAlliesEndCap" ),

	DEFINE_OUTPUT( m_AxisStartOutput,	"OnAxisStartCap" ),
	DEFINE_OUTPUT( m_AxisBreakOutput,	"OnAxisBreakCap" ),
	DEFINE_OUTPUT( m_AxisCapOutput,		"OnAxisEndCap" ),

	DEFINE_OUTPUT( m_StartOutput,	"OnStartCap" ),
	DEFINE_OUTPUT( m_BreakOutput,	"OnBreakCap" ),
	DEFINE_OUTPUT( m_CapOutput,		"OnEndCap" ),

//	DEFINE_OUTPUT( m_OnStartCap, "OnStartCap" );
//	DEFINE_OUTPUT( m_OnBreakCap, 

//	DEFINE_OUTPUT( m_OnEnterNoObj, "OnEnterNoObj" );

END_DATADESC();

LINK_ENTITY_TO_CLASS( dod_capture_area, CAreaCapture );

void CAreaCapture::Spawn( void )
{
	BaseClass::Spawn();
	
	InitTrigger();
	
	Precache();

	m_iAreaIndex = -1;
		
	SetTouch ( &CAreaCapture::AreaTouch );		

	m_bCapturing = false;
	m_nCapturingTeam = TEAM_UNASSIGNED;
	m_nOwningTeam = TEAM_UNASSIGNED;
	m_fTimeRemaining = 0.0f;
	
	SetNextThink( gpGlobals->curtime + AREA_THINK_TIME );

	if( m_nAlliesNumCap < 1 )
		m_nAlliesNumCap = 1;

	if( m_nAxisNumCap < 1 )
		m_nAxisNumCap = 1;

	m_bDisabled = false;

	m_iCapAttemptNumber = 0;
}

void CAreaCapture::Precache( void )
{
}

bool CAreaCapture::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}

//sends to all players at the start of the round
//needed?
void CAreaCapture::area_SetIndex( int index )
{
	m_iAreaIndex = index;
}

bool CAreaCapture::IsActive( void )
{
	return !m_bDisabled;
}

void CAreaCapture::AreaTouch( CBaseEntity *pOther )
{
	//if they are touching, set their SIGNAL flag on, and their m_iCapAreaNum to ours
	//then in think do all the scoring

	if( !IsActive() )
		return;

	//Don't cap areas unless the round is running
	if( DODGameRules()->State_Get() != STATE_RND_RUNNING || DODGameRules()->IsInWarmup() )
		return;

	Assert( m_iAreaIndex != -1 );

	if( m_pPoint )
	{
		m_nOwningTeam = m_pPoint->GetOwner();
	}
	
	//dont touch for non-alive or non-players
	if( !pOther->IsPlayer() )
		return;

	if( !pOther->IsAlive() )
		return;

	CDODPlayer *pPlayer = ToDODPlayer(pOther);

	ASSERT( pPlayer );

	if ( pPlayer->GetTeamNumber() != m_nOwningTeam )
	{
		bool bAbleToCap = ( pPlayer->GetTeamNumber() == TEAM_ALLIES && m_bAlliesCanCap ) ||
							( pPlayer->GetTeamNumber() == TEAM_AXIS && m_bAxisCanCap );

		if ( bAbleToCap )
            pPlayer->HintMessage( HINT_IN_AREA_CAP );
	}

	pPlayer->m_signals.Signal( SIGNAL_CAPTUREAREA );

	//add them to this area
	pPlayer->SetCapAreaIndex( m_iAreaIndex );

	if ( m_pPoint )
	{
		pPlayer->SetCPIndex( m_pPoint->GetPointIndex() );
	}	
}

/* three ways to be capturing a cap area
 * 1) have the required number of people in the area
 * 2) have less than the required number on your team, new required num is everyone
 * 3) have less than the required number alive, new required is numAlive, but time is lengthened
 */

ConVar dod_simulatemultiplecappers( "dod_simulatemultiplecappers", "1", FCVAR_CHEAT );

void CAreaCapture::Think( void )
{
	SetNextThink( gpGlobals->curtime + AREA_THINK_TIME );

	if( DODGameRules()->State_Get() != STATE_RND_RUNNING )
	{
		// If we were being capped, cancel it
		if( m_nNumAllies > 0 || m_nNumAxis > 0 )
		{
			m_nNumAllies = 0;
			m_nNumAxis = 0;
			SendNumPlayers();

			if( m_pPoint )
			{
				g_pObjectiveResource->SetCappingTeam( m_pPoint->GetPointIndex(), TEAM_UNASSIGNED );
			}
		}
		return;
	}

	// go through our list of players

	int iNumAllies = 0;
	int iNumAxis = 0;

	CDODPlayer *pFirstAlliedTouching = NULL;
	CDODPlayer *pFirstAxisTouching = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *ent = UTIL_PlayerByIndex( i );
		if ( ent )
		{
			CDODPlayer *pPlayer = ToDODPlayer(ent);

			//First check if the player is in fact in this area
			if ( ( pPlayer->m_signals.GetState() & SIGNAL_CAPTUREAREA ) &&
				pPlayer->GetCapAreaIndex() == m_iAreaIndex &&
				pPlayer->IsAlive() )		// alive check is kinda unnecessary, but there is some
											// case where non-present people are messing up this count
			{	
				if ( pPlayer->GetTeamNumber() == TEAM_ALLIES )
				{
					if ( iNumAllies == 0 )
						pFirstAlliedTouching = pPlayer;

					iNumAllies++;					
				}
				else if ( pPlayer->GetTeamNumber() == TEAM_AXIS )
				{
					if ( iNumAxis == 0 )
						pFirstAxisTouching = pPlayer;

					iNumAxis++;
				}
			}
		}
	}

	iNumAllies *= dod_simulatemultiplecappers.GetInt();
	iNumAxis *= dod_simulatemultiplecappers.GetInt();

	if( iNumAllies != m_nNumAllies || iNumAxis != m_nNumAxis )
	{
		m_nNumAllies = iNumAllies;
		m_nNumAxis = iNumAxis;
		SendNumPlayers();
	}

	// when a player blocks, tell them the cap index and attempt number
	// only give successive blocks to them if the attempt number is different

	if( m_bCapturing )
	{
		//its a regular cap
		//Subtract some time from the cap
		m_fTimeRemaining -= AREA_THINK_TIME;

		//if both teams are in the area
		if( iNumAllies > 0 && iNumAxis > 0 )
		{
			// See if anyone gets credit for the block
			float flPercentToGo = m_fTimeRemaining / m_flCapTime;
			if ( flPercentToGo <= 0.5 && m_pPoint )
			{
				// find the first player that is not on the capturing team
				// they have just broken a cap and should be rewarded		
				// tell the player the capture attempt number, for checking later
				CDODPlayer *pBlockingPlayer = ( m_nCapturingTeam == TEAM_ALLIES ) ? pFirstAxisTouching : pFirstAlliedTouching;

				if ( pBlockingPlayer )
				{
					if ( pBlockingPlayer->GetCapAreaIndex() == m_iAreaIndex &&
						 pBlockingPlayer->GetLastBlockCapAttempt() == m_iCapAttemptNumber )
					{
						// this is a repeat block on the same cap, ignore it
						NULL;
					}
					else
					{
                        m_pPoint->CaptureBlocked( pBlockingPlayer );
						pBlockingPlayer->StoreCaptureBlock( m_iAreaIndex, m_iCapAttemptNumber );
					}
				}
			}

			BreakCapture( false );
			return;
		}

		//if no-one is in the area
		if( iNumAllies == 0 && iNumAxis == 0 )
		{
			BreakCapture( true );
			return;
		}
		
		if( m_nCapturingTeam == TEAM_ALLIES )
		{
			if( iNumAllies < m_nAlliesNumCap )
			{
				BreakCapture( true );
			}
		}
		else if( m_nCapturingTeam == TEAM_AXIS )
		{
			if( iNumAxis < m_nAxisNumCap )
			{
				BreakCapture( true );
			}
		}

		//if the cap is done
		if( m_fTimeRemaining <= 0 )
		{
			EndCapture( m_nCapturingTeam );
			return;		//we're done
		}			
	}
	else	//not capturing yet
	{
		bool bStarted = false;
		
		if( iNumAllies > 0 && iNumAxis <= 0 && m_bAlliesCanCap && m_nOwningTeam != TEAM_ALLIES )
		{
			if( iNumAllies >= m_nAlliesNumCap )
			{
				m_iCappingRequired = m_nAlliesNumCap;	
				m_iCappingPlayers = iNumAllies;	
				StartCapture( TEAM_ALLIES, CAPTURE_NORMAL );
				bStarted = true;
			}
		}
		else if( iNumAxis > 0 && iNumAllies <= 0 && m_bAxisCanCap && m_nOwningTeam != TEAM_AXIS )
		{
			if( iNumAxis >= m_nAxisNumCap )
			{
				m_iCappingRequired = m_nAxisNumCap;	
				m_iCappingPlayers = iNumAxis;	
				StartCapture( TEAM_AXIS, CAPTURE_NORMAL );
				bStarted = true;
			}
		}
	}
}

void CAreaCapture::SetOwner( int team )
{
	//break any current capturing
	BreakCapture( false );

	//set the owner to the passed value
	m_nOwningTeam = team;
	g_pObjectiveResource->SetOwningTeam( m_pPoint->GetPointIndex(), m_nOwningTeam );
}

void CAreaCapture::SendNumPlayers( CBasePlayer *pPlayer )
{
	if( !m_pPoint )
		return;

	int index = m_pPoint->GetPointIndex();

	g_pObjectiveResource->SetNumPlayers( index, TEAM_ALLIES, m_nNumAllies );
	g_pObjectiveResource->SetNumPlayers( index, TEAM_AXIS, m_nNumAxis );
}

void CAreaCapture::StartCapture( int team, int capmode )
{
	int iNumCappers = 0;

	//trigger start
	if( team == TEAM_ALLIES )
	{
		m_AlliesStartOutput.FireOutput(this,this);
		iNumCappers = m_nAlliesNumCap;
	}
	else if( team == TEAM_AXIS )
	{
		m_AxisStartOutput.FireOutput(this,this);
		iNumCappers = m_nAxisNumCap;
	}

	m_StartOutput.FireOutput(this,this);
	
	m_nCapturingTeam = team;
	m_fTimeRemaining = m_flCapTime;
	m_bCapturing = true;
	m_iCapMode = capmode;

	if( m_pPoint )
	{
		//send a message that we're starting to cap this area
		g_pObjectiveResource->SetCappingTeam( m_pPoint->GetPointIndex(), m_nCapturingTeam );
	}	
}

#define MAX_AREA_CAPPERS 9

void CAreaCapture::EndCapture( int team )
{
	m_iCapAttemptNumber++;

	//do the triggering
	if( team == TEAM_ALLIES )
	{
		m_AlliesCapOutput.FireOutput(this,this);
	}
	else if( team == TEAM_AXIS )
	{
		m_AxisCapOutput.FireOutput(this,this);
	}

	m_CapOutput.FireOutput(this,this);

	m_iCappingRequired = 0;
	m_iCappingPlayers = 0;

	int numcappers = 0;
	int cappingplayers[MAX_AREA_CAPPERS];

	CDODPlayer *pCappingPlayer = NULL;

	CDODTeam *pTeam = GetGlobalDODTeam(team);
	if ( pTeam )
	{
		int iCount = pTeam->GetNumPlayers();

		for ( int i=0;i<iCount;i++ )
		{
			CDODPlayer *pPlayer = ToDODPlayer( pTeam->GetPlayer(i) );
			if ( pPlayer )
			{
				if( ( pPlayer->m_signals.GetState() & SIGNAL_CAPTUREAREA ) &&
					pPlayer->GetCapAreaIndex() == m_iAreaIndex && 
					pPlayer->IsAlive() )
				{
					if( pCappingPlayer == NULL )
						pCappingPlayer = pPlayer;

					if ( numcappers < MAX_AREA_CAPPERS-1 )
					{
						cappingplayers[numcappers] = pPlayer->entindex();
						numcappers++;
					}
				}
			}
		}
	}	

	if ( numcappers < MAX_AREA_CAPPERS )
	{
		cappingplayers[numcappers] = 0;	//null terminate :)
	}
			
	m_nOwningTeam = team;
	m_bCapturing = false;
	m_fTimeRemaining = 0.0f;

	//there may have been more than one capper, but only report this one.
	//he hasnt gotten points yet, and his name will go in the cap string if its needed
	//first capper gets name sent and points given by flag.
	//other cappers get points manually above, no name in message

	//send the player in the cap string
	if( m_pPoint )
	{
		DODGameRules()->CapEvent( CAP_EVENT_FLAG, m_nOwningTeam );

		g_pObjectiveResource->SetOwningTeam( m_pPoint->GetPointIndex(), m_nOwningTeam );
		m_pPoint->SetOwner( m_nOwningTeam, true, numcappers, cappingplayers );
	}
}

void CAreaCapture::BreakCapture( bool bNotEnoughPlayers )
{
	if( m_bCapturing )
	{
		m_iCappingRequired = 0;
		m_iCappingPlayers = 0;
				
		if( m_nCapturingTeam == TEAM_ALLIES )
			m_AlliesBreakOutput.FireOutput(this,this);

		else if( m_nCapturingTeam == TEAM_AXIS )
			m_AxisBreakOutput.FireOutput(this,this);

		m_BreakOutput.FireOutput(this,this);

		m_bCapturing = false;

		if( m_pPoint )
		{
			g_pObjectiveResource->SetCappingTeam( m_pPoint->GetPointIndex(), TEAM_UNASSIGNED );
		}

		if ( bNotEnoughPlayers )
		{
			m_iCapAttemptNumber++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAreaCapture::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAreaCapture::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CAreaCapture::InputRoundInit( inputdata_t &inputdata )
{
	// find the flag we're linked to
	if( !m_pPoint )
	{
		m_pPoint = dynamic_cast<CControlPoint*>( gEntList.FindEntityByName(NULL, STRING(m_iszCapPointName) ) );

		if ( m_pPoint )
		{
			m_pPoint->SetNumCappersRequired( m_nAlliesNumCap, m_nAxisNumCap );
			g_pObjectiveResource->SetCPRequiredCappers( m_pPoint->GetPointIndex(), m_nAlliesNumCap, m_nAxisNumCap );
			g_pObjectiveResource->SetCPCapTime( m_pPoint->GetPointIndex(), m_flCapTime, m_flCapTime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if this player's death causes a block
// return FALSE if the player is not in this area
// return TRUE otherwise ( eg player is in area, but his death does not cause break )
//-----------------------------------------------------------------------------
bool CAreaCapture::CheckIfDeathCausesBlock( CDODPlayer *pVictim, CDODPlayer *pKiller )
{
	// This shouldn't happen
	if ( !pVictim || !pKiller )
	{
		Assert( !"Why are null players getting here?" );
		return false;
	}

	// make sure this player is in this area
	if ( pVictim->GetCapAreaIndex() != m_iAreaIndex )
		return false;

	// Teamkills shouldn't give a block reward
	if ( pVictim->GetTeamNumber() == pKiller->GetTeamNumber() )
		return true;

	// return if the area is not being capped
	if ( !m_bCapturing )
		return true;

	int iTeam = pVictim->GetTeamNumber();

	// return if this player's team is not capping the area
	if ( iTeam != m_nCapturingTeam )
		return true;

	bool bBlocked = false;

	if ( m_nCapturingTeam == TEAM_ALLIES )
	{
		if ( m_nNumAllies-1 < m_nAlliesNumCap )
			bBlocked = true;
	}
	else if ( m_nCapturingTeam == TEAM_AXIS )
	{
		if ( m_nNumAxis-1 < m_nAxisNumCap )
			bBlocked = true;
	}

	// break early incase we kill multiple people in the same frame
	if ( bBlocked )
	{
		m_pPoint->CaptureBlocked( pKiller );
		BreakCapture( false );
	}

	return true;
}
