//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef TF_PVP_RANK_PANEL_H
#define TF_PVP_RANK_PANEL_H

#include "cbase.h"
#include "vgui_controls/EditablePanel.h"
#include "tf_match_description.h"
#include "GameEventListener.h"
#include "local_steam_shared_object_listener.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

class CBaseModelPanel;
namespace vgui
{
	class ContinuousProgressBar;
};

namespace GCSDK
{
	class CSharedObject;
};

using namespace GCSDK;

class CPvPRankPanel : public vgui::EditablePanel, public CLocalSteamSharedObjectListener, public CGameEventListener
{
public:
	DECLARE_CLASS_SIMPLE( CPvPRankPanel, vgui::EditablePanel );

	CPvPRankPanel( Panel *parent, const char *panelName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnThink() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;
	virtual void SetVisible( bool bVisible ) OVERRIDE;

	void SetMatchGroup( EMatchGroup eMatchGroup );
	void SetMatchStats( void );

	virtual void SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;
	virtual void SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;

	MESSAGE_FUNC_PARAMS( OnAnimEvent, "AnimEvent", pParams );

protected:

	virtual void PlayLevelUpEffects( const LevelInfo_t& level ) const;
	virtual void PlayLevelDownEffects( const LevelInfo_t& level ) const;

private:

	struct XPState_t : public CGameEventListener, public CLocalSteamSharedObjectListener
	{
		XPState_t( EMatchGroup eMatchGroup );

		virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;
		// Get the current XP value
		uint32 GetCurrentXP() const;
		// Get the previous XP value before any changes occurred
		uint32 GetStartXP() const { return m_nStartXP; }
		// Get the target XP that the delta lerp is headed to
		uint32 GetTargetXP() const { return m_nTargetXP; }
		uint32 GetActualXP() const { return m_nActualXP; }
		// Start the timer that causes GetCurrentXP to lerp from m_nStartXP to m_nTargetXP
		bool BeginXPDeltaLerp();

		virtual void SOCreated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;
		virtual void SOUpdated( const CSteamID & steamIDOwner, const CSharedObject *pObject, ESOCacheEvent eEvent ) OVERRIDE;

	private:

		void UpdateXP( bool bInitial );

		const EMatchGroup m_eMatchGroup;
		uint32 m_nStartXP;	// The XP we show to the user that was the last state they saw
		uint32 m_nTargetXP;	// The XP the user believes is their current value
		uint32 m_nActualXP;	// The actual latest XP value
		RealTimeCountdownTimer m_progressTimer;
		bool m_bCurrentDeltaViewed;
	};

	XPState_t& GetXPState() const;
	void UpdateControls( uint32 nPreviousXP, uint32 nCurrentXP, const LevelInfo_t& levelCurrent );
	void UpdateBaseState();

	virtual const char* GetResFile() const;
	virtual KeyValues* GetConditions() const;
	void BeginXPLerp();

	EMatchGroup m_eMatchGroup;
	vgui::ContinuousProgressBar* m_pContinuousProgressBar;
	vgui::EditablePanel* m_pXPBar;
	CBaseModelPanel* m_pModelPanel;
	const IProgressionDesc* m_pProgressionDesc;
	EditablePanel* m_pBGPanel;
	bool m_bClicked;
	Panel* m_pModelButton;

	uint32 m_nLastLerpXP;
	uint32 m_nLastSeenLevel;

	CPanelAnimationVarAliasType( int, m_iXPSourceNotificationCenterX, "xp_source_notification_center_x", "19", "proportional_int" );
};

#endif //TF_PVP_RANK_PANEL_H