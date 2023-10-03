#ifndef _INCLUDED_ASW_MISSION_CHOOSER_H
#define _INCLUDED_ASW_MISSION_CHOOSER_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_mission_chooser.h"
#include "tier3/tier3dm.h"

class IASW_Random_Missions;
class IASW_Mission_Chooser_Source;
class CASW_Location_Grid;
class CASW_Spawn_Selection;
class IEngineVGui;

// provides lists of missions, saves and campaigns from the local disk
class CASW_Mission_Chooser : public CTier3AppSystem< IASW_Mission_Chooser > 
{
	typedef CTier3AppSystem< IASW_Mission_Chooser > BaseClass;
public:

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();

public:
	bool GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second);

	virtual IASW_Random_Missions* RandomMissions();
	virtual IASW_Mission_Chooser_Source* LocalMissionSource();
	virtual IASW_Location_Grid* LocationGrid();
	virtual IASW_Mission_Text_Database* MissionTextDatabase();
	virtual IASW_Map_Builder *MapBuilder();
	virtual IASWSpawnSelection *SpawnSelection();
};

CASW_Location_Grid *LocationGrid();
IASW_Random_Missions *RandomMissions();
CASW_Spawn_Selection* SpawnSelection();

extern char	g_gamedir[1024];
extern char	g_layoutsdir[1024];

extern IEngineVGui	*enginevgui;

#endif // _INCLUDED_ASW_MISSION_CHOOSER_H
