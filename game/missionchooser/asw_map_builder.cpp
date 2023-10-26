#include "convar.h"
#include "asw_map_builder.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include "missionchooser/iasw_random_missions.h"
#include "filesystem.h"
#include "threadtools.h"
#include "KeyValues.h"
#include "asw_mission_chooser.h"
#ifdef SUPPORT_VBSP_2
#include "vbsp2lib/serializesimplebspfile.h"
#include "vbsp2lib/simplemapfile.h"
#include "vbsp2lib/simplebspfile.h"
#endif
#include "vstdlib/random.h"
#include "tilegen_core.h"
#include "MapLayout.h"
#include "layout_system/tilegen_layout_system.h"
#include "LevelTheme.h"
#include "VMFExporter.h"
#include "Room.h"
#include "cdll_int.h"

// includes needed for the creating of a new process and handling its output
// ASW TODO: Handle Linux/Xbox way of doing this
#pragma warning( disable : 4005 )
#include <windows.h>
#include <iostream>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// @TODO: Nuke all of the switches for VBSP1 vs VBSP2 and old mission system vs new mission system.  Move level generation to the background thread.

static ConVar asw_vbsp2( "asw_vbsp2", "0", FCVAR_REPLICATED ); // 0 = Use default map builder (VBSP.EXE), 1 = Use new, experimental level builder (VBSP2LIB.LIB)
static ConVar tilegen_retry_count( "tilegen_retry_count", "20", FCVAR_CHEAT, "The number of level generation retries to attempt after which tilegen will give up." );
ConVar asw_regular_floor_texture( "asw_regular_floor_texture", "REGULAR_FLOOR", FCVAR_NONE, "Regular floor texture to replace" );
ConVar asw_alien_floor_texture( "asw_alien_floor_texture", "ALIEN_FLOOR", FCVAR_NONE, "Alien floor texture used for replacement" );

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_TilegenGeneral, "TilegenGeneral" );

extern IVEngineClient *engine;

static const int g_ProgressAmounts[] = 
{
	0,		// Not yet started
	1,		// Initialized
	15,		// Map loaded
	30,		// Instances resolved
	70,		// BSP created
	100,	// BSP saved, all done.
};

static const char *g_ProgressLabels[] = 
{
	"Initializing...",
	"Loading map...",
	"Resolving Instances...",
	"Creating BSP...",
	"Saving BSP...",
	"Done!"
};

enum MapBuilderCommand_t
{
	MBC_PROCESS_MAP = 0,
	MBC_SHUTDOWN = 1
};

#ifdef SUPPORT_VBSP_2
// Callback passed in to CSimpleMapFile::ResolveInstances to perform fix-up on instanced maps before the BSP process.
static void FixupInstance( void *pContext, CSimpleMapFile *pInstanceMapFile, MapEntityKeyValuePair_t *pFuncInstanceKeyValuePairs, int nNumKeyValuePairs );
#endif

//-----------------------------------------------------------------------------
// Worker thread implementation to handle building maps.
//-----------------------------------------------------------------------------
class CMapBuilderWorkerThread : public CWorkerThread
{
public:
	CMapBuilderWorkerThread( CASW_Map_Builder *pMapBuilder ) :
	m_pMapBuilder( pMapBuilder )
	{
	}

	virtual int Run()
	{
		while ( true )
		{
			WaitForCall();

			if ( GetCallParam() == MBC_SHUTDOWN )
			{
				// exit cleanly
				return 0;
			}
			else
			{
#ifdef SUPPORT_VBSP_2
				char filename[MAX_PATH];
				// Safe to access m_szVBSP2MapName because it will not be changed by the main thread while this is happening.
				Q_snprintf( filename, sizeof( filename ), "maps\\%s", m_pMapBuilder->m_szVBSP2MapName );
				CSimpleMapFile *pSimpleMapFile;
				m_pMapBuilder->m_nVBSP2Progress = g_ProgressAmounts[1];
				CSimpleMapFile::LoadFromFile( g_pFullFileSystem, filename, &pSimpleMapFile );
				m_pMapBuilder->m_nVBSP2Progress = g_ProgressAmounts[2];
				pSimpleMapFile->ResolveInstances( CSimpleMapFile::CONVERT_STRUCTURAL_TO_DETAIL, FixupInstance, m_pMapBuilder );
				m_pMapBuilder->m_nVBSP2Progress = g_ProgressAmounts[3];

				// mess with the pSimpleMapFile here
				/*
				CMapLayout *pLayout = m_pMapBuilder->GetCurrentlyBuildingMapLayout();
				
				// find the alien floor texture
				int iBrushes = pSimpleMapFile->GetBrushCount();
				const MapBrush_t *pBrushes = pSimpleMapFile->GetBrushes();
				const MapBrushSide_t *pBrushSides = pSimpleMapFile->GetBrushSides();
				MapTextureInfo_t *pTextureInfos = pSimpleMapFile->GetTextureInfos();
				MapTextureData_t *pTextureDatas = pSimpleMapFile->GetTextureData();
				const MapTextureInfo_t *pAlienFloorTextureInfo = NULL;			// a texture info that's using the alien floor material
				for ( int i = 0; i < iBrushes; i++ )
				{
					for ( int side = pBrushes[i].m_nFirstSideIndex; side < pBrushes[i].m_nFirstSideIndex + pBrushes[i].m_nNumSides; side++ )
					{
						const MapTextureInfo_t *pTextureInfo = &pTextureInfos[ pBrushSides[ side ].m_nTextureInfoIndex ];
						const MapTextureData_t *pTextureData = &pTextureDatas[ pTextureInfo->m_nTextureDataIndex ];
						if ( !Q_stricmp( pTextureData->m_MaterialName, asw_alien_floor_texture.GetString() ) )
						{
							pAlienFloorTextureInfo = pTextureInfo;
							break;
						}
					}
				}

				if ( pAlienFloorTextureInfo )
				{
					// now find all sides using the regular floor texture
					for ( int i = 0; i < iBrushes; i++ )
					{
						// is this brush in an encounter room? - just check the center for now
						Vector vecCenter = ( pBrushes[i].m_vMinBounds + pBrushes[i].m_vMaxBounds ) * 0.5f;
						CRoom *pRoom = pLayout->GetRoom( vecCenter );
						if ( !pRoom || !pRoom->HasAlienEncounter() )
							continue;

						for ( int side = pBrushes[i].m_nFirstSideIndex; side < pBrushes[i].m_nFirstSideIndex + pBrushes[i].m_nNumSides; side++ )
						{
							MapTextureInfo_t *pTextureInfo = &pTextureInfos[ pBrushSides[ side ].m_nTextureInfoIndex ];
							MapTextureData_t *pTextureData = &pTextureDatas[ pTextureInfo->m_nTextureDataIndex ];
							if ( !Q_stricmp( pTextureData->m_MaterialName, asw_regular_floor_texture.GetString() ) )
							{
								// switch regular floor over to using the new texture index
								pTextureInfo->m_nTextureDataIndex = pAlienFloorTextureInfo->m_nTextureDataIndex;
							}
						}
					}
				}
				else
				{
					Warning( "Couldn't find alien floor texture in map\n" );
				}
				*/

				// re-resolve if necessary
				//pSimpleMapFile->ResolveInstances( CSimpleMapFile::CONVERT_STRUCTURAL_TO_DETAIL, NULL, NULL );

				char bspFilename[MAX_PATH];
				Q_strncpy( bspFilename, filename, MAX_PATH );
				Q_SetExtension( bspFilename, ".bsp", MAX_PATH );
				CUtlStreamBuffer outputBSPFile( bspFilename, NULL );
				CSimpleBSPFile *pBSPFile = new CSimpleBSPFile();
				pBSPFile->CreateFromMapFile( pSimpleMapFile );
				m_pMapBuilder->m_nVBSP2Progress = g_ProgressAmounts[4];
				SaveToFile( &outputBSPFile, pBSPFile );
				outputBSPFile.Close();
				delete pBSPFile;
				delete pSimpleMapFile;

				// Commit all reads/writes before ending task
				ThreadMemoryBarrier();
				m_pMapBuilder->m_nVBSP2Progress = g_ProgressAmounts[5];

				Reply( 0 );
#else
				return 0;
#endif
			}
		}
		return 0;
	}

private:
	CASW_Map_Builder *m_pMapBuilder;
};

CASW_Map_Builder::CASW_Map_Builder() :
m_flStartProcessingTime( 0.0f ),
m_iBuildStage( STAGE_NONE ),
m_bStartedGeneration( false ),
m_flProgress( 0.0f ),
m_pGeneratedMapLayout( NULL ),
m_pBuildingMapLayout( NULL ),
m_pLayoutSystem( NULL ),
m_nLevelGenerationRetryCount( 0 ),
m_pMissionSettings( NULL ),
m_pMissionDefinition( NULL ),
m_pWorkerThread( NULL )
{
	m_szLayoutName[0] = '\0';
	m_iCurrentBuildSearch = 0;
	m_bRunningProcess = false;
	m_bFinishedExecution = false;	
	
	Q_snprintf( m_szStatusMessage, sizeof( m_szStatusMessage ), "Generating map..." );

	m_pMapBuilderOptions = new KeyValues( "map_builder_options" );
	m_pMapBuilderOptions->LoadFromFile( g_pFullFileSystem, "resource/map_builder_options.txt", "GAME" );

	m_pWorkerThread = new CMapBuilderWorkerThread( this );
	m_pWorkerThread->Start();
}

CASW_Map_Builder::~CASW_Map_Builder()
{
	m_pMapBuilderOptions->deleteThis();

	delete m_pGeneratedMapLayout;
	delete m_pBuildingMapLayout;
	delete m_pLayoutSystem;

	// Tell the worker thread to shutdown and block until finished
	m_pWorkerThread->CallWorker( MBC_SHUTDOWN );
	delete m_pWorkerThread;
}

void CASW_Map_Builder::Update( float flEngineTime )
{
	if ( m_bRunningProcess )
	{
		ProcessExecution();
	}
	else if ( m_iBuildStage == STAGE_MAP_BUILD_SCHEDULED )
	{
		if ( m_flStartProcessingTime < flEngineTime )
		{
			BuildMap();
		}
	}
	else if ( m_iBuildStage == STAGE_VBSP2 )
	{
		UpdateVBSP2Progress();
	}
	else if ( m_iBuildStage == STAGE_GENERATE )
	{
		if ( m_flStartProcessingTime < flEngineTime )
		{
			if ( !m_bStartedGeneration )
			{
				delete m_pGeneratedMapLayout;
				delete m_pLayoutSystem;
				m_pLayoutSystem = new CLayoutSystem();
				AddListeners( m_pLayoutSystem );
				m_pGeneratedMapLayout = new CMapLayout( m_pMissionSettings->MakeCopy() );

				if ( !m_pLayoutSystem->LoadFromKeyValues( m_pMissionDefinition ) )
				{
					Log_Warning( LOG_TilegenLayoutSystem, "Failed to load mission from key values definition.\n" );
					m_iBuildStage = STAGE_NONE;
					return;
				}
				m_pLayoutSystem->BeginGeneration( m_pGeneratedMapLayout );
				m_bStartedGeneration = true;
			}
			else
			{
				if ( m_pLayoutSystem->IsGenerating() )
				{
					m_pLayoutSystem->ExecuteIteration();

					// If an error occurred and this map is randomly generated, try again and hope we get a successful layout this time.
					if ( m_pLayoutSystem->GenerationErrorOccurred() )
					{
						if ( m_nLevelGenerationRetryCount < tilegen_retry_count.GetInt() && m_pLayoutSystem->IsRandomlyGenerated() )
						{
							// Error generating layout
							Log_Msg( LOG_TilegenGeneral, "Retrying layout generation...\n" );
							m_pGeneratedMapLayout->Clear();
							m_pLayoutSystem->BeginGeneration( m_pGeneratedMapLayout );
							++ m_nLevelGenerationRetryCount;
						}
						else
						{
							Log_Warning( LOG_TilegenGeneral, "Failed to generate valid map layout after %d tries...\n", tilegen_retry_count.GetInt() );
							m_iBuildStage = STAGE_NONE;
						}
					}
				}
				else
				{
					Log_Msg( LOG_TilegenGeneral, "Map layout generated\n" );
					m_iBuildStage = STAGE_NONE;
					
					char layoutFilename[MAX_PATH];
					Q_snprintf( layoutFilename, MAX_PATH, "maps\\%s", m_szLayoutName );
					m_pGeneratedMapLayout->SaveMapLayout( layoutFilename );

					delete m_pGeneratedMapLayout;
					m_pGeneratedMapLayout = NULL;

					BuildMap();
				}
			}
		}
	}
}

bool CASW_Map_Builder::IsBuildingMission()
{
	return m_iBuildStage != STAGE_NONE;
}

void CASW_Map_Builder::Execute(const char* pszCmd, const char* pszCmdLine)
{
	m_bFinishedExecution = false;
	m_iProcessReturnValue = -1;
	SECURITY_ATTRIBUTES saAttr; 

	// Set the bInheritHandle flag so pipe handles are inherited.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

	// Create a pipe for the child's STDOUT. 
	if(CreatePipe(&m_hChildStdoutRd, &m_hChildStdoutWr, &saAttr, 0))
	{
		if(CreatePipe(&m_hChildStdinRd, &m_hChildStdinWr, &saAttr, 0))
		{
			if (DuplicateHandle(GetCurrentProcess(),m_hChildStdoutWr, GetCurrentProcess(),&m_hChildStderrWr,0, TRUE,DUPLICATE_SAME_ACCESS))
			{
				/* Now create the child process. */ 
				STARTUPINFO si;
				memset(&si, 0, sizeof si);
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESTDHANDLES;
				si.hStdInput = m_hChildStdinRd;
				si.hStdError = m_hChildStderrWr;
				si.hStdOutput = m_hChildStdoutWr;
				PROCESS_INFORMATION pi;
				char cmdbuffer[512];
				Q_snprintf(cmdbuffer, sizeof(cmdbuffer), "%s %s", pszCmd, pszCmdLine);
				Msg("Sending command %s\n", cmdbuffer);
				// run the process from the current game's map directory				
				char dirbuffer[512];
				Q_snprintf(dirbuffer, sizeof(dirbuffer), "%s/maps", engine->GetGameDirectory() );
				Msg("  from directory %s\n", dirbuffer);

				if(CreateProcess(pszCmd, cmdbuffer, NULL, NULL, TRUE, 
					DETACHED_PROCESS | BELOW_NORMAL_PRIORITY_CLASS, NULL, dirbuffer, &si, &pi))
				{
					m_hProcess = pi.hProcess;
					m_bRunningProcess = true;
					m_bFinishedExecution = false;

					// do one process of the execution
					ProcessExecution();					
				}
				else
				{
					Msg("* Could not execute the command:\r\n   %s\r\n", cmdbuffer);
					m_bRunningProcess = false;
					FinishExecution();	// closes all handles
				}								
			}
			else
			{
				// close the 4 handles we've opened so far
				CloseHandle(m_hChildStdinRd);
				CloseHandle(m_hChildStdinWr);
				CloseHandle(m_hChildStdoutRd);
				CloseHandle(m_hChildStdoutWr);
			}
		}
		else
		{
			// close the 2 handles we've opened so far
			CloseHandle(m_hChildStdoutRd);
			CloseHandle(m_hChildStdoutWr);
		}
	}
}

void CASW_Map_Builder::ProcessExecution()
{
	DWORD dwCount = 0;
	DWORD dwRead = 0;

	// read from input handle
	PeekNamedPipe(m_hChildStdoutRd, NULL, NULL, NULL, &dwCount, NULL);
	if (dwCount)
	{
		dwCount = MIN (dwCount, 4096 - 1);
		ReadFile(m_hChildStdoutRd, m_szProcessBuffer, dwCount, &dwRead, NULL);
	}
	if(dwRead)
	{
		m_szProcessBuffer[dwRead] = 0;
		UpdateProgress();
		Msg(m_szProcessBuffer);
	}
	// check process termination
	else if ( WaitForSingleObject(m_hProcess, 1000) != WAIT_TIMEOUT )
	{
		if(m_bFinishedExecution)
		{
			m_iProcessReturnValue = 0;
			FinishExecution();
		}
		else
		{
			m_bFinishedExecution = true;
		}
	}	
}

// called when one of our processes finishes
void CASW_Map_Builder::FinishExecution()
{
	m_bRunningProcess = false;	// next time we get it

	CloseHandle(m_hChildStderrWr);

	CloseHandle(m_hChildStdinRd);
	CloseHandle(m_hChildStdinWr);

	CloseHandle(m_hChildStdoutRd);
	CloseHandle(m_hChildStdoutWr);

	if (m_iBuildStage == STAGE_VBSP && m_iProcessReturnValue == 0)
	{
		char buffer[512];
		Q_snprintf(buffer, sizeof(buffer), "-game ..\\ %s %s", m_pMapBuilderOptions->GetString( "vvis", "" ), m_szLayoutName);	// todo: add code to chop into 256 blocks here
		Execute("bin/vvis.exe", buffer);
		m_iBuildStage = STAGE_VVIS;
	}
	else if (m_iBuildStage == STAGE_VVIS && m_iProcessReturnValue == 0)
	{
		if ( !m_pMapBuilderOptions->FindKey( "vrad", false ) )		// if no vrad key is specified in the map builder options, then skip vrad
		{
			m_iBuildStage = STAGE_NONE;
			Msg("Map Build finished!\n");
			m_flProgress = 1.0f;
			Q_snprintf( m_szStatusMessage, sizeof( m_szStatusMessage ), "Build complete!" );
		}
		else
		{
			char buffer[512];
			Q_snprintf(buffer, sizeof(buffer), "-low -game ..\\ %s %s", m_pMapBuilderOptions->GetString( "vrad", "" ), m_szLayoutName);
			Execute("bin/vrad.exe", buffer);
			m_iBuildStage = STAGE_VRAD;
		}
	}
	else if (m_iBuildStage == STAGE_VRAD && m_iProcessReturnValue == 0)
	{
		m_iBuildStage = STAGE_NONE;
		Msg("Map Build finished!\n");
		m_flProgress = 1.0f;
		Q_snprintf( m_szStatusMessage, sizeof( m_szStatusMessage ), "Build complete!" );
	}
	if (m_iProcessReturnValue == -1)
	{
		//Msg("Map Build error\n");
	}
}

// search terms used to work out how far through the build we are
static char const *s_szProgressTerms[]={
	"Valve Software - vbsp.exe",
	"ProcessBlock_Thread:",
	"Processing areas",
	"WriteBSP...",	
	"Displacement Alpha",
	"Building Physics collision data",
	"Valve Software - vvis.exe",
	"BasePortalVis:",
	"PortalFlow:",
	"Valve Software - vrad.exe",
	"BuildFacelights:",
	"FinalLightFace:",
	"ThreadComputeLeafAmbient:",
	"Computing static prop lighting"
};

static char const *s_szStatusLabels[]={
	"Creating BSP...",
	"Creating BSP...",
	"Creating BSP...",
	"Creating BSP...",
	"Creating BSP...",
	"Creating BSP...",
	"Calculating visibility...",
	"Calculating visibility...",
	"Calculating visibility...",
	"Calculating lighting...",
	"Calculating lighting...",
	"Calculating lighting...",
	"Calculating lighting...",
	"Calculating prop lighting...",
};

// monitor output from our process to determine which part of the build we're in
void CASW_Map_Builder::UpdateProgress()
{
	// copy the new chars into our buffer
	int newcharslen = Q_strlen(m_szProcessBuffer);
	for (int i=0;i<newcharslen;i++)
	{
		m_szOutputBuffer[m_iOutputBufferPos++] = m_szProcessBuffer[i];
		// if we go over the end of our output buffer, then shift everything back by half the buffer and continue
		if (m_iOutputBufferPos >= MAP_BUILD_OUTPUT_BUFFER_SIZE)
		{
			for (int k=0;k<MAP_BUILD_OUTPUT_BUFFER_HALF_SIZE;k++)
			{
				m_szOutputBuffer[k] = m_szOutputBuffer[k + MAP_BUILD_OUTPUT_BUFFER_HALF_SIZE];
			}
			m_iOutputBufferPos = MAP_BUILD_OUTPUT_BUFFER_HALF_SIZE;
		}
	}

	// now scan our buffer for progress messages in reverse order
	int iNumSearch = NELEMS(s_szProgressTerms);
	for (int iSearch=iNumSearch-1;iSearch>m_iCurrentBuildSearch;iSearch--)
	{
		char *pos = Q_strstr(m_szOutputBuffer, s_szProgressTerms[iSearch]);
		if ( pos )
		{
			//Msg("Output (%s) matched (%s) result %s at %d\n", m_szOutputBuffer, s_szProgressTerms[iSearch], pos, pos - m_szOutputBuffer);
			m_iCurrentBuildSearch = iSearch;
			m_flProgress = float(iSearch) / float (iNumSearch);
			if (Q_strlen(s_szStatusLabels[iSearch]) > 0)
			{
				Q_snprintf( m_szStatusMessage, sizeof(m_szStatusMessage), "%s", s_szStatusLabels[iSearch] );

			}
			break;
		}
	}
}

// schedules a map to be compiled
void CASW_Map_Builder::ScheduleMapBuild(const char* pszMap, const float fTime)
{
	if ( m_iBuildStage != STAGE_NONE )
	{
		Log_Warning( LOG_TilegenGeneral, "Map builder is currently busy, ignoring request to schedule map build for map '%s'", pszMap );
		return;
	}

	Q_strncpy( m_szLayoutName, Q_UnqualifiedFileName( pszMap ), _countof( m_szLayoutName ) );
	Q_SetExtension( m_szLayoutName, "layout", _countof( m_szLayoutName ) );
	m_flStartProcessingTime = fTime;
	m_bStartedGeneration = false;
	m_iBuildStage = STAGE_MAP_BUILD_SCHEDULED;
	m_flProgress = 0.0f;

	Q_snprintf( m_szStatusMessage, sizeof( m_szStatusMessage ), "Generating map..." );
}

// schedules a map to be randomly generated
void CASW_Map_Builder::ScheduleMapGeneration( const char* pszMap, const float fTime, KeyValues *pMissionSettings, KeyValues *pMissionDefinition )
{
	if ( m_iBuildStage != STAGE_NONE )
	{
		Log_Warning( LOG_TilegenGeneral, "Map builder is currently busy, ignoring request to schedule map generation for map '%s'", pszMap );
		return;
	}

	Q_strncpy( m_szLayoutName, Q_UnqualifiedFileName( pszMap ), _countof( m_szLayoutName ) );
	Q_SetExtension( m_szLayoutName, "layout", _countof( m_szLayoutName ) );
	m_pMissionSettings = pMissionSettings;
	m_pMissionDefinition = pMissionDefinition;
	m_flStartProcessingTime = fTime;
	m_iBuildStage = STAGE_GENERATE;
	m_bStartedGeneration = false;
	m_nLevelGenerationRetryCount = 0;
	m_flProgress = 0.0f;

	Q_snprintf( m_szStatusMessage, sizeof( m_szStatusMessage ), "Generating map..." );
}

// Builds a map from a .layout file
void CASW_Map_Builder::BuildMap()
{
	char layoutFilename[MAX_PATH];
	char vmfFilename[MAX_PATH];
	
	Q_snprintf( layoutFilename, MAX_PATH, "maps\\%s", m_szLayoutName );
	Q_strncpy( vmfFilename, m_szLayoutName, MAX_PATH );
	Q_SetExtension( vmfFilename, "vmf", MAX_PATH );

	Log_Msg( LOG_TilegenGeneral, "Building map from layout: %s, emitting map file: %s\n", layoutFilename, vmfFilename );

	// Make sure our themes are loaded
	CLevelTheme::LoadLevelThemes();

	// Load the .layout from disk
	// @TODO: keep this in memory and avoid the round-trip
	delete m_pBuildingMapLayout;
	m_pBuildingMapLayout = new CMapLayout();
	if ( !m_pBuildingMapLayout->LoadMapLayout( layoutFilename ) )
	{
		delete m_pBuildingMapLayout;
		m_pBuildingMapLayout = NULL;
		return;
	}

	// Export it to VMF
	VMFExporter *pExporter = new VMFExporter();
	bool bSuccess = pExporter->ExportVMF( m_pBuildingMapLayout, m_szLayoutName );
	delete pExporter;

	if ( !bSuccess )
	{
		Log_Warning( LOG_TilegenGeneral, "Failed to create VMF from layout '%s'.\n", m_szLayoutName );
		delete m_pBuildingMapLayout;
		m_pBuildingMapLayout = NULL;
	}
	
	if ( asw_vbsp2.GetInt() )
	{
		m_iBuildStage = STAGE_VBSP2;
		m_nVBSP2Progress = 0;
		Q_strncpy( m_szVBSP2MapName, vmfFilename, MAX_PATH );
		// Guarantee all reads/writes committed before kicking off the thread.  Probably unnecessary in practice due to lag between operations, but whatever...
		ThreadMemoryBarrier();

		// Call with a 0 ms timeout to return immediately
		m_pWorkerThread->CallWorker( MBC_PROCESS_MAP, 0 );
	}
	else
	{
		// Building map layout is ignored in VBSP1 codepath
		delete m_pBuildingMapLayout;
		m_pBuildingMapLayout = NULL;

		m_iBuildStage = STAGE_VBSP;

		char buffer[512];
		Q_snprintf( buffer, sizeof(buffer), "-game ..\\ %s %s", m_pMapBuilderOptions->GetString( "vbsp", "" ), vmfFilename );
		Execute( "bin/vbsp.exe", buffer );

		m_iCurrentBuildSearch = 0;
		m_iOutputBufferPos = 0;
		Q_memset( &m_szOutputBuffer, 0, sizeof( m_szOutputBuffer ) );
	}
}

void CASW_Map_Builder::UpdateVBSP2Progress()
{
	// Make sure any reads/writes are committed before reading progress from the background thread.
	ThreadMemoryBarrier();
	
	int nProgress = m_nVBSP2Progress;
	m_flProgress = ( nProgress == 100 ) ? 1.0f : (float) nProgress / 100.0f;
	
	int nNumProgressLevels = _countof( g_ProgressAmounts );
	for ( int i = nNumProgressLevels - 1; i >= 0; -- i )
	{
		if ( nProgress >= g_ProgressAmounts[i] )
		{
			Q_strncpy( m_szStatusMessage, g_ProgressLabels[i], _countof( m_szStatusMessage ) );
			break;
		}
	}
	if ( nProgress == 100 )
	{
		delete m_pBuildingMapLayout;
		m_pBuildingMapLayout = NULL;
		m_iBuildStage = STAGE_NONE;
	}
}


static CUniformRandomStream s_Random;

#ifdef SUPPORT_VBSP_2
static void AddFuncInstance( CSimpleMapFile *pInstanceMapFile, CInstanceSpawn *pInstanceSpawn, const Vector &vPosition )
{
	CUtlVector< MapEntityKeyValuePair_t > replacePairs;
	replacePairs.AddMultipleToTail( pInstanceSpawn->GetAdditionalKeyValueCount() );
	for ( int i = 0; i < pInstanceSpawn->GetAdditionalKeyValueCount(); ++ i )
	{
		replacePairs[i].m_pKey = pInstanceSpawn->GetAdditionalKeyValues()[i].m_Key;
		replacePairs[i].m_pValue = pInstanceSpawn->GetAdditionalKeyValues()[i].m_Value;
	}
	pInstanceMapFile->AddFuncInstance( pInstanceSpawn->GetInstanceFilename(), QAngle( 0, 0, 0 ), vPosition, replacePairs.Base(), replacePairs.Count() );
}

// Sup dawg, I herd u like instances in yo instances...
static void FixupInstance( void *pContext, CSimpleMapFile *pInstanceMapFile, MapEntityKeyValuePair_t *pFuncInstanceKeyValuePairs, int nNumKeyValuePairs )
{
	CASW_Map_Builder *pMapBuilder = ( CASW_Map_Builder * )pContext;
	CMapLayout *pMapLayout = pMapBuilder->GetCurrentlyBuildingMapLayout();

	int nPlacedRoomIndex = 0;
	MapEntityKeyValuePair_t *pPair = FindPair( "PlacedRoomIndex", pFuncInstanceKeyValuePairs, nNumKeyValuePairs );
	if ( pPair != NULL )
	{
		nPlacedRoomIndex = atoi( pPair->m_pValue );
	}
	else
	{
		// Only fix up placed room instances
		return;
	}

	CUtlVector< Vector > infoNodeLocations;

	// Populate the info node list only once
	// Technically we don't need to do this if we don't acutally have instances to spawn in this instance
	int nIndex = 0;
	while ( ( nIndex = pInstanceMapFile->FindEntity( "info_node", NULL, NULL, nIndex ) ) != -1 )
	{
		const MapEntity_t *pEntity = &pInstanceMapFile->GetEntities()[nIndex];
		infoNodeLocations.AddToTail( pEntity->m_vOrigin );
		++ nIndex;
	}
	
	if ( infoNodeLocations.Count() == 0 )
	{
		// No places to spawn instances in this instance
		return;
	}

	for ( int i = 0; i < pMapLayout->m_InstanceSpawns.Count(); ++ i )
	{
		CInstanceSpawn *pInstanceSpawn = &pMapLayout->m_InstanceSpawns[i];
		if ( pInstanceSpawn->GetPlacedRoomIndex() == nPlacedRoomIndex )
		{
			if ( pInstanceSpawn->GetInstanceSpawningMethod() == ISM_ADD_AT_RANDOM_NODE )
			{
				AddFuncInstance( pInstanceMapFile, pInstanceSpawn, infoNodeLocations[pInstanceSpawn->GetRandomSeed() % infoNodeLocations.Count()] );
			}
		}
	}
}
#endif