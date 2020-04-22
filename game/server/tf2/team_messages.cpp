//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "player.h"
#include "tf_team.h"
#include "team_messages.h"
#include "engine/IEngineSound.h"

//-----------------------------------------------------------------------------
// Purpose: Create the right class of message based upon the type
//-----------------------------------------------------------------------------
CTeamMessage *CTeamMessage::Create( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity )
{ 
	CTeamMessage *pMessage = NULL;

	// Create the right type
	switch ( iMessageID )
	{
		// Sound orders
		case TEAMMSG_REINFORCEMENTS_ARRIVED:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 5.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.Reinforcements" );
			break;
		case TEAMMSG_CARRIER_UNDER_ATTACK:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.CarrierAttacked" );
			break;
		case TEAMMSG_CARRIER_DESTROYED:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.CarrierDestroyed" );
			break;
		case TEAMMSG_HARVESTER_UNDER_ATTACK:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.HarvesterAttacked" );
			break;
		case TEAMMSG_HARVESTER_DESTROYED:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.HarvesterDestroyed" );
			break;
		case TEAMMSG_NEW_TECH_LEVEL_OPEN:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.NewTechLevel" );
			break;
		case TEAMMSG_RESOURCE_ZONE_EMPTIED:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			((CTeamMessage_Sound*)pMessage)->SetSound( "TeamMessage.ResourceZoneEmpty" );
			break;
		case TEAMMSG_CUSTOM_SOUND:
			pMessage = new CTeamMessage_Sound( pTeam, iMessageID, pEntity, 10.0 );
			break;

		default:
			break;
	};

	return pMessage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeamMessage::CTeamMessage( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity, float flTTL )
{
	m_pTeam = pTeam;
	m_iMessageID = iMessageID;
	m_hEntity = pEntity;
	m_flTTL = gpGlobals->curtime + flTTL;
}


//===============================================================================================================
// TEAM MESSAGE SOUND
//===============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeamMessage_Sound::CTeamMessage_Sound( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity, float flTTL ) :
	CTeamMessage( pTeam, iMessageID, pEntity, flTTL )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamMessage_Sound::SetSound( char *sSound )
{
	CBaseEntity::PrecacheScriptSound( sSound );
	m_SoundName = sSound;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the team manager wants me to fire myself
//-----------------------------------------------------------------------------
void CTeamMessage_Sound::FireMessage( void )
{
	Assert( m_SoundName.String() );
	
	// Play my sound to all the team's members
	for ( int i = 0; i < m_pTeam->GetNumPlayers(); i++ )
	{
		CBasePlayer *pPlayer = m_pTeam->GetPlayer(i);

		CSingleUserRecipientFilter filter( pPlayer );
		CBaseEntity::EmitSound( filter, pPlayer->entindex(), m_SoundName.String() );
	}
}