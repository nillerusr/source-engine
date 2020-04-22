//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CEconGameServerAccount object
//
// $NoKeywords: $
//=============================================================================//

#ifndef ECON_GAME_SERVER_ACCOUNT_H
#define ECON_GAME_SERVER_ACCOUNT_H
#ifdef _WIN32
#pragma once
#endif

enum eGameServerOrigin
{
	kGSAOrigin_Player		= 0,
	kGSAOrigin_Support		= 1,
	kGSAOrigin_AutoRegister	= 2, // for valve-owned servers
};

enum eGameServerScoreStanding
{
	kGSStanding_Good,
	kGSStanding_Bad,
};

enum eGameServerScoreStandingTrend
{
	kGSStandingTrend_Up,
	kGSStandingTrend_SteadyUp,
	kGSStandingTrend_Steady,
	kGSStandingTrend_SteadyDown,
	kGSStandingTrend_Down,
};

#ifdef GC
#include "gcsdk/schemasharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------------
class CEconGameServerAccount : public GCSDK::CSchemaSharedObject< CSchGameServerAccount, k_EEconTypeGameServerAccount >
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( CEconGameServerAccount );
#endif

public:
	CEconGameServerAccount() {}
	CEconGameServerAccount( uint32 unAccountID ) 
	{
		Obj().m_unAccountID = unAccountID;
	}
};

void GameServerAccount_GenerateIdentityToken( char* pIdentityToken, uint32 unMaxChars );
#endif // GC

inline const char *GameServerAccount_GetStandingString( eGameServerScoreStanding standing )
{
	const char *pStanding = "Good";
	switch ( standing )
	{
	case kGSStanding_Good:
		pStanding = "Good";
		break;
	case kGSStanding_Bad:
		pStanding = "Bad";
		break;
	} // switch
	return pStanding;
}

inline const char *GameServerAccount_GetStandingTrendString( eGameServerScoreStandingTrend trend )
{
	const char *pStandingTrend = "Steady";
	switch ( trend )
	{
	case kGSStandingTrend_Up:
		pStandingTrend = "Upward Fast";
		break;
	case kGSStandingTrend_SteadyUp:
		pStandingTrend = "Slightly Upward";
		break;
	case kGSStandingTrend_Steady:
		pStandingTrend = "Steady";
		break;
	case kGSStandingTrend_SteadyDown:
		pStandingTrend = "Slightly Downward";
		break;
	case kGSStandingTrend_Down:
		pStandingTrend = "Downward Fast";
		break;
	} // switch
	return pStandingTrend;
}

//---------------------------------------------------------------------------------
// Purpose: Selective account-level data for game servers
//---------------------------------------------------------------------------------
class CEconGameAccountForGameServers : public GCSDK::CProtoBufSharedObject < CSOEconGameAccountForGameServers, k_EEconTypeGameAccountForGameServers >
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CEconGameAccountForGameServers );
public:
	virtual bool BIsDatabaseBacked() const { return false; }
#endif
};

#endif //ECON_GAME_SERVER_ACCOUNT_H
