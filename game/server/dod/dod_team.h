//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_TEAM_H
#define DOD_TEAM_H

#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "team.h"
#include "playerclass_info_parse.h"
#include "dod_playerclass_info_parse.h"
#include "dod_shareddefs.h"
#include "dod_player.h"

typedef CUtlLinkedList< PLAYERCLASS_FILE_INFO_HANDLE, int > PlayerClassInfoList;

//-----------------------------------------------------------------------------
// Purpose: Team Manager
//-----------------------------------------------------------------------------
class CDODTeam : public CTeam
{
	DECLARE_CLASS( CDODTeam, CTeam );
	DECLARE_SERVERCLASS();

public:

	// Initialization
	virtual void Init( const char *pName, int iNumber );

	CDODPlayerClassInfo const &GetPlayerClassInfo( int iPlayerClass ) const;
	const unsigned char *GetEncryptionKey( void ) { return g_pGameRules->GetEncryptionKey(); }

	virtual void AddPlayerClass( const char *pszClassName );

	bool IsClassOnTeam( const char *pszClassName, int &iClassNum ) const;
	int GetNumPlayerClasses( void ) { return m_hPlayerClassInfoHandles.Count(); }

	void ResetScores( void );

	virtual const char *GetTeamName( void ) { return "#Teamname_Spectators"; }

	virtual CDODPlayer *GetDODPlayer( int iIndex ) { return ToDODPlayer(GetPlayer(iIndex)); }

private:
	CUtlVector < PLAYERCLASS_FILE_INFO_HANDLE >		m_hPlayerClassInfoHandles;
};


extern CDODTeam *GetGlobalDODTeam( int iIndex );


#endif // TF_TEAM_H
