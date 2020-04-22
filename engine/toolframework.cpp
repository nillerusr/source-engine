//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Engine system for loading and managing tools
//
//=============================================================================

#include "quakedef.h"
#include "tier0/icommandline.h"
#include "toolframework/itooldictionary.h"
#include "toolframework/itoolsystem.h"
#include "toolframework/itoolframework.h"
#include "toolframework/iclientenginetools.h"
#include "toolframework/iserverenginetools.h"
#include "tier1/KeyValues.h"
#include "tier1/utlvector.h"
#include "tier1/tier1.h"
#include "filesystem_engine.h"

class IToolSystem;
extern CreateInterfaceFn g_AppSystemFactory;
typedef bool (*FnQuitHandler)( void *pvUserData );
void EngineTool_InstallQuitHandler( void *pvUserData, FnQuitHandler func );

//-----------------------------------------------------------------------------
// Purpose: -tools loads framework
//-----------------------------------------------------------------------------
class CToolFrameworkInternal : public IToolFrameworkInternal
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool	Connect( CreateInterfaceFn factory );
	virtual void	Disconnect();

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void	*QueryInterface( const char *pInterfaceName );

	// Init, shutdown
	virtual InitReturnVal_t Init();
	virtual void	Shutdown();

	virtual bool	CanQuit();

public:
	// Level init, shutdown
	virtual void	ClientLevelInitPreEntityAllTools();
	// entities are created / spawned / precached here
	virtual void	ClientLevelInitPostEntityAllTools();

	virtual void	ClientLevelShutdownPreEntityAllTools();
	// Entities are deleted / released here...
	virtual void	ClientLevelShutdownPostEntityAllTools();

	virtual void	ClientPreRenderAllTools();
	virtual void	ClientPostRenderAllTools();

	virtual bool	IsThirdPersonCamera();

	virtual bool	IsToolRecording();

	// Level init, shutdown
	virtual void	ServerLevelInitPreEntityAllTools();
	// entities are created / spawned / precached here
	virtual void	ServerLevelInitPostEntityAllTools();

	virtual void	ServerLevelShutdownPreEntityAllTools();
	// Entities are deleted / released here...
	virtual void	ServerLevelShutdownPostEntityAllTools();
	// end of level shutdown

	// Called each frame before entities think
	virtual void	ServerFrameUpdatePreEntityThinkAllTools();
	// called after entities think
	virtual void	ServerFrameUpdatePostEntityThinkAllTools();
	virtual void	ServerPreClientUpdateAllTools();
	const char*		GetEntityData( const char *pActualEntityData );
	virtual void	ServerPreSetupVisibilityAllTools();

	virtual bool	PostInit();

	virtual bool	ServerInit( CreateInterfaceFn serverFactory ); 
	virtual bool	ClientInit( CreateInterfaceFn clientFactory ); 

	virtual void	ServerShutdown();
	virtual void	ClientShutdown();

	virtual void	Think( bool finalTick );

	virtual void	PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );

	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual bool	SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool	SetupAudioState( AudioState_t &audioState );

	virtual int		GetToolCount();
	virtual const char* GetToolName( int index );
	virtual void	SwitchToTool( int index );
	virtual IToolSystem* SwitchToTool( const char* pToolName );

	virtual bool	IsTopmostTool( const IToolSystem *sys );
	virtual const IToolSystem *GetToolSystem( int index ) const;
	virtual IToolSystem *GetTopmostTool();

	virtual void	PostMessage( KeyValues *msg );

	virtual bool	GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info );

	virtual void	HostRunFrameBegin();
	virtual void	HostRunFrameEnd();

	virtual void	RenderFrameBegin();
	virtual void	RenderFrameEnd();

	virtual void	VGui_PreRenderAllTools( int paintMode );
	virtual void	VGui_PostRenderAllTools( int paintMode );

	virtual void	VGui_PreSimulateAllTools();
	virtual void	VGui_PostSimulateAllTools();

	// Are we using tools?
	virtual bool	InToolMode();

	// Should the game be allowed to render the world?
	virtual bool	ShouldGameRenderView();

	virtual IMaterialProxy *LookupProxy( const char *proxyName );

private:
	void			LoadTools();
	void			LoadToolsFromLibrary( const char *dllname );

	void			InvokeMethod( ToolSystemFunc_t f );
	void			InvokeMethodInt( ToolSystemFunc_Int_t f, int arg );

	void			ShutdownTools();

	// Purpose: Shuts down all modules
	void			ShutdownModules();

	// Purpose: Shuts down all tool dictionaries
	void			ShutdownToolDictionaries();

	CUtlVector< IToolSystem * >		m_ToolSystems;
	CUtlVector< IToolDictionary * >	m_Dictionaries;
	CUtlVector< CSysModule * >		m_Modules;
	int m_nActiveToolIndex;
	bool m_bInToolMode;
};

static CToolFrameworkInternal g_ToolFrameworkInternal;
IToolFrameworkInternal *toolframework = &g_ToolFrameworkInternal;


//-----------------------------------------------------------------------------
// Purpose: Used to invoke a method of all added Game systems in order
// Input  : f - function to execute
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::InvokeMethod( ToolSystemFunc_t f )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *sys = m_ToolSystems[i];
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to invoke a method of all added Game systems in order
// Input  : f - function to execute
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::InvokeMethodInt( ToolSystemFunc_Int_t f, int arg )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *sys = m_ToolSystems[i];
		(sys->*f)( arg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Here's where the app systems get to learn about each other 
// Input  : factory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::Connect( CreateInterfaceFn factory )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Disconnect()
{
}


//-----------------------------------------------------------------------------
// Purpose:  Here's where systems can access other interfaces implemented by this object
//  Returns NULL if it doesn't implement the requested interface
// Input  : *pInterfaceName - 
//-----------------------------------------------------------------------------
void *CToolFrameworkInternal::QueryInterface( const char *pInterfaceName )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pvUserData - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool CToolFrameworkInternal_QuitHandler( void *pvUserData )
{
	CToolFrameworkInternal *tfm = reinterpret_cast< CToolFrameworkInternal * >( pvUserData );
	if ( tfm )
	{
		return tfm->CanQuit();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Init, shutdown
// Input  :  - 
// Output : InitReturnVal_t
//-----------------------------------------------------------------------------
InitReturnVal_t CToolFrameworkInternal::Init()
{
	m_bInToolMode = false;
	m_nActiveToolIndex = -1;

// Disabled in REL for now
#if 1
#ifndef SWDS
	EngineTool_InstallQuitHandler( this, CToolFrameworkInternal_QuitHandler );

	// FIXME: Eventually this should be -edit
	if ( CommandLine()->FindParm( "-tools" ) )
	{
		LoadTools();
	}
#endif
#endif
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Called at end of Host_Init
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::PostInit()
{
	bool bRetVal = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];

		// FIXME: Should this really get access to a list if factories
		bool success = system->Init( );
		if ( !success )
		{
			bRetVal = false;
		}
	}

	// Activate first tool if we didn't encounter an error
	if ( bRetVal )
	{
		SwitchToTool( 0 );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Shutdown()
{
	// Shut down all tools
	ShutdownTools();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : finalTick - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Think( bool finalTick )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->Think( finalTick );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : serverFactory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ServerInit( CreateInterfaceFn serverFactory )
{
	bool retval = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		// FIXME: Should this really get access to a list if factories
		bool success = system->ServerInit( serverFactory );
		if ( !success )
		{
			retval = false;
		}
	}
	return retval;
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clientFactory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ClientInit( CreateInterfaceFn clientFactory )
{
	bool retval = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		// FIXME: Should this really get access to a list if factories
		bool success = system->ClientInit( clientFactory );
		if ( !success )
		{
			retval = false;
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerShutdown()
{
	// Reverse order
	int toolCount = m_ToolSystems.Count();
	for ( int i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->ServerShutdown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientShutdown()
{
	// Reverse order
	int toolCount = m_ToolSystems.Count();
	for ( int i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->ClientShutdown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::CanQuit()
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		bool canquit = system->CanQuit();
		if ( !canquit )
		{
			return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all modules
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownModules()
{
	// Shutdown dictionaries
	int i;
	for ( i = m_Modules.Count(); --i >= 0; )
	{
		Sys_UnloadModule( m_Modules[i] );
	}

	m_Modules.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all tool dictionaries
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownToolDictionaries()
{
	// Shutdown dictionaries
	int i;
	for ( i = m_Dictionaries.Count(); --i >= 0; )
	{
		m_Dictionaries[i]->Shutdown();
	}

	for ( i = m_Dictionaries.Count(); --i >= 0; )
	{
		m_Dictionaries[i]->Disconnect();
	}

	m_Dictionaries.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all tools
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownTools()
{
	// Deactivate tool
	SwitchToTool( -1 );

	// Reverse order
	int i;
	int toolCount = m_ToolSystems.Count();
	for ( i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->Shutdown();
	}

	m_ToolSystems.RemoveAll();

	ShutdownToolDictionaries();
	ShutdownModules();
}


//-----------------------------------------------------------------------------
// Purpose: Adds tool from specified library
// Input  : *dllname - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::LoadToolsFromLibrary( const char *dllname )
{
	CSysModule *module = Sys_LoadModule( dllname );
	if ( !module )
	{
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Unable to load '%s'\n", dllname );
		return;
	}

	CreateInterfaceFn factory = Sys_GetFactory( module );
	if ( !factory )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' has no factory\n", dllname );
		return;
	}

	IToolDictionary *dictionary = ( IToolDictionary * )factory( VTOOLDICTIONARY_INTERFACE_VERSION, NULL );
	if ( !dictionary )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' doesn't support '%s'\n", dllname, VTOOLDICTIONARY_INTERFACE_VERSION );
		return;
	}

	if ( !dictionary->Connect( g_AppSystemFactory ) )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' connection phase failed.\n", dllname );
		return;
	}

	if ( dictionary->Init( ) != INIT_OK )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' initialization phase failed.\n", dllname );
		return;
	}

	dictionary->CreateTools();

	int toolCount = dictionary->GetToolCount();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = dictionary->GetTool( i );
		if ( tool )
		{
			Msg( "Loaded tool '%s'\n", tool->GetToolName() );
			m_ToolSystems.AddToTail( tool );
		}
	}

	m_Dictionaries.AddToTail( dictionary );
	m_Modules.AddToTail( module );
}


//-----------------------------------------------------------------------------
// Are we using tools?
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::InToolMode()
{
	return m_bInToolMode;
}


//-----------------------------------------------------------------------------
// Should the game be allowed to render the world?
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ShouldGameRenderView()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->ShouldGameRenderView( );
	}
	return true;
}

IMaterialProxy *CToolFrameworkInternal::LookupProxy( const char *proxyName )
{
	int toolCount = GetToolCount();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = m_ToolSystems[ i ];
		Assert( tool );

		IMaterialProxy *matProxy = tool->LookupProxy( proxyName );
		if ( matProxy )
		{
			return matProxy;
		}
	}
	return NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: FIXME:  Should scan a KeyValues file
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::LoadTools()
{
	m_bInToolMode = true;

	// Load rootdir/bin/enginetools.txt
	KeyValues *kv = new KeyValues( "enginetools" );
	Assert( kv );

	// We don't ship enginetools.txt to Steam public, so we'll need to load sdkenginetools.txt if enginetools.txt isn't present
	bool bLoadSDKFile = !g_pFileSystem->FileExists( "enginetools.txt", "EXECUTABLE_PATH" );

	if ( kv && kv->LoadFromFile( g_pFileSystem, bLoadSDKFile ? "sdkenginetools.txt" : "enginetools.txt", "EXECUTABLE_PATH" ) )
	{
		for ( KeyValues *tool = kv->GetFirstSubKey();
				tool != NULL;
				tool = tool->GetNextKey() )
		{
			if ( !Q_stricmp( tool->GetName(),  "library" ) )
			{
				// CHECK both bin/tools and gamedir/bin/tools
				LoadToolsFromLibrary( tool->GetString() );
			}
		}

		kv->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Level init, shutdown
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelInitPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelInitPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelInitPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelInitPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelShutdownPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelShutdownPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Entities are deleted / released here...
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelShutdownPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelShutdownPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientPreRenderAllTools()
{
	InvokeMethod( &IToolSystem::ClientPreRender );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::IsThirdPersonCamera()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->IsThirdPersonCamera( );
	}
	return false;
}

// is the current tool recording?
bool CToolFrameworkInternal::IsToolRecording()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->IsToolRecording( );
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientPostRenderAllTools()
{
	InvokeMethod( &IToolSystem::ClientPostRender );
}

//-----------------------------------------------------------------------------
// Purpose: Level init, shutdown
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelInitPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelInitPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: entities are created / spawned / precached here
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelInitPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelInitPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelShutdownPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelShutdownPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Entities are deleted / released here...
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelShutdownPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelShutdownPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Called each frame before entities think
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerFrameUpdatePreEntityThinkAllTools()
{
	InvokeMethod( &IToolSystem::ServerFrameUpdatePreEntityThink );
}

//-----------------------------------------------------------------------------
// Purpose: Called after entities think
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerFrameUpdatePostEntityThinkAllTools()
{
	InvokeMethod( &IToolSystem::ServerFrameUpdatePostEntityThink );
}

//-----------------------------------------------------------------------------
// Purpose: Called before client networking occurs on the server
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerPreClientUpdateAllTools()
{
	InvokeMethod( &IToolSystem::ServerPreClientUpdate );
}


//-----------------------------------------------------------------------------
// The server uses this to call into the tools to get the actual
// entities to spawn on startup
//-----------------------------------------------------------------------------
const char* CToolFrameworkInternal::GetEntityData( const char *pActualEntityData )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->GetEntityData( pActualEntityData );
	}
	return pActualEntityData;
}

void CToolFrameworkInternal::ServerPreSetupVisibilityAllTools()
{
	InvokeMethod( &IToolSystem::ServerPreSetupVisibility );
}


//-----------------------------------------------------------------------------
// Purpose: Post a message to tools
// Input  : hEntity - 
//			*msg - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg )
{
	// FIXME: Only message topmost tool?

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = m_ToolSystems[ i ];
		Assert( tool );
		tool->PostMessage( hEntity, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to adjust viewport
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		tool->AdjustEngineViewport( x, y, width, height );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to set the camera/view
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	if ( m_nActiveToolIndex < 0 )
		return false;

	IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
	Assert( tool );
	return tool->SetupEngineView( origin, angles, fov );
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to set the microphone
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::SetupAudioState( AudioState_t &audioState )
{
	if ( m_nActiveToolIndex < 0 )
		return false;

	IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
	Assert( tool );
	return tool->SetupAudioState( audioState );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::VGui_PreRenderAllTools( int paintMode )
{
	InvokeMethodInt( &IToolSystem::VGui_PreRender, paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::VGui_PostRenderAllTools( int paintMode )
{
	InvokeMethodInt( &IToolSystem::VGui_PostRender, paintMode );
}

void CToolFrameworkInternal::VGui_PreSimulateAllTools()
{
	InvokeMethod( &IToolSystem::VGui_PreSimulate );
}

void CToolFrameworkInternal::VGui_PostSimulateAllTools()
{
	InvokeMethod( &IToolSystem::VGui_PostSimulate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int CToolFrameworkInternal::GetToolCount()
{
	return m_ToolSystems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CToolFrameworkInternal::GetToolName( int index )
{
	if ( index < 0 || index >= m_ToolSystems.Count() )
	{
		return "";
	}
	IToolSystem *sys = m_ToolSystems[ index ];
	if ( sys )
	{
		return sys->GetToolName();
	}
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::SwitchToTool( int index )
{
	if ( ( m_ToolSystems.Count() < 1 ) || ( index >= m_ToolSystems.Count() ) )
		return;

	if ( index != m_nActiveToolIndex )
	{
		if ( m_nActiveToolIndex >= 0 )
		{
			IToolSystem *pOldTool = m_ToolSystems[ m_nActiveToolIndex ];
			pOldTool->OnToolDeactivate();
		}

		m_nActiveToolIndex = index;
		if ( m_nActiveToolIndex >= 0 )
		{
			IToolSystem *pNewTool = m_ToolSystems[ m_nActiveToolIndex ];
			pNewTool->OnToolActivate();
		}
	}
}


//-----------------------------------------------------------------------------
// Switches to a named tool
//-----------------------------------------------------------------------------
IToolSystem *CToolFrameworkInternal::SwitchToTool( const char* pToolName )
{
	int nCount = GetToolCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pToolName, GetToolName(i) ) )
		{
			SwitchToTool( i );
			return m_ToolSystems[i];
		}
	}

	return NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sys - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::IsTopmostTool( const IToolSystem *sys )
{
	if ( m_ToolSystems.Count() <= 0 || ( m_nActiveToolIndex < 0 ) )
		return false;

	return ( m_ToolSystems[ m_nActiveToolIndex ] == sys );
}

IToolSystem *CToolFrameworkInternal::GetTopmostTool()
{
	return m_nActiveToolIndex >= 0 ? m_ToolSystems[ m_nActiveToolIndex ] : NULL;
}

//-----------------------------------------------------------------------------
// returns a tool system by index
//-----------------------------------------------------------------------------
const IToolSystem *CToolFrameworkInternal::GetToolSystem( int index ) const
{
	if ( ( index < 0 ) || ( index >= m_ToolSystems.Count() ) )
		return NULL;

	return m_ToolSystems[index];
}


void CToolFrameworkInternal::PostMessage( KeyValues *msg )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		tool->PostMessage( 0, msg );
	}
}

bool CToolFrameworkInternal::GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->GetSoundSpatialization( iUserData, guid, info );
	}
	return true;
}

void CToolFrameworkInternal::HostRunFrameBegin()
{
	InvokeMethod( &IToolSystem::HostRunFrameBegin );
}

void CToolFrameworkInternal::HostRunFrameEnd()
{
	InvokeMethod( &IToolSystem::HostRunFrameEnd );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::RenderFrameBegin()
{
	InvokeMethod( &IToolSystem::RenderFrameBegin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::RenderFrameEnd()
{
	InvokeMethod( &IToolSystem::RenderFrameEnd );
}

// Exposed because it's an IAppSystem
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CToolFrameworkInternal, IToolFrameworkInternal, VTOOLFRAMEWORK_INTERFACE_VERSION, g_ToolFrameworkInternal );

//-----------------------------------------------------------------------------
// Purpose: exposed from engine to client .dll
//-----------------------------------------------------------------------------
class CClientEngineTools : public IClientEngineTools
{
public:
	virtual void LevelInitPreEntityAllTools();
	virtual void LevelInitPostEntityAllTools();
	virtual void LevelShutdownPreEntityAllTools();
	virtual void LevelShutdownPostEntityAllTools();
	virtual void PreRenderAllTools();
	virtual void PostRenderAllTools();
	virtual void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
	virtual void AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual bool SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool SetupAudioState( AudioState_t &audioState );
	virtual void VGui_PreRenderAllTools( int paintMode );
	virtual void VGui_PostRenderAllTools( int paintMode );
	virtual bool IsThirdPersonCamera( );
	virtual bool InToolMode();
};

EXPOSE_SINGLE_INTERFACE( CClientEngineTools, IClientEngineTools, VCLIENTENGINETOOLS_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelInitPreEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelInitPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelInitPostEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelInitPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelShutdownPreEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelShutdownPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelShutdownPostEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelShutdownPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PreRenderAllTools()
{
	g_ToolFrameworkInternal.ClientPreRenderAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PostRenderAllTools()
{
	g_ToolFrameworkInternal.ClientPostRenderAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hEntity - 
//			*msg - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg )
{
	g_ToolFrameworkInternal.PostToolMessage( hEntity, msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void CClientEngineTools::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
	g_ToolFrameworkInternal.AdjustEngineViewport( x, y, width, height );
}

bool CClientEngineTools::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	return g_ToolFrameworkInternal.SetupEngineView( origin, angles, fov );
}

bool CClientEngineTools::SetupAudioState( AudioState_t &audioState )
{
	return g_ToolFrameworkInternal.SetupAudioState( audioState );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientEngineTools::VGui_PreRenderAllTools( int paintMode )
{
	g_ToolFrameworkInternal.VGui_PreRenderAllTools( paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientEngineTools::VGui_PostRenderAllTools( int paintMode )
{
	g_ToolFrameworkInternal.VGui_PostRenderAllTools( paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CClientEngineTools::IsThirdPersonCamera( )
{
	return g_ToolFrameworkInternal.IsThirdPersonCamera( );
}

bool CClientEngineTools::InToolMode()
{
	return g_ToolFrameworkInternal.InToolMode();
}

//-----------------------------------------------------------------------------
// Purpose: Exposed to server.dll
//-----------------------------------------------------------------------------
class CServerEngineTools : public IServerEngineTools
{
public:
	// Inherited from IServerEngineTools
	virtual void LevelInitPreEntityAllTools();
	virtual void LevelInitPostEntityAllTools();
	virtual void LevelShutdownPreEntityAllTools();
	virtual void LevelShutdownPostEntityAllTools();
	virtual void FrameUpdatePreEntityThinkAllTools();
	virtual void FrameUpdatePostEntityThinkAllTools();
	virtual void PreClientUpdateAllTools();
	virtual void PreSetupVisibilityAllTools();
	virtual const char* GetEntityData( const char *pActualEntityData );
	virtual bool InToolMode();
};

EXPOSE_SINGLE_INTERFACE( CServerEngineTools, IServerEngineTools, VSERVERENGINETOOLS_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelInitPreEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelInitPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelInitPostEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelInitPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelShutdownPreEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelShutdownPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelShutdownPostEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelShutdownPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::FrameUpdatePreEntityThinkAllTools()
{
	g_ToolFrameworkInternal.ServerFrameUpdatePreEntityThinkAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::FrameUpdatePostEntityThinkAllTools()
{
	g_ToolFrameworkInternal.ServerFrameUpdatePostEntityThinkAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::PreClientUpdateAllTools()
{
	g_ToolFrameworkInternal.ServerPreClientUpdateAllTools();
}


//-----------------------------------------------------------------------------
// The server uses this to call into the tools to get the actual
// entities to spawn on startup
//-----------------------------------------------------------------------------
const char* CServerEngineTools::GetEntityData( const char *pActualEntityData )
{
	return g_ToolFrameworkInternal.GetEntityData( pActualEntityData );
}

void CServerEngineTools::PreSetupVisibilityAllTools()
{
	return g_ToolFrameworkInternal.ServerPreSetupVisibilityAllTools();
}

bool CServerEngineTools::InToolMode()
{
	return g_ToolFrameworkInternal.InToolMode();
}
