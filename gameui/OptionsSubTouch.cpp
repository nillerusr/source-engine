//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "OptionsSubTouch.h"
//#include "CommandCheckButton.h"
#include "KeyToggleCheckButton.h"
#include "CvarNegateCheckButton.h"
#include "CvarToggleCheckButton.h"
#include "cvarslider.h"

#include "EngineInterface.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include "tier1/convar.h"
#include <stdio.h>
#include <vgui_controls/TextEntry.h>
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

COptionsSubTouch::COptionsSubTouch(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_pTouchEnableCheckBox = new CCvarToggleCheckButton(this,
		"EnableTouch",
		"Enable touch",
		"touch_enable");

	m_pTouchDrawCheckBox = new CCvarToggleCheckButton(this,
		"DrawTouch",
		"Draw touch",
		"touch_draw");

	m_pReverseTouchCheckBox = new CCvarToggleCheckButton(
		this,
		"ReverseTouch",
		"Reverse touch",
		"touch_reverse" );

	m_pTouchFilterCheckBox = new CCvarToggleCheckButton(
		this,
		"TouchFilter",
		"Touch filter",
		"touch_filter" );

	m_pTouchAccelerationCheckBox = new CCvarToggleCheckButton(
		this,
		"TouchAccelerationCheckbox",
		"Touch acceleration",
		"touch_enable_accel" );

	m_pTouchSensitivitySlider = new CCvarSlider( this, "Slider", "Touch sensitivity",
		0.1f, 6.0f, "touch_sensitivity", true );

	m_pTouchSensitivityLabel = new TextEntry(this, "SensitivityLabel");
	m_pTouchSensitivityLabel->AddActionSignalTarget(this);

	m_pTouchAccelExponentSlider = new CCvarSlider( this, "TouchAccelerationSlider", "Touch acceleration",
		1.0f, 1.5f, "touch_accel", true );

	m_pTouchAccelExponentLabel = new TextEntry(this, "TouchAccelerationLabel");
	m_pTouchAccelExponentLabel->AddActionSignalTarget(this);

	m_pTouchYawSensitivitySlider = new CCvarSlider( this, "TouchYawSlider", "#GameUI_JoystickYawSensitivity",
		50.f, 300.f, "touch_yaw", true );
	m_pTouchYawSensitivityPreLabel = new Label(this, "TouchYawSensitivityPreLabel", "#GameUI_JoystickLookSpeedYaw" );
	m_pTouchYawSensitivityLabel = new TextEntry(this, "TouchYawSensitivityLabel");
	m_pTouchYawSensitivityLabel->AddActionSignalTarget(this);

	m_pTouchPitchSensitivitySlider = new CCvarSlider( this, "TouchPitchSlider", "#GameUI_JoystickPitchSensitivity",
		50.f, 300.f, "touch_pitch", true );
	m_pTouchPitchSensitivityPreLabel = new Label(this, "TouchPitchSensitivityPreLabel", "#GameUI_JoystickLookSpeedPitch" );
	m_pTouchPitchSensitivityLabel = new TextEntry(this, "TouchPitchSensitivityLabel");
	m_pTouchPitchSensitivityLabel->AddActionSignalTarget(this);

	LoadControlSettings("Resource\\OptionsSubTouch.res");

	UpdateLabel(m_pTouchSensitivitySlider, m_pTouchSensitivityLabel);
	UpdateLabel(m_pTouchAccelExponentSlider, m_pTouchAccelExponentLabel);
	UpdateLabel(m_pTouchYawSensitivitySlider, m_pTouchYawSensitivityLabel);
	UpdateLabel(m_pTouchPitchSensitivitySlider, m_pTouchPitchSensitivityLabel);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubTouch::~COptionsSubTouch()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubTouch::OnResetData()
{
	m_pReverseTouchCheckBox->Reset();
	m_pTouchFilterCheckBox->Reset();
	m_pTouchSensitivitySlider->Reset();
	m_pTouchAccelExponentSlider->Reset();
	m_pTouchYawSensitivitySlider->Reset();
	m_pTouchPitchSensitivitySlider->Reset();
	m_pTouchAccelerationCheckBox->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubTouch::OnApplyChanges()
{
	m_pReverseTouchCheckBox->ApplyChanges();
	m_pTouchFilterCheckBox->ApplyChanges();
	m_pTouchSensitivitySlider->ApplyChanges();
	m_pTouchAccelExponentSlider->ApplyChanges();
	m_pTouchYawSensitivitySlider->ApplyChanges();
	m_pTouchPitchSensitivitySlider->ApplyChanges();
	m_pTouchEnableCheckBox->ApplyChanges();
	m_pTouchDrawCheckBox->ApplyChanges();
	m_pTouchAccelerationCheckBox->ApplyChanges();
}

//-----------------------------------------------------------------------------
// Purpose: sets background color & border
//-----------------------------------------------------------------------------
void COptionsSubTouch::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubTouch::OnControlModified(Panel *panel)
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));

	// the HasBeenModified() check is so that if the value is outside of the range of the
	// slider, it won't use the slider to determine the display value but leave the
	// real value that we determined in the constructor
	if (panel == m_pTouchSensitivitySlider && m_pTouchSensitivitySlider->HasBeenModified())
		UpdateLabel( m_pTouchSensitivitySlider, m_pTouchSensitivityLabel );
	else if (panel == m_pTouchAccelExponentSlider && m_pTouchAccelExponentSlider->HasBeenModified())
		UpdateLabel( m_pTouchAccelExponentSlider, m_pTouchAccelExponentLabel );
	else if (panel == m_pTouchYawSensitivitySlider && m_pTouchYawSensitivitySlider->HasBeenModified())
		UpdateLabel( m_pTouchYawSensitivitySlider, m_pTouchYawSensitivityLabel );
	else if (panel == m_pTouchPitchSensitivitySlider && m_pTouchPitchSensitivitySlider->HasBeenModified())
		UpdateLabel( m_pTouchPitchSensitivitySlider, m_pTouchPitchSensitivityLabel );
	else if (panel == m_pTouchAccelerationCheckBox)
	{
		m_pTouchAccelExponentSlider->SetEnabled(m_pTouchAccelerationCheckBox->IsSelected());
		m_pTouchAccelExponentLabel->SetEnabled(m_pTouchAccelerationCheckBox->IsSelected());
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubTouch::OnTextChanged(Panel *panel)
{
	if ( panel == m_pTouchSensitivityLabel )
	{
		char buf[64];
		m_pTouchSensitivityLabel->GetText(buf, 64);

		float fValue;
		int numParsed = sscanf(buf, "%f", &fValue);
		if ( ( numParsed == 1 ) && ( fValue >= 0.0f ) )
		{
			m_pTouchSensitivitySlider->SetSliderValue(fValue);
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
		}
	}
	else if ( panel == m_pTouchAccelExponentLabel )
	{
		char buf[64];
		m_pTouchAccelExponentLabel->GetText(buf, 64);

		float fValue = (float) atof(buf);
		if (fValue >= 1.0)
		{
			m_pTouchAccelExponentSlider->SetSliderValue(fValue);
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
		}
	}
	else if( panel == m_pTouchPitchSensitivityLabel )
	{
		char buf[64];
		m_pTouchPitchSensitivityLabel->GetText(buf, 64);

		float fValue = (float) atof(buf);
		if (fValue >= 1.0)
		{
			m_pTouchPitchSensitivitySlider->SetSliderValue(fValue);
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
		}
	}
	else if( panel == m_pTouchYawSensitivityLabel )
	{
		char buf[64];
		m_pTouchYawSensitivityLabel->GetText(buf, 64);

		float fValue = (float) atof(buf);
		if (fValue >= 1.0)
		{
			m_pTouchYawSensitivitySlider->SetSliderValue(fValue);
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
		}
	}
}

void COptionsSubTouch::UpdateLabel(CCvarSlider *slider, vgui::TextEntry *label)
{
	char buf[64];
	Q_snprintf(buf, sizeof( buf ), " %.2f", slider->GetSliderValue());
	label->SetText(buf);
}
