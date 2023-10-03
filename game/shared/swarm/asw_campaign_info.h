#ifndef _INCLUDED_ASW_CAMPAIGN_INFO_H
#define _INCLUDED_ASW_CAMPAIGN_INFO_H
#ifdef _WIN32
#pragma once
#endif

// maximum number of missions that can be in a campaign
#define ASW_MAX_CAMPAIGN_MISSIONS 255

class KeyValues;

// this class contains information about the current campaign
class CASW_Campaign_Info
{
public:
	CASW_Campaign_Info();
	virtual ~CASW_Campaign_Info();

	// Initialises all campaign info from a keyvalues file
	bool LoadCampaign(const char *szCampaignName);
	void ClearCampaign();

	// adds a one way link from start mission to end mission
	void AddMissionLink(int iStartMission, int iEndMission);

	// name given to this campaign
	string_t	m_CampaignName;

	// intro/outro map names
	string_t	m_IntroMap;
	string_t	m_OutroMap;

	// texture used behind the campaign dots and links
	string_t	m_CampaignTextureName;
	string_t	m_CampaignTextureLayer1;
	string_t	m_CampaignTextureLayer2;
	string_t	m_CampaignTextureLayer3;
	string_t	m_CustomCreditsFile;
	int m_iGalaxyX;
	int m_iGalaxyY;

	int m_iSearchLightX[4];
	int m_iSearchLightY[4];
	int m_iSearchLightAngle[4];

	void GetGalaxyPos(int &x, int &y);

	// details on a particular mission in the campaign
	struct CASW_Campaign_Mission_t
	{
		int m_iMissionIndex;
		string_t	m_MissionName;  // human readable name for this mission
		string_t	m_MapName;		// name of the .bsp file to load for this mission
		int m_iLocationX;			// it's position on the mission texture (number goes from 0 to 1023)
		int m_iLocationY;
		int m_iDifficultyMod;
		CUtlVector<int> m_Links;		// list of mission indices, showing links from this mission to others
		string_t	m_LinksString;		// temp var holding the m_Links in human readable format (from the campaign script file)
		string_t	m_LocationDescription;	// short description of the location like, shown in the upper right of the campaign screen
		string_t	m_Briefing;				// short mission description shown in the briefing box of the campaign screen
		string_t	m_ThreatString;		// string to describe the overall threat level of this mission
		bool		m_bAlwaysVisible;
		bool		m_bNeedsMoreThanOneMarine;
	};

	// Array of missions that make up our campaign.  Entry [0] is our starting point and isn't a proper mission.
	CASW_Campaign_Mission_t *m_pMission[ASW_MAX_CAMPAIGN_MISSIONS];
	CASW_Campaign_Mission_t* GetMission(int iMissionIndex);
	CASW_Campaign_Mission_t* GetMissionByMissionName(const char *szMissionName);
	CASW_Campaign_Mission_t* GetMissionByMapName(const char *szMissionName);

	int m_iNumMissions;
	int GetNumMissions() { return m_iNumMissions; }

	bool AreMissionsLinked(int i, int j);	

	void DebugInfo();

	KeyValues *m_CampaignKeyValues;

	bool IsJacobCampaign();
	char m_szCampaignFilename[64];
};

#endif // _INCLUDED_ASW_CAMPAIGN_INFO_H