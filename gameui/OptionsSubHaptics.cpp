//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "OptionsSubHaptics.h"
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

bool HasFalcon() {
	ConVarRef hap_hasDevice("hap_hasDevice");
	return (hap_hasDevice.IsValid() && hap_hasDevice.GetBool());
}

void COptionsSubHaptics::OnCommand(const char *command)
{
	if ( !stricmp( command, "DoDefaults" ) )
	{
		engine->ClientCmd_Unrestricted("exec haptics_default.cfg");
		OnResetData();
	}

}
COptionsSubHaptics::COptionsSubHaptics(vgui::Panel *parent) : PropertyPage(parent, NULL)
{

	m_pForceMasterPreLabel = new Label(this, "ForceMasterPreLabel", "#GameUI_Haptics_ForceMasterScale" );
	m_pForceMasterSlider  = new CCvarSlider( this, "ForceMasterSlider", "#GameUI_Haptics_ForceMasterScale",
		0.000001f, 3.0f, "hap_ForceMasterScale", true );

	m_pForceRecoilPreLabel = new Label(this, "ForceRecoilPreLabel", "#GameUI_Haptics_RecoilScale" );
	m_pForceRecoilSlider  = new CCvarSlider( this, "ForceRecoilSlider", "#GameUI_Haptics_RecoilScale",
		0.000001f, 3.0f, "hap_ForceRecoilScale", true );

	m_pForceDamagePreLabel = new Label(this, "ForceDamagePreLabel", "#GameUI_Haptics_DamageScale" );
	m_pForceDamageSlider  = new CCvarSlider( this, "ForceDamageSlider", "#GameUI_Haptics_DamageScale",
		0.000001f, 5.0f, "hap_ForceDamageScale" , true );

	m_pForceMovementPreLabel = new Label(this, "ForceMovementPreLabel", "#GameUI_Haptics_MovementScale" );
	m_pForceMovementSlider  = new CCvarSlider( this, "ForceMovementSlider", "#GameUI_Haptics_MovementScale",
		0.000001f, 3.0f, "hap_ForceMovementScale", true );

	//Player scaling
	m_pPlayerBoxPreLabel = new Label(this, "PlayerBoxPreLabel", "#GameUI_Haptics_PlayerBoxLabel" );
	m_pPlayerBoxScalePreLabel = new Label(this, "PlayerScalePreLabel", "#GameUI_Haptics_Scale" );
	m_pPlayerBoxScale  = new CCvarSlider( this, "PlayerBoxScaleSlider", "#GameUI_Haptics_PlayerBoxScale",
		0.1f, 0.9f, "hap_PlayerBoxScale", true );
	m_pPlayerBoxVisual = new ControlBoxVisual(this, "PlayerBoxVisual", m_pPlayerBoxScale, m_pPlayerBoxScale, m_pPlayerBoxScale, m_pPlayerBoxScale, m_pPlayerBoxScale, m_pPlayerBoxScale);
	m_pPlayerBoxStiffnessPreLabel = new Label(this, "PlayerStiffnessPreLabel", "#GameUI_Haptics_PlayerBoxStiffness" );
	m_pPlayerBoxStiffnessSlider = new CCvarSlider( this, "PlayerStiffnessSlider", "#GameUI_Haptics_PlayerBoxScale",
		0.0f, 3.0f, "hap_PlayerBoxStiffness", true );
	m_pPlayerBoxTurnPreLabel = new Label(this, "PlayerTurnPreLabel", "#GameUI_Haptics_Turning" );
	m_pPlayerBoxTurnSlider = new CCvarSlider( this, "PlayerTurnSlider", "#GameUI_Haptics_Turning",
		0.0f, 2.0f, "hap_PlayerTurnScale", true );
	m_pPlayerBoxAimPreLabel = new Label(this, "PlayerAimPreLabel", "#GameUI_Haptics_Aiming" );
	m_pPlayerBoxAimSlider = new CCvarSlider( this, "PlayerAimSlider", "#GameUI_Haptics_Aiming",
		0.0f, 2.0f, "hap_PlayerAimScale", true );


	//Vehicle Scaling
	m_pVehicleBoxPreLabel = new Label(this, "VehicleBoxPreLabel", "#GameUI_Haptics_VehicleBoxLabel" );
	m_pVehicleBoxScalePreLabel = new Label(this, "VehicleScalePreLabel", "#GameUI_Haptics_Scale" );
	m_pVehicleBoxScale  = new CCvarSlider( this, "VehicleBoxScaleSlider", "#GameUI_Haptics_VehicleBoxScale",
		0.1f, 0.6f, "hap_VehicleBoxScale", true );
	m_pVehicleBoxVisual = new ControlBoxVisual(this, "VehicleBoxVisual", m_pVehicleBoxScale, m_pVehicleBoxScale, m_pVehicleBoxScale, m_pVehicleBoxScale, m_pVehicleBoxScale, m_pVehicleBoxScale);
	m_pVehicleBoxStiffnessPreLabel = new Label(this, "VehicleStiffnessPreLabel", "#GameUI_Haptics_VehicleBoxStiffness" );
	m_pVehicleBoxStiffnessSlider = new CCvarSlider( this, "VehicleStiffnessSlider", "#GameUI_Haptics_VehicleBoxScale",
		0.0f, 3.0f, "hap_VehicleBoxStiffness", true );
	m_pVehicleBoxTurnPreLabel = new Label(this, "VehicleTurnPreLabel", "#GameUI_Haptics_Turning" );
	m_pVehicleBoxTurnSlider = new CCvarSlider( this, "VehicleTurnSlider", "#GameUI_Haptics_Turning",
		0.0f, 2.0f, "hap_VehicleTurnScale", true );
	m_pVehicleBoxAimPreLabel = new Label(this, "VehicleAimPreLabel", "#GameUI_Haptics_Aiming" );
	m_pVehicleBoxAimSlider = new CCvarSlider( this, "VehicleAimSlider", "#GameUI_Haptics_Aiming",
		0.0f, 2.0f, "hap_VehicleAimScale", true );


	vgui::Button *defaults = new vgui::Button( this, "Defaults", "Use Defaults" );
	defaults->SetCommand("DoDefaults");

	LoadControlSettings("Resource\\OptionsSubHaptics.res");
	UpdateVehicleEnabled();

}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubHaptics::~COptionsSubHaptics()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubHaptics::OnResetData()
{
	m_pForceMasterSlider->Reset();
	m_pForceDamageSlider->Reset();
	m_pForceRecoilSlider->Reset();
	m_pForceMovementSlider->Reset();

	m_pPlayerBoxScale->Reset();
	m_pPlayerBoxStiffnessSlider->Reset();
	m_pPlayerBoxTurnSlider->Reset();
	m_pPlayerBoxAimSlider->Reset();

	m_pVehicleBoxScale->Reset();
	m_pVehicleBoxStiffnessSlider->Reset();
	m_pVehicleBoxTurnSlider->Reset();
	m_pVehicleBoxAimSlider->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubHaptics::OnApplyChanges()
{
	m_pForceMasterSlider->ApplyChanges();
	m_pForceDamageSlider->ApplyChanges();
	m_pForceRecoilSlider->ApplyChanges();
	m_pForceMovementSlider->ApplyChanges();

	m_pPlayerBoxScale->ApplyChanges();
	m_pPlayerBoxStiffnessSlider->ApplyChanges();
	m_pPlayerBoxTurnSlider->ApplyChanges();
	m_pPlayerBoxAimSlider->ApplyChanges();

	m_pVehicleBoxScale->ApplyChanges();
	m_pVehicleBoxStiffnessSlider->ApplyChanges();
	m_pVehicleBoxTurnSlider->ApplyChanges();
	m_pVehicleBoxAimSlider->ApplyChanges();


	//Write out our config file
	engine->ClientCmd_Unrestricted("writehapticconfig");
	engine->ClientCmd_Unrestricted("reloadhaptics"); 
}

//-----------------------------------------------------------------------------
// Purpose: sets background color & border
//-----------------------------------------------------------------------------
void COptionsSubHaptics::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubHaptics::OnControlModified(Panel *panel)
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubHaptics::OnTextChanged(Panel *panel)
{
}

void COptionsSubHaptics::UpdateVehicleEnabled()
{
	ConVarRef checkVehicle("hap_ui_vehicles");
	checkVehicle.Init("hap_ui_vehicles",true);
	bool bEnabled = checkVehicle.GetBool();

	m_pVehicleBoxPreLabel->SetEnabled( bEnabled );
	m_pVehicleBoxScalePreLabel->SetEnabled( bEnabled );
	m_pVehicleBoxScale->SetEnabled( bEnabled );
	m_pVehicleBoxStiffnessPreLabel->SetEnabled( bEnabled );
	m_pVehicleBoxStiffnessSlider->SetEnabled( bEnabled );
	m_pVehicleBoxTurnPreLabel->SetEnabled( bEnabled );
	m_pVehicleBoxTurnSlider->SetEnabled( bEnabled );
	m_pVehicleBoxAimPreLabel ->SetEnabled( bEnabled );
	m_pVehicleBoxAimSlider ->SetEnabled( bEnabled );
}
