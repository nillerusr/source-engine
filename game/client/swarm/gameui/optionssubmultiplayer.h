//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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

class CLabeledCommandComboBox;
class CBitmapImagePanel;

class CCvarToggleCheckButton;
class CCvarTextEntry;
class CCvarSlider;

class CrosshairImagePanel;
class CMultiplayerAdvancedDialog;

class AdvancedCrosshairImagePanel;

enum ConversionErrorType
{
	CE_SUCCESS,
	CE_MEMORY_ERROR,
	CE_CANT_OPEN_SOURCE_FILE,
	CE_ERROR_PARSING_SOURCE,
	CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED,
	CE_ERROR_WRITING_OUTPUT_FILE,
	CE_ERROR_LOADING_DLL
};

struct TGAHeader {
	byte  identsize;          // size of ID field that follows 18 byte header (0 usually)
	byte  colourmaptype;      // type of colour map 0=none, 1=has palette
	byte  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	short colourmapstart;     // first colour map entry in palette
	short colourmaplength;    // number of colours in palette
	byte  colourmapbits;      // number of bits per palette entry 15,16,24,32

	short xstart;             // image x origin
	short ystart;             // image y origin
	short width;              // image width in pixels
	short height;             // image height in pixels
	byte  bits;               // image bits per pixel 8,16,24,32
	byte  descriptor;         // image descriptor bits (vh flip bits)
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

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );
	MESSAGE_FUNC( OnApplyButtonEnable, "ControlModified" );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void RemapPalette(char *filename, int topcolor, int bottomcolor);

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

	// --- crosshair controls ----------------------------------
	CLabeledCommandComboBox *m_pCrosshairSize;
	void InitCrosshairSizeList(CLabeledCommandComboBox *cb);

	CCvarToggleCheckButton *m_pCrosshairTranslucencyCheckbox;

	vgui::ComboBox *m_pCrosshairColorComboBox;
	void InitCrosshairColorEntries();
	void ApplyCrosshairColorChanges();

	CrosshairImagePanel *m_pCrosshairImage;

	// called when the appearance of the crosshair has changed.
	void RedrawCrosshairImage();
	// ---------------------------------------------------------

	// --- advanced crosshair controls
	AdvancedCrosshairImagePanel *m_pAdvCrosshairImage;
	CCvarSlider *m_pAdvCrosshairRedSlider;		
	CCvarSlider *m_pAdvCrosshairBlueSlider;
	CCvarSlider *m_pAdvCrosshairGreenSlider;
	CCvarSlider *m_pAdvCrosshairScaleSlider;
	CLabeledCommandComboBox *m_pAdvCrosshairStyle;

	void InitAdvCrosshairStyleList(CLabeledCommandComboBox *cb);
	void RedrawAdvCrosshairImage();
	// -----

	// --- client download filter
	vgui::ComboBox	*m_pDownloadFilterCombo;

	// Begin Spray Import Functions
	ConversionErrorType ConvertJPEGToTGA(const char *jpgPath, const char *tgaPath);
	ConversionErrorType ConvertBMPToTGA(const char *bmpPath, const char *tgaPath);
	ConversionErrorType ConvertTGA(const char *tgaPath);
	unsigned char *ReadTGAAsRGBA(const char *tgaPath, int &width, int &height, ConversionErrorType &errcode, TGAHeader &tgaHeader);
	ConversionErrorType StretchRGBAImage(const unsigned char *srcBuf, const int srcWidth, const int srcHeight, unsigned char *destBuf, const int destWidth, const int destHeight);
	ConversionErrorType PadRGBAImage(const unsigned char *srcBuf, const int srcWidth, const int srcHeight, unsigned char *destBuf, const int destWidth, const int destHeight);
	ConversionErrorType ConvertTGAToVTF(const char *tgaPath);
	ConversionErrorType WriteSprayVMT(const char *vtfPath);
	void SelectLogo(const char *logoName);
	// End Spray Import Functions

	int	m_nTopColor;
	int	m_nBottomColor;

	int	m_nLogoR;
	int	m_nLogoG;
	int	m_nLogoB;

#ifndef _XBOX
	vgui::DHANDLE<CMultiplayerAdvancedDialog> m_hMultiplayerAdvancedDialog;
#endif
	vgui::FileOpenDialog *m_hImportSprayDialog;
};

#endif // OPTIONSSUBMULTIPLAYER_H
