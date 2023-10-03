#ifndef _INCLUDED_CASW_RANDOM_MISSIONS_H
#define _INCLUDED_CASW_RANDOM_MISSIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_random_missions.h"

class CMapLayout;

class CASW_Random_Missions : public IASW_Random_Missions
{
	typedef IASW_Random_Missions  BaseClass;
public:
	CASW_Random_Missions();
	~CASW_Random_Missions();
	
	virtual vgui::Panel* CreateTileGenFrame( vgui::Panel *parent );

	virtual void LevelInitPostEntity( const char *pszMapName );
	virtual bool ValidMapLayout();
	virtual IASW_Room_Details* GetRoomDetails( const Vector &vecPos );
	virtual IASW_Room_Details* GetRoomDetails( int iRoomIndex ) ;
	virtual IASW_Room_Details* GetStartRoomDetails();
	virtual int GetNumRooms();
	virtual void GetMapBounds( Vector *vecWorldMins, Vector *vecWorldMaxs );
	virtual KeyValues* GetGenerationOptions();		// returns the generation options for the currently loaded random map
	virtual int				GetNumEncounters();
	virtual IASW_Encounter* GetEncounter( int i );

	virtual bool CheckAndCleanDirtyLayout( void );

private:
	CMapLayout *m_pCurrentMapLayout;
	
	bool m_bDirtyLayoutForMinimap;
};

#endif // _INCLUDED_CASW_RANDOM_MISSIONS_H
