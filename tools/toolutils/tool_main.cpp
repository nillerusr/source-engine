//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "tier1/utlvector.h"
#include "tier1/convar.h"
#include "icvar.h"
#include "toolframework/itoolsystem.h"
#include "toolframework/itooldictionary.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "ienginevgui.h"
#include "icvar.h"
#include "toolutils/vgui_tools.h"
#include "mathlib/mathlib.h"
#include "iregistry.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "p4lib/ip4.h"
#include "engine/ivdebugoverlay.h"
#include "tier3/tier3dm.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "dmserializers/idmserializers.h"

//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
IEngineTool	*enginetools = NULL;
IEngineVGui	*enginevgui = NULL;
IFileSystem *g_pFileSystem = NULL;
IVDebugOverlay *debugoverlay = NULL;


//-----------------------------------------------------------------------------
// Assumed to be implemented within the specific tool DLL
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory );
void CreateTools( );
void DisconnectTools( );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateToolRootPanel( void )
{
	// Just using PANEL_GAMEDLL in HL2 right now
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyToolRootPanel( void )
{
}


//-----------------------------------------------------------------------------
// Global accessors for root tool panels
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetToolRootPanel( void )
{
	vgui::VPANEL root = enginevgui->GetPanel( PANEL_GAMEDLL );
	return root;
}

vgui::VPANEL VGui_GetRootPanel( void )
{
	vgui::VPANEL root = enginevgui->GetPanel( PANEL_ROOT );
	return root;
}


//-----------------------------------------------------------------------------
// Implementation of IToolDictionary
//-----------------------------------------------------------------------------
class CToolDictionary : public CTier3DmAppSystem< IToolDictionary >
{
	typedef CTier3DmAppSystem< IToolDictionary > BaseClass;

public:
	CToolDictionary();

	// Inherited from IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from IToolDictionary
	virtual void CreateTools();
	virtual int	GetToolCount() const;
	virtual IToolSystem	*GetTool( int index );

public:
	void RegisterTool( IToolSystem *tool );

private:
	CUtlVector< IToolSystem	* >	m_Tools;
};


//-----------------------------------------------------------------------------
// Singleton interface for tools 
//-----------------------------------------------------------------------------
static CToolDictionary g_ToolDictionary;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CToolDictionary, IToolDictionary, VTOOLDICTIONARY_INTERFACE_VERSION, g_ToolDictionary );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolDictionary::CToolDictionary()
{
}


//-----------------------------------------------------------------------------
// Inherited from IAppSystem
//-----------------------------------------------------------------------------
bool CToolDictionary::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	// FIXME: This interface pointer is taken care of in tier2 + tier1
	g_pFileSystem = g_pFullFileSystem;

	enginevgui = ( IEngineVGui * )factory( VENGINE_VGUI_VERSION, NULL );
	enginetools = ( IEngineTool * )factory( VENGINETOOL_INTERFACE_VERSION, NULL );
	debugoverlay = ( IVDebugOverlay * )factory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL );

	if ( !enginevgui || !debugoverlay || !g_pCVar || !enginetools || !g_pFileSystem  )
		return false;

	if ( !VGui_Startup( factory ) )
		return false;

	return ConnectTools( factory );
}

void CToolDictionary::Disconnect()
{
	DisconnectTools();
	enginevgui = NULL;
	enginetools = NULL;
	debugoverlay = NULL;
	g_pFileSystem = NULL;

	BaseClass::Disconnect( );
}

void *CToolDictionary::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, VTOOLDICTIONARY_INTERFACE_VERSION ) )
		return (IToolDictionary*)this;

	return NULL;
}

InitReturnVal_t CToolDictionary::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// Init registry
	if ( !registry->Init( "Source\\Tools" ) )
	{
		Warning( "registry->Init failed\n" );
		return INIT_FAILED;
	}

	// Re-enable this and VGui_Shutdown if we create root tool panels
//	VGui_PostInit();

	return INIT_OK;
}

void CToolDictionary::Shutdown()
{
	// Re-enable this and VGui_PostInit if we create root tool panels
	VGui_Shutdown();

	registry->Shutdown();

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Implementation of IToolDictionary methods
//-----------------------------------------------------------------------------
void CToolDictionary::CreateTools()
{
	::CreateTools( );
}

int	CToolDictionary::GetToolCount() const
{
	return m_Tools.Count();
}

IToolSystem	*CToolDictionary::GetTool( int index )
{
	if ( index < 0 || index >= m_Tools.Count() )
	{
		return NULL;
	}
	return m_Tools[ index ];
}

void CToolDictionary::RegisterTool( IToolSystem *tool )
{
	m_Tools.AddToTail( tool );
}


//-----------------------------------------------------------------------------
// Allows tools to install themselves into the dictionary 
//-----------------------------------------------------------------------------
void RegisterTool( IToolSystem *tool )
{
	g_ToolDictionary.RegisterTool( tool );
}


