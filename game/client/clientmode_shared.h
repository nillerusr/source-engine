//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTMODE_NORMAL_H )
#define CLIENTMODE_NORMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientmode.h"
#include "GameEventListener.h"
#include <baseviewport.h>

class CBaseHudChat;
class CBaseHudWeaponSelection;
class CViewSetup;
class C_BaseEntity;
class C_BasePlayer;

namespace vgui
{
class Panel;
}

#define USERID2PLAYER(i) ToBasePlayer( ClientEntityList().GetEnt( engine->GetPlayerForUserID( i ) ) )	

extern IClientMode *GetClientModeNormal(); // must be implemented
extern IClientMode *GetFullscreenClientMode();

// This class implements client mode functionality common to HL2 and TF2.
class ClientModeShared : public IClientMode, public CGameEventListener
{
// IClientMode overrides.
public:
	DECLARE_CLASS_NOBASE( ClientModeShared );

					ClientModeShared();
	virtual			~ClientModeShared();
	
	virtual void	Init();
	virtual void	InitViewport();
	virtual void	VGui_Shutdown();
	virtual void	Shutdown();

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );

	virtual void	Enable();
	virtual void	EnableWithRootPanel( vgui::VPANEL pRoot );
	virtual void	Disable();
	virtual void	Layout( bool bForce = false );

	virtual void	ReloadScheme( void );
	virtual void	ReloadSchemeWithRoot( vgui::VPANEL pRoot );
	virtual void	OverrideView( CViewSetup *pSetup );
	virtual void	OverrideAudioState( AudioState_t *pAudioState ) { return; }
	virtual bool	ShouldDrawDetailObjects( );
	virtual bool	ShouldDrawEntity(C_BaseEntity *pEnt);
	virtual bool	ShouldDrawLocalPlayer( C_BasePlayer *pPlayer );
	virtual bool	ShouldDrawViewModel();
	virtual bool	ShouldDrawParticles( );
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual void	PreRender(CViewSetup *pSetup);
	virtual void	PostRender();
	virtual void	PostRenderVGui();
	virtual void	ProcessInput(bool bActive);
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *cmd );
	virtual void	Update();
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const;
	virtual void	SetBlurFade( float scale ) {}
	virtual float	GetBlurFade( void ) { return 0.0f; }

	// Input
	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void	OverrideMouseInput( float *x, float *y );
	virtual void	StartMessageMode( int iMessageModeType );
	virtual vgui::Panel *GetMessagePanel();

	virtual void	ActivateInGameVGuiContext( vgui::Panel *pPanel );
	virtual void	DeactivateInGameVGuiContext();

	// The mode can choose to not draw fog
	virtual bool	ShouldDrawFog( void );
	
	virtual float	GetViewModelFOV( void );
	virtual vgui::Panel* GetViewport() { return m_pViewport; }
	virtual vgui::Panel *GetPanelFromViewport( const char *pchNamePath );

	// Gets at the viewports vgui panel animation controller, if there is one...
	virtual vgui::AnimationController *GetViewportAnimationController()
		{ return m_pViewport->GetAnimationController(); }
	
	virtual void FireGameEvent( IGameEvent *event );

	virtual bool CanRecordDemo( char *errorMsg, int length ) const { return true; }

	virtual int HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual void InitChatHudElement( void );
	virtual void InitWeaponSelectionHudElement( void );

protected:
	CBaseViewport			*m_pViewport;

	int			GetSplitScreenPlayerSlot() const;

	// Message mode handling
	// All modes share a common chat interface
	CBaseHudChat			*m_pChatElement;
	vgui::HCursor			m_CursorNone;
	CBaseHudWeaponSelection *m_pWeaponSelection;
	int						m_nRootSize[2];
};

#endif // CLIENTMODE_NORMAL_H

