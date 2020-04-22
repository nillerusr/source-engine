//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef IGAMECOORDINATOR_H
#define IGAMECOORDINATOR_H
#ifdef _WIN32
#pragma once
#endif

typedef uint32 AppId_t;
class CSteamID;
class IGameCoordinatorHost;
class IGCSQLResultSetList;

class IGameCoordinator
{
public:
	virtual bool BInit( AppId_t unAppID, const char *pchAppPath, IGameCoordinatorHost *pHost ) = 0;
	virtual bool BMainLoopOncePerFrame( uint64 ulLimitMicroseconds ) = 0;
	virtual bool BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds ) = 0;
	virtual void Shutdown() = 0;
	virtual void Uninit() = 0;
	virtual void MessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData ) = 0;
	virtual void Validate( CValidator &validator, const char *pchName ) = 0;
	virtual void SQLResults( GID_t gidContextID ) = 0;
};

#define GAMECOORDINATOR_INTERFACE_VERSION	"GAMECOORDINATOR003"

#endif // IGAMECOORDINATOR_H
