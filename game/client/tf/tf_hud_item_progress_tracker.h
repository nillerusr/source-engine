//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TF_HUD_ITEM_PROGRESS_TRACKER_H
#define TF_HUD_ITEM_PROGRESS_TRACKER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "hud_baseachievement_tracker.h"
#include "iachievementmgr.h"
#include "achievementmgr.h"
#include "baseachievement.h"
#include "gcsdk/gcclient_sharedobjectcache.h"
#include "econ_item_inventory.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace GCSDK;


class CItemAttributeProgressPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CItemAttributeProgressPanel, vgui::EditablePanel );
public:
	CItemAttributeProgressPanel( vgui::Panel* pParent, const char *pElementName, const CQuestObjectiveDefinition *pObjectiveDef, const char* pszResFileName );
	virtual ~CItemAttributeProgressPanel() {}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;

	virtual void OnThink() OVERRIDE;

	void SetProgress( Color glowColor );
	int GetContentTall() const;

	void SetIsValid( bool bIsValid );
	bool IsAdvanced() const { return m_bAdvanced; }

	const uint32 m_nDefIndex;
private:
	float	m_flLastThink;
	float	m_flUpdateTime;
	bool	m_bAdvanced;

	Label *m_pAttribDesc;
	Label *m_pAttribGlow;
	Label *m_pAttribBlur;

	Color m_enabledTextColor;
	Color m_disabledTextColor;

	CUtlString m_strNormalPointLocToken;
	CUtlString m_strAdvancedLocToken;

	CUtlString m_strResFileName;
};

class CItemTrackerPanel : public vgui::EditablePanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CItemTrackerPanel, vgui::EditablePanel );
public:

	CItemTrackerPanel( vgui::Panel* pParent, const char *pElementName, const CEconItem* pItem, const char* pszItemTrackerResFile );
	virtual ~CItemTrackerPanel();

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual int GetContentTall() const;

	virtual void OnThink() OVERRIDE;
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	bool IsStandardCompleted() const;
	bool IsEverythingCompleted() const;
	bool IsDoneProgressing() const { return m_flStandardCurrentProgress == m_flStandardTargetProgress && m_flBonusCurrentProgress == m_flBonusTargetProgress; }

	void SetItem( const CEconItem* pItem );
	CItemAttributeProgressPanel* GetPanelForObjective( const CQuestObjectiveDefinition* pObjective );
	bool IsValidForLocalPlayer() const;

	const CUtlVector< CItemAttributeProgressPanel* >& GetAttributePanels() const { return m_vecAttribPanels; }

protected:

	enum ESoundToPlay
	{
		SOUND_NONE = 0,
		SOUND_STANDARD_OBJECTIVE_TICK,
		SOUND_ADVANCED_OBJECTIVE_TICK,
		SOUND_QUEST_STANDARD_COMPLETE,
		SOUND_QUEST_ADVANCED_COMPLETE,
	};

	void CaptureProgress();
	void UpdateBars();

	vgui::Label *m_pItemName;


	CEconItemViewHandle m_pItem;
	CUtlVector< CItemAttributeProgressPanel* > m_vecAttribPanels;
	EditablePanel	*m_pCompletedContainer;
	Label			*m_pCompletedDescGlow;
	Label			*m_pCompletedNameGlow;

	EditablePanel		*m_pProgressBarBackground;
	EditablePanel		*m_pProgressBarStandard;
	EditablePanel		*m_pProgressBarBonus;
	EditablePanel		*m_pProgressBarStandardHighlight;
	EditablePanel		*m_pProgressBarBonusHighlight;

	float m_flStandardCurrentProgress;
	float m_flStandardTargetProgress;
	float m_flBonusCurrentProgress;
	float m_flBonusTargetProgress;
	float m_flUpdateTime;
	float m_flLastThink;

	uint32 m_nMaxStandardPoints;
	uint32 m_nMaxBonusPoints;

	ESoundToPlay m_eSoundToPlay;
	static float m_sflEventRecievedTime;

	int m_nContentTall;

	CUtlString m_strStandardObjectiveTick;
	CUtlString m_strStandardPointsComplete;
	CUtlString m_strAdvancedObjectiveComplete;
	CUtlString m_strAdvancedPointsComplete;

	CUtlString m_strItemAttributeResFile;
	CUtlString m_strItemTrackerResFile;

	CUtlString m_strProgressBarStandardLocToken;
	CUtlString m_strProgressBarAdvancedLocToken;

	CPanelAnimationVarAliasType( int, m_nAttribYStartOffset, "attrib_y_start_offset", "5", "proportional_int");
	CPanelAnimationVarAliasType( int, m_nAttribYStep, "attrib_y_step", "0", "proportional_int");
	CPanelAnimationVarAliasType( int, m_nAttribXOffset, "attrib_x_offset", "5", "proportional_int");
	CPanelAnimationVar( bool, m_bNoEffects, "no_effects", "0" );

	CPanelAnimationVarAliasType( int, m_nBarGap, "bar_gap", "0", "proportional_int");

	CPanelAnimationVar( Color, m_clrStandardHighlight, "standard_glow_color", "QuestStandardHighlight" );
	CPanelAnimationVar( Color, m_clrBonusHighlight, "bonus_glow_color", "QuestBonusHighlight" );
};

class CHudItemAttributeTracker : public CHudElement, public EditablePanel, public ISharedObjectListener
{
	DECLARE_CLASS_SIMPLE( CHudItemAttributeTracker, EditablePanel );
public:
	enum ETrackerHandling_t
	{
		TRACKER_INVALID,
		TRACKER_CREATE,
		TRACKER_UPDATE,
		TRACKER_REMOVE,
	};

	CHudItemAttributeTracker( const char *pElementName );
	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;
	virtual bool ShouldDraw();
	virtual void OnThink() OVERRIDE;

	virtual void SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE
	{
		HandleSOEvent( steamIDOwner, pObject, TRACKER_CREATE );
	}
	virtual void PreSOUpdate( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) OVERRIDE {};
	virtual void SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE
	{
		HandleSOEvent( steamIDOwner, pObject, TRACKER_UPDATE );
	}
	virtual void PostSOUpdate( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) OVERRIDE {};
	virtual void SODestroyed( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE
	{
		HandleSOEvent( steamIDOwner, pObject, TRACKER_REMOVE );
	}
	virtual void SOCacheSubscribed( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) OVERRIDE {};
	virtual void SOCacheUnsubscribed( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) OVERRIDE {};

	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;

private:

	void HandleSOEvent( const CSteamID & steamIDOwner, const CSharedObject *pObject, ETrackerHandling_t eHandling );
	bool FindTrackerForItem( const CEconItem* pItem, CItemTrackerPanel** ppTracker, bool bCreateIfNotFound );

	CUtlMap< itemid_t, CItemTrackerPanel* > m_mapTrackers;
	EditablePanel *m_pStatusContainer;
	Label *m_pCallToActionLabel;
	Label *m_pStatusHeaderLabel;
	CPanelAnimationVarAliasType( int, m_nStatusBufferWidth, "stats_buffer_width", "0", "proportional_int");

};

int QuestSort_PointsAscending( CItemAttributeProgressPanel* const* p1, CItemAttributeProgressPanel* const* p2 );

#endif // TF_HUD_ITEM_PROGRESS_TRACKER_H