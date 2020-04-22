//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: languages definition
//
//=============================================================================

#include "SourceAppInfo.h"
#include "tier0/dbg.h"

struct SourceAppInfo_t
{	
	const char *m_pchFullName;
	const char *m_pchModName;
	int m_nSteamAppId;
	ESourceApp m_ESourceApp;
};


static const SourceAppInfo_t s_SteamAppInfo[] = 
{
	{	"Source SDK Base",				"sourcetest",	215,	k_App_SDK_BASE },
	{	"Half-Life 2",					"hl2",			220,	k_App_HL2 } ,
	{	"Counter-Strike: Source",		"cstrike",		240,	k_App_CSS } ,
	{	"Day of Defeat: Source",		"dod",			300,	k_App_DODS } ,
	{	"Half-Life 2: Deathmatch",		"hl2mp",		320,	k_App_HL2MP } ,
	{	"Half-Life 2: Lost Coast",		"lostcoast",	340,	k_App_LOST_COAST } ,
	{	"Half-Life Deathmatch: Source",	"hl1mp",		360,	k_App_HL1DM } ,
	{	"Half-Life 2: Episode One",		"episodic",		380,	k_App_HL2_EP1 },
	{	"Portal",						"portal",		400,	k_App_PORTAL } ,
	{	"Half-Life 2: Episode Two",		"ep2",			420,	k_App_HL2_EP2 } ,
	{	"Team Fortress 2",				"tf",			440,	k_App_TF2 } ,
};


//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetAppFullName( ESourceApp eSourceApp )
{
	Assert( Q_ARRAYSIZE(s_SteamAppInfo) == k_App_MAX );
	if ( s_SteamAppInfo[ eSourceApp ].m_ESourceApp == eSourceApp )
	{
		return s_SteamAppInfo[ eSourceApp ].m_pchFullName;
	}

	Assert( !"enum ESourceApp order mismatched from AppInfo_t s_SteamAppInfo, fix it!" );
	return s_SteamAppInfo[0].m_pchFullName;
}

//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetAppModName( ESourceApp eSourceApp )
{
	Assert( Q_ARRAYSIZE(s_SteamAppInfo) == k_App_MAX );
	if ( s_SteamAppInfo[ eSourceApp ].m_ESourceApp == eSourceApp )
	{
		return s_SteamAppInfo[ eSourceApp ].m_pchModName;
	}

	Assert( !"enum ESourceApp order mismatched from AppInfo_t s_SteamAppInfo, fix it!" );
	return s_SteamAppInfo[0].m_pchModName;
}


//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const int GetAppSteamAppId( ESourceApp eSourceApp )
{
	Assert( Q_ARRAYSIZE(s_SteamAppInfo) == k_App_MAX );
	if ( s_SteamAppInfo[ eSourceApp ].m_ESourceApp == eSourceApp )
	{
		return s_SteamAppInfo[ eSourceApp ].m_nSteamAppId;
	}

	Assert( !"enum ESourceApp order mismatched from AppInfo_t s_SteamAppInfo, fix it!" );
	return s_SteamAppInfo[0].m_nSteamAppId;
}

