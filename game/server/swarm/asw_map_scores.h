#ifndef _INCLUDED_ASW_MAP_SCORES_H
#define _INCLUDED_ASW_MAP_SCORES_H
#ifdef _WIN32
#pragma once
#endif

class KeyValues;

// holds data about each map played on the server (such as best times, modes unlocked, etc.)

class CASW_Map_Scores
{	
public:
	CASW_Map_Scores();	

	// adds an entry for a particular map
	void OnMapCompleted(const char *szMapName, int iSkill, float fTime, int iKills, int iUnlockMode);

	// reports previously entered data
	bool IsModeUnlocked(const char *szMapName, int iSkill, int iGameMode);
	float GetBestTime(const char *szMapName, int iSkill);
	int GetBestKills(const char *szMapName, int iSkill);

private:
	void LoadMapScores();
	bool SaveMapScores();

	KeyValues* GetSubkeyForMap(const char *szMapName);
	const char* GetBestKillsKeyName(int iSkill);
	const char* GetBestTimeKeyName(int iSkill);
	const char* GetVariationsKeyName(int iSkill);

	KeyValues* m_pScoreKeyValues;
};

CASW_Map_Scores* MapScores();

#endif // _INCLUDED_ASW_MAP_SCORES_H