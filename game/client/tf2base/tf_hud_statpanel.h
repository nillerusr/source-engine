//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFSTATPANEL_H
#define TFSTATPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"
#include "../game/shared/tf2base/tf_shareddefs.h"
#include "../game/shared/tf2base/tf_gamestats_shared.h"
#include "tf_hud_playerstatus.h"

using namespace vgui;

enum PlayerStatsVersions_t
{
	PLAYERSTATS_FILE_VERSION
};

struct ClassStats_t
{
	int					iPlayerClass;		// which class these stats refer to
	int					iNumberOfRounds;	// how many times player has played this class
	RoundStats_t		accumulated;
	RoundStats_t		max;

	ClassStats_t()
	{
		iPlayerClass	= TF_CLASS_UNDEFINED;
		iNumberOfRounds = 0;
	}

	void AccumulateRound( const RoundStats_t &other )
	{
		iNumberOfRounds++;
		accumulated.AccumulateRound( other );
	}
};

enum RecordBreakType_t
{
	RECORDBREAK_NONE,
	RECORDBREAK_CLOSE,
	RECORDBREAK_TIE,
	RECORDBREAK_BEST,

	RECORDBREAK_MAX
};

class C_TFPlayer;

class CTFStatPanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CTFStatPanel, EditablePanel );

public:
	CTFStatPanel( const char *pElementName );
	virtual ~CTFStatPanel();

	virtual void Reset();
	virtual void Init();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void FireGameEvent( IGameEvent * event );
	virtual void OnTick();
	virtual void LevelShutdown();
	void Show();
	void Hide();
	virtual bool ShouldDraw( void );
	void OnLocalPlayerRemove( C_TFPlayer *pPlayer );

	void		ShowStatPanel( int iClass, int iTeam, int iCurStatValue, TFStatType_t statType, RecordBreakType_t recordBreakType,
								bool bAlive );
	void		TestStatPanel( TFStatType_t statType, RecordBreakType_t recordType );

	void		WriteStats( void );
	bool		ReadStats( void );
	int			CalcCRC( int iSteamID );
	void		ClearStatsInMemory( void );
	void		ResetStats( void );
	static ClassStats_t &GetClassStats( int iClass );
	void		UpdateStatSummaryPanel();
	bool		IsLocalFileTrusted() { return m_bLocalFileTrusted; }
	void		SetStatsChanged( bool bChanged ) { m_bStatsChanged = bChanged; }
	RoundStats_t &GetRoundStatsCurrentGame() { return m_RoundStatsCurrentGame; }

	void		CalcMaxsAndRecords();

	void MsgFunc_PlayerStatsUpdate( bf_read &msg );

	virtual int GetRenderGroupPriority() { return 40; }	// less than winpanel, build menu

private:
	void		GetStatValueAsString( int iValue, TFStatType_t statType, char *value, int valuelen );
	void		UpdateStats( int iMsgType );
	void		ResetDisplayedStat();

	int							m_iCurStatValue;			// the value of the currently displayed stat
	int							m_iCurStatClass;			// the player class for current stat
	int							m_iCurStatTeam;				// the team of current stat
	TFStatType_t				m_statRecord;				// which stat broke a record
	RecordBreakType_t			m_recordBreakType;			// was record broken, tied, or just close
	float						m_flTimeLastSpawn;
	float						m_flTimeHide;				// time at which to hide the panel
	bool						m_bNeedToCalcMaxs;

	CUtlVector<ClassStats_t>	m_aClassStats;
	RoundStats_t				m_RoundStatsCurrentGame;	// accumulated stats for game since last score reset
	RoundStats_t				m_RoundStatsLifeStart;		// stats at start of current life, so we can compute delta
	RoundStats_t				m_RoundStatsCurrentLife;	// accumulated stats for current life
	int							m_iClassCurrentLife;		// class that current life stats are for
	int							m_iTeamCurrentLife;		// class that current life stats are for
	float						m_flTimeCurrentLifeStart;	// time that current life stats started
	bool						m_bStatsChanged;
	bool						m_bLocalFileTrusted;		// do we believe our local stats data file has not been tampered with
	CTFClassImage				*m_pClassImage;

	bool						m_bShouldBeVisible;
};

CTFStatPanel *GetStatPanel();

#endif //TFSTATPANEL_H
