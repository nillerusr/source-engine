//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_MATCHMAKING_DASHBOARD_H
#define TF_MATCHMAKING_DASHBOARD_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_match_join_handlers.h"
#include <vgui_controls/EditablePanel.h>
#include "tf_controls.h"
#include <vgui_controls/PHandle.h>
#include "local_steam_shared_object_listener.h"

CUtlVector< class CTFMatchmakingPopup* >& CreateMMPopupPanels( bool bRecreate = false );
class CTFMatchmakingDashboard* GetMMDashboard();
class CMMDashboardParentManager* GetMMDashboardParentManager();

bool BInEndOfMatch();

//-----------------------------------------------------------------------------
// Purpose: Popup that goes underneath the dashboard and displays anything
//			important the user needs to know about
//-----------------------------------------------------------------------------
class CTFMatchmakingPopup : public CExpandablePanel
						  , public CGameEventListener
						  , public IMatchJoiningHandler
{
	friend class CTFMatchmakingPopupState;
	friend class CTFMatchmakingDashboard;
	DECLARE_CLASS_SIMPLE( CTFMatchmakingPopup, CExpandablePanel );
public:

	CTFMatchmakingPopup( const char* pszName, const char* pszResFile );
	virtual ~CTFMatchmakingPopup();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void OnThink() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void OnTick() OVERRIDE;

	virtual void OnEnter();
	virtual void OnUpdate();
	virtual void OnExit();
	virtual void Update();

	virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;

private:

	virtual void MatchFound() {} // We dont need to do anything special
	virtual bool ShouldBeActve() const = 0;
	void UpdateRematchtime();
	void UpdateAutoJoinTime();

	bool m_bActive;
	const char* m_pszResFile;
};

//-----------------------------------------------------------------------------
// Purpose: Matchmaking panel that contains controls for matchmaking
//-----------------------------------------------------------------------------
class CTFMatchmakingDashboard : public CExpandablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CTFMatchmakingDashboard, CExpandablePanel );
	CTFMatchmakingDashboard();
	virtual ~CTFMatchmakingDashboard();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void OnTick() OVERRIDE;
};

//-----------------------------------------------------------------------------
// CMMDashboardParentManager
// Purpose: This guy keeps the MM dashboard as the top-most panel but does so
//			*without making it a popup*.  This is important because popups look
//			awful whenever they overlap and transparency is involved.  This class
//			does its dirty work by keeping track of the top-most fullscreen popup
//			and setting that panel as the MM dashboard's parent.  When that popup
//			goes away, we set the parent to the next popup on the stack, or to
//			the GameUI if none are active.  If we're in-game, then we parent to
//			the our special popup container.  Why not always just parent to that
//			single popup container?  Because we want the MINIMUM mouse focus area
//			possible because the dashboard is not a rectangle (it grows/shrinks).
//			
//			
//			If anything draws on top of the MM dashboard and you dont want it to
//			have that panel add itself to this class using PushModalFullscreenPopup
//			when it goes visible and PopModalFullscreenPopup when it hides itself
//-----------------------------------------------------------------------------
class CMMDashboardParentManager : public CGameEventListener
{
public:
	friend class CTFMatchmakingDashboard;
	friend class CTFMatchmakingPopup;

	CMMDashboardParentManager();

	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	void PushModalFullscreenPopup( vgui::Panel* pPanel );
	void PopModalFullscreenPopup( vgui::Panel* pPanel );
	void UpdateParenting();
private:

	void AddPanel( CExpandablePanel* pPanel );
	void RemovePanel( CExpandablePanel* pPanel );

	void AttachToGameUI();
	void AttachToTopMostPopup();

	bool m_bAttachedToGameUI;

	class CUtlSortVectorPanelZPos
	{
	public:
		bool Less( const vgui::Panel* lhs, const vgui::Panel* rhs, void * )
		{
			return lhs->GetZPos() < rhs->GetZPos();
		}
	};

	CUtlSortVector< CExpandablePanel*, CUtlSortVectorPanelZPos > m_vecPanels;
	CUtlVector< vgui::Panel* > m_vecFullscreenPopups;

	vgui::PHandle m_pHUDPopup;
};

class IMMPopupFactory
{
public:
	virtual CTFMatchmakingPopup* Create() const = 0;
	static CUtlVector< IMMPopupFactory* > s_vecPopupFactories;
};

template< typename Type >
class CMMPopupFactoryImplementation : public IMMPopupFactory
{
public:
	CMMPopupFactoryImplementation( const char* pszName, const char* pszResFile ) : m_pszName( pszName ), m_pszResFile( pszResFile )
	{ s_vecPopupFactories.AddToTail( this ); }

	virtual CTFMatchmakingPopup* Create() const OVERRIDE { return new Type( m_pszName, m_pszResFile ); }
private:
	const char* m_pszName;
	const char* m_pszResFile;
};

#define REG_MM_POPUP_FACTORY( type, name, resfile )	 CMMPopupFactoryImplementation< type > g_##type##Factory( name, resfile );

#endif // TF_MATCHMAKING_DASHBOARD_H
