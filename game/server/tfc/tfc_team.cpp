//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tfc_team.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Datatable
IMPLEMENT_SERVERCLASS_ST(CTFCTeam, DT_TFCTeam)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( tfc_team_manager, CTFCTeam );


Vector rgbcolors[5];
team_color_t teamcolors[5][PC_LASTCLASS]; // Colors for each of the 4 teams
int number_of_teams = 0;	// This is incremented for each map as info_player_teamspawn are created.
const char *teamnames[5] =
{
	"spectator",
	"blue",
	"red",
	"yellow",
	"green"
};

//========================================================================
// Set the color for the team corresponding to the no passed in, to team_no
void TeamFortress_TeamSetColor()
{
	// Blue Team
	teamcolors[1][PC_SCOUT].topColor		= 153;
	teamcolors[1][PC_SCOUT].bottomColor		= 139;

	teamcolors[1][PC_SNIPER].topColor		= 153;
	teamcolors[1][PC_SNIPER].bottomColor	= 145;

	teamcolors[1][PC_SOLDIER].topColor		= 153;
	teamcolors[1][PC_SOLDIER].bottomColor	= 130;

	teamcolors[1][PC_DEMOMAN].topColor		= 153;
	teamcolors[1][PC_DEMOMAN].bottomColor	= 145;

	teamcolors[1][PC_MEDIC].topColor		= 153;
	teamcolors[1][PC_MEDIC].bottomColor		= 140;

	teamcolors[1][PC_HWGUY].topColor		= 148;
	teamcolors[1][PC_HWGUY].bottomColor	= 138;

	teamcolors[1][PC_PYRO].topColor			= 140;
	teamcolors[1][PC_PYRO].bottomColor		= 145;

	teamcolors[1][PC_SPY].topColor			= 150;
	teamcolors[1][PC_SPY].bottomColor		= 145;

	teamcolors[1][PC_ENGINEER].topColor		= 140;
	teamcolors[1][PC_ENGINEER].bottomColor	= 148;

	teamcolors[1][PC_CIVILIAN].topColor		= 150;
	teamcolors[1][PC_CIVILIAN].bottomColor	= 140;

#ifdef TFCTODO // sentry colors
	teamcolors[1][SENTRY_COLOR].topColor	= 150;
	teamcolors[1][SENTRY_COLOR].bottomColor	= 0;

	teamcolors[2][SENTRY_COLOR].topColor	= 250;
	teamcolors[2][SENTRY_COLOR].bottomColor	= 0;

	teamcolors[3][SENTRY_COLOR].topColor	= 45;
	teamcolors[3][SENTRY_COLOR].bottomColor	= 0;

	teamcolors[4][SENTRY_COLOR].topColor	= 100;
	teamcolors[4][SENTRY_COLOR].bottomColor	= 0;
#endif

	// Red Team
	teamcolors[2][PC_SCOUT].topColor		= 255;
	teamcolors[2][PC_SCOUT].bottomColor		= 10;

	teamcolors[2][PC_SNIPER].topColor		= 255;
	teamcolors[2][PC_SNIPER].bottomColor	= 10;

	teamcolors[2][PC_SOLDIER].topColor		= 250;
	teamcolors[2][PC_SOLDIER].bottomColor	= 28;

	teamcolors[2][PC_DEMOMAN].topColor		= 255;
	teamcolors[2][PC_DEMOMAN].bottomColor	= 20;

	teamcolors[2][PC_MEDIC].topColor		= 255;
	teamcolors[2][PC_MEDIC].bottomColor		= 250;

	teamcolors[2][PC_HWGUY].topColor		= 255;
	teamcolors[2][PC_HWGUY].bottomColor	= 25;

	teamcolors[2][PC_PYRO].topColor			= 250;
	teamcolors[2][PC_PYRO].bottomColor		= 25;

	teamcolors[2][PC_SPY].topColor			= 250;
	teamcolors[2][PC_SPY].bottomColor		= 240;

	teamcolors[2][PC_ENGINEER].topColor		= 5;
	teamcolors[2][PC_ENGINEER].bottomColor	= 250;

	teamcolors[2][PC_CIVILIAN].topColor		= 250;
	teamcolors[2][PC_CIVILIAN].bottomColor	= 240;


	// Yellow Team
	teamcolors[3][PC_SCOUT].topColor		= 45;
	teamcolors[3][PC_SCOUT].bottomColor		= 35;

	teamcolors[3][PC_SNIPER].topColor		= 45;
	teamcolors[3][PC_SNIPER].bottomColor	= 35;

	teamcolors[3][PC_SOLDIER].topColor		= 45;
	teamcolors[3][PC_SOLDIER].bottomColor	= 35;

	teamcolors[3][PC_DEMOMAN].topColor		= 45;
	teamcolors[3][PC_DEMOMAN].bottomColor	= 35;

	teamcolors[3][PC_MEDIC].topColor		= 45;
	teamcolors[3][PC_MEDIC].bottomColor		= 35;

	teamcolors[3][PC_HWGUY].topColor		= 45;
	teamcolors[3][PC_HWGUY].bottomColor	= 40;

	teamcolors[3][PC_PYRO].topColor			= 45;
	teamcolors[3][PC_PYRO].bottomColor		= 35;

	teamcolors[3][PC_SPY].topColor			= 45;
	teamcolors[3][PC_SPY].bottomColor		= 35;

	teamcolors[3][PC_ENGINEER].topColor		= 45;
	teamcolors[3][PC_ENGINEER].bottomColor	= 45;

	teamcolors[3][PC_CIVILIAN].topColor		= 45;
	teamcolors[3][PC_CIVILIAN].bottomColor	= 35;

	// Green Team
	teamcolors[4][PC_SCOUT].topColor		= 100;
	teamcolors[4][PC_SCOUT].bottomColor		= 90;

	teamcolors[4][PC_SNIPER].topColor		= 80;
	teamcolors[4][PC_SNIPER].bottomColor	= 90;

	teamcolors[4][PC_SOLDIER].topColor		= 100;
	teamcolors[4][PC_SOLDIER].bottomColor	= 40;

	teamcolors[4][PC_DEMOMAN].topColor		= 100;
	teamcolors[4][PC_DEMOMAN].bottomColor	= 90;

	teamcolors[4][PC_MEDIC].topColor		= 100;
	teamcolors[4][PC_MEDIC].bottomColor		= 90;

	teamcolors[4][PC_HWGUY].topColor		= 100;
	teamcolors[4][PC_HWGUY].bottomColor	= 90;

	teamcolors[4][PC_PYRO].topColor			= 100;
	teamcolors[4][PC_PYRO].bottomColor		= 50;

	teamcolors[4][PC_SPY].topColor			= 100;
	teamcolors[4][PC_SPY].bottomColor		= 90;

	teamcolors[4][PC_ENGINEER].topColor		= 100;
	teamcolors[4][PC_ENGINEER].bottomColor	= 90;

	teamcolors[4][PC_CIVILIAN].topColor		= 100;
	teamcolors[4][PC_CIVILIAN].bottomColor	= 90;

	rgbcolors[0] = Vector( 255, 255, 255 );	// White for non-owned
	rgbcolors[1] = Vector( 0, 0, 255 );
	rgbcolors[2] = Vector( 255, 0, 0 );
	rgbcolors[3] = Vector( 255, 255, 30 );
	rgbcolors[4] = Vector( 0, 255, 0 );
}
class CColorInitializer
{
public:
	CColorInitializer() 
	{
		TeamFortress_TeamSetColor(); 
	}
} g_ColorInitializer;



//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified TF team manager
//-----------------------------------------------------------------------------
CTFCTeam *GetGlobalTFCTeam( int iIndex )
{
	return (CTFCTeam*)GetGlobalTeam( iIndex );
}


// Display all the Team Scores
void TeamFortress_TeamShowScores(BOOL bLong, CBasePlayer *pPlayer)
{
	for (int i = 1; i < g_Teams.Count(); i++)
	{
		if (!bLong)
		{
			// Dump short scores
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs("%s: %d\n", g_szTeamColors[i], GetGlobalTeam(i)->GetScore()) );
		}
		else
		{
			// Dump long scores
			if (pPlayer == NULL)
				UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs("Team %d (%s): %d\n", i, g_szTeamColors[i], GetGlobalTeam(i)->GetScore()) );
			else // Print to just one client
				ClientPrint( pPlayer, HUD_PRINTNOTIFY,  UTIL_VarArgs("Team %d (%s): %d\n", i, g_szTeamColors[i], GetGlobalTeam(i)->GetScore()) );
		}
	}
}


//=========================================================================
// Return the score/frags of a team, depending on whether TeamFrags is on
int TeamFortress_TeamGetScoreFrags(int tno)
{
	CTeam *pTeam = GetGlobalTeam( tno );
	if ( pTeam )
	{
		return pTeam->GetScore();
	}
	else
	{
		Assert( false );
		return -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
void CTFCTeam::Init( const char *pName, int iNumber )
{
	BaseClass::Init( pName, iNumber );

	// Only detect changes every half-second.
	NetworkProp()->SetUpdateInterval( 0.75f );
}


color32 CTFCTeam::GetTeamColor()
{
	int i = GetTeamNumber();
	if ( i >= 0 && i < ARRAYSIZE( rgbcolors ) )
	{
		return Vector255ToRGBColor( rgbcolors[i] );
	}
	else
	{
		Assert( false );
		color32 x;
		memset( &x, 0, sizeof( x ) );
		return x;
	}
}


color32 Vector255ToRGBColor( const Vector &vColor )
{
	color32 ret;
	ret.a = 0;
	ret.r = (byte)vColor.x;
	ret.g = (byte)vColor.y;
	ret.b = (byte)vColor.z;
	return ret;
}
