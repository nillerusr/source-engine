//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONSSUBMULTIPLAYER_H
#define OPTIONSSUBMULTIPLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/ImagePanel.h>
#include "imageutils.h"

class CLabeledCommandComboBox;
class CBitmapImagePanel;

class CCvarToggleCheckButton;
class CCvarTextEntry;
class CCvarSlider;

class CMultiplayerAdvancedDialog;

class COptionsSubMultiplayer;
 
class CrosshairImagePanelBase : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( CrosshairImagePanelBase, vgui::ImagePanel );
public:
	CrosshairImagePanelBase( Panel *parent, const char *name ) : BaseClass(parent, name) {}
	virtual void ResetData() {}
	virtual void ApplyChanges() {}
	virtual void UpdateVisibility() {}
};

//-----------------------------------------------------------------------------
// Purpose: multiplayer options property page
//-----------------------------------------------------------------------------
class COptionsSubMultiplayer : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubMultiplayer, vgui::PropertyPage );

public:
	COptionsSubMultiplayer(vgui::Panel *parent);
	~COptionsSubMultiplayer();

	virtual vgui::Panel *CreateControlByName(const char *controlName);

	MESSAGE_FUNC( OnControlModified, "ControlModified" );

protected:
	// Called when page is loaded.  Data should be reloaded from document into controls.
	virtual void OnResetData();
	// Called when the OK / Apply button is pressed.  Changed data should be written into document.
	virtual void OnApplyChanges();

	virtual void OnCommand( const char *command );

private:
	void InitModelList(CLabeledCommandComboBox *cb);
	void InitLogoList(CLabeledCommandComboBox *cb);

	void RemapModel();
	void RemapLogo();

	void ConversionError( ConversionErrorType nError );

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void ColorForName(char const *pszColorName, int &r, int &g, int &b);

	CBitmapImagePanel *m_pModelImage;
	CLabeledCommandComboBox *m_pModelList;
	char m_ModelName[128];

	vgui::ImagePanel *m_pLogoImage;
	CLabeledCommandComboBox *m_pLogoList;
    char m_LogoName[128];

    CCvarSlider *m_pPrimaryColorSlider;
    CCvarSlider *m_pSecondaryColorSlider;
	CCvarToggleCheckButton *m_pHighQualityModelCheckBox;

	// Mod specific general checkboxes
	vgui::Dar< CCvarToggleCheckButton * > m_cvarToggleCheckButtons;

	CCvarToggleCheckButton *m_pLockRadarRotationCheckbox;

	CrosshairImagePanelBase *m_pCrosshairImage;

	// --- client download filter
	vgui::ComboBox	*m_pDownloadFilterCombo;

	// Begin Spray Import Functions
	ConversionErrorType WriteSprayVMT(const char *vtfPath);
	void SelectLogo(const char *logoName);
	// End Spray Import Functions

	int	m_nLogoR;
	int	m_nLogoG;
	int	m_nLogoB;

#ifndef _XBOX
	vgui::DHANDLE<CMultiplayerAdvancedDialog> m_hMultiplayerAdvancedDialog;
#endif
	vgui::FileOpenDialog *m_hImportSprayDialog;
};

#endif // OPTIONSSUBMULTIPLAYER_H
