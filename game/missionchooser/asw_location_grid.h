#ifndef _INCLUDED_ASW_LOCATION_GRID_H
#define _INCLUDED_ASW_LOCATION_GRID_H

#define ASW_RANDOM_MISSION_GRID_WIDE 16
#define ASW_RANDOM_MISSION_GRID_TALL 10

class KeyValues;

#include "missionchooser/iasw_mission_chooser.h"
#include "utlvector.h"
#include "color.h"
#include "vstdlib/random.h"

class CASW_KeyValuesDatabase;
class CASW_Location;
class CASW_Location_Grid;
class CTilegenMissionPreprocessor;

class CASW_Location_Group : public IASW_Location_Group
{
public:
	CASW_Location_Group( CASW_Location_Grid *pLocationGrid );
	~CASW_Location_Group();

	virtual const char* GetGroupName() { return m_szGroupName; }
	virtual const char* GetTitleText() { return m_szTitleText; }
	virtual const char* GetDescriptionText() { return m_szDescriptionText; }
	virtual const char* GetImageName() { return m_szImageName; }
	virtual Color& GetColor() { return m_Color; };
	virtual bool IsGroupLocked( CUtlVector<int> &completedMissions );

	KeyValues* GetKeyValuesForEditor();
	void LoadFromKeyValues( KeyValues *pKeys );		// random stream is used to pick difficulty between bounds and specific mission spec

	virtual int GetHighestUnlockMissionID();
	int GetNumLocations() { return m_Locations.Count(); }
	IASW_Location* GetLocation( int iIndex );
	CASW_Location* GetCLocation( int iIndex );

	const char *m_szGroupName;
	const char *m_szTitleText;
	const char *m_szDescriptionText;
	const char *m_szImageName;
	CUtlVector<int> m_UnlockedBy;		// IDs of missions that unlock this group	
	int m_iRequiredUnlocks;
	Color m_Color;		// background color for locations in this group
	CUniformRandomStream m_Random;

	CUtlVector< CASW_Location* > m_Locations;
	CASW_Location_Grid *m_pLocationGrid;
};

class CASW_Reward : public IASW_Reward
{
public:
	CASW_Reward();
	~CASW_Reward();

	virtual ASW_Reward_Type GetRewardType() { return m_RewardType; }
	virtual int GetRewardAmount() { return m_iRewardAmount; }

	// for item rewards
	virtual const char* GetRewardName() { return m_szRewardName; }
	virtual int GetRewardLevel() { return m_iRewardLevel; }
	virtual int GetRewardQuality() { return m_iRewardQuality; }	

	virtual bool LoadFromKeyValues( KeyValues *pKeys, int iMissionDifficulty );
	virtual KeyValues* GetKeyValuesForEditor();

	ASW_Reward_Type m_RewardType;
	int m_iRewardAmount;
	const char *m_szRewardName;
	int m_iRewardLevel;
	int m_iRewardQuality;
};

class CASW_Location : public IASW_Location
{
public:
	CASW_Location( CASW_Location_Grid *pLocationGrid );
	~CASW_Location();

	virtual int GetID() { return m_iLocationID; }
	virtual IASW_Location_Group* GetGroup() { return m_pGroup; }
	virtual CASW_Location_Group* GetCGroup() { return m_pGroup; }
	virtual int GetDifficulty() { return m_iDifficulty; }
	virtual bool GetCompleted() { return m_bCompleted; }
	virtual int GetXPos() { return m_iXPos; }
	virtual int GetYPos() { return m_iYPos; }
	virtual KeyValues* GetMissionSettings();
	virtual KeyValues* GetMissionDefinition();
	
	virtual const char* GetMapName() { return m_szMapName; }
	virtual const char* GetStoryScene( void ) { return m_szStoryScene; }
	virtual const char* GetImageName() { return m_szImageName; }
	virtual const char* GetCompanyName( void );
	virtual const char* GetCompanyImage( void );
	virtual int GetNumRewards();
	virtual IASW_Reward* GetReward( int iRewardIndex );
	virtual int GetMoneyReward();
	virtual int GetXPReward();
	virtual int IsMissionOptional() { return m_bIsMissionOptional; }
	virtual bool IsLocationLocked( CUtlVector<int> &completedMissions );
	virtual void SetPos( int x, int y ) { m_iXPos = x; m_iYPos = y; }

	KeyValues* GetKeyValuesForEditor();
	void LoadFromKeyValues( KeyValues *pKeys, CUniformRandomStream* pStream );		// random stream is used to pick difficulty between bounds and specific mission spec

	char m_szMapName[64];
	int m_iLocationID;
	int m_iDifficulty;
	int m_iMinDifficulty;	// bounds for randomly picking a mission txt
	int m_iMaxDifficulty;
	const char *m_szStoryScene;
	const char *m_szImageName;
	CASW_Location_Group *m_pGroup;
	bool m_bCompleted;
	int m_iXPos;
	int m_iYPos;
	int m_iCompanyIndex;
	bool m_bIsMissionOptional;
	KeyValues *m_pMissionKV;
	char *m_pszCustomMission;
	CASW_Location_Grid *m_pLocationGrid;
	CUtlVector<CASW_Reward*> m_Rewards;
};

// stores a grid of possible missions the players can play
class CASW_Location_Grid : public IASW_Location_Grid
{
public:
	CASW_Location_Grid();
	virtual ~CASW_Location_Grid();

	bool LoadLocationGrid();		
	bool SaveLocationGrid();	

	virtual int GetNumGroups() { return m_Groups.Count(); }
	virtual IASW_Location_Group* GetGroup( int iIndex );
	virtual CASW_Location_Group* GetCGroup( int iIndex );
	virtual CASW_Location_Group* GetGroupByName( const char *szName );

	void DeleteGroup( int iIndex );
	void CreateNewGroup();

	virtual IASW_Location* GetLocationByID( int iLocationID );
	virtual void SetLocationComplete( int iLocationID );
	int GetFreeLocationID();

	KeyValues *GetMissionData( const char *pMissionName );

private:
	CUtlVector<CASW_Location_Group*> m_Groups;
	
	CASW_KeyValuesDatabase *m_pMissionDatabase;
	CTilegenMissionPreprocessor *m_pPreprocessor;
};

CASW_Location_Grid *ASWMissionGrid();

#endif // _INCLUDED_ASW_LOCATION_GRID_H
