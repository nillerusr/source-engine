//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VLOADINGPROGRESS_H__
#define __VLOADINGPROGRESS_H__

#include "basemodui.h"
#include "vgui/IScheme.h"
#include "const.h"
#include "loadingtippanel.h"

namespace BaseModUI {

class LoadingProgress : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( LoadingProgress, CBaseModFrame );

public:
	enum LoadingType
	{
		LT_UNDEFINED = 0,
		LT_MAINMENU,
		LT_TRANSITION,
		LT_POSTER,
	};

	enum LoadingWindowType
	{
		LWT_LOADINGPLAQUE,
		LWT_BKGNDSCREEN,
	};

#define	NUM_LOADING_CHARACTERS	4

public:
	LoadingProgress( vgui::Panel *parent, const char *panelName, LoadingWindowType eLoadingType );
	~LoadingProgress();

	virtual void		Close();

	void				SetProgress( float progress );
	float				GetProgress();

	void				SetLoadingType( LoadingType loadingType );
	LoadingType			GetLoadingType();

	bool				ShouldShowPosterForLevel( KeyValues *pMissionInfo, KeyValues *pChapterInfo );
	void				SetPosterData( KeyValues *pMissionInfo, KeyValues *pChapterInfo, const char **pPlayerNames, unsigned int botFlags, const char *pszGameMode );

	bool				IsDrawingProgressBar( void ) { return m_bDrawProgress; }

protected:
	virtual void		OnThink();
	virtual void		OnCommand(const char *command);
	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		PaintBackground();

private:
	void				SetupControlStates( void );
	void				SetupPoster( void );
	void				UpdateWorkingAnim();
	void				RearrangeNames( const char *pszCharacterOrder, const char **pPlayerNames );

	vgui::ProgressBar	*m_pProTotalProgress;
	vgui::ImagePanel	*m_pWorkingAnim;
	vgui::ImagePanel	*m_pBGImage;
	vgui::ImagePanel	*m_pPoster; 
	vgui::EditablePanel *m_pFooter;
	LoadingType			m_LoadingType;
	LoadingWindowType	m_LoadingWindowType;

	bool				m_bFullscreenPoster;

	// Poster Data
	char				m_PlayerNames[NUM_LOADING_CHARACTERS][MAX_PLAYER_NAME_LENGTH];
	KeyValues			*m_pMissionInfo;
	KeyValues			*m_pChapterInfo;
	KeyValues			*m_pDefaultPosterDataKV;
	int					m_botFlags;
	bool				m_bValid;

	int					m_textureID_LoadingBar;
	int					m_textureID_LoadingBarBG;
	int					m_textureID_DefaultPosterImage;

	bool				m_bDrawBackground;
	bool				m_bDrawPoster;
	bool				m_bDrawProgress;
	bool				m_bDrawSpinner;

	float				m_flPeakProgress;

	float				m_flLastEngineTime;

	char				m_szGameMode[MAX_PATH];

	CLoadingTipPanel			*m_pTipPanel;
};

};

#endif // __VLOADINGPROGRESS_H__