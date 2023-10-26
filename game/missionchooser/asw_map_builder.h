#ifndef _INCLUDED_ASW_MAP_BUILDER_H
#define _INCLUDED_ASW_MAP_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_map_builder.h"

class KeyValues;
class CMapLayout;
class CLayoutSystem;
class CMapBuilderWorkerThread;

enum MapBuildStage
{
	// Shared states
	STAGE_NONE = 0,  // not building a map
	STAGE_MAP_BUILD_SCHEDULED, // map build scheduled but not yet begun
	
	// VBSP1 states
	STAGE_GENERATE, // generating random mission
	STAGE_VBSP, // executing VBSP.EXE (.VMF -> .BSP)
	STAGE_VVIS, // executing VVIS.EXE on .BSP 
	STAGE_VRAD, // executing VRAD.EXE on .BSP
	
	// VBSP2 states
	STAGE_VBSP2, // converts .VMF -> .BSP with vis information
};

static const int MAP_BUILD_OUTPUT_BUFFER_SIZE = 4096;
static const int MAP_BUILD_OUTPUT_BUFFER_HALF_SIZE = 2048;

// this class manages generation and compilation of maps
class CASW_Map_Builder : public IASW_Map_Builder
{
public:
	CASW_Map_Builder();
	~CASW_Map_Builder();

	virtual void Update( float flEngineTime );

	virtual void ScheduleMapGeneration( 
		const char* pszMap, 
		float fTime, 
		KeyValues *pMissionSettings, 
		KeyValues *pMissionDefinition );

	void ScheduleMapBuild(const char* pszMap, const float fTime);

	float GetProgress() { return m_flProgress; }
	const char* GetStatusMessage() { return m_szStatusMessage; }
	const char* GetMapName() { return m_szLayoutName; }
	MapBuildStage GetMapBuildStage() { return m_iBuildStage; }
	bool IsBuildingMission();
	CMapLayout *GetCurrentlyBuildingMapLayout() const { return m_pBuildingMapLayout; }
	
	// A value that ranges from 0 to 100 indicating percentage of map progress complete
	CInterlockedInt m_nVBSP2Progress;
	char m_szVBSP2MapName[MAX_PATH];

private:
	void BuildMap();

	void Execute(const char* pszCmd, const char* pszCmdLine);

	void ProcessExecution();
	void FinishExecution();
	void UpdateProgress();

	// VBSP1 build options
	KeyValues *m_pMapBuilderOptions;

	// VBSP1 stuff, used to parse status from calling external processes (VBSP/VVIS/VRAD)
	int m_iCurrentBuildSearch;
	char m_szProcessBuffer[MAP_BUILD_OUTPUT_BUFFER_SIZE];
	int m_iOutputBufferPos;
	char m_szOutputBuffer[MAP_BUILD_OUTPUT_BUFFER_SIZE];
	bool m_bRunningProcess, m_bFinishedExecution;
	int m_iProcessReturnValue;
	HANDLE m_hChildStdinRd, m_hChildStdinWr, m_hChildStdoutRd, m_hChildStdoutWr, m_hChildStderrWr; 
	HANDLE m_hProcess;

	// Time at which to start map processing (map generation, map build, etc.).
	// Can be set to delay the start of processing.
	float m_flStartProcessingTime;
	// Current state of map builder.
	MapBuildStage m_iBuildStage;
	// Name of the layout file from which the map is built.
	char m_szLayoutName[MAX_PATH];

	// True if map generation has begun (within STAGE_GENERATE), false otherwise
	bool m_bStartedGeneration;
	// Progress/status of map build
	float m_flProgress;
	char m_szStatusMessage[128];

	// Map layout being generated.
	CMapLayout *m_pGeneratedMapLayout;
	// Map layout of the level being compiled.
	CMapLayout *m_pBuildingMapLayout;
	// Layout system object used to generate map layout.
	CLayoutSystem *m_pLayoutSystem;
	int m_nLevelGenerationRetryCount;
	
	// Auxiliary settings/metadata that affect runtime behavior for a mission.
	KeyValues *m_pMissionSettings;
	// Specification for building the mission layout.
	KeyValues *m_pMissionDefinition;
		
	// Handles updating asynchronous VBSP2 status
	void UpdateVBSP2Progress();

	// Background thread for VBSP2 processing
	CMapBuilderWorkerThread *m_pWorkerThread;
};

#endif // _INCLUDED_ASW_MAP_BUILDER_H
