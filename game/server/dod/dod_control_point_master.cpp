//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_control_point_master.h"

BEGIN_DATADESC( CControlPointMaster )
	DEFINE_KEYFIELD( m_bDisabled,		FIELD_BOOLEAN,	"StartDisabled" ),

	DEFINE_KEYFIELD( m_iTimerLength,	FIELD_INTEGER, "cpm_timer_length" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	DEFINE_INPUTFUNC( FIELD_VOID, "RoundInit", InputRoundInit ),

	DEFINE_FUNCTION( CPMThink ),

	DEFINE_OUTPUT( m_AlliesWinOutput,	"OnAlliesWin" ),
	DEFINE_OUTPUT( m_AxisWinOutput,		"OnAxisWin" ),

	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddTimerSeconds", InputAddTimerSeconds ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( dod_control_point_master, CControlPointMaster );

CControlPointMaster::CControlPointMaster()
{
	m_bUseTimer = false;
	m_iTimerTeam = TEAM_UNASSIGNED;

	SetDefLessFunc( m_ControlPoints );
}

void CControlPointMaster::Spawn( void )
{
	SetTouch( NULL );
	m_bFoundPoints = false;
	BaseClass::Spawn();
}

ConVar mp_tickpointinterval( "mp_tickpointinterval", "30", FCVAR_GAMEDLL, "Delay between point gives.", true, 1, false, 0 );

void CControlPointMaster::RoundRespawn( void )
{
	m_fGivePointsTime = gpGlobals->curtime + mp_tickpointinterval.GetInt();
}


// KeyValue
// ========
// this function interfaces with the keyvalues set by the mapper
//
// Values
// point_give_delay_time - how often to give time based points ( seconds )
// allies_capture_target - target to fire on allies complete capture
// axis_capture_target - target to fire on axis complete capture

bool CControlPointMaster::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "cpm_use_timer"))
	{
		m_bUseTimer = ( atoi(szValue) > 0 );
	}
	else if (FStrEq(szKeyName, "cpm_timer_team"))
	{
		m_iTimerTeam = atoi(szValue);
	}
	else
	{
		return CBaseEntity::KeyValue( szKeyName, szValue );
	}	

	return true;
}

void CControlPointMaster::Reset( void )
{

}

bool CControlPointMaster::FindControlPoints( void )
{
	//go through all the points
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "dod_control_point" );

	int numFound = 0;
	
	while( pEnt )
	{
		CControlPoint *pPoint = (CControlPoint *)pEnt;

		if( pPoint->IsActive() )
		{
			int index = pPoint->GetPointIndex();

			Assert( index >= 0 );

			if( m_ControlPoints.Find( index ) == m_ControlPoints.InvalidIndex())
			{
				DevMsg( 2, "Adding control point %s with index %d\n", pPoint->GetName(), index );
				m_ControlPoints.Insert( index, pPoint );
				numFound++;
			}
			else
			{
				Warning( "!!!!\nMultiple control points with the same index, duplicates ignored\n!!!!\n" );
				UTIL_Remove( pPoint );
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point" );
	}

	if( numFound > MAX_CONTROL_POINTS )
	{
		Warning( "Too many control points! Max is %d\n", MAX_CONTROL_POINTS );
	}

	//Remap the indeces of the control points so they are 0-based
	//======================
	unsigned int j;

	bool bHandled[MAX_CONTROL_POINTS];
	memset( bHandled, 0, sizeof(bHandled) );

	unsigned int numPoints = m_ControlPoints.Count();
	unsigned int newIndex = 0;

	while( newIndex < numPoints )
	{
		//Find the lowest numbered, unhandled point
		int lowestIndex = -1;
		int lowestValue = 999;

		//find the lowest unhandled index
		for( j=0; j<numPoints; j++ )
		{
			if( !bHandled[j] && m_ControlPoints[j]->GetPointIndex() < lowestValue )
			{
				lowestIndex = j;
				lowestValue = m_ControlPoints[j]->GetPointIndex();
			}
		}

		//Don't examine this point again
		Assert( lowestIndex >= 0 );
		bHandled[lowestIndex] = true;

		//Give it its new index
		m_ControlPoints[lowestIndex]->SetPointIndex( newIndex );
		newIndex++;
	}
	
	if( m_ControlPoints.Count() == 0 ) 
	{
		Warning( "Error! No control points found in map!\n");
		return false;	
	}

	g_pObjectiveResource->SetNumControlPoints( m_ControlPoints.Count() );

	unsigned int i;
	for( i=0;i<m_ControlPoints.Count();i++ )
	{
		CControlPoint *pPoint = m_ControlPoints[i];

		int iPointIndex = m_ControlPoints[i]->GetPointIndex();

		g_pObjectiveResource->SetOwningTeam( iPointIndex, pPoint->GetOwner() );
		g_pObjectiveResource->SetCPVisible( iPointIndex, pPoint->PointIsVisible() );
		g_pObjectiveResource->SetCPPosition( iPointIndex, pPoint->GetAbsOrigin() );
	
		g_pObjectiveResource->SetBombsRequired( iPointIndex, pPoint->GetBombsRequired() );
		g_pObjectiveResource->SetBombsRemaining( iPointIndex, pPoint->GetBombsRemaining() );

		g_pObjectiveResource->SetCPIcons( iPointIndex,
			pPoint->GetHudIconIndexForTeam(TEAM_ALLIES),
			pPoint->GetHudIconIndexForTeam(TEAM_AXIS),
			pPoint->GetHudIconIndexForTeam(TEAM_UNASSIGNED),
			pPoint->GetTimerCapHudIcon(),
			pPoint->GetBombedHudIcon() );
	}
	
	return true;
}

// Think
// =====
// Think is called every 0.1 seconds and checks the status of all the control points
// if one team owns them all, it gives points and resets
// Think also gives the time based points at the specified time intervals
//
// I moved the search for spawn points to the initial think - sometimes the points spawned 
// after the master and it wasnt finding them.

void CControlPointMaster::CPMThink( void )
{
	// search for all "dod_control_point" entities in the map
	// and put them in the array
	// only done the first Think

	//try to establish if any dod_area_capture ents are linked to our flags
	//via dod_point_relay

	//if there exists a point relay that has this as the target,
	//AND there exists a capture area that has that relay as a target
	//then we have our area!

	//every think

	//go through all the control points and make sure theyre the same as the last think
	//check masters

	//if they are, do nothing
	//else
	//

	//Note! Only one cpm should ever be active at the same time
	//funny things may happen if there are two of em!
	//if we are recently mastered on or off, touch the cps
					
	//---------------------------------------------------------------------------
	//below here we shouldn't execute unless we are an active control point master 
	
	//if the round has been decided, don't do any more thinking
	//if this cpm is not active, dont' do any thinking

	int iRoundState = DODGameRules()->State_Get();

	// No points or triggering new wins if we are not active 
	if( m_bDisabled || iRoundState != STATE_RND_RUNNING )
	{
		SetContextThink( &CControlPointMaster::CPMThink, gpGlobals->curtime + 0.2, FLAGS_CONTEXT );
		return;
	}

	// is it time to give time-based points yet?
	if( gpGlobals->curtime > m_fGivePointsTime )
	{
		int AlliesPoints = 0;
		int AxisPoints = 0;

		//give points based on who owns what
		unsigned int i;	
		for( i=0;i<m_ControlPoints.Count();i++ )
		{
			switch( m_ControlPoints[i]->GetOwner() )
			{
			case TEAM_ALLIES:
				AlliesPoints += m_ControlPoints[i]->PointValue();
				break;
			case TEAM_AXIS:
				AxisPoints += m_ControlPoints[i]->PointValue();
				break;
			default:
				break;
			}
		}

		//================
		//give team points
		//================
		// See if there are players active on these teams

		bool bFoundAllies = ( GetGlobalTeam(TEAM_ALLIES)->GetNumPlayers() > 0 );
		bool bFoundAxis = ( GetGlobalTeam(TEAM_AXIS)->GetNumPlayers() > 0 );

		bool bReportScore = true;
		
		// don't give team points to a team with no-one on it!
		if( AlliesPoints > 0 && bFoundAllies && DODGameRules()->State_Get() == STATE_RND_RUNNING  ) 
		{
			if( bReportScore )
			{
				if (AlliesPoints == 1)
					UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_allie_point" );
				else
				{
					char buf[8];
					Q_snprintf( buf, sizeof(buf), "%d", AlliesPoints );
					UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_allie_points", buf );
				}

			}
			GetGlobalTeam(TEAM_ALLIES)->AddScore( AlliesPoints );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_tick_points" );

			if ( event )
			{
				event->SetInt( "team", TEAM_ALLIES );
				event->SetInt( "score", AlliesPoints );
				event->SetInt( "totalscore", GetGlobalTeam(TEAM_ALLIES)->GetScore() );

				gameeventmanager->FireEvent( event );
			}
		}

		if( AxisPoints > 0 && bFoundAxis && DODGameRules()->State_Get() == STATE_RND_RUNNING ) 
		{
			if( bReportScore )
			{
				if (AxisPoints == 1)
					UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_axis_point" );
				else
				{
					char buf[8];
					Q_snprintf( buf, sizeof(buf), "%d", AxisPoints );
					UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_score_axis_points", buf );
				}
			}
			GetGlobalTeam(TEAM_AXIS)->AddScore( AxisPoints );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_tick_points" );

			if ( event )
			{
                event->SetInt( "team", TEAM_AXIS );
				event->SetInt( "score", AxisPoints );
				event->SetInt( "totalscore", GetGlobalTeam(TEAM_AXIS)->GetScore() );
				gameeventmanager->FireEvent( event );
			}
		}

		// the next time we'll give points
		m_fGivePointsTime = gpGlobals->curtime + mp_tickpointinterval.GetInt();
	}

	// If we call this from dod_control_point, this function should never 
	// trigger a win. but we'll leave it here just incase.
	CheckWinConditions();

	// the next time we 'think'
	SetContextThink( &CControlPointMaster::CPMThink, gpGlobals->curtime + 0.2, FLAGS_CONTEXT );
}

void CControlPointMaster::CheckWinConditions( void )
{
	// ============
	// Check that the points aren't all held by one team
	// if they are this will reset the round
	// and will reset all the points
	// ============
	switch( TeamOwnsAllPoints() )
	{
	case TEAM_ALLIES:	//allies own all		
		{
			TeamWins( TEAM_ALLIES );
		}			
		break;
	case TEAM_AXIS:	//axis owns all
		{
			TeamWins( TEAM_AXIS );
		}
		break;
	default:
		break;
	}
}

void CControlPointMaster::InputRoundInit( inputdata_t &input )
{
	//clear out old control points
	m_ControlPoints.RemoveAll();

	//find the control points

	//if successful, do CPMThink
	if( FindControlPoints() )
	{
		SetContextThink( &CControlPointMaster::CPMThink, gpGlobals->curtime + 0.1, FLAGS_CONTEXT );
	}

	//init the ClientAreas
	int index = 0;
	
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "dod_capture_area" );
	while( pEnt )
	{
		CAreaCapture *pArea = (CAreaCapture *)pEnt;
		Assert( pArea );
		pArea->area_SetIndex( index );
		index++;

		pEnt = gEntList.FindEntityByClassname( pEnt, "dod_capture_area" );
	}
	
	g_pObjectiveResource->ResetControlPoints();
}

void CControlPointMaster::FireTeamWinOutput( int iWinningTeam )
{
	switch( iWinningTeam )
	{
	case TEAM_ALLIES:
		m_AlliesWinOutput.FireOutput(this,this);
		break;
	case TEAM_AXIS:
		m_AxisWinOutput.FireOutput(this,this);
		break;
	default:
		Assert(0);
		break;
	}
}

void CControlPointMaster::TeamWins( int iWinningTeam )
{
	DODGameRules()->SetWinningTeam( iWinningTeam );

	FireTeamWinOutput( iWinningTeam );
}

void CControlPointMaster::BecomeActive( void )
{
	Assert( m_bDisabled );
	m_bDisabled = false;
}

void CControlPointMaster::BecomeInactive( void )
{
	Assert( !m_bDisabled );
	m_bDisabled = true;
}

int CControlPointMaster::GetPointOwner( int point )
{
	Assert( point >= 0 );
	Assert( point < MAX_CONTROL_POINTS );

	CControlPoint *pPoint = m_ControlPoints[point];

	if( pPoint )
	{
		return pPoint->GetOwner();
	}
	else
		return TEAM_UNASSIGNED;
}

// TeamOwnsAllPoints
// =================
// This function returns the team that owns all
// the cap points. if its not the case that one 
// team owns them all, it returns 0

// New - cps are now broken into groups. 
// A team can win by owning all flags within a single group.

// Can be passed an overriding team. If this is not null, the passed team
// number will be used for that cp. Used to predict if that CP changing would
// win the game.

int CControlPointMaster::TeamOwnsAllPoints( CControlPoint *pOverridePoint /* = NULL */, int iOverrideNewTeam /* = TEAM_UNASSIGNED */ )
{
	unsigned int i;

	int iWinningTeam[MAX_CONTROL_POINT_GROUPS];

	for( i=0;i<MAX_CONTROL_POINT_GROUPS;i++	)
	{
		iWinningTeam[i] = TEAM_INVALID;
	}

	// if TEAM_INVALID, haven't found a flag for this group yet
	// if TEAM_UNASSIGNED, the group is still being contested 

	// for each control point
	for( i=0;i<m_ControlPoints.Count();i++ )
	{
		int group = m_ControlPoints[i]->GetCPGroup();
		int owner = m_ControlPoints[i]->GetOwner();

		if ( pOverridePoint == m_ControlPoints[i] )
		{
			owner = iOverrideNewTeam;
		}

		// the first one we find in this group, set the win to true
		if ( iWinningTeam[group] == TEAM_INVALID )
		{
			iWinningTeam[group] = owner;
		}
		// unassigned means this group is already contested, move on
		else if ( iWinningTeam[group] == TEAM_UNASSIGNED )
		{
			continue;
		}
		// if we find another one in the group that isn't the same owner, set the win to false
		else if ( owner != iWinningTeam[group] )
		{
			iWinningTeam[group] = TEAM_UNASSIGNED;
		}		
	}

	// report the first win we find as the winner
	for ( i=0;i<MAX_CONTROL_POINT_GROUPS;i++ )
	{
		if ( iWinningTeam[i] == TEAM_ALLIES || iWinningTeam[i] == TEAM_AXIS )
			return iWinningTeam[i];
	}

	// no wins yet
	return TEAM_UNASSIGNED;
}

// an advantage flag is a flag that we've taken that didn't initially
// belong to our team
int CControlPointMaster::CountAdvantageFlags( int team )
{
	int numFlags = 0;

	unsigned int i;
	for( i = 0;i<m_ControlPoints.Count();i++ )
	{
		CControlPoint *pPoint = m_ControlPoints[i];

		if ( pPoint->GetOwner() != pPoint->GetDefaultOwner() )
		{
			if ( pPoint->GetOwner() == team )
			{
				// we own a flag we didn't initially
				numFlags++;
			}
			else // 
			{
				// the enemy owns one of our flags
				numFlags--;
			}
		}
	}

	return numFlags;
}

void CControlPointMaster::InputAddTimerSeconds( inputdata_t &inputdata )
{
	DODGameRules()->AddTimerSeconds( inputdata.value.Int() );
}

void CControlPointMaster::GetTimerData( int &iTimerSeconds, int &iTimerWinTeam )
{
	iTimerSeconds = m_iTimerLength;
	iTimerWinTeam = m_iTimerTeam;
}

bool CControlPointMaster::WouldNewCPOwnerWinGame( CControlPoint *pPoint, int iNewOwner )
{
	return ( TeamOwnsAllPoints( pPoint, iNewOwner ) == iNewOwner );
}

int CControlPointMaster::GetNumPoints( void )
{
	return m_ControlPoints.Count();
}

CControlPoint *CControlPointMaster::GetCPByIndex( int index )
{
	CControlPoint *pPoint = NULL;

	if ( index >= 0 && index < (int)m_ControlPoints.Count() )
	{
		pPoint = m_ControlPoints[index];
	}

	return pPoint;
}

// custom scoring entity
BEGIN_DATADESC( CDODCustomScoring )
	DEFINE_INPUTFUNC( FIELD_INTEGER, "GiveTickPoints", InputGivePoints ),

	DEFINE_KEYFIELD( m_iPointTeam, FIELD_INTEGER, "TeamNum" ),
	DEFINE_KEYFIELD( m_iTickLength, FIELD_INTEGER, "point_give_delay" ),
	DEFINE_KEYFIELD( m_iPointsToGive, FIELD_INTEGER, "point_give_amount" ),
	DEFINE_KEYFIELD( m_iNumPointGives, FIELD_INTEGER, "point_give_max_times" ),

	DEFINE_THINKFUNC( PointsThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( dod_scoring, CDODCustomScoring );