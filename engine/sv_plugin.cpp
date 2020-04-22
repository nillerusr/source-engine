//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "server_pch.h"
#include "sv_plugin.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "eiface.h"
#include "sys.h"
#include "client.h"
#include "sys_dll.h"
#include "pr_edict.h"
#include "networkstringtable.h"
#include "networkstringtableserver.h"
#include "tier0/etwprof.h"

extern CreateInterfaceFn g_AppSystemFactory;
extern CSysModule *g_GameDLL;

CServerPlugin s_ServerPlugin;
CServerPlugin *g_pServerPluginHandler = &s_ServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerPlugin, IServerPluginHelpers, INTERFACEVERSION_ISERVERPLUGINHELPERS, s_ServerPlugin );

// Ascending value so they have a unique cookie to relate queries to the responses.
static int g_iQueryCvarCookie = 1;


QueryCvarCookie_t SendCvarValueQueryToClient( IClient *client, const char *pCvarName, bool bPluginQuery )
{
	// Send a message to the client asking for the value.
	SVC_GetCvarValue msg;
	msg.m_iCookie = g_iQueryCvarCookie++;
	msg.m_szCvarName = pCvarName;

	// If the query came from the game DLL instead of from a plugin, then we negate the cookie
	// so it knows who to callback on when the value arrives back from the client.
	if ( !bPluginQuery )
		msg.m_iCookie = -msg.m_iCookie;
	
	client->SendNetMsg( msg );
	return msg.m_iCookie;
}


//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CPlugin::CPlugin()
{
	m_pPlugin = NULL;
	m_pPluginModule = NULL;
	m_bDisable = false;
	m_szName[0] = 0;
}

CPlugin::~CPlugin()
{
	if ( m_pPlugin )
	{
		Unload();
	}
	m_pPlugin = NULL;

	if ( m_pPluginModule )
	{
		g_pFileSystem->UnloadModule( m_pPluginModule );
	}
	m_pPluginModule = NULL;
}

//---------------------------------------------------------------------------------
// Purpose: loads and initializes a plugin
//---------------------------------------------------------------------------------
bool CPlugin::Load( const char *fileName )
{
	if ( IsX360() )
	{
		return false;
	}

	char fixedFileName[ MAX_PATH ];
	Q_strncpy( fixedFileName, fileName, sizeof(fixedFileName) );
	Q_FixSlashes( fixedFileName );

#if defined ( OSX ) || defined( LINUX )
	// Linux doesn't check signatures, so in that case disable plugins on the client completely unless -insecure is specified
	if ( !sv.IsDedicated() && Host_IsSecureServerAllowed() )
		return false;
#endif
	// Only allow unsigned plugins in -insecure mode
	if ( !Host_AllowLoadModule( fixedFileName, "GAME", false ) )
		return false;

	m_pPluginModule = g_pFileSystem->LoadModule( fixedFileName, "GAME", false );
	if ( m_pPluginModule )
	{
		CreateInterfaceFn pluginFactory = Sys_GetFactory( m_pPluginModule );
		if ( pluginFactory )
		{
			m_iPluginInterfaceVersion = 3;
			m_bDisable = true;
			m_pPlugin = (IServerPluginCallbacks *)pluginFactory( INTERFACEVERSION_ISERVERPLUGINCALLBACKS, NULL );
			if ( !m_pPlugin )
			{
				m_iPluginInterfaceVersion = 2;
				m_pPlugin = (IServerPluginCallbacks *)pluginFactory( INTERFACEVERSION_ISERVERPLUGINCALLBACKS_VERSION_2, NULL );
				if ( !m_pPlugin )
				{
					m_iPluginInterfaceVersion = 1;
					m_pPlugin = (IServerPluginCallbacks *)pluginFactory( INTERFACEVERSION_ISERVERPLUGINCALLBACKS_VERSION_1, NULL );
					if ( !m_pPlugin )
					{				
						Warning( "Could not get IServerPluginCallbacks interface from plugin \"%s\"", fileName );
						return false;
					}
				}
			}

			CreateInterfaceFn gameServerFactory = Sys_GetFactory( g_GameDLL );

			if ( !m_pPlugin->Load( g_AppSystemFactory,  gameServerFactory ) )
			{
				Warning( "Failed to load plugin \"%s\"\n", fileName );
				return false;
			}
			SetName( m_pPlugin->GetPluginDescription() );
			m_bDisable = false;
		}
	}
	else
	{
		Warning( "Unable to load plugin \"%s\"\n", fileName );
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: unloads and cleans up a module
//---------------------------------------------------------------------------------
void CPlugin::Unload()
{
	if ( m_pPlugin )
	{
		m_pPlugin->Unload();
	}
	m_pPlugin = NULL;
	m_bDisable = true;

	g_pFileSystem->UnloadModule( m_pPluginModule );
	m_pPluginModule = NULL;
}

//---------------------------------------------------------------------------------
// Purpose: sets the name of the plugin
//---------------------------------------------------------------------------------
void CPlugin::SetName( const char *name )
{
	Q_strncpy( m_szName, name, sizeof(m_szName) );
}

//---------------------------------------------------------------------------------
// Purpose: returns the name of the plugin
//---------------------------------------------------------------------------------
const char *CPlugin::GetName()
{
	return m_szName;
}

//---------------------------------------------------------------------------------
// Purpose: returns the callback interface of a module
//---------------------------------------------------------------------------------
IServerPluginCallbacks *CPlugin::GetCallback()
{
	Assert( m_pPlugin );
	if ( m_pPlugin ) 
	{	
		return m_pPlugin;
	}
	else
	{
		Assert( !"Unable to get plugin callback interface" );
		Warning( "Unable to get callback interface for \"%s\"\n", GetName() );
		return NULL;
	}
}

//---------------------------------------------------------------------------------
// Purpose: enables or disabled a plugin (i.e stops it running)
//---------------------------------------------------------------------------------
void CPlugin::Disable( bool state )
{
	Assert( m_pPlugin );
	if ( state )
	{
		m_pPlugin->Pause();
	}
	else
	{
		m_pPlugin->UnPause();
	}
	m_bDisable = state; 
}


//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CServerPlugin::CServerPlugin()
{
	m_PluginHelperCheck = NULL;
}

CServerPlugin::~CServerPlugin()
{
}

//---------------------------------------------------------------------------------
// Purpose: loads all plugins
//---------------------------------------------------------------------------------
void CServerPlugin::LoadPlugins()
{
	if ( IsX360() )
	{
		return;
	}

	m_Plugins.PurgeAndDeleteElements();

	char const *findfn = Sys_FindFirst( "addons/*.vdf", NULL, 0 );
	while ( findfn )
	{
		DevMsg( "Plugins: found file \"%s\"\n", findfn );
		if ( !g_pFileSystem->FileExists( va("addons/%s", findfn), "MOD" ) ) // verify its in the mods directory
		{
			findfn = Sys_FindNext( NULL, 0 );
			continue;
		}
	
		KeyValues *pluginsFile = new KeyValues("Plugins");
		pluginsFile->LoadFromFile( g_pFileSystem, va("addons/%s", findfn), "MOD" );

		if ( pluginsFile->GetString("file", NULL) ) 
		{
			LoadPlugin(pluginsFile->GetString("file"));
		}

		pluginsFile->deleteThis();

		// move to next item
		findfn = Sys_FindNext( NULL, 0  );
	}

	Sys_FindClose();

	CreateInterfaceFn gameServerFactory = Sys_GetFactory( g_GameDLL );
	m_PluginHelperCheck = (IPluginHelpersCheck *)gameServerFactory( INTERFACEVERSION_PLUGINHELPERSCHECK, NULL );
}

//---------------------------------------------------------------------------------
// Purpose: unloads all plugins
//---------------------------------------------------------------------------------
void CServerPlugin::UnloadPlugins()
{
	for ( int i = m_Plugins.Count() - 1; i >= 0; --i )
	{
		m_Plugins[i]->Unload();
		m_Plugins.Remove(i);
	}
}

//---------------------------------------------------------------------------------
// Purpose: unload a single plugin
//---------------------------------------------------------------------------------
bool CServerPlugin::UnloadPlugin( int index )
{
	if ( m_Plugins.IsValidIndex( index ) )
	{
		m_Plugins[index]->Unload();
		m_Plugins.Remove(index);
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------------
// Purpose: loads a particular dll
//---------------------------------------------------------------------------------
bool CServerPlugin::LoadPlugin( const char *fileName )
{
	CPlugin *plugin = new CPlugin();
	if ( plugin->Load( fileName ) )
	{
		m_Plugins.AddToTail( plugin );
		return true;
	}
	else
	{
		delete plugin;
		return false;
	}
}

//---------------------------------------------------------------------------------
// Purpose: stop all plugins from running
//---------------------------------------------------------------------------------
void CServerPlugin::DisablePlugins()
{
	for ( int i = 0; i < m_Plugins.Count(); i++ )
	{
		m_Plugins[i]->Disable(true);
	}
}

//---------------------------------------------------------------------------------
// Purpose: turns all plugins back on
//---------------------------------------------------------------------------------
void CServerPlugin::EnablePlugins()
{
	for ( int i = 0; i < m_Plugins.Count(); i++ )
	{
		m_Plugins[i]->Disable(false);
	}
}

//---------------------------------------------------------------------------------
// Purpose: stops a single plugin
//---------------------------------------------------------------------------------
void CServerPlugin::DisablePlugin( int index )
{
	if ( m_Plugins.IsValidIndex( index ) )
	{
		m_Plugins[index]->Disable(true);
	}
}

//---------------------------------------------------------------------------------
// Purpose: stops a single plugin
//---------------------------------------------------------------------------------
void CServerPlugin::EnablePlugin( int index )
{
	if ( m_Plugins.IsValidIndex( index ) )
	{
		m_Plugins[index]->Disable(false);
	}
}

//---------------------------------------------------------------------------------
// Purpose: prints info about loaded plugins and their state
//---------------------------------------------------------------------------------
void CServerPlugin::PrintDetails()
{
	ConMsg( "Loaded plugins:\n");
	ConMsg( "---------------------\n" );
	for ( int i = 0; i < m_Plugins.Count(); i++ )
	{
		ConMsg( "%i:\t\"%s\"%s\n", i, m_Plugins[i]->GetName(), m_Plugins[i]->IsDisabled() ? " (disabled)": "" );
	}
	ConMsg( "---------------------\n" );
}

// helper macro to stop this being typed for every passthrough
#define FORALL_PLUGINS	for( int i = 0; i < m_Plugins.Count(); i++ ) 
#define CALL_PLUGIN_IF_ENABLED(call) \
	do { \
		CPlugin *plugin = m_Plugins[i]; \
		if ( ! plugin->IsDisabled() ) \
		{ \
			IServerPluginCallbacks *callbacks = plugin->GetCallback(); \
			if ( callbacks ) \
			{ \
				callbacks-> call ; \
			} \
		} \
	} while (0)


extern CNetworkStringTableContainer *networkStringTableContainerServer;
//---------------------------------------------------------------------------------
// Purpose: pass through functions for the 3rd party API
//---------------------------------------------------------------------------------
void CServerPlugin::LevelInit(	char const *pMapName, 
								char const *pMapEntities, char const *pOldLevel, 
								char const *pLandmarkName, bool loadGame, bool background )
{
	CETWScope timer( "CServerPlugin::LevelInit" );

	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( LevelInit( pMapName ) );
	}

	bool bPrevState = networkStringTableContainerServer->Lock( false );
	serverGameDLL->LevelInit( pMapName, pMapEntities, pOldLevel, pLandmarkName, loadGame, background );
	networkStringTableContainerServer->Lock( bPrevState );

}

void CServerPlugin::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( ServerActivate( pEdictList, edictCount, clientMax ) );
	}

	serverGameDLL->ServerActivate( pEdictList, edictCount, clientMax );
}

void CServerPlugin::GameFrame( bool simulating )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( GameFrame( simulating ) );
	}

	serverGameDLL->GameFrame( simulating );
}

void CServerPlugin::LevelShutdown( void )
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( LevelShutdown() );
	}

	serverGameDLL->LevelShutdown();
}


void CServerPlugin::ClientActive( edict_t *pEntity, bool bLoadGame )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( ClientActive( pEntity ) );
	}

	serverGameClients->ClientActive( pEntity, bLoadGame );
}

void CServerPlugin::ClientDisconnect( edict_t *pEntity )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( ClientDisconnect( pEntity ) );
	}

	serverGameClients->ClientDisconnect( pEntity );
}

void CServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( ClientPutInServer( pEntity, playername ) );
	}

	serverGameClients->ClientPutInServer( pEntity, playername );
}

void CServerPlugin::SetCommandClient( int index )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( SetCommandClient( index ) );
	}

	serverGameClients->SetCommandClient( index );
}

void CServerPlugin::ClientSettingsChanged( edict_t *pEdict )
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED( ClientSettingsChanged( pEdict ) );
	}

	serverGameClients->ClientSettingsChanged( pEdict );
}

bool CServerPlugin::ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	PLUGIN_RESULT result = PLUGIN_CONTINUE;
	bool bAllowConnect = true, bSavedRetVal = true, bRetValOverridden = false;
	FORALL_PLUGINS
	{
		if ( ! m_Plugins[i]->IsDisabled() )
		{
			result = m_Plugins[i]->GetCallback()->ClientConnect( &bAllowConnect, pEntity, pszName, pszAddress, reject, maxrejectlen );
			if ( result == PLUGIN_STOP ) // stop executing right away
			{
				Assert( bAllowConnect == false );
				return bAllowConnect;
			}
			else if ( result == PLUGIN_OVERRIDE  && bRetValOverridden == false ) // only the first PLUGIN_OVERRIDE return set the retval
			{
				bSavedRetVal = bAllowConnect;
				bRetValOverridden = true;
			}
		}
	}
	
	bAllowConnect = serverGameClients->ClientConnect( pEntity, pszName, pszAddress, reject, maxrejectlen );
	return bRetValOverridden ? bSavedRetVal : bAllowConnect;
}

void CServerPlugin::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	PLUGIN_RESULT result = PLUGIN_CONTINUE;
	FORALL_PLUGINS
	{
		if ( !m_Plugins[i]->IsDisabled() )
		{
			result = m_Plugins[i]->GetCallback()->ClientCommand( pEntity, args );
			if ( result == PLUGIN_STOP ) // stop executing right away
				return;
		}
	}

	serverGameClients->ClientCommand( pEntity, args );
}

QueryCvarCookie_t CServerPlugin::StartQueryCvarValue( edict_t *pEntity, const char *pCvarName )
{
	// Figure out which client they're talking about.
	int clientnum = NUM_FOR_EDICT( pEntity );
	if (clientnum < 1 || clientnum > sv.GetClientCount() )
	{
		Warning( "StartQueryCvarValue: Invalid entity\n" );
		return InvalidQueryCvarCookie;
	}
	
	IClient *client = sv.Client( clientnum-1 );
	return SendCvarValueQueryToClient( client, pCvarName, true );
}

void CServerPlugin::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	PLUGIN_RESULT result = PLUGIN_CONTINUE;
	FORALL_PLUGINS
	{
		if ( ! m_Plugins[i]->IsDisabled() )
		{
			result = m_Plugins[i]->GetCallback()->NetworkIDValidated( pszUserName, pszNetworkID );
			if ( result == PLUGIN_STOP ) // stop executing right away
			{
				return;
			}
		}
	}
}

void CServerPlugin::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
	FORALL_PLUGINS
	{
		CPlugin *p = m_Plugins[i];
		if ( !p->IsDisabled() )
		{
			// OnQueryCvarValueFinished was added in version 2 of this interface.
			if ( p->GetPluginInterfaceVersion() >= 2 )
			{
				p->GetCallback()->OnQueryCvarValueFinished( iCookie, pPlayerEntity, eStatus, pCvarName, pCvarValue );
			}
		}
	}
}
void CServerPlugin::OnEdictAllocated( edict_t *edict )
{
	FORALL_PLUGINS
	{
		CPlugin *p = m_Plugins[i];
		if ( !p->IsDisabled() )
		{
			// OnEdictAllocated was added in version 3 of this interface.
			if ( p->GetPluginInterfaceVersion() >= 3 )
			{
				p->GetCallback()->OnEdictAllocated( edict );
			}
		}
	}
}
void CServerPlugin::OnEdictFreed( const edict_t *edict )
{
	FORALL_PLUGINS
	{
		CPlugin *p = m_Plugins[i];
		if ( !p->IsDisabled() )
		{
			// OnEdictFreed was added in version 3 of this interface.
			if ( p->GetPluginInterfaceVersion() >= 3 )
			{
				p->GetCallback()->OnEdictFreed( edict );
			}
		}
	}
}
//---------------------------------------------------------------------------------
// Purpose: creates a VGUI menu on a clients screen
//---------------------------------------------------------------------------------
void  CServerPlugin::CreateMessage( edict_t *pEntity, DIALOG_TYPE type, KeyValues *data, IServerPluginCallbacks *plugin )
{
	if ( !pEntity )
	{
		ConMsg( "Invaid pEntity\n" );
		return;
	}

	if ( !data )
	{
		ConMsg( "No data keyvalues provided\n" );
		return;
	}

	if ( !plugin )
	{
		ConMsg( "No plugin provided\n" );
		return;
	}

	if ( m_PluginHelperCheck && !m_PluginHelperCheck->CreateMessage( plugin->GetPluginDescription(), pEntity, type, data ) )
	{
		ConMsg( "Disallowed by game dll\n" );
		return;
	}

	int clientnum = NUM_FOR_EDICT( pEntity );
	if (clientnum < 1 || clientnum > sv.GetClientCount() )
	{
		ConMsg( "Invalid entity\n" );
		return;
	}
	
	IClient *client = sv.Client(clientnum-1);

	SVC_Menu menu( type, data );
	client->SendNetMsg( menu );
}

void CServerPlugin::ClientCommand( edict_t *pEntity, const char *cmd )
{
	int entnum = NUM_FOR_EDICT( pEntity );
	
	if ( ( entnum < 1 ) || ( entnum >  sv.GetClientCount() ) )
	{
		Msg("\n!!!\nCServerPlugin::ClientCommand:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
			cmd, entnum, sv.GetMaxClients()	);
		return;
	}
	
	sv.GetClient(entnum-1)->ExecuteStringCommand( cmd );
	
}

//---------------------------------------------------------------------------------
//
//
// Purpose: client commands for plugin functions
//
//
//---------------------------------------------------------------------------------
CON_COMMAND( plugin_print, "Prints details about loaded plugins" )
{
	g_pServerPluginHandler->PrintDetails();
}
 
CON_COMMAND( plugin_pause, "plugin_pause <index> : pauses a loaded plugin" )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Syntax: plugin_pause <index>\n" );
	}
	else
	{
		g_pServerPluginHandler->DisablePlugin( atoi(args[2]) );
		ConMsg( "Plugin disabled\n" );
	}
}

CON_COMMAND( plugin_unpause, "plugin_unpause <index> : unpauses a disabled plugin" )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "Syntax: plugin_unpause <index>\n" );
	}
	else
	{
		g_pServerPluginHandler->EnablePlugin( atoi(args[2]) );
		ConMsg( "Plugin enabled\n" );
	}
}

CON_COMMAND( plugin_pause_all, "pauses all loaded plugins" )
{
	g_pServerPluginHandler->DisablePlugins();
	ConMsg( "Plugins disabled\n" );
}

CON_COMMAND( plugin_unpause_all, "unpauses all disabled plugins" )
{
	g_pServerPluginHandler->EnablePlugins();
	ConMsg( "Plugins enabled\n" );
}

CON_COMMAND( plugin_load, "plugin_load <filename> : loads a plugin" )
{
	if ( sv.IsDedicated() && sv.IsActive() )
	{
		ConMsg( "plugin_load : cannot load a plugin while running a map\n" );
	}
	else if ( !sv.IsDedicated() && cl.IsConnected() )
	{
		ConMsg( "plugin_load : cannot load a plugin while connected to a server\n" );
	}
	else
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "plugin_load <filename>\n" );
		}
		else
		{
			if ( !g_pServerPluginHandler->LoadPlugin( args[1] ) )
			{
				Warning( "Unable to load plugin \"%s\"\n", args[1] );
				return;
			}
			ConMsg( "Loaded plugin \"%s\"\n", args[1] );
		}
	}
}

CON_COMMAND( plugin_unload, "plugin_unload <index> : unloads a plugin" )
{
	if ( args.ArgC() < 2 )
	{
		Warning( "plugin_unload <index>\n" );
	}
	else
	{
		if ( !g_pServerPluginHandler->UnloadPlugin( atoi(args[1]) ) )
		{
			Warning( "Unable to unload plugin \"%s\", not found\n", args[1] );
			return;
		}
		ConMsg( "Unloaded plugin \"%s\"\n", args[1] );
	}
}

