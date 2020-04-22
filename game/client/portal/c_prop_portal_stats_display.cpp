//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_prop_portal_stats_display.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "engine/IEngineSound.h"
#include "dlight.h"
#include "iefx.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "point_bonusmaps_accessor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern const ConVar *sv_cheats;


CUtlVector< C_PropPortalStatsDisplay* > g_PropPortalStatsDisplays;


IMPLEMENT_CLIENTCLASS_DT(C_PropPortalStatsDisplay, DT_PropPortalStatsDisplay, CPropPortalStatsDisplay)
	RecvPropBool( RECVINFO(m_bEnabled) ),

	RecvPropInt( RECVINFO(m_iNumPortalsPlaced) ),
	RecvPropInt( RECVINFO(m_iNumStepsTaken) ),
	RecvPropFloat( RECVINFO(m_fNumSecondsTaken) ),

	RecvPropInt( RECVINFO(m_iBronzeObjective) ),
	RecvPropInt( RECVINFO(m_iSilverObjective) ),
	RecvPropInt( RECVINFO(m_iGoldObjective) ),

	RecvPropString( RECVINFO( szChallengeFileName ) ),
	RecvPropString( RECVINFO( szChallengeMapName ) ),
	RecvPropString( RECVINFO( szChallengeName ) ),

	RecvPropInt( RECVINFO(m_iDisplayObjective) ),
END_RECV_TABLE()


C_PropPortalStatsDisplay::C_PropPortalStatsDisplay()
{
	g_PropPortalStatsDisplays.AddToTail( this );

	m_fEnabledCounter = -2.0f;
}

C_PropPortalStatsDisplay::~C_PropPortalStatsDisplay()
{
	g_PropPortalStatsDisplays.FindAndRemove( this );
}

void C_PropPortalStatsDisplay::Spawn( void )
{
	BaseClass::Spawn();

	m_bHasCheated = false;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_PropPortalStatsDisplay::OnPreDataChanged( DataUpdateType_t updateType )
{
	// Remember the enable status from before the update
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
		m_bPrevEnabled = m_bEnabled;

	BaseClass::OnPreDataChanged( updateType );
}

void C_PropPortalStatsDisplay::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		// Check if it was just enabled
		if ( m_bEnabled && !m_bPrevEnabled )
		{
			ResetDisplayAnimation();

			// Don't update if the player has cheated (and isn't at least developer level 2)
			if ( !m_bHasCheated || developer.GetInt() >= 2 )
			{
				int iStat = 0;
				if ( m_iDisplayObjective == 0 )
					iStat = m_iNumPortalsPlaced;
				else if ( m_iDisplayObjective == 1 )
					iStat = m_iNumStepsTaken;
				else if ( m_iDisplayObjective == 2 )
					iStat = m_fNumSecondsTaken;

				BonusMapChallengeUpdate( szChallengeFileName, szChallengeMapName, szChallengeName, iStat );
			}
		}
	}
}

void C_PropPortalStatsDisplay::ClientThink( void )
{
	BaseClass::ClientThink();

	// Remembers if the player has cheated
	// This doesn't persist across save/loads, 
	// this is only to prevent players from accidentally wiping their stats while no clipping through a map
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}
	if ( sv_cheats && sv_cheats->GetInt() != 0 )
		m_bHasCheated = true;

	if ( !m_bEnabled )
		return;

	// Check if the display needs to be initialized
	if ( m_fEnabledCounter < -1.0f )
	{
		m_fEnabledCounter = -1.0f;

		if ( m_iDisplayObjective == 0 )
			m_fNumPlayerDisplay = m_iNumPortalsPlaced;
		else if ( m_iDisplayObjective == 1 )
			m_fNumPlayerDisplay = m_iNumStepsTaken;
		else
			m_fNumPlayerDisplay = m_fNumSecondsTaken;

		SetDisplayMedals();

		if ( m_iGoalLevelDisplay == 0 )
			m_iGoalDisplay = m_iBronzeObjective;
		else if ( m_iGoalLevelDisplay == 1 )
			m_iGoalDisplay = m_iSilverObjective;
		else
			m_iGoalDisplay = m_iGoldObjective;

		m_bGoalVisible = true;
		m_iGoalSuccess = -1;

		return;
	}

	//Check if the display is done animating
	if ( m_fEnabledCounter < 0.0f )
		return;

	// Add up how much time has passed
	m_fEnabledCounter += gpGlobals->frametime;

	switch ( m_iCurrentDisplayStep )
	{
	// Count down to making objective visible
	case 0:
		if ( m_fEnabledCounter > 1.0f )
		{
			m_fEnabledCounter = 0.0f;

			EmitSound( "Portal.stair_clack" );
			m_bGoalVisible = true;
			++m_iCurrentDisplayStep;
		}
		break;

	// Count up how the player actually did
	case 1:
		switch ( m_iDisplayObjective )
		{
		case PORTAL_LEVEL_STAT_NUM_PORTALS:
			if ( m_fEnabledCounter > 1.5f / static_cast<float>( m_iNumPortalsPlaced + 1 ) )
			{
				m_fEnabledCounter = 0.0f;

				if ( m_fNumPlayerDisplay < m_iNumPortalsPlaced )
				{
					m_fNumPlayerDisplay += 1.0f;

					if ( static_cast<int>( m_fNumPlayerDisplay ) % 2 == 0 )
						EmitSound( "Weapon_Portalgun.fire_blue" );
					else
						EmitSound( "Weapon_Portalgun.fire_red" );
				}
				else
					++m_iCurrentDisplayStep;
			}
			break;

		case PORTAL_LEVEL_STAT_NUM_STEPS:
			if ( m_fEnabledCounter > 1.5f / static_cast<float>( m_iNumStepsTaken + 1 ) )
			{
				int iNumStepsThisFrame = m_fEnabledCounter * static_cast<float>( m_iNumStepsTaken + 1 );
				m_fEnabledCounter = 0.0f;

				if ( m_fNumPlayerDisplay < m_iNumStepsTaken )
				{
					m_fNumPlayerDisplay += MIN( iNumStepsThisFrame, m_iNumStepsTaken - m_fNumPlayerDisplay );

					int iStepsMod10 = static_cast<int>( m_fNumPlayerDisplay ) % 10;
					if ( iStepsMod10 == 0 )
						EmitSound( "NPC_Citizen.RunFootstepLeft" );
					else if ( iStepsMod10 == 5 )
						EmitSound( "NPC_Citizen.RunFootstepRight" );
				}
				else
					++m_iCurrentDisplayStep;
			}
			break;

		case PORTAL_LEVEL_STAT_NUM_SECONDS:
			if ( m_fNumPlayerDisplay < m_fNumSecondsTaken )
			{
				if ( m_fEnabledCounter > 1.5f / ( m_fNumSecondsTaken + 1.0f ) )
				{
					m_fEnabledCounter = 0.0f;

					EmitSound( "Portal.room1_Clock" );
				}

				m_fNumPlayerDisplay += gpGlobals->frametime * m_fNumSecondsTaken;

				if ( m_fNumPlayerDisplay >= m_fNumSecondsTaken )
				{
					m_fNumPlayerDisplay = m_fNumSecondsTaken;

					m_fEnabledCounter = 0.0f;
					++m_iCurrentDisplayStep;
				}
			}
			break;
		}

		break;

	// Count down to reveal success or failure
	case 2:
		if ( m_fEnabledCounter > 1.5f )
		{
			m_fEnabledCounter = 0.0f;

			int iCurretStat = -1;

			switch ( m_iDisplayObjective )
			{
			case 0:
				iCurretStat = m_iNumPortalsPlaced;
				break;

			case 1:
				iCurretStat = m_iNumStepsTaken;
				break;

			case 2:
				iCurretStat = static_cast<int>( m_fNumSecondsTaken );
				break;
			}

			int (*(pObjectives[ 3 ])) = { &m_iBronzeObjective, &m_iSilverObjective, &m_iGoldObjective };

			if ( iCurretStat <= (*(pObjectives[ m_iGoalLevelDisplay ])) )
			{
				m_bMedalCompleted[ m_iGoalLevelDisplay ] = true;
				m_iGoalSuccess = 1;

				EmitSound( "Portal.button_down" );

				if ( m_iGoalLevelDisplay == 2 )
					++m_iCurrentDisplayStep;	// skip the next step
			}
			else
			{
				m_iGoalSuccess = 0;

				EmitSound( "Portal.button_up" );
				++m_iCurrentDisplayStep;	// skip the next step
			}
			
			++m_iCurrentDisplayStep;
		}
		break;

	// Count down to reveal new objective
	case 3:
		if ( m_fEnabledCounter > 1.5f )
		{
			m_fEnabledCounter = 0.0f;

			++m_iGoalLevelDisplay;

			int (*(pObjectives[ 3 ])) = { &m_iBronzeObjective, &m_iSilverObjective, &m_iGoldObjective };
			m_iGoalDisplay = (*(pObjectives[ m_iGoalLevelDisplay ]));

			m_iGoalSuccess = -1;

			EmitSound( "Portal.stair_clack" );
			--m_iCurrentDisplayStep;	// go back to success or failure step
		}
		break;

	// Short pause before going to next objective
	case 4:
		if ( m_fEnabledCounter > 1.5f )
		{
			m_fEnabledCounter = 0.0f;

			m_iGoalSuccess = -1;

			m_fEnabledCounter = -1.0f;
			m_iCurrentDisplayStep = 0;
		}
		break;
	}
}

void C_PropPortalStatsDisplay::ResetDisplayAnimation( void )
{
	EmitSound( "Portal.elevator_ding" );

	m_fEnabledCounter = 0.0f;
	m_iCurrentDisplayStep = 0;

	m_fNumPlayerDisplay = 0.0f;

	SetDisplayMedals();

	m_bGoalVisible = false;
}

void C_PropPortalStatsDisplay::SetDisplayMedals( void )
{
	m_bMedalCompleted[ 0 ] = false;
	m_bMedalCompleted[ 1 ] = false;
	m_bMedalCompleted[ 2 ] = false;

	m_iGoalDisplay = m_iBronzeObjective;
	m_iGoalLevelDisplay = 0;
	m_iGoalSuccess = -1;
}