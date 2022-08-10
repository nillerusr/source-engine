//======= Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: CTF Flag Capture Zone.
//
//=============================================================================//
#include "cbase.h"
#include "func_capture_zone.h"
#include "tf_player.h"
#include "tf_item.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "entity_capture_flag.h"

//=============================================================================
//
// CTF Flag Capture Zone tables.
//

BEGIN_DATADESC( CCaptureZone )

// Keyfields.
DEFINE_KEYFIELD( m_nCapturePoint, FIELD_INTEGER, "CapturePoint" ),

// Functions.
DEFINE_FUNCTION( Touch ),

// Inputs.
DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

// Outputs.
DEFINE_OUTPUT( m_outputOnCapture, "OnCapture" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( func_capturezone, CCaptureZone );


IMPLEMENT_SERVERCLASS_ST( CCaptureZone, DT_CaptureZone )
END_SEND_TABLE()

//=============================================================================
//
// CTF Flag Capture Zone functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureZone::Spawn()
{
	InitTrigger();
	SetTouch( &CCaptureZone::Touch );

	if ( m_bDisabled )
	{
		SetDisabled( true );
	}

	m_flNextTouchingEnemyZoneWarning = -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureZone::Touch( CBaseEntity *pOther )
{
	// Is the zone enabled?
	if ( IsDisabled() )
		return;

	// Get the TF player.
	CTFPlayer *pPlayer = ToTFPlayer( pOther );
	if ( pPlayer )
	{
		// Check to see if the player has the capture flag.
		if ( pPlayer->HasItem() && ( pPlayer->GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG ) )
		{
			CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( pPlayer->GetItem() );
			if ( pFlag && !pFlag->IsCaptured() )
			{
				// does this capture point have a team number asssigned?
				if ( GetTeamNumber() != TEAM_UNASSIGNED )
				{
					// Check to see if the capture zone team matches the player's team.
					if ( pPlayer->GetTeamNumber() != TEAM_UNASSIGNED && pPlayer->GetTeamNumber() != GetTeamNumber() )
					{
						if ( pFlag->GetGameType() == TF_FLAGTYPE_CTF )
						{
							// Do this at most once every 5 seconds
							if ( m_flNextTouchingEnemyZoneWarning < gpGlobals->curtime )
							{
								CSingleUserRecipientFilter filter( pPlayer );
								TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_TOUCHING_ENEMY_CTF_CAP );
								m_flNextTouchingEnemyZoneWarning = gpGlobals->curtime + 5;
							}
						}
						else if ( pFlag->GetGameType() == TF_FLAGTYPE_INVADE )
						{
							TFTeamMgr()->PlayerCenterPrint( pPlayer, "#TF_Invade_Wrong_Goal" );
						}

						return;
					}
				}

				if ( TFGameRules()->FlagsMayBeCapped() )
				{
					pFlag->Capture( pPlayer, m_nCapturePoint );

					// Output.
					m_outputOnCapture.FireOutput( this, this );

					IGameEvent *event = gameeventmanager->CreateEvent( "ctf_flag_captured" );
					if ( event )
					{
						int iCappingTeam = pPlayer->GetTeamNumber();
						int	iCappingTeamScore = 0;
						CTFTeam* pCappingTeam = pPlayer->GetTFTeam();
						if ( pCappingTeam )
						{
							iCappingTeamScore = pCappingTeam->GetFlagCaptures();
						}

						event->SetInt( "capping_team", iCappingTeam );
						event->SetInt( "capping_team_score", iCappingTeamScore );
						event->SetInt( "capper", pPlayer->GetUserID() );
						event->SetInt( "priority", 9 ); // HLTV priority

						gameeventmanager->FireEvent( event );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The timer is always transmitted to clients
//-----------------------------------------------------------------------------
int CCaptureZone::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCaptureZone::IsDisabled( void )
{
	return m_bDisabled;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureZone::InputEnable( inputdata_t &inputdata )
{
	SetDisabled( false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureZone::InputDisable( inputdata_t &inputdata )
{
	SetDisabled( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureZone::SetDisabled( bool bDisabled )
{
	m_bDisabled = bDisabled;

	if ( bDisabled )
	{
		BaseClass::Disable();
		SetTouch( NULL );
	}
	else
	{
		BaseClass::Enable();
		SetTouch( &CCaptureZone::Touch );
	}
}