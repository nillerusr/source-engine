//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h> // SRC only!!
#endif

#include "OptionsSubMultiplayer.h"
#include "MultiplayerAdvancedDialog.h"
#include <stdio.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/CheckButton.h>
#include "tier1/KeyValues.h"
#include <vgui_controls/Label.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/Cursor.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui_controls/MessageBox.h>

#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "cvarslider.h"
#include "LabeledCommandComboBox.h"
#include "filesystem.h"
#include "EngineInterface.h"
#include "BitmapImagePanel.h"
#include "tier1/utlbuffer.h"
#include "ModInfo.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"

// use the JPEGLIB_USE_STDIO define so that we can read in jpeg's from outside the game directory tree.  For Spray Import.
#define JPEGLIB_USE_STDIO
#include "jpeglib/jpeglib.h"
#undef JPEGLIB_USE_STDIO

#include <setjmp.h>

#include "bitmap/tgawriter.h"
#include "ivtex.h"
#ifdef WIN32
#include <io.h>
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


#define DEFAULT_SUIT_HUE 30
#define DEFAULT_PLATE_HUE 6

void UpdateLogoWAD( void *hdib, int r, int g, int b );

struct ColorItem_t
{
	const char	*name;
	int			r, g, b;
};

static ColorItem_t itemlist[]=
{
	{ "#Valve_Orange", 255, 120, 24 },
	{ "#Valve_Yellow", 225, 180, 24 },
	{ "#Valve_Blue", 0, 60, 255 },
	{ "#Valve_Ltblue", 0, 167, 255 },
	{ "#Valve_Green", 0, 167, 0 },
	{ "#Valve_Red", 255, 43, 0 },
	{ "#Valve_Brown", 123, 73, 0 },
	{ "#Valve_Ltgray", 100, 100, 100 },
	{ "#Valve_Dkgray", 36, 36, 36 },
};

static ColorItem_t s_crosshairColors[] = 
{
	{ "#Valve_Green",	50,		250,	50 },
	{ "#Valve_Red",		250,	50,		50 },
	{ "#Valve_Blue",	50,		50,		250 },
	{ "#Valve_Yellow",	250,	250,	50 },
	{ "#Valve_Ltblue",	50,		250,	250 },
};
static const int NumCrosshairColors = ARRAYSIZE(s_crosshairColors);


//-----------------------------------------------------------------------------
class CrosshairImagePanelSimple : public CrosshairImagePanelBase
{
	DECLARE_CLASS_SIMPLE( CrosshairImagePanelSimple, CrosshairImagePanelBase );
public:
	CrosshairImagePanelSimple( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel );
	virtual void Paint();
	virtual void ResetData();
	virtual void ApplyChanges();
	static void DrawCrosshairRect( int x, int y, int w, int h, bool bAdditive );

protected:
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	void InitCrosshairColorEntries();
	void InitCrosshairSizeList();
	void UpdateCrosshair();

private:
	COptionsSubMultiplayer* m_pOptionsPanel;
	vgui::ComboBox *m_pCrosshairColorCombo;
	CLabeledCommandComboBox *m_pCrosshairSizeCombo;
	CCvarToggleCheckButton *m_pCrosshairTranslucencyCheckbox;
	int m_R, m_G, m_B;
	int m_barSize;
	int m_barGap;
	int m_iCrosshairTextureID;
};

//-----------------------------------------------------------------------------
CrosshairImagePanelSimple::CrosshairImagePanelSimple( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel ) : CrosshairImagePanelBase( parent, name )
{
	m_pOptionsPanel = pOptionsPanel;
	m_pCrosshairTranslucencyCheckbox = new CCvarToggleCheckButton(m_pOptionsPanel, "CrosshairTranslucencyCheckbox", "#GameUI_Translucent", "cl_crosshairusealpha");
	m_pCrosshairColorCombo = new ComboBox(m_pOptionsPanel, "CrosshairColorComboBox", 6, false);
	m_pCrosshairSizeCombo = new CLabeledCommandComboBox(m_pOptionsPanel, "CrosshairSizeComboBox");

	m_pCrosshairColorCombo->AddActionSignalTarget( this );
	m_pCrosshairSizeCombo->AddActionSignalTarget( this );

	m_iCrosshairTextureID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iCrosshairTextureID, "vgui/white_additive" , true, false);

	InitCrosshairSizeList();
	InitCrosshairColorEntries();
	ResetData();
}

//-----------------------------------------------------------------------------
// Purpose: initialize the crosshair size list.
//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::InitCrosshairSizeList()
{
	// add in the auto, small, medium, and large size selections.
	m_pCrosshairSizeCombo->AddItem("#GameUI_Auto", "cl_crosshairscale 0");
	m_pCrosshairSizeCombo->AddItem("#GameUI_Small", "cl_crosshairscale 1200");
	m_pCrosshairSizeCombo->AddItem("#GameUI_Medium", "cl_crosshairscale 768");
	m_pCrosshairSizeCombo->AddItem("#GameUI_Large", "cl_crosshairscale 600");
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::InitCrosshairColorEntries()
{
	if (m_pCrosshairColorCombo != NULL)
	{
		KeyValues *data = new KeyValues("data");

		// add in the "Default" selection
		data->Clear();

		// add in the colors for the color list
		for ( int i = 0; i < NumCrosshairColors; i++ )
		{
			data->SetInt("color", i);
			m_pCrosshairColorCombo->AddItem( s_crosshairColors[ i ].name, data);
		}

		data->deleteThis();
	}
}


//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::DrawCrosshairRect( int x0, int y0, int x1, int y1, bool bAdditive )
{
	if ( bAdditive )
		vgui::surface()->DrawTexturedRect( x0, y0, x1, y1 );
	else
		vgui::surface()->DrawFilledRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::Paint()
{
	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );;

	BaseClass::Paint();

	if ( !m_pCrosshairTranslucencyCheckbox )
		return;

	int wide, tall;
	GetSize( wide, tall );

	bool bAdditive = !m_pCrosshairTranslucencyCheckbox->IsSelected();

	int a = 200;
	ConVarRef cl_crosshairalpha( "cl_crosshairalpha", true );
	if ( !bAdditive && cl_crosshairalpha.IsValid() )
	{
		a = clamp( cl_crosshairalpha.GetInt(), 0, 255 );
	}
	vgui::surface()->DrawSetColor( m_R, m_G, m_B, a );

	if ( bAdditive )
	{
		vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
	}

	int centerX = wide / 2;
	int centerY = tall / 2;
	int iCrosshairDistance = m_barGap;

	int iBarThickness = 1;
	int iBarSize = m_barSize;

	// draw horizontal crosshair lines
	int iInnerLeft	= centerX - iCrosshairDistance - iBarThickness / 2;
	int iInnerRight	= iInnerLeft + 2 * iCrosshairDistance + iBarThickness;
	int iOuterLeft	= iInnerLeft - iBarSize;
	int iOuterRight	= iInnerRight + iBarSize;
	int y0 = centerY - iBarThickness / 2;
	int y1 = y0 + iBarThickness;
	DrawCrosshairRect( iOuterLeft, y0, iInnerLeft, y1, bAdditive );
	DrawCrosshairRect( iInnerRight, y0, iOuterRight, y1, bAdditive );

	// draw vertical crosshair lines
	int iInnerTop		= centerY - iCrosshairDistance - iBarThickness / 2;
	int iInnerBottom	= iInnerTop + 2 * iCrosshairDistance + iBarThickness;
	int iOuterTop		= iInnerTop - iBarSize;
	int iOuterBottom	= iInnerBottom + iBarSize;
	int x0 = centerX - iBarThickness / 2;
	int x1 = x0 + iBarThickness;
	DrawCrosshairRect( x0, iOuterTop, x1, iInnerTop, bAdditive );
	DrawCrosshairRect( x0, iInnerBottom, x1, iOuterBottom, bAdditive );
}


//-----------------------------------------------------------------------------
// Purpose: takes the settings from the crosshair settings combo boxes and sliders
//          and apply it to the crosshair illustrations.
//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::UpdateCrosshair()
{
	// get the color selected in the combo box.
	KeyValues *data = m_pCrosshairColorCombo->GetActiveItemUserData();
	int colorIndex = data->GetInt("color");
	colorIndex = clamp( colorIndex, 0, NumCrosshairColors );

	int selectedColor = 0;
	int actualVal = 0;
	if (m_pCrosshairColorCombo != NULL)
	{
		selectedColor = m_pCrosshairColorCombo->GetActiveItem();
	}

	ConVarRef cl_crosshaircolor( "cl_crosshaircolor", true );
	if ( cl_crosshaircolor.IsValid() )
	{
		actualVal = clamp( cl_crosshaircolor.GetInt(), 0, NumCrosshairColors );
	}

	m_R = s_crosshairColors[selectedColor].r;
	m_G = s_crosshairColors[selectedColor].g;
	m_B = s_crosshairColors[selectedColor].b;

	if ( m_pCrosshairSizeCombo )
	{
		int size = m_pCrosshairSizeCombo->GetActiveItem();

		if ( size == 0 )
		{
			int screenWide, screenTall;
			surface()->GetScreenSize( screenWide, screenTall );
			if ( screenTall <= 600 )
			{
				// if the screen height is 600 or less, set the crosshair num to 3 (large)
				size = 3;
			}
			else if ( screenTall <= 768 )
			{
				// if the screen height is between 600 and 768, set the crosshair num to 2 (medium)
				size = 2;
			}
			else
			{
				// if the screen height is greater than 768, set the crosshair num to 1 (small)
				size = 1;
			}
		}

		int scaleBase;
		switch( size )
		{
		case 1:
			scaleBase = 1200;
			break;
		case 2:
			scaleBase = 768;
			break;
		case 3:
			scaleBase = 600;
			break;
		default:
			scaleBase = 1200;
			break;
		}

		m_barSize = (int) 9 * 1200 / scaleBase;
		m_barGap = (int) 5 * 1200 / scaleBase;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever color combo changes
//-----------------------------------------------------------------------------
void CrosshairImagePanelSimple::OnTextChanged(vgui::Panel *panel)
{
	m_pOptionsPanel->OnControlModified();
	UpdateCrosshair();
}


void CrosshairImagePanelSimple::ResetData()
{
	m_pCrosshairTranslucencyCheckbox->Reset();

	// parse out the size value from the cvar and set the initial value.
	int initialScale = 0;
	ConVarRef cl_crosshairscale( "cl_crosshairscale", true );
	if ( cl_crosshairscale.IsValid() )
	{
		initialScale = cl_crosshairscale.GetInt();
		if ( initialScale <= 0 )
		{
			initialScale = 0;
		}
		else if ( initialScale <= 600 )
		{
			initialScale = 3;
		}
		else if ( initialScale <= 768 )
		{
			initialScale = 2;
		}
		else
		{
			initialScale = 1;
		}
	}
	m_pCrosshairSizeCombo->SetInitialItem( initialScale );

	// parse the string for the custom color settings and get the initial settings.
	ConVarRef cl_crosshaircolor( "cl_crosshaircolor", true );
	int index = 0;
	if ( cl_crosshaircolor.IsValid() )
	{
		index = clamp( cl_crosshaircolor.GetInt(), 0, NumCrosshairColors );
	}
	m_pCrosshairColorCombo->ActivateItemByRow(index);

	UpdateCrosshair();
}

void CrosshairImagePanelSimple::ApplyChanges()
{
	m_pCrosshairTranslucencyCheckbox->ApplyChanges();

	char cmd[256];
	cmd[0] = 0;

	if (m_pCrosshairColorCombo != NULL)
	{
		int val = m_pCrosshairColorCombo->GetActiveItem();
		Q_snprintf( cmd, sizeof(cmd), "cl_crosshaircolor %d\n", val );
		engine->ClientCmd_Unrestricted( cmd );
	}
}


//-----------------------------------------------------------------------------
class CrosshairImagePanelCS : public CrosshairImagePanelBase
{
	DECLARE_CLASS_SIMPLE( CrosshairImagePanelCS, CrosshairImagePanelBase );

public:
	CrosshairImagePanelCS( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel );
	virtual void ResetData();
	virtual void ApplyChanges();

protected:
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" );

	virtual void Paint();
	static void DrawCrosshairRect( int x, int y, int w, int h, bool bAdditive );
	void InitCrosshairColorEntries();
	void UpdateCrosshair();

private:
	COptionsSubMultiplayer* m_pOptionsPanel;
	vgui::ComboBox *m_pColorComboBox;
	CCvarToggleCheckButton *m_pAlphaCheckbox;
	CCvarToggleCheckButton *m_pDynamicCheckbox;
	CCvarToggleCheckButton *m_pDotCheckbox;
	CCvarSlider *m_pColorAlphaSlider;
	CCvarSlider *m_pColorRSlider;
	CCvarSlider *m_pColorGSlider;
	CCvarSlider *m_pColorBSlider;
	CCvarSlider *m_pSizeSlider;
	CCvarSlider *m_pThicknessSlider;
	int m_R, m_G, m_B;
	float m_barSize;
	float m_barThickness;
	int m_iCrosshairTextureID;
};

//-----------------------------------------------------------------------------
CrosshairImagePanelCS::CrosshairImagePanelCS( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel ) : CrosshairImagePanelBase( parent, name )
{
	m_pOptionsPanel = pOptionsPanel;
	m_pColorComboBox = new ComboBox(m_pOptionsPanel, "CrosshairColorComboBox", 6, false);
	m_pAlphaCheckbox = new CCvarToggleCheckButton(m_pOptionsPanel, "CrosshairTranslucencyCheckbox", "#GameUI_Crosshair_Blend", "cl_crosshairusealpha");
	m_pDynamicCheckbox = new CCvarToggleCheckButton(m_pOptionsPanel, "CrosshairDynamicCheckbox", "#GameUI_CrosshairDynamic", "cl_dynamiccrosshair");
	m_pDotCheckbox = new CCvarToggleCheckButton(m_pOptionsPanel, "CrosshairDotCheckbox", "#GameUI_CrosshairDot", "cl_crosshairdot");
	m_pColorAlphaSlider = new CCvarSlider( m_pOptionsPanel, "Alpha Slider", "#GameUI_CrosshairColor_Alpha",
		0.0f, 255.0f, "cl_crosshairalpha" );
	m_pColorRSlider = new CCvarSlider( m_pOptionsPanel, "Red Color Slider", "#GameUI_CrosshairColor_Red",
		0.0f, 255.0f, "cl_crosshaircolor_r" );
	m_pColorGSlider = new CCvarSlider( m_pOptionsPanel, "Green Color Slider", "#GameUI_CrosshairColor_Green",
		0.0f, 255.0f, "cl_crosshaircolor_g" );
	m_pColorBSlider = new CCvarSlider( m_pOptionsPanel, "Blue Color Slider", "#GameUI_CrosshairColor_Blue",
		0.0f, 255.0f, "cl_crosshaircolor_b" );
	m_pSizeSlider = new CCvarSlider( m_pOptionsPanel, "Size Slider", "#GameUI_Crosshair_Size",
		0.0f, 12.0f, "cl_crosshairsize" );
	m_pThicknessSlider = new CCvarSlider( m_pOptionsPanel, "Thickness Slider", "#GameUI_Crosshair_Thickness",
		0.0f, 3.0f, "cl_crosshairthickness" );

	m_pColorAlphaSlider->SetTickCaptions("", "");
	m_pColorRSlider->SetTickCaptions("", "");
	m_pColorGSlider->SetTickCaptions("", "");
	m_pColorBSlider->SetTickCaptions("", "");
	m_pSizeSlider->SetTickCaptions("", "");
	m_pThicknessSlider->SetTickCaptions("", "");

	m_pAlphaCheckbox->AddActionSignalTarget(this);
	m_pDynamicCheckbox->AddActionSignalTarget(this);
	m_pDotCheckbox->AddActionSignalTarget(this);
	m_pColorComboBox->AddActionSignalTarget( this );
	m_pColorAlphaSlider->AddActionSignalTarget( this );
	m_pColorRSlider->AddActionSignalTarget( this );
	m_pColorGSlider->AddActionSignalTarget( this );
	m_pColorBSlider->AddActionSignalTarget( this );
	m_pSizeSlider->AddActionSignalTarget( this );
	m_pThicknessSlider->AddActionSignalTarget( this );

	InitCrosshairColorEntries();

	m_iCrosshairTextureID = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_iCrosshairTextureID, "vgui/white_additive" , true, false);

	ResetData();
}

void CrosshairImagePanelCS::InitCrosshairColorEntries()
{
	if (m_pColorComboBox != NULL)
	{
		KeyValues *data = new KeyValues("data");

		// add in the "Default" selection
		data->Clear();

		// add in the colors for the color list
		for ( int i = 0; i < NumCrosshairColors; i++ )
		{
			data->SetInt("color", i);
			m_pColorComboBox->AddItem( s_crosshairColors[ i ].name, data);
		}

		data->SetInt("color", NumCrosshairColors);
		m_pColorComboBox->AddItem( "Custom", data);

		data->deleteThis();
	}
}

//-----------------------------------------------------------------------------
void CrosshairImagePanelCS::DrawCrosshairRect( int x0, int y0, int x1, int y1, bool bAdditive )
{
	if ( bAdditive )
		vgui::surface()->DrawTexturedRect( x0, y0, x1, y1 );
	else
		vgui::surface()->DrawFilledRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
void CrosshairImagePanelCS::Paint()
{
	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );;

	BaseClass::Paint();

	int wide, tall;
	GetSize( wide, tall );

	bool bAdditive = !m_pAlphaCheckbox->IsSelected();
	bool bDynamic = m_pDynamicCheckbox->IsSelected();

	int a = 255;
	if ( !bAdditive )
		a = m_pColorAlphaSlider->GetSliderValue();

	vgui::surface()->DrawSetColor( m_R, m_G, m_B, a );

	if ( bAdditive )
	{
		vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
	}

	int centerX = wide / 2;
	int centerY = tall / 2;

	int iBarSize = RoundFloatToInt(m_barSize * screenTall / 480.0f);
	int iBarThickness = max(1, RoundFloatToInt(m_barThickness * (float)screenTall / 480.0f));

	float fBarGap = 4.0f;
	if ( bDynamic )
	{
		float curtime = system()->GetFrameTime();
		fBarGap *= (1.0f + cosf(curtime * 1.5f) * 0.5f);
	}

	int iBarGap = RoundFloatToInt(fBarGap * screenTall / 480.0f);

	// draw horizontal crosshair lines
	int iInnerLeft	= centerX - iBarGap - iBarThickness / 2;
	int iInnerRight	= iInnerLeft + 2 * iBarGap + iBarThickness;
	int iOuterLeft	= iInnerLeft - iBarSize;
	int iOuterRight	= iInnerRight + iBarSize;
	int y0 = centerY - iBarThickness / 2;
	int y1 = y0 + iBarThickness;
	DrawCrosshairRect( iOuterLeft, y0, iInnerLeft, y1, bAdditive );
	DrawCrosshairRect( iInnerRight, y0, iOuterRight, y1, bAdditive );

	// draw vertical crosshair lines
	int iInnerTop		= centerY - iBarGap - iBarThickness / 2;
	int iInnerBottom	= iInnerTop + 2 * iBarGap + iBarThickness;
	int iOuterTop		= iInnerTop - iBarSize;
	int iOuterBottom	= iInnerBottom + iBarSize;
	int x0 = centerX - iBarThickness / 2;
	int x1 = x0 + iBarThickness;
	DrawCrosshairRect( x0, iOuterTop, x1, iInnerTop, bAdditive );
	DrawCrosshairRect( x0, iInnerBottom, x1, iOuterBottom, bAdditive );

	// draw dot
	if ( m_pDotCheckbox->IsSelected() )
	{
		x0 = centerX - iBarThickness / 2;
		x1 = x0 + iBarThickness;
		y0 = centerY - iBarThickness / 2;
		y1 = y0 + iBarThickness;
		DrawCrosshairRect( x0, y0, x1, y1, bAdditive );
	}
}


//-----------------------------------------------------------------------------
// Purpose: takes the settings from the crosshair settings combo boxes and sliders
//          and apply it to the crosshair illustrations.
//-----------------------------------------------------------------------------
void CrosshairImagePanelCS::UpdateCrosshair()
{
	// get the color selected in the combo box.
	KeyValues *data = m_pColorComboBox->GetActiveItemUserData();
	int colorIndex = data->GetInt("color");
	colorIndex = clamp( colorIndex, 0, NumCrosshairColors + 1 );

	int actualVal = 0;
	int selectedColor = m_pColorComboBox->GetActiveItem();

	ConVarRef cl_crosshaircolor( "cl_crosshaircolor", true );
	if ( cl_crosshaircolor.IsValid() )
	{
		actualVal = clamp( cl_crosshaircolor.GetInt(), 0, NumCrosshairColors + 1 );
	}

	if ( selectedColor != NumCrosshairColors )	// not custom
	{
		m_R = s_crosshairColors[selectedColor].r;
		m_G = s_crosshairColors[selectedColor].g;
		m_B = s_crosshairColors[selectedColor].b;
		m_pColorRSlider->SetSliderValue(m_R);
		m_pColorGSlider->SetSliderValue(m_G);
		m_pColorBSlider->SetSliderValue(m_B);
	}
	else
	{
		m_R = clamp( m_pColorRSlider->GetSliderValue(), 0, 255 );
		m_G = clamp( m_pColorGSlider->GetSliderValue(), 0, 255 );
		m_B = clamp( m_pColorBSlider->GetSliderValue(), 0, 255 );
	}

	m_barSize = m_pSizeSlider->GetSliderValue();
	m_barThickness = m_pThicknessSlider->GetSliderValue();
}


void CrosshairImagePanelCS::OnSliderMoved(KeyValues *data)
{
	vgui::Panel* pPanel = static_cast<vgui::Panel*>(data->GetPtr("panel"));

	if ( pPanel == m_pColorRSlider || pPanel == m_pColorGSlider || pPanel == m_pColorBSlider )
	{
		m_pColorComboBox->ActivateItem(NumCrosshairColors);
	}
	m_pOptionsPanel->OnControlModified();

	UpdateCrosshair();
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever color combo changes
//-----------------------------------------------------------------------------
void CrosshairImagePanelCS::OnTextChanged(vgui::Panel *panel)
{
	m_pOptionsPanel->OnControlModified();
	UpdateCrosshair();
}

void CrosshairImagePanelCS::OnCheckButtonChecked()
{
	m_pColorAlphaSlider->SetEnabled(m_pAlphaCheckbox->IsSelected());
	m_pOptionsPanel->OnControlModified();
	UpdateCrosshair();
}

void CrosshairImagePanelCS::ResetData()
{
	// parse the string for the custom color settings and get the initial settings.
	ConVarRef cl_crosshaircolor( "cl_crosshaircolor", true );
	int index = 0;
	if ( cl_crosshaircolor.IsValid() )
	{
		index = clamp( cl_crosshaircolor.GetInt(), 0, NumCrosshairColors + 1);
	}
	m_pColorComboBox->ActivateItemByRow(index);

	m_pAlphaCheckbox->Reset();
	m_pDynamicCheckbox->Reset();
	m_pDotCheckbox->Reset();
	m_pColorRSlider->Reset();
	m_pColorGSlider->Reset();
	m_pColorBSlider->Reset();
	m_pColorAlphaSlider->Reset();
	m_pSizeSlider->Reset();
	m_pThicknessSlider->Reset();

	UpdateCrosshair();
}

void CrosshairImagePanelCS::ApplyChanges()
{
	m_pAlphaCheckbox->ApplyChanges();
	m_pDynamicCheckbox->ApplyChanges();
	m_pDotCheckbox->ApplyChanges();
	m_pColorRSlider->ApplyChanges();
	m_pColorGSlider->ApplyChanges();
 	m_pColorBSlider->ApplyChanges();
	m_pColorAlphaSlider->ApplyChanges();
	m_pSizeSlider->ApplyChanges();
	m_pThicknessSlider->ApplyChanges();

	char cmd[256];
	cmd[0] = 0;

	if (m_pColorComboBox != NULL)
	{
		int val = m_pColorComboBox->GetActiveItem();
		Q_snprintf( cmd, sizeof(cmd), "cl_crosshaircolor %d\n", val );
		engine->ClientCmd_Unrestricted( cmd );
	}
}

//-----------------------------------------------------------------------------
class CrosshairImagePanelAdvanced : public CrosshairImagePanelBase
{
	DECLARE_CLASS_SIMPLE( CrosshairImagePanelAdvanced, CrosshairImagePanelBase );
public:
	CrosshairImagePanelAdvanced( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel );
	virtual ~CrosshairImagePanelAdvanced();

	virtual void ResetData();
	virtual void ApplyChanges();
	virtual void UpdateVisibility();

protected:
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );

	virtual void Paint();
	static void DrawCrosshairRect( int x, int y, int w, int h, bool bAdditive );
	void InitAdvCrosshairStyleList();
	void SetCrosshairTexture( const char *crosshairname );
	void UpdateCrosshair();

private:
	COptionsSubMultiplayer* m_pOptionsPanel;

	// --- advanced crosshair controls
	CCvarSlider *m_pAdvCrosshairRedSlider;		
	CCvarSlider *m_pAdvCrosshairBlueSlider;
	CCvarSlider *m_pAdvCrosshairGreenSlider;
	CCvarSlider *m_pAdvCrosshairScaleSlider;

	CLabeledCommandComboBox *m_pAdvCrosshairStyle;

	int m_R, m_G, m_B;
	float m_flScale;

	// material
	int				m_iCrosshairTextureID;
	IVguiMatInfo	*m_pAdvCrosshairMaterial;

	// animation
	IVguiMatInfoVar	*m_pFrameVar;
	float			m_flNextFrameChange;
	int				m_nNumFrames;
	bool			m_bAscending;	// animating forward or in reverse?
};

//-----------------------------------------------------------------------------
CrosshairImagePanelAdvanced::CrosshairImagePanelAdvanced( Panel *parent, const char *name, COptionsSubMultiplayer* pOptionsPanel ) : CrosshairImagePanelBase( parent, name )
{
	m_pOptionsPanel = pOptionsPanel;
	m_pAdvCrosshairMaterial = NULL;
	m_pFrameVar = NULL;

	m_pAdvCrosshairRedSlider = new CCvarSlider( pOptionsPanel, "Red Color Slider", "#GameUI_CrosshairColor_Red",
		0.0f, 255.0f, "cl_crosshair_red" );
	m_pAdvCrosshairGreenSlider = new CCvarSlider( pOptionsPanel, "Green Color Slider", "#GameUI_CrosshairColor_Green",
		0.0f, 255.0f, "cl_crosshair_green" );
	m_pAdvCrosshairBlueSlider = new CCvarSlider( pOptionsPanel, "Blue Color Slider", "#GameUI_CrosshairColor_Blue",
		0.0f, 255.0f, "cl_crosshair_blue" );

	m_pAdvCrosshairRedSlider->SetTickCaptions("", "");
	m_pAdvCrosshairGreenSlider->SetTickCaptions("", "");
	m_pAdvCrosshairBlueSlider->SetTickCaptions("", "");

	m_pAdvCrosshairScaleSlider = new CCvarSlider( pOptionsPanel, "Scale Slider", "#GameUI_CrosshairScale",
		16.0f, 48.0f, "cl_crosshair_scale" );

	m_pAdvCrosshairStyle = new CLabeledCommandComboBox( pOptionsPanel, "AdvCrosshairList" );

	m_pAdvCrosshairRedSlider->AddActionSignalTarget( this );
	m_pAdvCrosshairGreenSlider->AddActionSignalTarget( this );
	m_pAdvCrosshairBlueSlider->AddActionSignalTarget( this );
	m_pAdvCrosshairScaleSlider->AddActionSignalTarget( this );
	m_pAdvCrosshairStyle->AddActionSignalTarget(this);

	InitAdvCrosshairStyleList();

	m_iCrosshairTextureID = vgui::surface()->CreateNewTextureID();
	SetCrosshairTexture("vgui/crosshairs/crosshair1");

	UpdateCrosshair();
}

CrosshairImagePanelAdvanced::~CrosshairImagePanelAdvanced()
{
	if ( m_pFrameVar )
	{
		delete m_pFrameVar;
		m_pFrameVar = NULL;
	}

	if ( m_pAdvCrosshairMaterial )
	{
		delete m_pAdvCrosshairMaterial;
		m_pAdvCrosshairMaterial = NULL;
	}
}

//-----------------------------------------------------------------------------
void CrosshairImagePanelAdvanced::SetCrosshairTexture( const char *crosshairname )
{
	if ( !crosshairname || !crosshairname[0] )
	{
		SetVisible( false );
		return;
	}

	SetVisible( true );

	vgui::surface()->DrawSetTextureFile( m_iCrosshairTextureID, crosshairname, true, false );

	if ( m_pAdvCrosshairMaterial )
	{
		delete m_pAdvCrosshairMaterial;
	}

	m_pAdvCrosshairMaterial = vgui::surface()->DrawGetTextureMatInfoFactory( m_iCrosshairTextureID );

	Assert(m_pAdvCrosshairMaterial);

	m_pFrameVar = m_pAdvCrosshairMaterial->FindVarFactory( "$frame", NULL );
	m_nNumFrames = m_pAdvCrosshairMaterial->GetNumAnimationFrames();

	m_flNextFrameChange = system()->GetFrameTime() + 0.2;
	m_bAscending = true;
}

//-----------------------------------------------------------------------------
void CrosshairImagePanelAdvanced::Paint()
{
	BaseClass::Paint();

	int wide, tall;
	GetSize( wide, tall );

	int iClipX0, iClipY0, iClipX1, iClipY1;
	ipanel()->GetClipRect(GetVPanel(), iClipX0, iClipY0, iClipX1, iClipY1 );

	// scroll through all frames
	if ( m_pFrameVar )
	{	
		float curtime = system()->GetFrameTime();

		if ( curtime >= m_flNextFrameChange )
		{
			m_flNextFrameChange = curtime + 0.2;

			int frame = m_pFrameVar->GetIntValue();

			if ( m_bAscending )
			{
				frame++;
				if ( frame >= m_nNumFrames )
				{
					m_bAscending = !m_bAscending;
					frame--;
				}
			}
			else
			{
				frame--;
				if ( frame < 0 )
				{
					m_bAscending = !m_bAscending;
					frame++;
				}
			}

			m_pFrameVar->SetIntValue(frame);
		}
	}

	float x, y;

	// assume square
	float flDrawWidth = ( m_flScale/48.0 ) * (float)wide;	
	int flHalfWidth = (int)( flDrawWidth / 2 );

	x = wide/2 - flHalfWidth;
	y = tall/2 - flHalfWidth;

	vgui::surface()->DrawSetColor( m_R, m_G, m_B, 255 );
	vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
	vgui::surface()->DrawTexturedRect( x, y, x+flDrawWidth, y+flDrawWidth );
	vgui::surface()->DrawSetTexture(0);
}

void CrosshairImagePanelAdvanced::UpdateCrosshair()
{
	// get the color selected in the combo box.
	m_R = clamp( m_pAdvCrosshairRedSlider->GetSliderValue(), 0, 255 );
	m_G = clamp( m_pAdvCrosshairGreenSlider->GetSliderValue(), 0, 255 );
	m_B = clamp( m_pAdvCrosshairBlueSlider->GetSliderValue(), 0, 255 );

	m_flScale = m_pAdvCrosshairScaleSlider->GetSliderValue();

	if ( m_pAdvCrosshairStyle )
	{
		char crosshairname[256];
		m_pAdvCrosshairStyle->GetText( crosshairname, sizeof(crosshairname)	);

		if ( ModInfo().AdvCrosshairLevel() == 1 && m_pAdvCrosshairStyle->GetActiveItem() == 0 ) // this is the "none" selection
		{
			SetCrosshairTexture(NULL);
		}
		else
		{
			char texture[ 256 ];
			Q_snprintf ( texture, sizeof( texture ), "vgui/crosshairs/%s", crosshairname );
			SetCrosshairTexture( texture );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: initialize the crosshair style list
//-----------------------------------------------------------------------------
void CrosshairImagePanelAdvanced::InitAdvCrosshairStyleList()
{
	// Find out images
	FileFindHandle_t fh;
	char directory[ 512 ];

	ConVarRef cl_crosshair_file( "cl_crosshair_file", true );
	if ( !cl_crosshair_file.IsValid() )
		return;

	m_pAdvCrosshairStyle->DeleteAllItems();

	if ( ModInfo().AdvCrosshairLevel() == 1 )
	{
		m_pAdvCrosshairStyle->AddItem( "#GameUI_None", "" );
	}

	char crosshairfile[256];
	Q_snprintf( crosshairfile, sizeof(crosshairfile), "materials/vgui/crosshairs/%s.vtf", cl_crosshair_file.GetString() );

	Q_snprintf( directory, sizeof( directory ), "materials/vgui/crosshairs/*.vtf" );
	const char *fn = g_pFullFileSystem->FindFirst( directory, &fh );
	int i = 0, initialItem = 0; 
	while (fn)
	{
		char filename[ 512 ];
		Q_snprintf( filename, sizeof(filename), "materials/vgui/crosshairs/%s", fn );
		if ( strlen( filename ) >= 4 )
		{
			filename[ strlen( filename ) - 4 ] = 0;
			Q_strncat( filename, ".vmt", sizeof( filename ), COPY_ALL_CHARACTERS );
			if ( g_pFullFileSystem->FileExists( filename ) )
			{
				// strip off the extension
				Q_strncpy( filename, fn, sizeof( filename ) );
				filename[ strlen( filename ) - 4 ] = 0;
				m_pAdvCrosshairStyle->AddItem( filename, "" );

				// check to see if this is the one we have set
				if ( crosshairfile[0] )
				{
					Q_snprintf( filename, sizeof(filename), "materials/vgui/crosshairs/%s", fn );
					if (!stricmp(filename, crosshairfile))
					{
						if ( ModInfo().AdvCrosshairLevel() == 1 )
						{
							initialItem = i+1;
						}
						else
						{
							initialItem = i;
						}
					}
				}

				++i;
			}
		}

		fn = g_pFullFileSystem->FindNext( fh );
	}

	g_pFullFileSystem->FindClose( fh );
	m_pAdvCrosshairStyle->SetInitialItem(initialItem);
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever style combo changes
//-----------------------------------------------------------------------------
void CrosshairImagePanelAdvanced::OnTextChanged(vgui::Panel *panel)
{
	m_pOptionsPanel->OnControlModified();
	UpdateCrosshair();
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever one of the color or scale sliders move
//-----------------------------------------------------------------------------
void CrosshairImagePanelAdvanced::OnSliderMoved(KeyValues *data)
{
	m_pOptionsPanel->OnControlModified();
	UpdateCrosshair();
}

void CrosshairImagePanelAdvanced::ResetData()
{
	m_pAdvCrosshairRedSlider->Reset();
	m_pAdvCrosshairGreenSlider->Reset();
	m_pAdvCrosshairBlueSlider->Reset();
	m_pAdvCrosshairScaleSlider->Reset();

	// TODO: update style combo from cvar
}

void CrosshairImagePanelAdvanced::ApplyChanges()
{
	m_pAdvCrosshairRedSlider->ApplyChanges();
	m_pAdvCrosshairGreenSlider->ApplyChanges();
	m_pAdvCrosshairBlueSlider->ApplyChanges();
	m_pAdvCrosshairScaleSlider->ApplyChanges();

	// save the crosshair
	char cmd[512];
	char crosshair[256];
	m_pAdvCrosshairStyle->GetText(crosshair, sizeof(crosshair));

	if ( ModInfo().AdvCrosshairLevel() == 1 && m_pAdvCrosshairStyle->GetActiveItem() == 0 ) // this is the "none" selection
	{
		engine->ClientCmd_Unrestricted("cl_crosshair_file \"\"");
	}
	else
	{
		Q_snprintf(cmd, sizeof(cmd), "cl_crosshair_file %s\n", crosshair);
		engine->ClientCmd_Unrestricted(cmd);
	}
}

void CrosshairImagePanelAdvanced::UpdateVisibility()
{
	SetVisible(true);
	m_pAdvCrosshairRedSlider->SetVisible(true);
	m_pAdvCrosshairBlueSlider->SetVisible(true);
	m_pAdvCrosshairGreenSlider->SetVisible(true);
	m_pAdvCrosshairScaleSlider->SetVisible(true);
	m_pAdvCrosshairStyle->SetVisible(true);

	if ( ModInfo().NoCrosshair() )
	{
		Panel *pTempPanel = NULL;

		pTempPanel = FindSiblingByName( "CrosshairImage" );
		pTempPanel->SetVisible( false );

		pTempPanel = FindSiblingByName( "CrosshairLabel" );
		if ( pTempPanel )
			pTempPanel->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::COptionsSubMultiplayer(vgui::Panel *parent) : vgui::PropertyPage(parent, "OptionsSubMultiplayer") 
{
	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	Button *ok = new Button( this, "OK", "#GameUI_OK" );
	ok->SetCommand( "Ok" );

	Button *apply = new Button( this, "Apply", "#GameUI_Apply" );
	apply->SetCommand( "Apply" );

	Button *advanced = new Button( this, "Advanced", "#GameUI_AdvancedEllipsis" );
	advanced->SetCommand( "Advanced" );

	Button *importSprayImage = new Button( this, "ImportSprayImage", "#GameUI_ImportSprayEllipsis" );
	importSprayImage->SetCommand("ImportSprayImage");

	m_hImportSprayDialog = NULL;

	m_pPrimaryColorSlider = new CCvarSlider( this, "Primary Color Slider", "#GameUI_PrimaryColor",
		0.0f, 255.0f, "topcolor" );

	m_pSecondaryColorSlider = new CCvarSlider( this, "Secondary Color Slider", "#GameUI_SecondaryColor",
		0.0f, 255.0f, "bottomcolor" );

	m_pHighQualityModelCheckBox = new CCvarToggleCheckButton( this, "High Quality Models", "#GameUI_HighModels", "cl_himodels" );

	m_pModelList = new CLabeledCommandComboBox( this, "Player model" );
	m_ModelName[0] = 0;
	InitModelList( m_pModelList );

	m_pLogoList = new CLabeledCommandComboBox( this, "SpraypaintList" );
    m_LogoName[0] = 0;
	InitLogoList( m_pLogoList );

	m_pModelImage = new CBitmapImagePanel( this, "ModelImage", NULL );
	m_pModelImage->AddActionSignalTarget( this );

	m_pLogoImage = new ImagePanel( this, "LogoImage" );
	m_pLogoImage->AddActionSignalTarget( this );

	m_nLogoR = 255;
	m_nLogoG = 255;
	m_nLogoB = 255;

	m_pCrosshairImage = NULL;
	switch ( ModInfo().AdvCrosshairLevel() )
	{
	case 0:
		if ( !ModInfo().NoCrosshair() )
			m_pCrosshairImage = new CrosshairImagePanelSimple( this, "CrosshairImage", this );
		break;

	case 1:	// TF
	case 2:	// DOD
		m_pCrosshairImage = new CrosshairImagePanelAdvanced( this, "AdvCrosshairImage", this );
		break;

	case 3:	// Counter-Strike
		m_pCrosshairImage = new CrosshairImagePanelCS( this, "CrosshairImage", this );
		break;

	}

	m_pLockRadarRotationCheckbox = new CCvarToggleCheckButton( this, "LockRadarRotationCheckbox", "#Cstrike_RadarLocked", "cl_radar_locked" );

	m_pDownloadFilterCombo = new ComboBox( this, "DownloadFilterCheck", 4, false );
	m_pDownloadFilterCombo->AddItem( "#GameUI_DownloadFilter_ALL", NULL );
	m_pDownloadFilterCombo->AddItem( "#GameUI_DownloadFilter_NoSounds", NULL );
	m_pDownloadFilterCombo->AddItem( "#GameUI_DownloadFilter_MapsOnly", NULL );
	m_pDownloadFilterCombo->AddItem( "#GameUI_DownloadFilter_None", NULL );

	//=========

	LoadControlSettings("Resource/OptionsSubMultiplayer.res");

	// this is necessary because some of the game .res files don't have visiblity flags set up correctly for their controls
	if ( m_pCrosshairImage )
		m_pCrosshairImage->UpdateVisibility();

	// turn off model selection stuff if the mod specifies "nomodels" in the gameinfo.txt file
	if ( ModInfo().NoModels() )
	{
		Panel *pTempPanel = NULL;

		if ( m_pModelImage )
		{
			m_pModelImage->SetVisible( false );
		}

		if ( m_pModelList )
		{
			m_pModelList->SetVisible( false );
		}

		if ( m_pPrimaryColorSlider )
		{
			m_pPrimaryColorSlider->SetVisible( false );
		}

		if ( m_pSecondaryColorSlider )
		{
			m_pSecondaryColorSlider->SetVisible( false );
		}

		// #GameUI_PlayerModel (from "Resource/OptionsSubMultiplayer.res")
		pTempPanel = FindChildByName( "Label1" );

		if ( pTempPanel )
		{
			pTempPanel->SetVisible( false );
		}

		// #GameUI_ColorSliders (from "Resource/OptionsSubMultiplayer.res")
		pTempPanel = FindChildByName( "Colors" );

		if ( pTempPanel )
		{
			pTempPanel->SetVisible( false );
		}
	}

	// turn off the himodel stuff if the mod specifies "nohimodel" in the gameinfo.txt file
	if ( ModInfo().NoHiModel() )
	{
		if ( m_pHighQualityModelCheckBox )
		{
			m_pHighQualityModelCheckBox->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::~COptionsSubMultiplayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnCommand( const char *command )
{
	if ( !stricmp( command, "Advanced" ) )
	{
#ifndef _XBOX
		if (!m_hMultiplayerAdvancedDialog.Get())
		{
			m_hMultiplayerAdvancedDialog = new CMultiplayerAdvancedDialog( this );
		}
		m_hMultiplayerAdvancedDialog->Activate();
#endif
	}
	else if (!stricmp( command, "ImportSprayImage" ) )
	{
		if (m_hImportSprayDialog == NULL)
		{
			m_hImportSprayDialog = new FileOpenDialog(NULL, "#GameUI_ImportSprayImage", true);
#ifdef WIN32
			m_hImportSprayDialog->AddFilter("*.tga,*.jpg,*.bmp,*.vtf", "#GameUI_All_Images", true);
#else
			m_hImportSprayDialog->AddFilter("*.tga,*.jpg,*.vtf", "#GameUI_All_ImagesNoBmp", true);
#endif
			m_hImportSprayDialog->AddFilter("*.tga", "#GameUI_TGA_Images", false);
			m_hImportSprayDialog->AddFilter("*.jpg", "#GameUI_JPEG_Images", false);
#ifdef WIN32
			m_hImportSprayDialog->AddFilter("*.bmp", "#GameUI_BMP_Images", false);
#endif
			m_hImportSprayDialog->AddFilter("*.vtf", "#GameUI_VTF_Images", false);
			m_hImportSprayDialog->AddActionSignalTarget(this);
		}
		m_hImportSprayDialog->DoModal(false);
		m_hImportSprayDialog->Activate();
	}

	else if ( !stricmp( command, "ResetStats" ) )
	{
		QueryBox *box = new QueryBox("#GameUI_ConfirmResetStatsTitle", "#GameUI_ConfirmResetStatsText", this);
		box->SetOKButtonText("#GameUI_Reset");
		box->SetOKCommand(new KeyValues("Command", "command", "ResetStats_NoConfirm"));
		box->SetCancelCommand(new KeyValues("Command", "command", "ReleaseModalWindow"));
		box->AddActionSignalTarget(this);
		box->DoModal();
	}

	else if ( !stricmp( command, "ResetStats_NoConfirm" ) )
	{
		engine->ClientCmd_Unrestricted("stats_reset");
	}

	BaseClass::OnCommand( command );
}

void COptionsSubMultiplayer::ConversionError( ConversionErrorType nError )
{
	const char *pErrorText = NULL;

	switch ( nError )
	{
	case CE_MEMORY_ERROR:
		pErrorText = "#GameUI_Spray_Import_Error_Memory";
		break;

	case CE_CANT_OPEN_SOURCE_FILE:
		pErrorText = "#GameUI_Spray_Import_Error_Reading_Image";
		break;

	case CE_ERROR_PARSING_SOURCE:
		pErrorText = "#GameUI_Spray_Import_Error_Image_File_Corrupt";
		break;

	case CE_SOURCE_FILE_SIZE_NOT_SUPPORTED:
		pErrorText = "#GameUI_Spray_Import_Image_Wrong_Size";
		break;

	case CE_SOURCE_FILE_FORMAT_NOT_SUPPORTED:
		pErrorText = "#GameUI_Spray_Import_Image_Wrong_Size";
		break;

	case CE_SOURCE_FILE_TGA_FORMAT_NOT_SUPPORTED:
		pErrorText = "#GameUI_Spray_Import_Error_TGA_Format_Not_Supported";
		break;

	case CE_SOURCE_FILE_BMP_FORMAT_NOT_SUPPORTED:
		pErrorText = "#GameUI_Spray_Import_Error_BMP_Format_Not_Supported";
		break;

	case CE_ERROR_WRITING_OUTPUT_FILE:
		pErrorText = "#GameUI_Spray_Import_Error_Writing_Temp_Output";
		break;

	case CE_ERROR_LOADING_DLL:
		pErrorText = "#GameUI_Spray_Import_Error_Cant_Load_VTEX_DLL";
		break;
	}

	if ( pErrorText )
	{
		// Create the dialog
		vgui::MessageBox *pErrorDlg = new vgui::MessageBox("#GameUI_Spray_Import_Error_Title", pErrorText );	Assert( pErrorDlg );

		// Display
		if ( pErrorDlg )	// Check for a NULL just to be extra cautious...
		{
			pErrorDlg->DoModal();
		}
	}
}

void COptionsSubMultiplayer::OnFileSelected(const char *fullpath)
{
#ifndef _XBOX
	// this can take a while, put up a waiting cursor
	surface()->SetCursor(dc_hourglass);

	ConversionErrorType nErrorCode = ImgUtl_ConvertToVTFAndDumpVMT( fullpath, IsPosix() ? "/vgui/logos" : "\\vgui\\logos", 256, 256 );
	if ( nErrorCode == CE_SUCCESS )
	{
		// refresh the logo list so the new spray shows up.
		InitLogoList(m_pLogoList);

		// Get the filename
		char szRootFilename[MAX_PATH];
		V_FileBase( fullpath, szRootFilename, sizeof( szRootFilename ) );

		// automatically select the logo that was just imported.
		SelectLogo(szRootFilename);
	}
	else
	{
		ConversionError( nErrorCode );
	}

	// change the cursor back to normal
	surface()->SetCursor(dc_user);
#endif
}

struct ValveJpegErrorHandler_t 
{
	// The default manager
	struct jpeg_error_mgr	m_Base;
	// For handling any errors
	jmp_buf					m_ErrorContext;
};

//-----------------------------------------------------------------------------
// Purpose: We'll override the default error handler so we can deal with errors without having to exit the engine
//-----------------------------------------------------------------------------
static void ValveJpegErrorHandler( j_common_ptr cinfo )
{
	ValveJpegErrorHandler_t *pError = reinterpret_cast< ValveJpegErrorHandler_t * >( cinfo->err );

	char buffer[ JMSG_LENGTH_MAX ];

	/* Create the message */
	( *cinfo->err->format_message )( cinfo, buffer );

	Warning( "%s\n", buffer );

	// Bail
	longjmp( pError->m_ErrorContext, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Builds the list of logos
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitLogoList( CLabeledCommandComboBox *cb )
{
	// Find out images
	FileFindHandle_t fh;
	char directory[ 512 ];

	ConVarRef cl_logofile( "cl_logofile", true );
	if ( !cl_logofile.IsValid() )
		return;

	cb->DeleteAllItems();

	const char *logofile = cl_logofile.GetString();
	Q_snprintf( directory, sizeof( directory ), "materials/vgui/logos/*.vtf" );
	const char *fn = g_pFullFileSystem->FindFirst( directory, &fh );
	int i = 0, initialItem = 0; 
	while (fn)
	{
		char filename[ 512 ];
		Q_snprintf( filename, sizeof(filename), "materials/vgui/logos/%s", fn );
		if ( strlen( filename ) >= 4 )
		{
			filename[ strlen( filename ) - 4 ] = 0;
			Q_strncat( filename, ".vmt", sizeof( filename ), COPY_ALL_CHARACTERS );
			if ( g_pFullFileSystem->FileExists( filename ) )
			{
				// strip off the extension
				Q_strncpy( filename, fn, sizeof( filename ) );
				filename[ strlen( filename ) - 4 ] = 0;
				cb->AddItem( filename, "" );

				// check to see if this is the one we have set
				Q_snprintf( filename, sizeof(filename), "materials/vgui/logos/%s", fn );
				if (!Q_stricmp(filename, logofile))
				{
					initialItem = i;
				}

				++i;
			}
		}

		fn = g_pFullFileSystem->FindNext( fh );
	}

	g_pFullFileSystem->FindClose( fh );
	cb->SetInitialItem(initialItem);
}


//-----------------------------------------------------------------------------
// Purpose: Selects the given logo in the logo list.
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::SelectLogo(const char *logoName)
{
	int numEntries = m_pLogoList->GetItemCount();
	int index;
	wchar_t itemText[MAX_PATH];
	wchar_t itemToSelectText[MAX_PATH];

	// convert the logo filename to unicode
	g_pVGuiLocalize->ConvertANSIToUnicode(logoName, itemToSelectText, sizeof(itemToSelectText));

	// find the index of the spray we want.
	for (index = 0; index < numEntries; ++index)
	{
		m_pLogoList->GetItemText(index, itemText, sizeof(itemText));
		if (!wcscmp(itemText, itemToSelectText))
		{
			break;
		}
	}

	if (index < numEntries)
	{
		// select the logo.
		m_pLogoList->ActivateItem(index);
	}
}

#define MODEL_MATERIAL_BASE_FOLDER "materials/vgui/playermodels/"


void StripStringOutOfString( const char *pPattern, const char *pIn, char *pOut )
{
	int iLengthBase = strlen( pPattern );
	int iLengthString = strlen( pIn );

	int k = 0;

	for ( int j = iLengthBase; j < iLengthString; j++ )
	{
		pOut[k] = pIn[j];
		k++;
	}

	pOut[k] = 0;
}

void FindVMTFilesInFolder( const char *pFolder, const char *pFolderName, CLabeledCommandComboBox *cb, int &iCount, int &iInitialItem )
{
	ConVarRef cl_modelfile( "cl_playermodel", true );
	if ( !cl_modelfile.IsValid() )
		return;

	char directory[ 512 ];
	Q_snprintf( directory, sizeof( directory ), "%s/*.*", pFolder );

	FileFindHandle_t fh;

	const char *fn = g_pFullFileSystem->FindFirst( directory, &fh );
	const char *modelfile = cl_modelfile.GetString();

	while ( fn )
	{
		if ( !stricmp( fn, ".") || !stricmp( fn, "..") )
		{
			fn = g_pFullFileSystem->FindNext( fh );
			continue;
		}

		if ( g_pFullFileSystem->FindIsDirectory( fh ) )
		{
			char folderpath[512];

			Q_snprintf( folderpath, sizeof( folderpath ), "%s/%s", pFolder, fn );

			FindVMTFilesInFolder( folderpath, fn, cb, iCount, iInitialItem );
			fn = g_pFullFileSystem->FindNext( fh );
			continue;
		}

		if ( !strstr( fn, ".vmt" ) )
		{
			fn = g_pFullFileSystem->FindNext( fh );
			continue;
		}


		char filename[ 512 ];
		Q_snprintf( filename, sizeof(filename), "%s/%s", pFolder, fn );
		if ( strlen( filename ) >= 4 )
		{
			filename[ strlen( filename ) - 4 ] = 0;
			Q_strncat( filename, ".vmt", sizeof( filename ), COPY_ALL_CHARACTERS );
			if ( g_pFullFileSystem->FileExists( filename ) )
			{
				char displayname[ 512 ];
				char texturepath[ 512 ];
				// strip off the extension
				Q_strncpy( displayname, fn, sizeof( displayname ) );
				StripStringOutOfString( MODEL_MATERIAL_BASE_FOLDER, filename, texturepath );
				
				displayname[ strlen( displayname ) - 4 ] = 0;
				
				if ( CORRECT_PATH_SEPARATOR == texturepath[0] )
				    cb->AddItem( displayname, texturepath + 1 ); // ignore the initial "/" in texture path
				else
				    cb->AddItem( displayname, texturepath );


				char realname[ 512 ];
				Q_FileBase( modelfile, realname, sizeof( realname ) );
				Q_FileBase( filename, filename, sizeof( filename ) );
				
				if (!stricmp(filename, realname))
				{
					iInitialItem = iCount;
				}

				++iCount;
			}
		}

		fn = g_pFullFileSystem->FindNext( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds model list
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitModelList( CLabeledCommandComboBox *cb )
{
	// Find out images
	int i = 0, initialItem = 0;

	cb->DeleteAllItems();
	FindVMTFilesInFolder( MODEL_MATERIAL_BASE_FOLDER, "", cb, i, initialItem );
	cb->SetInitialItem( initialItem );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapLogo()
{
	char logoname[256];

	m_pLogoList->GetText( logoname, sizeof( logoname ) );
	if( !logoname[ 0 ] )
		return;

	char fullLogoName[512];

	// make sure there is a version with the proper shader
	g_pFullFileSystem->CreateDirHierarchy( "materials/VGUI/logos/UI", "GAME" );
	Q_snprintf( fullLogoName, sizeof( fullLogoName ), "materials/VGUI/logos/UI/%s.vmt", logoname );
	if ( !g_pFullFileSystem->FileExists( fullLogoName ) )
	{
		FileHandle_t fp = g_pFullFileSystem->Open( fullLogoName, "wb" );
		if ( !fp )
			return;

		char data[1024];
		Q_snprintf( data, sizeof( data ), "\"UnlitGeneric\"\n\
{\n\
	// Original shader: BaseTimesVertexColorAlphaBlendNoOverbright\n\
	\"$translucent\" 1\n\
	\"$basetexture\" \"VGUI/logos/%s\"\n\
	\"$vertexcolor\" 1\n\
	\"$vertexalpha\" 1\n\
	\"$no_fullbright\" 1\n\
	\"$ignorez\" 1\n\
}\n\
", logoname );

		g_pFullFileSystem->Write( data, strlen( data ), fp );
		g_pFullFileSystem->Close( fp );
	}

	Q_snprintf( fullLogoName, sizeof( fullLogoName ), "logos/UI/%s", logoname );
	m_pLogoImage->SetImage( fullLogoName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapModel()
{
	const char *pModelName = m_pModelList->GetActiveItemCommand();
	
	if( pModelName == NULL )
		return;

	char texture[ 256 ];
	Q_snprintf ( texture, sizeof( texture ), "vgui/playermodels/%s", pModelName );
	texture[ strlen( texture ) - 4 ] = 0;

	m_pModelImage->setTexture( texture );
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever model name changes
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnTextChanged(vgui::Panel *panel)
{
	RemapModel();
	RemapLogo();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnControlModified()
{
	PostMessage(GetParent(), new KeyValues("ApplyButtonEnable"));
	InvalidateLayout();
}

#define DIB_HEADER_MARKER   ((WORD) ('M' << 8) | 'B')
#define SUIT_HUE_START 192
#define SUIT_HUE_END 223
#define PLATE_HUE_START 160
#define PLATE_HUE_END 191

#ifdef POSIX 
typedef struct tagRGBQUAD { 
  uint8 rgbBlue;
  uint8 rgbGreen;
  uint8 rgbRed;
  uint8 rgbReserved;
} RGBQUAD;
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void PaletteHueReplace( RGBQUAD *palSrc, int newHue, int Start, int end )
{
	int i;
	float r, b, g;
	float maxcol, mincol;
	float hue, val, sat;

	hue = (float)(newHue * (360.0 / 255));

	for (i = Start; i <= end; i++)
	{
		b = palSrc[ i ].rgbBlue;
		g = palSrc[ i ].rgbGreen;
		r = palSrc[ i ].rgbRed;
		
		maxcol = max( max( r, g ), b ) / 255.0f;
		mincol = min( min( r, g ), b ) / 255.0f;
		
		val = maxcol;
		sat = (maxcol - mincol) / maxcol;

		mincol = val * (1.0f - sat);

		if (hue <= 120)
		{
			b = mincol;
			if (hue < 60)
			{
				r = val;
				g = mincol + hue * (val - mincol)/(120 - hue);
			}
			else
			{
				g = val;
				r = mincol + (120 - hue)*(val-mincol)/hue;
			}
		}
		else if (hue <= 240)
		{
			r = mincol;
			if (hue < 180)
			{
				g = val;
				b = mincol + (hue - 120)*(val-mincol)/(240 - hue);
			}
			else
			{
				b = val;
				g = mincol + (240 - hue)*(val-mincol)/(hue - 120);
			}
		}
		else
		{
			g = mincol;
			if (hue < 300)
			{
				b = val;
				r = mincol + (hue - 240)*(val-mincol)/(360 - hue);
			}
			else
			{
				r = val;
				b = mincol + (360 - hue)*(val-mincol)/(hue - 240);
			}
		}

		palSrc[ i ].rgbBlue = (unsigned char)(b * 255);
		palSrc[ i ].rgbGreen = (unsigned char)(g * 255);
		palSrc[ i ].rgbRed = (unsigned char)(r * 255);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::ColorForName( char const *pszColorName, int&r, int&g, int&b )
{
	r = g = b = 0;
	
	int count = sizeof( itemlist ) / sizeof( itemlist[0] );

	for ( int i = 0; i < count; i++ )
	{
		if (!Q_strnicmp(pszColorName, itemlist[ i ].name, strlen(itemlist[ i ].name)))
		{
			r = itemlist[ i ].r;
			g = itemlist[ i ].g;
			b = itemlist[ i ].b;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnResetData()
{
	// reset the DownloadFilter combo box
	if ( m_pDownloadFilterCombo )
	{
		// cl_downloadfilter
		ConVarRef cl_downloadfilter( "cl_downloadfilter");

		if ( Q_stricmp( cl_downloadfilter.GetString(), "none" ) == 0 )
		{
			m_pDownloadFilterCombo->ActivateItem( 3 );
		}
		else if ( Q_stricmp( cl_downloadfilter.GetString(), "nosounds" ) == 0 )
		{
			m_pDownloadFilterCombo->ActivateItem( 1 );
		}
		else if ( Q_stricmp( cl_downloadfilter.GetString(), "mapsonly" ) == 0 )
		{
			m_pDownloadFilterCombo->ActivateItem( 2 );
		}
		else
		{
			m_pDownloadFilterCombo->ActivateItem( 0 );
		}
	}

	if ( m_pCrosshairImage )
		m_pCrosshairImage->ResetData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnApplyChanges()
{
	m_pPrimaryColorSlider->ApplyChanges();
	m_pSecondaryColorSlider->ApplyChanges();
//	m_pModelList->ApplyChanges();
	m_pLogoList->ApplyChanges();
    m_pLogoList->GetText(m_LogoName, sizeof(m_LogoName));
	m_pHighQualityModelCheckBox->ApplyChanges();

	for ( int i=0; i<m_cvarToggleCheckButtons.GetCount(); ++i )
	{
		CCvarToggleCheckButton *toggleButton = m_cvarToggleCheckButtons[i];
		if( toggleButton->IsVisible() && toggleButton->IsEnabled() )
		{
			toggleButton->ApplyChanges();
		}
	}

	if ( m_pLockRadarRotationCheckbox != NULL )
	{
		m_pLockRadarRotationCheckbox->ApplyChanges();
	}

	if ( m_pCrosshairImage != NULL )
		m_pCrosshairImage->ApplyChanges();

	// save the logo name
	char cmd[512];
	if ( m_LogoName[ 0 ] )
	{
		Q_snprintf(cmd, sizeof(cmd), "cl_logofile materials/vgui/logos/%s.vtf\n", m_LogoName);
	}
	else
	{
		Q_strncpy( cmd, "cl_logofile \"\"\n", sizeof( cmd ) );
	}
	engine->ClientCmd_Unrestricted(cmd);

	if ( m_pModelList && m_pModelList->IsVisible() && m_pModelList->GetActiveItemCommand() )
	{
		Q_strncpy( m_ModelName, m_pModelList->GetActiveItemCommand(), sizeof( m_ModelName ) );
		Q_StripExtension( m_ModelName, m_ModelName, sizeof ( m_ModelName ) );
		
		// save the player model name
		Q_snprintf(cmd, sizeof(cmd), "cl_playermodel models/%s.mdl\n", m_ModelName );
		engine->ClientCmd_Unrestricted(cmd);
	}
	else
	{
		m_ModelName[0] = 0;
	}

	// set the DownloadFilter cvar
	if ( m_pDownloadFilterCombo )
	{
		ConVarRef  cl_downloadfilter( "cl_downloadfilter" );
		
		switch ( m_pDownloadFilterCombo->GetActiveItem() )
		{
		default:
		case 0:
			cl_downloadfilter.SetValue( "all" );
			break;
		case 1:
			cl_downloadfilter.SetValue( "nosounds" );
			break;
		case 2:
			cl_downloadfilter.SetValue( "mapsonly" );
			break;
		case 3:
			cl_downloadfilter.SetValue( "none" );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow the res file to create controls on per-mod basis
//-----------------------------------------------------------------------------
Panel *COptionsSubMultiplayer::CreateControlByName( const char *controlName )
{
	if( !Q_stricmp( "CCvarToggleCheckButton", controlName ) )
	{
		CCvarToggleCheckButton *newButton = new CCvarToggleCheckButton( this, controlName, "", "" );
		m_cvarToggleCheckButtons.AddElement( newButton );
		return newButton;
	}
	else
	{
		return BaseClass::CreateControlByName( controlName );
	}
}

