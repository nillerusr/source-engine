//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGameOptions.h"
#include "VSpinnerControl.h"
#include "VFooterPanel.h"
#include "gameui_util.h"
#include "vgui_controls/CheckButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

GenericSpinnerItem gInvertYAxisItems[] = {
	{ "#L4D360UI_DisabledDefault",		"",		0 },
	{ "#L4D360UI_Enabled",				"",		1 },
};
#define INVERT_YAXIS_DEFAULT 0

GenericSpinnerItem gVibrationItems[] = {
	{ "#L4D360UI_ControllerVibration0",	"",		0 },
	{ "#L4D360UI_ControllerVibration1",	"",		1 },
	{ "#L4D360UI_ControllerVibration2",	"",		2 },
	{ "#L4D360UI_ControllerVibration3",	"",		3 },
	{ "#L4D360UI_ControllerVibration4",	"",		4 },
	{ "#L4D360UI_ControllerVibration5",	"",		5 },
};
#define VIBRATION_DEFAULT 5

GenericSpinnerItem gAutoCrouchItems[] = {
	{ "#L4D360UI_Disabled",				"",		0 },
	{ "#L4D360UI_EnabledDefault",		"",		1 },
};
#define SWAP_AUTOCROUCH_DEFAULT 0

GenericSpinnerItem gLookSensitivityItems[] = {
	{ "#L4D360UI_LookSensitivity1",		"",		1 },
	{ "#L4D360UI_LookSensitivity2",		"",		2 },
	{ "#L4D360UI_LookSensitivity3",		"",		3 },
	{ "#L4D360UI_LookSensitivity4",		"",		4 },
	{ "#L4D360UI_LookSensitivity5",		"",		5 },
};
#define LOOK_SENSITIVITY_MIN 1
#define LOOK_SENSITIVITY_MAX 5
#define LOOK_SENSITIVITY_DEFAULT 3

// gets a multiplier 0.5 to 2.0 based on a range and current value
static float GetMult(const int& min, const int& max, const int& cur)
{
	float half = static_cast<float>(max - min) / 2.0f;
	float diff = static_cast<float>(cur) - half;
	float fMin = static_cast<float>(min);
	float modifier = (diff - fMin) / half;

	if(diff > 0.0f)
	{
		return 1.0f + modifier;
	}
	else
	{
		return 1.0f + (modifier * 0.5f);
	}
}

//=============================================================================
GameOptions::GameOptions(Panel *parent, const char *panelName):
BaseClass(parent, panelName, true, true)
{
	SetDeleteSelfOnClose(true);
	SetProportional( true );
	SetTitle( "#L4D360UI_GameOptions", false );

	int i  = 0;

	m_SpnInvertYAxis = new SpinnerControl( this, "SpnInvertYAxis" );
	for( i = 0; i < sizeof( gInvertYAxisItems ) / sizeof( GenericSpinnerItem ); ++i )
		m_SpnInvertYAxis->AddItem( gInvertYAxisItems[i] );

	m_SpnVibration = new SpinnerControl( this, "SpnVibration" );
	for( i = 0; i < sizeof( gVibrationItems ) / sizeof( GenericSpinnerItem ); ++i )
		m_SpnVibration->AddItem( gVibrationItems[i] );

	m_SpnAutoCrouch = new SpinnerControl( this, "SpnAutoCrouch" );
	for( i = 0; i < sizeof( gAutoCrouchItems ) / sizeof( GenericSpinnerItem ); ++i )
		m_SpnAutoCrouch->AddItem( gAutoCrouchItems[i] );

	m_SpnLookSensitivity = new SpinnerControl( this, "SpnLookSensitivity" );
	for( i = 0; i < sizeof( gLookSensitivityItems ) / sizeof( GenericSpinnerItem ); ++i )
		m_SpnLookSensitivity->AddItem( gLookSensitivityItems[i] );

	SetUpperGarnishEnabled( true );
	SetLowerGarnishEnabled( true );

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_ABUTTON | FB_BBUTTON, FF_AB_ONLY, true );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}

	m_ActiveControl = m_SpnInvertYAxis;
}

//=============================================================================
GameOptions::~GameOptions()
{
}

//=============================================================================
void GameOptions::OnCommand(const char *command)
{
}

//=============================================================================
void GameOptions::Activate()
{
	BaseClass::Activate();

	int i = 0;

	CGameUIConVarRef joy_inverty("joy_inverty");
	if(joy_inverty.IsValid())
	{
		for(i = 0; i < sizeof(gInvertYAxisItems) / sizeof(GenericSpinnerItem); ++i)
		{
			if(gInvertYAxisItems[i].UIGameDataValue == joy_inverty.GetInt())
			{
				m_SpnInvertYAxis->SetCurrentItem(gInvertYAxisItems[i].LocalizedTextKey);
				break;
			}
		}
	}

	CGameUIConVarRef cl_rumblescale("cl_rumblescale");
	if(cl_rumblescale.IsValid())
	{
		int convertedValue = static_cast<int>(cl_rumblescale.GetFloat() * 5.0f);
		for(i = 0; i < sizeof(gVibrationItems) / sizeof(GenericSpinnerItem); ++i)
		{
			if(gVibrationItems[i].UIGameDataValue == convertedValue)
			{
				m_SpnVibration->SetCurrentItem(gVibrationItems[i].LocalizedTextKey);
				break;
			}
		}
	}

	CGameUIConVarRef cl_autocrouch("cl_autocrouch");
	if(cl_autocrouch.IsValid())
	{
		for(i = 0; i < sizeof(gAutoCrouchItems) / sizeof(GenericSpinnerItem); ++i)
		{
			if(gAutoCrouchItems[i].UIGameDataValue == cl_autocrouch.GetInt())
			{
				m_SpnAutoCrouch->SetCurrentItem(gAutoCrouchItems[i].LocalizedTextKey);
				break;
			}
		}
	}

	int lookSensitivity = static_cast<int>(CUIGameData::Get()->GetLookSensitivity() * (static_cast<float>(LOOK_SENSITIVITY_MAX + LOOK_SENSITIVITY_MIN) / 2.0f));
	for(i = 0; i < sizeof(gLookSensitivityItems) / sizeof(GenericSpinnerItem); ++i)
	{
		if(gLookSensitivityItems[i].UIGameDataValue == lookSensitivity)
		{
			m_SpnLookSensitivity->SetCurrentItem(gLookSensitivityItems[i].LocalizedTextKey);
			break;
		}
	}
}

//=============================================================================
void GameOptions::OnSetCurrentItem(const char* panelName)
{
	if(!Q_strcmp(panelName, m_SpnInvertYAxis->GetName()))
	{
		CGameUIConVarRef joy_inverty("joy_inverty");
		if(joy_inverty.IsValid())
		{
			joy_inverty.SetValue(m_SpnInvertYAxis->GetActiveItemUserData());
		}
	}
	else if(!Q_strcmp(panelName, m_SpnVibration->GetName()))
	{
		CGameUIConVarRef cl_rumblescale("cl_rumblescale");
		if(cl_rumblescale.IsValid())
		{
			cl_rumblescale.SetValue(static_cast<float>(m_SpnVibration->GetActiveItemUserData()) / 5.0f);
		}
	}
	else if(!Q_strcmp(panelName, m_SpnAutoCrouch->GetName()))
	{
		CGameUIConVarRef cl_autocrouch("cl_autocrouch");
		if(cl_autocrouch.IsValid())
		{
			cl_autocrouch.SetValue(m_SpnAutoCrouch->GetActiveItemUserData());
		}
	}
	else if(!Q_strcmp(panelName, m_SpnLookSensitivity->GetName()))
	{
		CUIGameData::Get()->SetLookSensitivity(GetMult(LOOK_SENSITIVITY_MIN, LOOK_SENSITIVITY_MAX, m_SpnLookSensitivity->GetActiveItemUserData()));
	}
}