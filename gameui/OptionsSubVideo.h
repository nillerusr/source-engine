//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_VIDEO_H
#define OPTIONS_SUB_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PropertyPage.h>
#include "EngineInterface.h"
#include "IGameUIFuncs.h"
#include "URLButton.h"

class CCvarSlider;

//-----------------------------------------------------------------------------
// Purpose: Video Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVideo : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubVideo, vgui::PropertyPage );

public:
	COptionsSubVideo(vgui::Panel *parent);
	~COptionsSubVideo();

	virtual void OnResetData();
	virtual void OnApplyChanges();
	virtual void PerformLayout();

	virtual bool RequiresRestart();

private:
    void        SetCurrentResolutionComboItem();
	void		EnableOrDisableWindowedForVR();

    MESSAGE_FUNC( OnDataChanged, "ControlModified" );
	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
	MESSAGE_FUNC( OpenAdvanced, "OpenAdvanced" );
	MESSAGE_FUNC( LaunchBenchmark, "LaunchBenchmark" );
	MESSAGE_FUNC( OpenGammaDialog, "OpenGammaDialog" );

   
	void		PrepareResolutionList();

	bool		BUseHDContent();
	void		SetUseHDContent( bool bUse );

	int m_nSelectedMode; // -1 if we are running in a nonstandard mode

	bool m_bDisplayedVRModeMessage;

	vgui::ComboBox		*m_pMode;
	vgui::ComboBox		*m_pWindowed;
	vgui::ComboBox		*m_pAspectRatio;
	vgui::ComboBox		*m_pVRMode;
	vgui::Button		*m_pGammaButton;
	vgui::Button		*m_pAdvanced;
	vgui::Button		*m_pBenchmark;
	vgui::CheckButton	*m_pHDContent;

	vgui::DHANDLE<class COptionsSubVideoAdvancedDlg> m_hOptionsSubVideoAdvancedDlg;
	vgui::DHANDLE<class CGammaDialog> m_hGammaDialog;

	bool m_bRequireRestart;
   MESSAGE_FUNC( OpenThirdPartyVideoCreditsDialog, "OpenThirdPartyVideoCreditsDialog" );
   vgui::URLButton   *m_pThirdPartyCredits;
   vgui::DHANDLE<class COptionsSubVideoThirdPartyCreditsDlg> m_OptionsSubVideoThirdPartyCreditsDlg;
};



#endif // OPTIONS_SUB_VIDEO_H