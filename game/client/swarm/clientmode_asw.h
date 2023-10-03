#ifndef _INCLUDED_CLIENTMODE_ASW_H
#define _INCLUDED_CLIENTMODE_ASW_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>
#include "GameUI/igameui.h"

class CHudViewport;

namespace vgui
{
	typedef unsigned long HScheme;
	class Panel;
	class Frame;
}
class C_ASW_Info_Message;
class C_ASW_Player;

class ClientModeASW : public ClientModeShared
{
public:
	DECLARE_CLASS( ClientModeASW, ClientModeShared );

	ClientModeASW();
	~ClientModeASW();

	virtual void	Init();
	virtual void	InitWeaponSelectionHudElement() { return; }
	virtual void	InitViewport();
	virtual void	Shutdown();
	virtual void	OverrideView( CViewSetup *pSetup );
	virtual void	OverrideAudioState( AudioState_t *pAudioState );
	virtual bool	ShouldDrawCrosshair( void ) { return false; }	// don't draw the HL2 crosshair

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );

	virtual void	Update( void );
	virtual void	FireGameEvent( IGameEvent *event );
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const { return 1.0f; }
	virtual void	ClearCurrentColorCorrection() { m_pCurrentColorCorrection = NULL; }

	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	
	virtual void ASW_CloseAllWindows();
	virtual void ASW_CloseAllWindowsFrom(vgui::Panel* pPanel);	

	void ToggleMessageMode();

	bool m_bLaunchedBriefing, m_bLaunchedDebrief, m_bLaunchedCampaignMap, m_bLaunchedOutro;
	vgui::DHANDLE<vgui::Frame> m_hMissionCompleteFrame;
	vgui::DHANDLE<vgui::Frame> m_hCampaignFrame;
	vgui::DHANDLE<vgui::Frame> m_hOutroFrame;
	vgui::DHANDLE<vgui::Frame> m_hInGameBriefingFrame;
	
	CHandle<C_ASW_Info_Message> m_hLastInfoMessage;
	typedef CHandle<C_ASW_Info_Message> InfoMessageHandle_t;
	CUtlVector<InfoMessageHandle_t> m_InfoMessageLog;

	void StartBriefingMusic();
	void StopBriefingMusic(bool bInstantly = false);
	CSoundPatch		*m_pBriefingMusic;

	bool m_bSpectator;	// client wants to spectate and will auto-ready at various stages of the game

	float m_fNextProgressUpdate;

	bool HasAwardedExperience( C_ASW_Player *pPlayer );
	void SetAwardedExperience( C_ASW_Player *pPlayer );
	CUtlVector<int> m_aAwardedExperience;

	CUtlVector<int> m_aAchievementsEarned;	// list of achievements earned on this mission

	bool m_bTechFailure;

	bool IsOfficialMap() { return m_bOfficialMap; }

private:
	IGameUI			*m_pGameUI;

	void DrawSniperScopeStencilMask();
	void DoObjectMotionBlur( const CViewSetup *pSetup );
	void UpdatePostProcessingEffects();

	const C_PostProcessController *m_pCurrentPostProcessController;
	PostProcessParameters_t m_CurrentPostProcessParameters;
	PostProcessParameters_t m_LerpStartPostProcessParameters, m_LerpEndPostProcessParameters;
	CountdownTimer m_PostProcessLerpTimer;

	CHandle<C_ColorCorrection> m_pCurrentColorCorrection;

	ClientCCHandle_t	m_CCFailedHandle;
	float				m_fFailedCCWeight;

	ClientCCHandle_t	m_CCInfestedHandle;
	float				m_fInfestedCCWeight;

	bool m_bOfficialMap;
};

extern IClientMode *GetClientModeNormal();
extern vgui::HScheme g_hVGuiCombineScheme;

extern ClientModeASW* GetClientModeASW();

#endif // _INCLUDED_CLIENTMODE_ASW_H
