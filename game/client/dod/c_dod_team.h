//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_DOD_TEAM_H
#define C_DOD_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"
#include "dod_playerclass_info_parse.h"
#include "dod_shareddefs.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_DODTeam : public C_Team
{
	DECLARE_CLASS( C_DODTeam, C_Team );
public:
	DECLARE_CLIENTCLASS();

					C_DODTeam();
	virtual			~C_DODTeam();

	CDODPlayerClassInfo const &GetPlayerClassInfo( int iPlayerClass ) const;
	const unsigned char *GetEncryptionKey( void ) { return g_pGameRules->GetEncryptionKey(); }

	virtual void AddPlayerClass( const char *pszClassName );

	bool IsClassOnTeam( const char *pszClassName, int &iClassNum ) const;
	bool IsClassOnTeam( int iClassNum ) const;
	int GetNumPlayerClasses( void ) { return m_hPlayerClassInfoHandles.Count(); }

	int CountPlayersOfThisClass( int iPlayerClass );

private:
	CUtlVector < PLAYERCLASS_FILE_INFO_HANDLE >		m_hPlayerClassInfoHandles;
};

class C_DODTeam_Allies : public C_DODTeam
{
	DECLARE_CLASS( C_DODTeam_Allies, C_DODTeam );
public:
	DECLARE_CLIENTCLASS();

				     C_DODTeam_Allies();
	 virtual		~C_DODTeam_Allies() {}
};

class C_DODTeam_Axis : public C_DODTeam
{
	DECLARE_CLASS( C_DODTeam_Axis, C_DODTeam );
public:
	DECLARE_CLIENTCLASS();

					 C_DODTeam_Axis();
	virtual			~C_DODTeam_Axis() {}
};

#endif // C_DOD_TEAM_H
