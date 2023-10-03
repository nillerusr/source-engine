//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef ENGINECALLBACK_H
#define ENGINECALLBACK_H

#ifndef EIFACE_H
#include "eiface.h"
#endif

#ifdef POSIX
#define random random_valve// stdlib.h defined random()..., and so does vstdlib/random.h
#endif

#include "tier3/tier3.h"
#include "tier2/tier2_logging.h"

class IFileSystem;				// include FileSystem.h
class IUniformRandomStream;		// include vstdlib/random.h
class IEngineSound;				// include engine/IEngineSound.h
class IVEngineServer;			
class IVoiceServer;
class IStaticPropMgrServer;
class ISpatialPartition;
class IVModelInfo;
class IEngineTrace;
class IGameEventManager2;
class IVDebugOverlay;
class IDataCache;
class IMDLCache;
class IServerEngineTools;
class IXboxSystem;
class IScriptManager;
class IServerFoundry;
class CSteamAPIContext;
class CSteamGameServerAPIContext;

extern IVEngineServer			*engine;
extern IVoiceServer				*g_pVoiceServer;
extern IFileSystem				*filesystem;
extern IStaticPropMgrServer		*staticpropmgr;
extern ISpatialPartition		*partition;
extern IEngineSound				*enginesound;
extern IVModelInfo				*modelinfo;
extern IEngineTrace				*enginetrace;
extern IFileLoggingListener		*filelogginglistener;
extern IGameEventManager2		*gameeventmanager;
extern IVDebugOverlay			*debugoverlay;
extern IServerEngineTools		*serverenginetools;
extern IServerFoundry			*serverfoundry;
extern IXboxSystem				*xboxsystem; // 360 only
extern IScriptManager			*scriptmanager;
extern CSteamAPIContext			*steamapicontext; // available on game clients
extern CSteamGameServerAPIContext *steamgameserverapicontext; //available on game servers
#ifdef INFESTED_DLL
class IASW_Mission_Chooser;
extern IASW_Mission_Chooser *missionchooser;
#endif


//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex );


//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
int PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );


//-----------------------------------------------------------------------------
// Precache-related methods for effects (used by DispatchEffect)
//-----------------------------------------------------------------------------
void PrecacheEffect( const char *pParticleSystemName );
int GetEffectIndex( const char *pParticleSystemName );
const char *GetEffectNameFromIndex( int nIndex );


class IRecipientFilter;
void EntityMessageBegin( CBaseEntity * entity, bool reliable = false );
void UserMessageBegin( IRecipientFilter& filter, const char *messagename );
void MessageEnd( void );

// bytewise
void MessageWriteByte( int iValue);
void MessageWriteChar( int iValue);
void MessageWriteShort( int iValue);
void MessageWriteWord( int iValue );
void MessageWriteLong( int iValue);
void MessageWriteFloat( float flValue);
void MessageWriteAngle( float flValue);
void MessageWriteCoord( float flValue);
void MessageWriteVec3Coord( const Vector& rgflValue);
void MessageWriteVec3Normal( const Vector& rgflValue);
void MessageWriteAngles( const QAngle& rgflValue);
void MessageWriteString( const char *sz );
void MessageWriteEntity( int iValue);
void MessageWriteEHandle( CBaseEntity *pEntity ); //encoded as a long


// bitwise
void MessageWriteBool( bool bValue );
void MessageWriteUBitLong( unsigned int data, int numbits );
void MessageWriteSBitLong( int data, int numbits );
void MessageWriteBits( const void *pIn, int nBits );
void MessageWriteBitVecIntegral( const Vector& vecValue );


// Bytewise
#define WRITE_BYTE		(MessageWriteByte)
#define WRITE_CHAR		(MessageWriteChar)
#define WRITE_SHORT		(MessageWriteShort)
#define WRITE_WORD		(MessageWriteWord)
#define WRITE_LONG		(MessageWriteLong)
#define WRITE_FLOAT		(MessageWriteFloat)
#define WRITE_ANGLE		(MessageWriteAngle)
#define WRITE_COORD		(MessageWriteCoord)
#define WRITE_VEC3COORD	(MessageWriteVec3Coord)
#define WRITE_VEC3NORMAL (MessageWriteVec3Normal)
#define WRITE_ANGLES	(MessageWriteAngles)
#define WRITE_STRING	(MessageWriteString)
#define WRITE_ENTITY	(MessageWriteEntity)
#define WRITE_EHANDLE	(MessageWriteEHandle)

// Bitwise
#define WRITE_BOOL		(MessageWriteBool)
#define WRITE_UBITLONG	(MessageWriteUBitLong)
#define WRITE_SBITLONG	(MessageWriteSBitLong)
#define WRITE_BITS		(MessageWriteBits)
#define WRITE_VEC3_INTEGRAL  (MessageWriteBitVecIntegral)

#endif		//ENGINECALLBACK_H
