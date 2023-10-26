#include "asw_random_missions.h"
#include "asw_mission_chooser_source_local.h"
#include "asw_system.h"
#include "asw_mission_chooser.h"
#include "asw_key_values_database.h"
#include "asw_mission_text_database.h"
#include "asw_location_grid.h"
#include "cdll_int.h"
#include "asw_spawn_selection.h"
#include "tier2/tier2_logging.h"
#include "asw_map_builder.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Random_Missions g_RandomMissions;
CASW_Mission_Chooser_Source_Local g_LocalMissionSource;
CASW_MissionTextDB g_MissionTextDatabase;
CASW_Location_Grid *g_pLocationGrid = NULL;
CASW_Map_Builder *g_pMapBuilder = NULL;
CASW_Spawn_Selection g_SpawnSelection;

char	g_gamedir[1024];
char	g_layoutsdir[1024];

IVEngineClient *engine = NULL;
IEngineVGui	*enginevgui = NULL;
IFileLoggingListener *filelogginglistener = NULL;
static LoggingFileHandle_t s_TilegenLogHandle;
DECLARE_LOGGING_CHANNEL( LOG_TilegenLayoutSystem );

CASW_Location_Grid* LocationGrid()
{
	if ( !g_pLocationGrid )
	{
		g_pLocationGrid = new CASW_Location_Grid();
	}
	return g_pLocationGrid;
}


IASW_Random_Missions* RandomMissions()
{
	return &g_RandomMissions;
}

bool CASW_Mission_Chooser::GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second)
{
	return ASW_System_GetCurrentTimeAndDate(year, month, dayOfWeek, day, hour, minute, second);
}

IASW_Random_Missions* CASW_Mission_Chooser::RandomMissions()
{
	return ::RandomMissions();
}

IASW_Mission_Chooser_Source* CASW_Mission_Chooser::LocalMissionSource()
{
	return &g_LocalMissionSource;
}

IASW_Mission_Text_Database* CASW_Mission_Chooser::MissionTextDatabase()
{
	return &g_MissionTextDatabase;
}

IASW_Location_Grid* CASW_Mission_Chooser::LocationGrid()
{
	return ::LocationGrid();
}

IASW_Map_Builder *CASW_Mission_Chooser::MapBuilder()
{
#ifdef PLATFORM_WINDOWS_PC
	if ( !g_pMapBuilder )
	{
		g_pMapBuilder = new CASW_Map_Builder();
	}
	return g_pMapBuilder;
#else
	Error("The MapBuilder is not supported on this platform!\n");
	return NULL;
#endif
}

IASWSpawnSelection *CASW_Mission_Chooser::SpawnSelection()
{
	return &g_SpawnSelection;
}

CASW_Spawn_Selection* SpawnSelection()
{
	return &g_SpawnSelection;
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CASW_Mission_Chooser::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if( !g_pFullFileSystem )
	{
		Error( "Missionchooser requires the filesystem to run!\n" );
		return false;
	}

	engine = ( IVEngineClient * )factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
	if ( !engine )
	{
		Msg("Failed to load engine\n");
	}

	enginevgui = ( IEngineVGui * )factory( VENGINE_VGUI_VERSION, NULL );
	if ( !enginevgui )
	{
		Msg("Failed to load enginevgui\n");
	}

	if ( (filelogginglistener = (IFileLoggingListener *)factory(FILELOGGINGLISTENER_INTERFACE_VERSION, NULL)) == NULL )
		return INIT_FAILED;

	s_TilegenLogHandle = filelogginglistener->BeginLoggingToFile( "tilegen_log.txt", "w" );
	filelogginglistener->AssignLogChannel( LOG_TilegenLayoutSystem, s_TilegenLogHandle );
	return true;
}


void CASW_Mission_Chooser::Disconnect()
{
	filelogginglistener->EndLoggingToFile( s_TilegenLogHandle );
	BaseClass::Disconnect();
}

InitReturnVal_t CASW_Mission_Chooser::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	Q_MakeAbsolutePath( g_gamedir, sizeof( g_gamedir ), engine->GetGameDirectory() );	
	Q_AppendSlash( g_gamedir, sizeof( g_gamedir ) );

	char tilegendir[MAX_PATH];
	Q_snprintf( tilegendir, sizeof(tilegendir), "%stilegen", g_gamedir );
	//g_pFullFileSystem->AddSearchPath( tilegendir, "TILEGEN", PATH_ADD_TO_HEAD );

	Q_snprintf( g_layoutsdir, sizeof(g_layoutsdir), "%stilegen\\layouts", g_gamedir );
	
	g_MissionTextDatabase.LoadKeyValuesFile( "tilegen/objective_text.txt" );

	// Initialize the spawn selection.
	g_SpawnSelection.Init();

	return INIT_OK;
}

//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CASW_Mission_Chooser::QueryInterface( const char *pInterfaceName )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}

EXPOSE_SINGLE_INTERFACE( CASW_Mission_Chooser, IASW_Mission_Chooser, ASW_MISSION_CHOOSER_VERSION );
