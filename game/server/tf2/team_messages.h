//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEAM_MESSAGES_H
#define TEAM_MESSAGES_H
#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"

// Message IDs
enum
{
	// Reinforcements
	TEAMMSG_REINFORCEMENTS_ARRIVED,

	// Carriers / Harvesters
	TEAMMSG_CARRIER_UNDER_ATTACK,
	TEAMMSG_CARRIER_DESTROYED,
	TEAMMSG_HARVESTER_UNDER_ATTACK,
	TEAMMSG_HARVESTER_DESTROYED,

	// Resources
	TEAMMSG_RESOURCE_ZONE_EMPTIED,

	// Techtree
	TEAMMSG_NEW_TECH_LEVEL_OPEN,

	// Custom sounds
	TEAMMSG_CUSTOM_SOUND,
};

//-----------------------------------------------------------------------------
// Purpose: Message sent to a team for the purpose of updating its members about some event
//-----------------------------------------------------------------------------
abstract_class CTeamMessage
{
public:
	CTeamMessage( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity, float flTTL );

	static CTeamMessage *Create( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity );

	// Called when the team manager wants me to fire myself
	virtual void	FireMessage( void ) = 0;

	// Accessors
	virtual int			GetID( void ) { return m_iMessageID; };
	virtual float		GetTTL( void ) { return m_flTTL; };
	virtual CBaseEntity *GetEntity( void ) { return m_hEntity; };
	virtual CTFTeam		*GetTeam( void ) { return m_pTeam; };

	virtual void		SetData( char *pszData ) { return; }

protected:
	int			m_iMessageID;
	float		m_flTTL;
	EHANDLE		m_hEntity;
	CTFTeam		*m_pTeam;
	CUtlSymbol	m_SoundName;
};

//-----------------------------------------------------------------------------
// Purpose: Team message that plays a sound to the members of the team
//-----------------------------------------------------------------------------
class CTeamMessage_Sound : public CTeamMessage
{
public:
	CTeamMessage_Sound( CTFTeam *pTeam, int iMessageID, CBaseEntity *pEntity, float flTTL );

	// Set my sound
	virtual void	SetSound( char *sSound );
	// Called when the team manager wants me to fire myself
	virtual void	FireMessage( void );

	virtual void	SetData( char *pszData ) { SetSound( pszData ); }
};

#endif // TEAM_MESSAGES_H
