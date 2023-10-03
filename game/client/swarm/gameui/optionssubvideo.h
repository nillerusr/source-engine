//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "igameuifuncs.h"
#include "URLButton.h"
#include "vgui_controls/frame.h"


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

	MESSAGE_FUNC( OpenGammaDialog, "OpenGammaDialog" );
	static vgui::DHANDLE<class CGammaDialog> m_hGammaDialog;

private:
    void        SetCurrentResolutionComboItem();

    MESSAGE_FUNC( OnDataChanged, "ControlModified" );
	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
	MESSAGE_FUNC( OpenAdvanced, "OpenAdvanced" );
	MESSAGE_FUNC( LaunchBenchmark, "LaunchBenchmark" );

	void		PrepareResolutionList();

	int m_nSelectedMode; // -1 if we are running in a nonstandard mode

	vgui::ComboBox		*m_pMode;
	vgui::ComboBox		*m_pWindowed;
	vgui::ComboBox		*m_pAspectRatio;
	vgui::Button		*m_pGammaButton;
	vgui::Button		*m_pAdvanced;
	vgui::Button		*m_pBenchmark;

	vgui::DHANDLE<class COptionsSubVideoAdvancedDlg> m_hOptionsSubVideoAdvancedDlg;

	bool m_bRequireRestart;
   MESSAGE_FUNC( OpenThirdPartyVideoCreditsDialog, "OpenThirdPartyVideoCreditsDialog" );
   vgui::URLButton   *m_pThirdPartyCredits;
   vgui::DHANDLE<class COptionsSubVideoThirdPartyCreditsDlg> m_OptionsSubVideoThirdPartyCreditsDlg;
};

class COptionsSubVideoThirdPartyCreditsDlg : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( COptionsSubVideoThirdPartyCreditsDlg, vgui::Frame );
public:
	COptionsSubVideoThirdPartyCreditsDlg( vgui::VPANEL hParent );

	virtual void Activate();
	void OnKeyCodeTyped(vgui::KeyCode code);

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};


#endif // OPTIONS_SUB_VIDEO_H