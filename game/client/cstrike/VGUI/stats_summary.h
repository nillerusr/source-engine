//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSSTATSSUMMARY_H
#define CSSTATSSUMMARY_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "c_cs_player.h"
#include "cs_gamestats_shared.h"
#include "achievements_page.h"
#include "GameEventListener.h"
#include "utlmap.h"

class IAchievement;
class IScheme;
class CAchievementsPageGroupPanel;
class StatCard;
class CCSBaseAchievement;

class CStatsSummary : public vgui::PropertyPage, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE ( CStatsSummary, vgui::PropertyPage );

public:
	CStatsSummary( vgui::Panel *parent, const char *name );
	~CStatsSummary();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *pResourceData );
	virtual void OnSizeChanged( int newWide, int newTall );
	virtual void FireGameEvent( IGameEvent *event );

	virtual void OnThink();
	virtual void OnPageShow();

	void UpdateStatsData();
	void UpdateStatsSummary();
	void UpdateKillHistory();
	void UpdateFavoriteWeaponData();
	void UpdateMapsData();
	void UpdateRecentAchievements();
	void UpdateLastMatchStats();

	void UpdateLastMatchFavoriteWeaponStats();

private:

	static int AchivementDateSortPredicate( CCSBaseAchievement* const* pLeft, CCSBaseAchievement* const* pRight);
	void DisplayCompressedLocalizedStat(CSStatType_t stat, const char* dialogVariable, const char* localizationToken = NULL);
	void DisplayFormattedLabel(const char* localizationToken, const wchar_t* valueText, const char* dialogVariable);
	
	int m_iFixedWidth;
	int m_iDefaultWeaponImage;
	int m_iDefaultMapImage;

	vgui::Label*		m_pLabelRoundsPlayed;
	vgui::Label*		m_pLabelRoundsWon;
	vgui::ImagePanel*	m_pImagePanelFavWeapon;
	vgui::ImagePanel*	m_pImagePanelLastMapFavWeapon;
	vgui::ImagePanel*	m_pImagePanelFavMap;

	vgui::ImageList		*m_pImageList;

	vgui::PanelListPanel	*m_pRecentAchievementsList;

	StatCard*			m_pStatCard;
	
	bool				m_bRecentAchievementsDirty;
	bool				m_bStatsDirty;

	CUtlMap<CSStatType_t, int> m_StatImageMap;
};
#endif // CSSTATSSUMMARY_H