//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef SV_SERVERPLUGIN_H
#define SV_SERVERPLUGIN_H

#ifdef _WIN32
#pragma once
#endif

#include "eiface.h"
#include "engine/iserverplugin.h"

//---------------------------------------------------------------------------------
// Purpose: a single plugin
//---------------------------------------------------------------------------------
class CPlugin
{
public:
	CPlugin();
	~CPlugin();

	const char *GetName();
	bool Load( const char *fileName );
	void Unload();
	void Disable( bool state );
	bool IsDisabled() { return m_bDisable; }
	int GetPluginInterfaceVersion() const { return m_iPluginInterfaceVersion; }

	IServerPluginCallbacks *GetCallback();

private:
	void SetName( const char *name );
	char m_szName[128];
	bool m_bDisable;
	
	IServerPluginCallbacks *m_pPlugin;
	int m_iPluginInterfaceVersion;	// Tells if we got INTERFACEVERSION_ISERVERPLUGINCALLBACKS or an older version.
	
	CSysModule		*m_pPluginModule;
};

//---------------------------------------------------------------------------------
// Purpose: implenents passthroughs for plugins and their special helper functions
//---------------------------------------------------------------------------------
class CServerPlugin : public IServerPluginHelpers
{
public:
	CServerPlugin();
	~CServerPlugin();

	// management functions
	void LoadPlugins();
	void UnloadPlugins();
	bool UnloadPlugin( int index );
	bool LoadPlugin( const char *fileName );

	void DisablePlugins();
	void DisablePlugin( int index );

	void EnablePlugins();
	void EnablePlugin( int index );

	void PrintDetails();

	// multiplex the passthroughs
	virtual void			LevelInit( char const *pMapName, 
									char const *pMapEntities, char const *pOldLevel, 
									char const *pLandmarkName, bool loadGame, bool background );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			GameFrame( bool simulating );
	virtual void			LevelShutdown( void );

	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername );
	virtual void			SetCommandClient( int index );
	virtual void			ClientSettingsChanged( edict_t *pEdict );
	virtual bool			ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );


	// implement helpers
	virtual void CreateMessage( edict_t *pEntity, DIALOG_TYPE type, KeyValues *data, IServerPluginCallbacks *plugin );
	virtual void ClientCommand( edict_t *pEntity, const char *cmd );
	virtual QueryCvarCookie_t StartQueryCvarValue( edict_t *pEntity, const char *pName );

	int						GetNumLoadedPlugins( void ){ return m_Plugins.Count(); }

private:
	CUtlVector<CPlugin *>	m_Plugins;
	IPluginHelpersCheck		*m_PluginHelperCheck;

public:
	//New plugin interface callbacks
	virtual void			OnEdictAllocated( edict_t *edict );
	virtual void			OnEdictFreed( const edict_t *edict  ); 
};

extern CServerPlugin *g_pServerPluginHandler;

class IClient;
QueryCvarCookie_t SendCvarValueQueryToClient( IClient *client, const char *pCvarName, bool bPluginQuery );

#endif //SV_SERVERPLUGIN_H
