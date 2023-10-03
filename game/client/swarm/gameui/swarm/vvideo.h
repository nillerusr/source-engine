//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VVIDEO_H__
#define __VVIDEO_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"
#include "OptionsSubVideo.h"


#define MAX_DYNAMIC_AA_MODES 10
#define MAX_DYNAMIC_VIDEO_MODES 15

class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;


struct AAMode_t
{
	int m_nNumSamples;
	int m_nQualityLevel;
};

struct ResolutionMode_t
{
	int m_nWidth;
	int m_nHeight;
};


class Video : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( Video, CBaseModFrame );

public:
	Video(vgui::Panel *parent, const char *panelName);
	~Video();

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	virtual void PerformLayout();

	Panel* NavigateBack();

	void OpenPagedPoolMem( void );

protected:
	virtual void Activate( bool bRecommendedSettings = false );
	virtual void OnThink();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );

	int FindMSAAMode( int nAASamples, int nAAQuality );
	void PrepareResolutionList();
	void ApplyChanges();

private:
	void SetupActivateData( void );
	bool SetupRecommendedActivateData( void );
	void UpdateFooter();

	void OpenThirdPartyVideoCreditsDialog();

private:
	int					m_nNumAAModes;
	AAMode_t			m_nAAModes[ MAX_DYNAMIC_AA_MODES ];

	int					m_nNumResolutionModes;
	ResolutionMode_t	m_nResolutionModes[ MAX_DYNAMIC_VIDEO_MODES ];

	DropDownMenu		*m_drpAspectRatio;
	DropDownMenu		*m_drpResolution;
	DropDownMenu		*m_drpDisplayMode;
	DropDownMenu		*m_drpLockMouse;
	SliderControl		*m_sldFilmGrain;

	BaseModHybridButton	*m_btnAdvanced;

	DropDownMenu		*m_drpModelDetail;
	DropDownMenu		*m_drpPagedPoolMem;
	DropDownMenu		*m_drpAntialias;
	DropDownMenu		*m_drpFiltering;
	DropDownMenu		*m_drpVSync;
	DropDownMenu		*m_drpQueuedMode;
	DropDownMenu		*m_drpShaderDetail;
	DropDownMenu		*m_drpCPUDetail;

	CNB_Header_Footer *m_pHeaderFooter;

	BaseModHybridButton	*m_btnUseRecommended;
	BaseModHybridButton	*m_btnCancel;
	BaseModHybridButton	*m_btnDone;

	BaseModHybridButton	*m_btn3rdPartyCredits;

	int iBtnUseRecommendedYPos;
	int iBtnCancelYPos;
	int iBtnDoneYPos;

	bool	m_bDirtyValues;
	int		m_iAspectRatio;
	int		m_iResolutionWidth;
	int		m_iResolutionHeight;
	bool	m_bWindowed;
	bool	m_bNoBorder;
	int		m_iModelTextureDetail;
	int		m_iPagedPoolMem;
	int		m_iAntiAlias;
	int		m_iFiltering;
	int		m_nAASamples;
	int		m_nAAQuality;
	bool	m_bVSync;
	bool	m_bTripleBuffered;
	int		m_iQueuedMode;
	int		m_iGPUDetail;
	int		m_iCPUDetail;
	int		m_flFilmGrain;
	bool	m_bLockMouse;

	float	m_flFilmGrainInitialValue;

	vgui::DHANDLE<class COptionsSubVideoThirdPartyCreditsDlg> m_OptionsSubVideoThirdPartyCreditsDlg;
};

};

#endif // __VVIDEO_H__
