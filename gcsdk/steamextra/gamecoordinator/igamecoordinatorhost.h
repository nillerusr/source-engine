//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides an interface that the server hosting a GC must implement
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMECOORDINATORHOST_H
#define IGAMECOORDINATORHOST_H
#ifdef _WIN32
#pragma once
#endif

class CSteamID;
class IGCSQLQuery;

class IGameCoordinatorHost
{
public:
	virtual bool BSendMessageToClient( AppId_t unAppID, const CSteamID & steamIDTarget, uint32 unMsgType, const void *pubData, uint32 cubData ) = 0;
	virtual GID_t GenerateGID() = 0;
	virtual void EmitMessage( const char *pchGroupName, SpewType_t spewType, int iSpewLevel, int iLevelLog, const char *pchMsg ) = 0;
	virtual void SQLQuery( GID_t gidContextID, IGCSQLQuery *pQuery, int eSchemaCatalog ) = 0;
	virtual void StartupComplete( bool bSuccess ) = 0;
	virtual void ShutdownComplete() = 0;
	virtual EUniverse GetUniverse() = 0;
};

#define GAMECOORDINATORHOST_INTERFACE_VERSION "GAMECOORDINATORHOST002"

#endif // IGAMECOORDINATORHOST_H
