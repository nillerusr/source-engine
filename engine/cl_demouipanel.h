//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOUIPANEL_H
#define CL_DEMOUIPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>


namespace vgui
{
class Button;
class CheckButton;
class Label;
class ProgressBar;
class FileOpenDialog;
class Slider;
};

class CDemoEditorPanel;
class CDemoSmootherPanel;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoUIPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoUIPanel, vgui::Frame );

public:
	CDemoUIPanel( vgui::Panel *parent );
	~CDemoUIPanel();

	virtual void OnTick();

	// Command issued
	virtual void OnCommand(const char *command);
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void	OnVDMChanged( void );

	virtual bool	OverrideView( democmdinfo_t& info, int frame );
    virtual void	DrawDebuggingInfo();

	static	void	InstallDemoUI( vgui::Panel *parent );

			bool	IsInDriveMode();
			void	SetDriveViewPoint( Vector &origin, QAngle &angle );
			void	GetDriveViewPoint( Vector &origin, QAngle &angle );

protected:
	
	void			HandleInput( bool active );
	bool			IsHoldingFastForward();
	void			SetPlaybackScale( float scale );
	float			GetPlaybackScale();
	void			GetCurrentView();

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void		OnEdit();
	void		OnSmooth();
	void		OnLoad();
	
	vgui::Label		*m_pCurrentDemo;
	vgui::Button	*m_pStop;
	vgui::Button	*m_pLoad;

	// special editor buttons
	vgui::Button	*m_pEdit;
	vgui::Button	*m_pSmooth;
	vgui::Button	*m_pDriveCamera;

	// player controls
	vgui::Button	*m_pPlayPauseResume;
	vgui::Button	*m_pGoStart;
	vgui::Button	*m_pGoEnd;
	vgui::Button	*m_pFastForward;
	vgui::Button	*m_pFastBackward;
	vgui::Button	*m_pPrevFrame;
	vgui::Button	*m_pNextFrame;

	vgui::ProgressBar *m_pProgress;
	vgui::Label	*m_pProgressLabelFrame;
	vgui::Label	*m_pProgressLabelTime;

	vgui::Slider *m_pSpeedScale;
	vgui::Label	*m_pSpeedScaleLabel;

	vgui::DHANDLE< CDemoEditorPanel > m_hDemoEditor;
	vgui::DHANDLE< CDemoSmootherPanel > m_hDemoSmoother;
	vgui::DHANDLE< vgui::FileOpenDialog > m_hFileOpenDialog;

	vgui::Button	*m_pGo;
	vgui::TextEntry *m_pGotoTick;

	bool		m_bInputActive;
	int			m_nOldCursor[2];
	Vector		m_ViewOrigin;
	QAngle		m_ViewAngles;
};

extern CDemoUIPanel *g_pDemoUI;

//-----------------------------------------------------------------------------
// Purpose: a special demo UI panel that is always visible allowing you
// to interact with the game and adding more features to the old
// demo UI panel.
//-----------------------------------------------------------------------------
class CDemoUIPanel2 : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoUIPanel2, vgui::Frame );

public:
	CDemoUIPanel2( vgui::Panel *pParentBkgnd, vgui::Panel *pParentFgnd, bool bPutToForeground );
	~CDemoUIPanel2();

	virtual void OnTick();

	// Command issued
	virtual void OnCommand(const char *command);
	virtual void OnMessage(const KeyValues *params,  vgui::VPANEL fromPanel);

	virtual void	OnVDMChanged( void );

	virtual bool	OverrideView( democmdinfo_t& info, int frame );
	virtual void	DrawDebuggingInfo();

	static	void	Install( vgui::Panel *pParentBkgnd, vgui::Panel *pParentFgnd, bool bPutToForeground );

	bool	IsInDriveMode();
	void	SetDriveViewPoint( Vector &origin, QAngle &angle );
	void	GetDriveViewPoint( Vector &origin, QAngle &angle );

	void	MakePanelForeground( bool bPutToForeground );

protected:

	void			HandleInput( bool active );
	bool			IsHoldingFastForward();
	void			SetPlaybackScale( float scale );
	float			GetPlaybackScale();

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void		OnLoad();

	vgui::Button	*m_pStop;
	vgui::Button	*m_pLoad;

	// player controls
	vgui::Button	*m_pPlayPauseResume;
	vgui::Button	*m_pGoStart;
	vgui::Button	*m_pGoEnd;
	vgui::Button	*m_pFastForward;
	vgui::Button	*m_pFastBackward;
	vgui::Button	*m_pPrevFrame;
	vgui::Button	*m_pNextFrame;

	vgui::Slider *m_pProgress;
	vgui::Label	*m_pProgressLabelFrame;
	vgui::Label	*m_pProgressLabelTime;

	vgui::Slider *m_pSpeedScale;
	vgui::Label	*m_pSpeedScaleLabel;

	vgui::DHANDLE< vgui::FileOpenDialog > m_hFileOpenDialog;

	bool		m_bInputActive;
	int			m_nOldCursor[2];

	// Bkgnd-fgnd switch
	vgui::Panel *m_arrParents[2];
	bool		m_bIsInForeground;
};

extern CDemoUIPanel2 *g_pDemoUI2;


#endif // CL_DEMOUIPANEL_H
