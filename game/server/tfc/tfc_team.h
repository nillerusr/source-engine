//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFC_TEAM_H
#define TFC_TEAM_H

#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "team.h"
#include "tfc_shareddefs.h"


//-----------------------------------------------------------------------------
// Purpose: Team Manager
//-----------------------------------------------------------------------------
class CTFCTeam : public CTeam
{
	DECLARE_CLASS( CTFCTeam, CTeam );
	DECLARE_SERVERCLASS();

public:

	// Initialization
	virtual void Init( const char *pName, int iNumber );
	color32 GetTeamColor();
};


extern CTFCTeam *GetGlobalTFCTeam( int iIndex );

void TeamFortress_TeamShowScores(BOOL bLong, CBasePlayer *pPlayer);
int TeamFortress_TeamGetScoreFrags(int tno);

// Colors for each team.
typedef struct
{
	int topColor;
	int bottomColor;
} team_color_t;

extern Vector rgbcolors[5];
extern team_color_t teamcolors[5][PC_LASTCLASS]; // Colors for each of the 4 teams
extern int number_of_teams;	// This is incremented for each map as info_player_teamspawn are created.
extern const char *teamnames[5];
#define g_szTeamColors teamnames

color32 Vector255ToRGBColor( const Vector &vColor );


#endif // TF_TEAM_H
