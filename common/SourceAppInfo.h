//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef SOURCE_APP_INFO_H
#define SOURCE_APP_INFO_H
#ifdef _WIN32
#pragma once
#endif

enum ESourceApp
{
	k_App_SDK_BASE = 0,
	k_App_HL2,
	k_App_CSS,
	k_App_DODS,
	k_App_HL2MP,
	k_App_LOST_COAST,
	k_App_HL1DM,
	k_App_HL2_EP1,
	k_App_PORTAL,
	k_App_HL2_EP2,
	k_App_TF2,
	k_App_MAX
};

const int GetAppSteamAppId( ESourceApp eSourceApp );
const char *GetAppModName( ESourceApp eSourceApp );
const char *GetAppFullName( ESourceApp eSourceApp );

#endif