//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Display a list of achievements for the current game
//
//=============================================================================//

#include "cbase.h"
#include "stats_summary.h"
#include "vgui_controls/Button.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "filesystem.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ImageList.h"
#include "fmtstr.h"
#include "c_cs_playerresource.h"

#include "../../../public/steam/steam_api.h"
#include "achievementmgr.h"
#include "../../../../public/vgui/IScheme.h"
#include "../vgui_controls/ScrollBar.h"
#include "stat_card.h"

#include "weapon_csbase.h"
#include "cs_weapon_parse.h"
#include "buy_presets/buy_presets.h"
#include "win_panel_round.h"
#include "cs_client_gamestats.h"
#include "achievements_cs.h"
#include "ienginevgui.h"


#define STAT_NUM_ARRAY_LENGTH	8

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include <locale.h>

extern CAchievementMgr g_AchievementMgrCS;

const int maxRecentAchievementToDisplay = 10;

/**
 *	public ConvertNumberToMantissaSuffixForm()
 *
 *		Convert a floating point number to a string form incorporating a mantissa and a magnitude suffix (such as "K" for thousands).
 *
 *  Parameters:
 * 		fNum -
 * 		textBuffer - output buffer
 * 		textBufferLen - size of output buffer in characters
 * 		bTrimTrailingZeros - indicates whether to remove trailing zeros of numbers without suffixes (e.g., "32.00000" becomes "32")
 * 		iSignificantDigits - the desired number of significant digits in the result (e.g, "1.23" has 3 significant digits)
 *
 *	Returns:
 *		bool - true on success
 */
bool ConvertNumberToMantissaSuffixForm(float fNum, wchar_t* textBuffer, int textBufferLen, bool bTrimTrailingZeros = true, int iSignificantDigits = 3)
{
	// we need room for at least iSignificantDigits + 3 characters
	Assert(textBufferLen >= iSignificantDigits + 3);

	// find the correct power of 1000 exponent
	Assert(fNum >= 0.0f);
	int iExponent = 0;
	while (fNum >= powf(10.0f, iExponent + 3))
	{
		iExponent += 3;
	}

	// look in the localization table for the matching suffix; fallback to lower suffixes when necessary
	wchar_t *pSuffix = NULL;
	for (; iExponent > 0; iExponent -= 3)
	{
		char localizationToken[64];
		V_snprintf(localizationToken, sizeof(localizationToken), "#GameUI_NumSuffix_E%i", iExponent);
		pSuffix = g_pVGuiLocalize->Find(localizationToken);
		if (pSuffix != NULL)
			break;
	}

	V_snwprintf(textBuffer, textBufferLen, L"%f", fNum / powf(10.0f, iExponent));
	textBuffer[textBufferLen - 1] = L'\0';

	lconv* pLocaleSettings = localeconv();
	wchar_t decimalWChar = *pLocaleSettings->decimal_point;
	wchar_t* pDecimalPos = wcschr(textBuffer, decimalWChar);
	if (pDecimalPos == NULL)
	{
		Msg("ConvertNumberToMantissaSuffixForm(): decimal point not found in string %S for value %f\n", textBuffer, fNum);
		return false;
	}

	// trim trailing zeros
	int iNumCharsInBuffer = wcslen(textBuffer);
	if (pSuffix == NULL && bTrimTrailingZeros)
	{
		wchar_t* pLastChar;
		for (pLastChar = &textBuffer[iNumCharsInBuffer - 1]; pLastChar > pDecimalPos; --pLastChar)
		{
			if (*pLastChar != L'0')	// note that this is checking for '0', not a NULL terminator
				break;

			*pLastChar = L'\0';
		}
		if (pLastChar == pDecimalPos)
			*pLastChar = L'\0';
	}

	// truncate the mantissa to the right of the decimal point so that it doesn't exceed the significant digits
	if (pDecimalPos != NULL && iNumCharsInBuffer > iSignificantDigits + 1)
	{
		if (pDecimalPos - &textBuffer[0] < iSignificantDigits)
			textBuffer[iSignificantDigits + 1] = L'\0'; // truncate at the correct number of significant digits
		else
			*pDecimalPos = L'\0';	// no room for any digits to the right of the decimal point, just truncate it at the "."
	}

	if (pSuffix != NULL)
	{
#ifdef WIN32
		int retVal = V_snwprintf( textBuffer, textBufferLen, L"%s%s", textBuffer, pSuffix );
#else
		int retVal = V_snwprintf( textBuffer, textBufferLen, L"%S%S", textBuffer, pSuffix );
#endif 

		if ( retVal < 0 )
		{
			Msg("ConvertNumberToMantissaSuffixForm(): unable to add suffix %S for value %f (buffer size was %i)\n", pSuffix, fNum, textBufferLen);
			return false;
		}
	}

	return true;
}


CStatsSummary::CStatsSummary(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSAchievementsDialog"),
	m_StatImageMap(DefLessFunc(CSStatType_t))
{
	m_iFixedWidth = 900; // Give this an initial value in order to set a proper size
	SetBounds(0, 0, 900, 780);
	SetMinimumSize(256, 780);

	m_pStatCard = new StatCard(this, "ignored");

	m_pImageList = NULL;
	m_iDefaultWeaponImage = -1;
	m_iDefaultMapImage = -1;

	m_pImagePanelFavWeapon = new ImagePanel(this, "FavoriteWeaponImage");
	m_pImagePanelLastMapFavWeapon = new ImagePanel(this, "FavWeaponIcon");
	m_pImagePanelFavMap = new ImagePanel(this, "FavoriteMapImage");

	//Setup the recent achievement list by adding all achieved achievements to the list
	m_pRecentAchievementsList = new vgui::PanelListPanel(this, "RecentAchievementsListPanel");
	m_pRecentAchievementsList->SetFirstColumnWidth(0);

	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "achievement_earned_local" );

	m_bRecentAchievementsDirty = true;
	m_bStatsDirty = true;
}

CStatsSummary::~CStatsSummary()
{
	if (m_pRecentAchievementsList)
	{
		m_pRecentAchievementsList->DeleteAllItems();
	}

	delete m_pRecentAchievementsList;
	delete m_pImageList;
}

void CStatsSummary::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	LoadControlSettings("Resource/UI/CSStatsSummary.res");

	SetBgColor(Color(86,86,86,255));

	if (m_pImageList)
		delete m_pImageList;
	m_pImageList = new ImageList(false);

	if (m_pImageList)
	{
		//Load defaults
		m_iDefaultWeaponImage = m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/defaultweapon", true));
		m_iDefaultMapImage = m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_generic_map", true));

		//load weapon images
		m_StatImageMap.Insert(CSSTAT_KILLS_DEAGLE, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/deserteagle", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_USP, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/usp45", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_GLOCK, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/glock18", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_P228, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/p228", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_ELITE, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/elites", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_FIVESEVEN, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/fiveseven", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_AWP, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/awp", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_AK47, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/ak47", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_M4A1, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/m4a1", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_AUG, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/aug", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_SG552, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/sg552", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_SG550, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/sg550", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_GALIL, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/galil", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_FAMAS, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/famas", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_SCOUT, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/scout", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_G3SG1, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/g3sg1", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_P90, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/p90", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_MP5NAVY, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/mp5", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_TMP, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/tmp", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_MAC10, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/mac10", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_UMP45, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/ump45", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_M3, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/m3", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_XM1014, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/xm1014", true)));
		m_StatImageMap.Insert(CSSTAT_KILLS_M249, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/fav_weap/m249", true)));

		//load map images
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_ASSAULT, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_assault", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_COMPOUND, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_compound", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_HAVANA, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_havana", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_ITALY, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_italy", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_MILITIA, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_militia", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_CS_OFFICE, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_cs_office", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_AZTEC, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_aztec", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_CBBLE, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_cbble", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_CHATEAU, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_chateau", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_DUST2, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_dust2", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_DUST, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_dust", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_INFERNO, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_inferno", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_NUKE, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_nuke", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_PIRANESI, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_piranesi", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_PORT, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_port", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_PRODIGY, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_prodigy", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_TIDES, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_tides", true)));
		m_StatImageMap.Insert(CSSTAT_MAP_WINS_DE_TRAIN, m_pImageList->AddImage(scheme()->GetImage("gfx/vgui/summary_maps/summary_de_train", true)));
	}
}
//----------------------------------------------------------
// Get the width we're going to lock at
//----------------------------------------------------------
void CStatsSummary::ApplySettings(KeyValues *pResourceData)
{
	m_iFixedWidth = pResourceData->GetInt("wide", 512);

	BaseClass::ApplySettings(pResourceData);
}

//----------------------------------------------------------
// Preserve our width to the one in the .res file
//----------------------------------------------------------
void CStatsSummary::OnSizeChanged(int newWide, int newTall)
{	
	// Lock the width, but allow height scaling
	if (newWide != m_iFixedWidth)
	{
		SetSize(m_iFixedWidth, newTall);
		return;
	}

	BaseClass::OnSizeChanged(newWide, newTall);
}


void CStatsSummary::OnThink()
{
	if ( m_bRecentAchievementsDirty )
		UpdateRecentAchievements();

	if ( m_bStatsDirty )
		UpdateStatsData();
}


void CStatsSummary::OnPageShow()
{	
	UpdateStatsData();
	UpdateRecentAchievements();	
}

//----------------------------------------------------------
// Update all of the stats displays
//----------------------------------------------------------
void CStatsSummary::UpdateStatsData()
{
	UpdateStatsSummary();
	UpdateKillHistory();
	UpdateFavoriteWeaponData();
	UpdateMapsData();


	UpdateLastMatchStats();
	m_pStatCard->UpdateInfo();

	m_bStatsDirty = false;
}

//----------------------------------------------------------
// Update the stats located in the stats summary section
//----------------------------------------------------------
void CStatsSummary::UpdateStatsSummary()
{
	DisplayCompressedLocalizedStat(CSSTAT_ROUNDS_PLAYED, "roundsplayed");
	DisplayCompressedLocalizedStat(CSSTAT_ROUNDS_WON, "roundswon");
	DisplayCompressedLocalizedStat(CSSTAT_SHOTS_HIT, "shotshit");
	DisplayCompressedLocalizedStat(CSSTAT_SHOTS_FIRED, "shotsfired");

	wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];
	
	//Calculate Win Ratio
	int wins = g_CSClientGameStats.GetStatById(CSSTAT_ROUNDS_WON).iStatValue;
	int rounds = g_CSClientGameStats.GetStatById(CSSTAT_ROUNDS_PLAYED).iStatValue;
	float winRatio = 0;	
	if (rounds > 0)
	{
		winRatio = ((float)wins / (float)rounds) * 100.0f;
	}
	V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f%%", winRatio);	
	SetDialogVariable("winratio", tempWNumString);

	
	//Calculate accuracy
	int hits = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_HIT).iStatValue;
	int shots = g_CSClientGameStats.GetStatById(CSSTAT_SHOTS_FIRED).iStatValue;
	float accuracy = 0;	
	if (shots > 0)
	{
		accuracy = ((float)hits / (float)shots) * 100.0f;
	}
	V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f%%", accuracy);	
	SetDialogVariable("hitratio", tempWNumString);
}

//----------------------------------------------------------
// Update the stats located in the stats summary section
//----------------------------------------------------------
void CStatsSummary::UpdateKillHistory()
{
	DisplayCompressedLocalizedStat(CSSTAT_KILLS, "kills");
	DisplayCompressedLocalizedStat(CSSTAT_DEATHS, "deaths");

	wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];

	//Calculate Win Ratio
	int kills = g_CSClientGameStats.GetStatById(CSSTAT_KILLS).iStatValue;
	int deaths = g_CSClientGameStats.GetStatById(CSSTAT_DEATHS).iStatValue;
	float killDeathRatio = 0;	
	if (deaths > 0)
	{
		killDeathRatio = (float)kills / (float)deaths;
	}
	V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f", killDeathRatio);	
	SetDialogVariable("killdeathratio", tempWNumString);
}
//----------------------------------------------------------
// Update the stats located in the favorite weapon section
//----------------------------------------------------------
void CStatsSummary::UpdateFavoriteWeaponData()
{
	// First set the image to the favorite weapon
	if (m_pImageList)
	{
		//start with a dummy stat
		const WeaponName_StatId* pFavoriteWeaponStatEntry = NULL;

		//Find the weapon stat with the most kills
		int numKills = 0;
		for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
		{
			// ignore weapons with no hit counts (knife and grenade)
			if (WeaponName_StatId_Table[i].hitStatId == CSSTAT_UNDEFINED )
				continue;

			const WeaponName_StatId& weaponStatEntry = WeaponName_StatId_Table[i];
			PlayerStatData_t stat = g_CSClientGameStats.GetStatById(weaponStatEntry.killStatId);
			if (stat.iStatValue > numKills)
			{
				pFavoriteWeaponStatEntry = &weaponStatEntry;
				numKills = stat.iStatValue;
			}
		}

		if (pFavoriteWeaponStatEntry)
		{			
			CUtlMap<CSStatType_t, int>::IndexType_t idx = m_StatImageMap.Find((CSStatType_t)pFavoriteWeaponStatEntry->killStatId);
			if (m_StatImageMap.IsValidIndex(idx))
			{
				m_pImagePanelFavWeapon->SetImage(m_pImageList->GetImage(m_StatImageMap[idx]));
			}
			
			DisplayCompressedLocalizedStat(pFavoriteWeaponStatEntry->hitStatId, "weaponhits", "#GameUI_Stats_WeaponShotsHit");
			DisplayCompressedLocalizedStat(pFavoriteWeaponStatEntry->shotStatId, "weaponshotsfired", "#GameUI_Stats_WeaponShotsFired");
			DisplayCompressedLocalizedStat(pFavoriteWeaponStatEntry->killStatId, "weaponkills", "#GameUI_Stats_WeaponKills");

			//Calculate accuracy
			int kills = g_CSClientGameStats.GetStatById(pFavoriteWeaponStatEntry->killStatId).iStatValue;
			int shots = g_CSClientGameStats.GetStatById(pFavoriteWeaponStatEntry->shotStatId).iStatValue;
			float killsPerShot = 0;	
			if (shots > 0)
			{
				killsPerShot = (float)kills / (float)shots;
			}
			wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];
			V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f", (killsPerShot));
			DisplayFormattedLabel("#GameUI_Stats_WeaponKillRatio", tempWNumString, "weaponkillratio");
		}
		else
		{
			if (m_iDefaultWeaponImage != -1)
			{
				m_pImagePanelFavWeapon->SetImage(m_pImageList->GetImage(m_iDefaultWeaponImage));
			}
			DisplayFormattedLabel("#GameUI_Stats_WeaponShotsHit", L"0", "weaponhits");
			DisplayFormattedLabel("#GameUI_Stats_WeaponShotsFired", L"0", "weaponshotsfired");
			DisplayFormattedLabel("#GameUI_Stats_WeaponKills", L"0", "weaponkills");
			DisplayFormattedLabel("#GameUI_Stats_WeaponKillRatio", L"0", "weaponkillratio");		
		}
	}
}

//----------------------------------------------------------
// Update the stats located in the favorite weapon section
//----------------------------------------------------------
void CStatsSummary::UpdateMapsData()
{
	//start with a dummy stat
	const MapName_MapStatId* pFavoriteMapStat = NULL;
	int numRounds = 0;
	for (int i = 0; MapName_StatId_Table[i].statWinsId != CSSTAT_UNDEFINED; ++i)
	{
		const MapName_MapStatId& mapStatId = MapName_StatId_Table[i];
		PlayerStatData_t stat = g_CSClientGameStats.GetStatById(mapStatId.statRoundsId);
		if (stat.iStatValue > numRounds)
		{
			pFavoriteMapStat = &mapStatId;
			numRounds = stat.iStatValue;
		}
	}

	if (pFavoriteMapStat)
	{	
		CUtlMap<CSStatType_t, int>::IndexType_t idx = m_StatImageMap.Find((CSStatType_t)pFavoriteMapStat->statWinsId);
		if (m_StatImageMap.IsValidIndex(idx))
		{
			m_pImagePanelFavMap->SetImage(m_pImageList->GetImage(m_StatImageMap[idx]));
		}
		
		// map name
		SetDialogVariable("mapname", pFavoriteMapStat->szMapName);

		// rounds played
		DisplayCompressedLocalizedStat(pFavoriteMapStat->statRoundsId,"mapplayed", "#GameUI_Stats_MapPlayed");
		DisplayCompressedLocalizedStat(pFavoriteMapStat->statWinsId, "mapwins", "#GameUI_Stats_MapWins");

		//Calculate Win Ratio
		int wins = g_CSClientGameStats.GetStatById(pFavoriteMapStat->statWinsId).iStatValue;
		int rounds = g_CSClientGameStats.GetStatById(pFavoriteMapStat->statRoundsId).iStatValue;
		float winRatio = 0;	
		if (rounds > 0)
		{
			winRatio = ((float)wins / (float)rounds) * 100.0f;
		}
		wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];
		V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f%%", winRatio);	
		DisplayFormattedLabel("#GameUI_Stats_MapWinRatio", tempWNumString, "mapwinratio");				
	}
	else
	{
		if (m_iDefaultMapImage != -1)
		{
			m_pImagePanelFavMap->SetImage(m_pImageList->GetImage(m_iDefaultMapImage));
		}
		SetDialogVariable("mapname", L"");
		DisplayFormattedLabel("#GameUI_Stats_MapPlayed", L"0", "mapplayed");				
		DisplayFormattedLabel("#GameUI_Stats_MapWins", L"0", "mapwins");				
		DisplayFormattedLabel("#GameUI_Stats_MapWinRatio", L"0", "mapwinratio");	
	}
}

int CStatsSummary::AchivementDateSortPredicate( CCSBaseAchievement* const* pLeft, CCSBaseAchievement* const* pRight )
{
	if (!pLeft || !pRight || !(*pLeft) || ! (*pRight))
	{
		return 0;
	}

	if	((*pLeft)->GetSortKey() < (*pRight)->GetSortKey())
	{
		return	1;
	}
	else if ((*pLeft)->GetSortKey() > (*pRight)->GetSortKey())
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

void CStatsSummary::UpdateRecentAchievements()
{
	CUtlVector<CCSBaseAchievement*> sortedAchivementList;

	//Get a list of all the achievements that have been earned
	int iCount = g_AchievementMgrCS.GetAchievementCount();
	sortedAchivementList.EnsureCapacity(iCount);

	for (int i = 0; i < iCount; ++i)
	{
		CCSBaseAchievement* pAchievement = (CCSBaseAchievement*)g_AchievementMgrCS.GetAchievementByIndex(i);
		if (pAchievement && pAchievement->IsAchieved())
		{
			sortedAchivementList.AddToTail(pAchievement);
		}
	}

	//Sort the earned achievements by time and date earned.
	sortedAchivementList.Sort(AchivementDateSortPredicate);

	//Clear the UI list
	m_pRecentAchievementsList->DeleteAllItems();	

	// Add each achievement in the sorted list as a panel.
	int numPanelsToAdd = MIN(maxRecentAchievementToDisplay, sortedAchivementList.Count());
	for ( int i = 0; i < numPanelsToAdd; i++ )
	{
		CCSBaseAchievement* pAchievement = sortedAchivementList[i];
		if (pAchievement)
		{
			CAchievementsPageItemPanel *achievementItemPanel = new CAchievementsPageItemPanel(m_pRecentAchievementsList, "AchievementDialogItemPanel");
			achievementItemPanel->SetAchievementInfo(pAchievement);

			// force all our new panel to have the correct internal layout and size so that our parent container can layout properly
			achievementItemPanel->InvalidateLayout(true, true);

			m_pRecentAchievementsList->AddItem(NULL, achievementItemPanel);
		}
	}

	m_bRecentAchievementsDirty = false;
}

void CStatsSummary::UpdateLastMatchStats()
{
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_T_ROUNDS_WON, "lastmatchtwins", "#GameUI_Stats_LastMatch_TWins");			
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_CT_ROUNDS_WON, "lastmatchctwins", "#GameUI_Stats_LastMatch_CTWins");			
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_ROUNDS_WON, "lastmatchwins", "#GameUI_Stats_LastMatch_RoundsWon");			
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_MAX_PLAYERS, "lastmatchmaxplayers", "#GameUI_Stats_LastMatch_MaxPlayers");		
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_MVPS, "lastmatchstars", "#GameUI_Stats_LastMatch_MVPS");
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_KILLS, "lastmatchkills", "#GameUI_Stats_WeaponKills");
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_DEATHS, "lastmatchdeaths", "#GameUI_Stats_LastMatch_Deaths");	
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_DAMAGE, "lastmatchtotaldamage", "#GameUI_Stats_LastMatch_Damage");	
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_DOMINATIONS, "lastmatchdominations", "#GameUI_Stats_LastMatch_Dominations");
	DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_REVENGES, "lastmatchrevenges", "#GameUI_Stats_LastMatch_Revenges");

	UpdateLastMatchFavoriteWeaponStats();

	wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];

	//Calculate Kill:Death ratio
	int deaths = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_DEATHS).iStatValue;
	int kills = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_KILLS).iStatValue;
	float killDeathRatio = deaths;	
	if (deaths != 0)
	{
		killDeathRatio = (float)kills / (float)deaths;
	}	
	V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f", killDeathRatio);	
	DisplayFormattedLabel("#GameUI_Stats_LastMatch_KDRatio", tempWNumString, "lastmatchkilldeathratio");		

	//Calculate cost per kill
	PlayerStatData_t statMoneySpent = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_MONEYSPENT);
	int costPerKill = 0;
	if (kills > 0)
	{
		costPerKill = statMoneySpent.iStatValue / kills;
	}	
	ConvertNumberToMantissaSuffixForm(costPerKill, tempWNumString, STAT_NUM_ARRAY_LENGTH);
	DisplayFormattedLabel("#GameUI_Stats_LastMatch_MoneySpentPerKill", tempWNumString, "lastmatchmoneyspentperkill");	
}

void CStatsSummary::UpdateLastMatchFavoriteWeaponStats()
{
	wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];

	PlayerStatData_t statFavWeaponId = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_FAVWEAPON_ID);
	const WeaponName_StatId& favoriteWeaponStat = GetWeaponTableEntryFromWeaponId((CSWeaponID)statFavWeaponId.iStatValue);

	// If we have a valid weapon use it's data; otherwise use nothing. We check the shot ID to make sure it is a weapon with bullets.
	if (favoriteWeaponStat.hitStatId != CSSTAT_UNDEFINED)
	{
		// Set the image and (non-copyright) name of this weapon
		CUtlMap<CSStatType_t, int>::IndexType_t idx = m_StatImageMap.Find((CSStatType_t)favoriteWeaponStat.killStatId);
		if (m_StatImageMap.IsValidIndex(idx))
		{
			m_pImagePanelLastMapFavWeapon->SetImage(m_pImageList->GetImage(m_StatImageMap[idx]));
		}

		const wchar_t* tempName = WeaponIDToDisplayName(favoriteWeaponStat.weaponId);
		if (tempName)
		{
			SetDialogVariable("lastmatchfavweaponname", tempName);
		}
		else
		{
			SetDialogVariable("lastmatchfavweaponname", L"");
		}

		DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_FAVWEAPON_KILLS, "lastmatchfavweaponkills", "#GameUI_Stats_WeaponKills");			
		DisplayCompressedLocalizedStat(CSSTAT_LASTMATCH_FAVWEAPON_HITS, "lastmatchfavweaponhits", "#GameUI_Stats_WeaponShotsHit");			

		int shots = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_FAVWEAPON_SHOTS).iStatValue;
		int hits = g_CSClientGameStats.GetStatById(CSSTAT_LASTMATCH_FAVWEAPON_HITS).iStatValue;
		float accuracy = 0.0f;
		if (shots != 0)
		{
			accuracy = ((float)hits / (float)shots) * 100.0f;
		}
		V_snwprintf(tempWNumString, ARRAYSIZE(tempWNumString), L"%.1f%%", accuracy);
		DisplayFormattedLabel("#GameUI_Stats_LastMatch_FavWeaponAccuracy", tempWNumString, "lastmatchfavweaponaccuracy");	
	}
	else
	{
		if (m_iDefaultWeaponImage != -1)
		{
			m_pImagePanelLastMapFavWeapon->SetImage(m_pImageList->GetImage(m_iDefaultWeaponImage));
		}


		//Set this label to diferent text, indicating that there is not a favorite weapon
		SetDialogVariable("lastmatchfavweaponname", g_pVGuiLocalize->Find("#GameUI_Stats_LastMatch_NoFavWeapon"));

		DisplayFormattedLabel("#GameUI_Stats_WeaponKills", L"0", "lastmatchfavweaponkills");
		DisplayFormattedLabel("#GameUI_Stats_WeaponShotsHit", L"0", "lastmatchfavweaponhits");
		DisplayFormattedLabel("#GameUI_Stats_LastMatch_FavWeaponAccuracy", L"0", "lastmatchfavweaponaccuracy");
	}
}

void CStatsSummary::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if ( 0 == Q_strcmp( type, "achievement_earned_local" ) )
	{
		m_bRecentAchievementsDirty = true;
	}

	else if ( 0 == Q_strcmp( type, "player_stats_updated" ) )
	{
		m_bStatsDirty = true;
	}	
}

void CStatsSummary::DisplayCompressedLocalizedStat(CSStatType_t stat, const char* dialogVariable, const char* localizationToken /* = NULL */)
{
	wchar_t tempWNumString[STAT_NUM_ARRAY_LENGTH];
	int statValue = g_CSClientGameStats.GetStatById(stat).iStatValue;
	ConvertNumberToMantissaSuffixForm(statValue, tempWNumString, STAT_NUM_ARRAY_LENGTH);
	if (localizationToken)
	{
		DisplayFormattedLabel(localizationToken, tempWNumString, dialogVariable);	
	}
	else
	{
		SetDialogVariable(dialogVariable, tempWNumString);
	}
}


void CStatsSummary::DisplayFormattedLabel( const char* localizationToken, const wchar_t* valueText, const char* dialogVariable )
{
	wchar_t tempWStatString[256];
	g_pVGuiLocalize->ConstructString(tempWStatString, sizeof(tempWStatString),
		g_pVGuiLocalize->Find(localizationToken), 1, valueText);
	SetDialogVariable(dialogVariable, tempWStatString);
}
