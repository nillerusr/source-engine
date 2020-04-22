//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( SERVER_H )
#define SERVER_H
#ifdef _WIN32
#pragma once
#endif


#include "basetypes.h"
#include "filesystem.h"
#include "packed_entity.h"
#include "bitbuf.h"
#include "netadr.h"
#include "checksum_crc.h"
#include "quakedef.h"
#include "engine/IEngineSound.h"
#include "precache.h"
#include "sv_client.h"
#include "baseserver.h"
#include <ihltvdirector.h>


class CGameTrace;
class ITraceFilter;
class CEventInfo;
typedef CGameTrace trace_t;
typedef int TABLEID;
class IChangeInfoAccessor;
class CPureServerWhitelist;


// find a server class
ServerClass* SV_FindServerClass( const char *pName );
ServerClass* SV_FindServerClass( int index );


//=============================================================================

// Max # of master servers this server can be associated with

class CGameServer : public CBaseServer
{

public:
	CGameServer();
	virtual ~CGameServer();


public: // IServer implementation

	bool	IsPausable( void ) const;
	void	Init( bool isDedicated );
	void	Clear( void );
	void	Shutdown( void );
	void	SetMaxClients(int number);

public: 
	void	InitMaxClients( void );
	bool	SpawnServer( const char *szMapName, const char *szMapFile, const char *startspot );
	void	SetQueryPortFromSteamServer();
	void	CopyPureServerWhitelistToStringTable();
	void 	RemoveClientFromGame( CBaseClient *client );
	void	SendClientMessages ( bool bSendSnapshots );
	void	FinishRestore();
	void	BroadcastSound( SoundInfo_t &sound, IRecipientFilter &filter );
	bool	IsLevelMainMenuBackground( void )	{ return m_bIsLevelMainMenuBackground; }

	// This is true when we start a level and sv_pure is set to 1.
	bool	IsInPureServerMode() const;
	CPureServerWhitelist * GetPureServerWhitelist() const;
	
	inline  CGameClient *Client( int i ) { return static_cast<CGameClient*>(m_Clients[i]); };

protected :

	// Reload the whitelist files for pure server mode.
	void		ReloadWhitelist( const char *pMapName );

	CBaseClient *CreateNewClient( int slot );
	bool		FinishCertificateCheck( netadr_t &adr, int nAuthProtocol, const char *szRawCertificate, int clientChallenge );
	void		SendClientDatagrams ( int clientCount, CGameClient** clients, CFrameSnapshot* pSnapshot );
	void		CopyTempEntities( CFrameSnapshot* pSnapshot );
	void		AssignClassIds();

	virtual void UpdateMasterServerPlayers();

	// Data
public:

	bool		m_bLoadgame;			// handle connections specially
	
	char		m_szStartspot[64];
	
	int			num_edicts;
	int			max_edicts;
	int			free_edicts; // how many edicts in num_edicts are free, in use is num_edicts - free_edicts
	edict_t		*edicts;			// Can array index now, edict_t is fixed
	IChangeInfoAccessor *edictchangeinfo; // HACK to allow backward compat since we can't change edict_t layout

	int			m_nMaxClientsLimit;    // Max allowed on server.
	
	bool		allowsignonwrites;
	bool	    dll_initialized;    // Have we loaded the game dll.

	bool		m_bIsLevelMainMenuBackground;	// true if the level running only as the background to the main menu

	CUtlVector<CEventInfo*>	m_TempEntities;		// temp entities

	bf_write			m_FullSendTables;
	CUtlMemory<byte>	m_FullSendTablesBuffer;

	bool		m_bLoadedPlugins;

public:

	// New style precache lists are done this way
	void		CreateEngineStringTables( void );

	INetworkStringTable *GetModelPrecacheTable( void ) const;
	INetworkStringTable *GetGenericPrecacheTable( void ) const;
	INetworkStringTable *GetSoundPrecacheTable( void ) const;
	INetworkStringTable *GetDecalPrecacheTable( void ) const;
	
	INetworkStringTable *GetDynamicModelsTable( void ) const { return m_pDynamicModelsTable; }


	// Accessors to model precaching stuff
	int			PrecacheModel( char const *name, int flags, model_t *model = NULL );
	model_t		*GetModel( int index );
	int			LookupModelIndex( char const *name );

	// Accessors to model precaching stuff
	int			PrecacheSound( char const *name, int flags );
	char const	*GetSound( int index );
	int			LookupSoundIndex( char const *name );

	int			PrecacheGeneric( char const *name, int flags );
	char const	*GetGeneric( int index );
	int			LookupGenericIndex( char const *name );

	int			PrecacheDecal( char const *name, int flags );
	int			LookupDecalIndex( char const *name );

	void		DumpPrecacheStats( INetworkStringTable *table );

	bool		IsHibernating() const;
	void		UpdateHibernationState();

private:
	void		SetHibernating( bool bHibernating );

	CPrecacheItem	model_precache[ MAX_MODELS ];
	CPrecacheItem	generic_precache[ MAX_GENERIC ];
	CPrecacheItem	sound_precache[ MAX_SOUNDS ];
	CPrecacheItem	decal_precache[ MAX_BASE_DECALS ];

	INetworkStringTable *m_pModelPrecacheTable;	
	INetworkStringTable *m_pSoundPrecacheTable;
	INetworkStringTable *m_pGenericPrecacheTable;
	INetworkStringTable *m_pDecalPrecacheTable;

	INetworkStringTable *m_pDynamicModelsTable;

	CPureServerWhitelist *m_pPureServerWhitelist;
	bool m_bHibernating; 	// Are we hibernating.  Hibernation makes server process consume approx 0 CPU when no clients are connected
};

//============================================================================

class IServerGameDLL;
class IServerGameEnts;
class IServerGameClients;
class IServerGameTags;
extern IServerGameDLL	*serverGameDLL;
extern int g_iServerGameDLLVersion;
extern IServerGameEnts *serverGameEnts;

extern IServerGameClients *serverGameClients;
extern int g_iServerGameClientsVersion;	// This matches the number at the end of the interface name (so for "ServerGameClients004", this would be 4).

extern IHLTVDirector *serverGameDirector;

extern IServerGameTags *serverGameTags;

// Master server address struct for use in building heartbeats
extern	ConVar	skill;
extern	ConVar	deathmatch;
extern	ConVar	coop;

extern	CGameServer	sv;				// local server
extern	CGameClient	*host_client;	// current processing client


#endif // SERVER_H
