//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cs_shareddefs.h"

const float CS_PLAYER_SPEED_RUN		 = 260.0f;
const float CS_PLAYER_SPEED_VIP		 = 227.0f;
const float CS_PLAYER_SPEED_WALK	 = 100.0f;
const float CS_PLAYER_SPEED_SHIELD	 = 160.0f;
const float CS_PLAYER_SPEED_STOPPED	 =   1.0f;
const float CS_PLAYER_SPEED_OBSERVER = 900.0f;

const float CS_PLAYER_SPEED_DUCK_MODIFIER	= 0.34f;
const float CS_PLAYER_SPEED_WALK_MODIFIER	= 0.52f;
const float CS_PLAYER_SPEED_CLIMB_MODIFIER	= 0.34f;


CCSClassInfo g_ClassInfos[] =
{
	{ "None" },

	{ "Phoenix Connection" },
	{ "L337 KREW" },
	{ "Arctic Avengers" },
	{ "Guerilla Warfare" },

	{ "Seal Team 6" },
	{ "GSG-9" },
	{ "SAS" },
	{ "GIGN" }
};

const CCSClassInfo* GetCSClassInfo( int i )
{
	Assert( i >= 0 && i < ARRAYSIZE( g_ClassInfos ) );
	return &g_ClassInfos[i];
}

const char *pszWinPanelCategoryHeaders[] =
{
	"",
	"#winpanel_topdamage",
	"#winpanel_topheadshots",
	"#winpanel_kills"
};

// Construct some arrays of player model strings, so we can statically initialize CUtlVectors for general usage
const char *CTPlayerModelStrings[] =
{
	"models/player/ct_urban.mdl",
	"models/player/ct_gsg9.mdl",
	"models/player/ct_sas.mdl",
	"models/player/ct_gign.mdl",
};
const char *TerroristPlayerModelStrings[] =
{
	"models/player/t_phoenix.mdl",
	"models/player/t_leet.mdl",
	"models/player/t_arctic.mdl",
	"models/player/t_guerilla.mdl",
};
CUtlVectorInitialized< const char * > CTPlayerModels( CTPlayerModelStrings, ARRAYSIZE( CTPlayerModelStrings ) );
CUtlVectorInitialized< const char * > TerroristPlayerModels( TerroristPlayerModelStrings, ARRAYSIZE( TerroristPlayerModelStrings ) );
